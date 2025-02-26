/**
 * @file StatefulGSMLib.cpp
 * @brief Implementation of SIM800L class methods
 */

#include "StatefulGSMLib.h"

#if SERIAL_LOG_LEVEL > 0
#define LOG_ERROR(x) Serial.println(x)
#else
#define LOG_ERROR(x)
#endif

#if SERIAL_LOG_LEVEL > 0
#define LOG_WARN(x) Serial.println(x)
#else
#define LOG_WARN(x)
#endif

#if SERIAL_LOG_LEVEL > 1
#define LOG_INFO(x) Serial.println(x)
#else
#define LOG_INFO(x)
#endif

#if SERIAL_LOG_LEVEL > 1
#define LOG_DEBUG(x) Serial.println(x)
#else
#define LOG_DEBUG(x)
#endif

 /**
  * Constructor
  */
 SIM800L::SIM800L(HardwareSerial &serial) : _serial(serial),
 _modemState(STATE_RESET),
 _unreadSMS(false),
 _atAckOK(false),
 _smsLoaded(false),
 _counterATDead(0),
 _counterNoNetwork(0),
 _counterCommFailures(0),
 _modemResetCounts(0),
 _signalStrength(0),
 _lastSimReset(0),
 _lastAliveCheck(0),
 _lastNetworkOK(0),
 _regularTimer(0),
 _networkHealthTime(0),
 _lastTxTry(0),
 sms_available(false),
 lastErrorMessage("") {
 
  _txBuffMsg.reserve(160);
}
 
 /**
  * Initialize the modem
  */
 void SIM800L::begin(unsigned long baudrate, int rx_pin, int tx_pin, int pwr_key_pin, int rst_pin, int pwr_ext_pin) {
   // Configure serial port for modem
   _serial.begin(baudrate, SERIAL_8N1, rx_pin, tx_pin);
   delay(500);
   _pwr_key_pin = pwr_key_pin;
    _rst_pin = rst_pin;
    _pwr_ext_pin = pwr_ext_pin;
    pinMode(_pwr_key_pin, OUTPUT);
    #if (_rst_pin != -1)
    pinMode(_rst_pin, OUTPUT);
    #endif
    #if (_pwr_ext_pin != -1)
    pinMode(_pwr_ext_pin, OUTPUT);
    #endif
   // Reset modem to start fresh
   resetModem();
   delay(500);
 }
 
 /**
  * Main loop handling the state machine
  */
 void SIM800L::loop() {
   // Get current time
   unsigned long mills = millis();
   
   // Handle state machine
   switch (_modemState) {
     case STATE_RESET:
       if ((mills < 10000) || ((mills - _lastSimReset) > MODEM_REGULAR_RESET)) {
         LOG_INFO("\nSIM: Power reset");
         resetModem(); // takes 2.7 seconds
         LOG_INFO("\nSIM: reset done");
         _lastSimReset = millis();
         _counterATDead = 0;
         _counterNoNetwork = 0;
         
         _modemResetCounts += 1;
         _modemState = STATE_POST_RESET;
       }
       break;
       
     case STATE_POST_RESET:
       if ((mills - _lastSimReset) > MODEM_RESET_WAIT) {
         #if SERIAL_LOG_LEVEL>0
         Serial.println("\nSIM: After reset wait");
         #endif
         _counterATDead = 0;
         _counterNoNetwork = 0;
         _modemState = STATE_CHECK_AT;
       }
       break;
       
     case STATE_CHECK_AT:
       if ((mills - _lastAliveCheck) > 1000) {
         #if SERIAL_LOG_LEVEL>0
         Serial.println("\nSIM: Check AT alive");
         #endif
         if (checkATAlive()) {
           _counterATDead = 0;
           _modemState = STATE_CHECK_SIM;
         } else {
           _counterATDead++;
           if (_counterATDead > 5) 
           {
            LOG_ERROR("SIM: AT dead. Check wiring.");
           }
           if (_counterATDead > MAX_AT_RETRIES) 
           {
            _modemState = STATE_RESET;
           }
         }
         _lastAliveCheck = millis();
       }
       break;
       
     case STATE_CHECK_SIM:
       if ((((_counterNoNetwork < 3) && ((mills - _lastAliveCheck) > 1000)) || ((mills - _lastAliveCheck) > 30000))) {
         #if SERIAL_LOG_LEVEL>0
         Serial.println("\nSIM: Check Sim");
         #endif
         if (checkSimAvailable()) {
           _counterATDead = 0;
           _counterNoNetwork = 0;
           _modemState = STATE_CHECK_NETWORK;
         } else {
           #if SERIAL_LOG_LEVEL>0
           Serial.print("\nSIM: No Sim. errors: "); Serial.println(_counterNoNetwork);
           #endif
           _counterNoNetwork++;
           if (_counterNoNetwork > 100) _modemState = STATE_RESET;
         }
         _lastAliveCheck = millis();
       }
       break;
       
     case STATE_CHECK_NETWORK:
       if ((((_counterNoNetwork < 3) && ((mills - _lastAliveCheck) > 1000)) || ((mills - _lastAliveCheck) > 10000))) {
         #if SERIAL_LOG_LEVEL>0
         Serial.println("\nSIM: Check Network"); // and signal strength
         #endif
         if (hasNetwork()) {
           _counterATDead = 0;
           _counterNoNetwork = 0;
           _modemState = STATE_INITIALIZE;
           _signalStrength = getRSSI();
           if (_signalStrength == 0) {
             #if SERIAL_LOG_LEVEL>0
             Serial.println("SIM: No signal");
             #endif
           }
           _lastNetworkOK = millis();
         } else {
           _counterNoNetwork++;
           #if SERIAL_LOG_LEVEL>0
           Serial.print("\nSIM: No network. errors: "); Serial.println(_counterNoNetwork);
           #endif
           if (_counterNoNetwork > MAX_NETWORK_RETRIES) _modemState = STATE_RESET; // 5 minutes
         }
         _lastAliveCheck = millis();
       }
       break;
       
     case STATE_INITIALIZE:
       if ((((_counterATDead < 3) && ((mills - _lastAliveCheck) > 1000)) || ((mills - _lastAliveCheck) > 5000))) {
         #if SERIAL_LOG_LEVEL>0
         Serial.println("\nSIM: Initial Settings");
         #endif
         if (initialSettings() || ((_counterATDead > 5) && (_modemResetCounts > 2))) {
           _counterATDead = 0;
           _modemState = STATE_READY;
           _signalStrength = getRSSI();
           if (_signalStrength == 0) {
             #if SERIAL_LOG_LEVEL>0
             Serial.println("SIM: No signal");
             #endif
           }
         } else {
           #if SERIAL_LOG_LEVEL>0
           Serial.print("\nSIM: settings fail. errors: "); Serial.println(_counterATDead);
           #endif
           _counterATDead++;
           if (_counterATDead > 30) _modemState = STATE_RESET;
           else if (_counterATDead > 3) {
             //check sms anyways
             if (checkSMSFifo()) {
                sms_available = true;
               _modemState = STATE_READY;
             } // move on if rx sms successfully
             handleTxSmsLoop();
           }
         }
         _lastAliveCheck = millis();
       }
       break;
    // Improved case STATE_READY section for the loop() method in SIM800L.cpp

    case STATE_READY: {
        unsigned long mills = millis();
        
        // First priority - process SMS if buffer is jammed
        if (_smsLoaded && _counterCommFailures > 2) {
          LOG_INFO("Processing stuck SMS first");
          handleTxSmsLoop();
        }
        
        // Second priority - check for new SMS
        if (_unreadSMS) {
          LOG_INFO("\nSIM: Processing SMS notification");
          resetBufferState(); // Reset buffer before checking
          if (checkSMSFifo()) {
            sms_available = true;
          }
          _unreadSMS = false;
        }
        
        // Regular SMS check interval
        if ((mills - _regularTimer) > SMS_CHECK_INTERVAL) {
          LOG_INFO("\nSIM: Regular SMS check");
          resetBufferState(); // Reset buffer before checking
          
          for (uint8_t i = 0; i < MAX_SMS_CHECK_PER_CYCLE; i++) {
            if (checkSMSFifo()) {
              sms_available = true;
            } else {
              break;
            }
          }
          _regularTimer = millis();
        } 
        // Network health check
        else if ((mills - _networkHealthTime) > NETWORK_HEALTH_CHECK) {
          _signalStrength = getRSSI();
          if (_signalStrength == 0) {
            LOG_INFO("SIM: No signal");
          } else {
            _networkHealthTime = millis();
          }
          
          if ((millis() - _networkHealthTime) > NETWORK_RESET_TIMEOUT) {
            _modemState = STATE_RESET;
          }
        }
        
        // Last priority - handle SMS sending
        if (!_unreadSMS) {
          handleTxSmsLoop();
        }
        
        // Minimal buffer check
        checkResponse(20, false);
        break;
      }

        
       
     default:
       LOG_ERROR("Invalid state");
       _modemState = STATE_CHECK_AT;
       break;
   }
 }
 
 int SIM800L::state() { return _modemState;}

/**
 * Send SMS message (queues it for sending)
 */
void SIM800L::sendSMS(String number, String message) {
    // Clean up any old messages first
    if (_smsLoaded && ((millis() - _lastTxTry) > 10000)) {
      #if SERIAL_LOG_LEVEL>0
      Serial.println("Clearing stuck SMS in queue");
      #endif
      resetBufferState(); // Reset buffer state before clearing
      _txBuffNum = "";
      _txBuffMsg = "";
      _smsLoaded = false;
    }
    
    _txBuffNum = number;
    _txBuffMsg = message;
    _smsLoaded = true;
    _lastTxTry = 0; // Reset timer to force immediate sending attempt
  }
  
 
 /**
  * Get signal strength
  */
 int SIM800L::getSignalStrength() {
   return _signalStrength;
 }
 

 
 /**
  * Reset modem hardware
  */
 void SIM800L::resetModem() {
   // Keep reset high
   #if (_rst_pin != -1)
     pinMode(_rst_pin, OUTPUT);
     digitalWrite(_rst_pin, HIGH);
   #endif

   #if (_pwr_ext_pin != -1)
   pinMode(_pwr_ext_pin, OUTPUT);
   #endif
      
    pinMode(_pwr_key_pin, OUTPUT);
    digitalWrite(_pwr_key_pin, HIGH);


    // Power cycle sequence
    // Turn off power completely
    #if (_pwr_ext_pin != -1)
    digitalWrite(_pwr_ext_pin, LOW);
    #endif
    digitalWrite(_pwr_key_pin, LOW);
    delay(1000);

    // Turn on the Modem power
    #if (_pwr_ext_pin != -1)
    digitalWrite(_pwr_ext_pin, HIGH);
    #endif
    //Serial.println("Main power ON");
    delay(500);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    //Serial.println("PWRKEY sequence starting");
    digitalWrite(_pwr_key_pin, HIGH);
    delay(100);
    digitalWrite(_pwr_key_pin, LOW);
    delay(1200);  // Increased for reliability
    digitalWrite(_pwr_key_pin, HIGH);
    
 }
 
 /**
  * Check if AT command interface is responsive
  */
 bool SIM800L::checkATAlive() {
   checkResponse(100, false); // Clear input buffer first
 
   String resp = "";
   for (uint8_t i = 0; i < 3; i++) {
     sendAT("");
     resp = checkResponse(1000, true);
     if (resp.indexOf("OK") != -1) {
       return true;
     }
     delay(100);
   }
   
   if (_counterATDead > 10) {
     _counterATDead = 1;
     // If basic AT fails, check CIPSTATUS
     sendAT("+CIPSTATUS");
     
     resp = checkResponse(3000, false);
 
     if ((resp.indexOf("STATE: IP INITIAL") != -1) || 
         (resp.indexOf("STATE: IP START") != -1) || 
         (resp.indexOf("STATE: IP CONFIG") != -1) || 
         (resp.indexOf("STATE: IP GPRSACT") != -1)) return true;
     
     // Check for error states
     if (resp.indexOf("STATE: IP CLOSE") != -1 || 
         resp.indexOf("STATE: PDP DEACT") != -1 ||
         resp.indexOf("STATE: CONNECT FAIL") != -1) {
       return false;
     }
   }
 
   return false;
 }
 
 /**
  * Check if SIM card is available
  */
 bool SIM800L::checkSimAvailable() {
   sendAT("+CMGF=1");
   String resp = checkResponse(1000, true);
   
   if (resp.indexOf("ERROR:") != -1) { //+CME ERROR: SIM not inserted or +CME ERROR: operation not allowed
     return false;
   } else if (resp.indexOf("OK") != -1) {
     return true;
   }
   
   return false;
 }
 
 /**
  * Check if network is available
  */
 bool SIM800L::hasNetwork() {
   sendAT("+CREG?");  // Check network registration status
   String resp = checkResponse(1000, true);
 
   int status = extractParam(resp, "+CREG:", 2);
   
   /*
   Network registration status codes:
   0 = Not registered, MT is not currently searching a new operator
   1 = Registered, home network
   2 = Not registered, but MT is currently searching for a new operator
   3 = Registration denied
   4 = Unknown
   5 = Registered, roaming
   */
   
   // Only return true if properly registered (home network or roaming)
   return (status == 1 || status == 5);
 }
 
 /**
  * Get signal strength (RSSI)
  */
 int SIM800L::getRSSI() {
   sendAT("+CSQ");  //check signal quality
   String resp = checkResponse(1000, true);
 
   int rssi = extractParam(resp, "+CSQ:", 1);
   
   #if SERIAL_LOG_LEVEL>0
   Serial.print("\nRSSI=");
   Serial.println(rssi);
  #endif
   if ((rssi >= 99) || (rssi == -1)) return 0;
   else return rssi;
 }
 
 /**
  * Initialize modem settings
  */
 bool SIM800L::initialSettings() {
   checkResponse(100, false);
   // Initial configuration
   sendAT("");  // Test AT
   checkResponse(1000, true);
   
   sendAT("E0");  // Turn off echo
   checkResponse(1000, true);
   
   sendAT("+CMEE=2");  // Enable verbose error messages
   checkResponse(1000, true);
 
   // Initialize SMS notification
   sendAT("+CMGF=1"); // Set SMS text mode
   checkResponse(1000, true);
   
   if (_atAckOK == true) {
     sendAT("+CNMI=1,1,0,0,0"); // Configure new message notifications
     checkResponse(1000, true);
     
     // Configure message format
     if (_atAckOK == true) {
       sendAT("+CSMP=17,167,0,0");  // Set SMS parameters
       checkResponse(1000, true);
     }
   }
 
   if (initializeTxSmsSettings() == false) return false;
   
   return _atAckOK;  
 }
 
 /**
  * Initialize SMS sending settings
  */
 bool SIM800L::initializeTxSmsSettings() {
   _counterCommFailures = 0;
   // Set SMS text mode
   sendAT("+CMGF=1");
   if (!checkResponse(1000, true)) {
    
    #if SERIAL_LOG_LEVEL>0
     Serial.println("Failed to set SMS mode");
    #endif
     return false;
   }
 
   // Check current SMSC
   sendAT("+CSCA?");
   String response = checkResponse(1000, true);
   
   // Extract and store the SMSC number
   String smsc = extractSMSCNumber(response);
   if (smsc.length() > 0) {
    
    #if SERIAL_LOG_LEVEL>0
     Serial.print("\nSMSC=");
     Serial.println(smsc);  // currently not stored or used
    #endif
   } else {
    
    #if SERIAL_LOG_LEVEL>0
     Serial.println("Failed to detect SMSC number");
    #endif
     return false;
   }
   
   // Set message parameters for concatenated messages support
   sendAT("+CSMP=17,167,0,0");
   if (!checkResponse(1000, true)) {
    
    #if SERIAL_LOG_LEVEL>0
     Serial.println("Failed to set message parameters");
    #endif
     return false;
   }
 
   return true;
 }
 
 /**
  * Send AT command to modem
  */
 void SIM800L::sendAT(String command) {
  #if PRINT_RAW_AT != 0
   Serial.println("\r\nAT >> " + command);
 #endif
   _serial.print("AT");
   _serial.println(command);
 }
 
 /**
  * Check for response from modem
  */
 String SIM800L::checkResponse(unsigned long wait, bool returnAtOK) {
   String s = "";
   unsigned long waiter = 0;
   unsigned long wait_extendable = wait;
   _atAckOK = false;
   while (waiter <= wait_extendable) {
    while (_serial.available()) {
            char c = _serial.read();
             #if PRINT_RAW_AT != 0
            Serial.write(c);
            #endif
            s += c;
        }
        waiter = waiter + 1;
    if (returnAtOK == true) {
            if (s.indexOf("OK") != -1) {
               #if PRINT_RAW_AT != 0
            Serial.write('\n');
            #endif
                break;
            } else delay(1);
        }
    else
        {
            if ((waiter >= wait_extendable) && (wait_extendable < 100) )    // wait a  little more if there is something but line not complete
            {  
                int len = s.length();
                if (len >0)
                if ((len<6) or (s.indexOf("\n") == -1))
                {
                    wait_extendable += 10;
                }
            }
            delay(1);
        }
   }


 
   if (s.indexOf("+CMTI") != -1) { 
     _unreadSMS = true;
     #if SERIAL_LOG_LEVEL>0
     Serial.println("\tNEW SMS received!!!");
     #endif
     _networkHealthTime = millis();
   } else if (s.indexOf("PSUT") != -1) {  // *PSUTTZ: 2025,2,6,20,58,31,"+0",0
     _networkHealthTime = millis();
   }
 
   if (s.indexOf("OK") != -1) {
     _atAckOK = true;
   }


   // Check for error messages and store them
    if (s.indexOf("ERROR") != -1) {
        int errorStart = s.indexOf("ERROR");
        if (errorStart != -1) {
        int lineEnd = s.indexOf("\r\n", errorStart);
        if (lineEnd != -1) {
            lastErrorMessage = s.substring(errorStart, lineEnd);
            #if SERIAL_LOG_LEVEL>0
            Serial.print("Error detected: ");
            Serial.println(lastErrorMessage);
            #endif
        }
        }
    }
 
   return s;
 }
 
 /**
  * Extract parameter from AT command response
  */
 int SIM800L::extractParam(String resp, String confirmHeader, int paramNum) {
   if (resp.indexOf("ERROR") != -1) return -1;
 
   int idx1 = resp.indexOf(confirmHeader);
   if (idx1 >= 0) {
     int idx2 = resp.indexOf(',');
     if ((idx2 >= 0) || ((paramNum == 1) && (resp.indexOf('\n') > 0))) {
       if (paramNum == 2) {
         resp = resp.substring(idx2 + 1);
         idx2 = resp.indexOf('\n');
         resp = resp.substring(0, idx2);
         return resp.toInt();
       } else {
         if (idx2 == -1) idx2 = resp.indexOf('\n');
         resp = resp.substring(idx1 + confirmHeader.length() + 1, idx2);
         return resp.toInt();
       }
     }
   }
   return -1;
 }
 
 /**
  * Extract SMSC number from AT response
  */
 String SIM800L::extractSMSCNumber(String response) {
   // Find the CSCA response
   int start = response.indexOf("+CSCA: \"");
   if (start == -1) return "";
   
   // Move past the +CSCA: " part
   start += 8;
   
   // Find the end quote
   int end = response.indexOf("\"", start);
   if (end == -1) return "";
   
   // Extract just the number
   return response.substring(start, end);
 }
 
 /**
  * Check for unread SMS and read them
  */
 bool SIM800L::checkSMSFifo() {
   sendAT("+CMGF=1");  // Set SMS text mode
   if (!checkResponse(1000, true)) return false;
   
   sendAT("+CMGL=\"REC UNREAD\"");  // List only unread messages
   String response = checkResponse(2000, true);
   
   if (response.indexOf("+CMGL:") == -1) {
     return false;  // No unread messages found
   }
   
   // Parse the response to get the message details
   int msgIndex = response.indexOf("+CMGL:");
   if (msgIndex != -1) {
     // Extract message ID - we'll need this for deleting the message
     int idStart = msgIndex + 6;
     int idEnd = response.indexOf(",", idStart);
     int messageId = response.substring(idStart, idEnd).toInt();
     
     // Extract phone number
     int phoneStart = response.indexOf("\",\"", msgIndex) + 3;
     int phoneEnd = response.indexOf("\",\"", phoneStart);
     receivedNumber = response.substring(phoneStart, phoneEnd);
     
     // Find where the actual message content starts
     int contentStart = response.indexOf("\r\n", phoneEnd) + 2;
     int contentEnd = response.indexOf("\r\n\r\nOK");
     if (contentEnd == -1) contentEnd = response.indexOf("\r\nOK");
     if (contentEnd == -1) contentEnd = response.length();
     
     receivedMessage = response.substring(contentStart, contentEnd);
     receivedMessage.trim();  // Remove any trailing whitespace
     
     
     #if SERIAL_LOG_LEVEL>0
     Serial.print("\nMSG ID: ");
     Serial.println(messageId);
     #endif
     
     // Delete the read message
     sendAT("+CMGD=" + String(messageId));
     checkResponse(1000, true);
     return true;
   }
   
   return false;
 }

















 
 
 /**
  * Initialize TCP connection
  */
 bool SIM800L::initTCP(String host, int port) {
   // Close any existing connections
   sendAT("+CIPSHUT");
   checkResponse(5000, true);
   
   // Set connection mode to single connection
   sendAT("+CIPMUX=0");
   if (!checkResponse(1000, true)) return false;
   
   // Set APN info - may need to adjust for your carrier
   sendAT("+CSTT=\"internet\",\"\",\"\"");
   if (!checkResponse(1000, true)) return false;
   
   // Bring up wireless connection
   sendAT("+CIICR");
   if (!checkResponse(10000, true)) return false;
   
   // Get local IP address
   sendAT("+CIFSR");
   String ipResponse = checkResponse(2000, true);
   if (ipResponse.indexOf(".") == -1) return false;
   
   // Start TCP connection
   sendAT("+CIPSTART=\"TCP\",\"" + host + "\"," + String(port));
   String response = checkResponse(1000, true);
   response += checkResponse(10000, true);
   
   return (response.indexOf("CONNECT OK") != -1);
 }
 
 /**
  * Initialize UDP connection
  */
 bool SIM800L::initUDP(String host, int port) {
   // Close any existing connections
   sendAT("+CIPSHUT");
   checkResponse(5000, true);
   
   // Set connection mode to single connection
   sendAT("+CIPMUX=0");
   if (!checkResponse(1000, true)) return false;
   
   // Set APN info - may need to adjust for your carrier
   sendAT("+CSTT=\"internet\",\"\",\"\"");
   if (!checkResponse(1000, true)) return false;
   
   // Bring up wireless connection
   sendAT("+CIICR");
   if (!checkResponse(10000, true)) return false;
   
   // Get local IP address
   sendAT("+CIFSR");
   String ipResponse = checkResponse(2000, true);
   if (ipResponse.indexOf(".") == -1) return false;
   
   // Start UDP connection
   sendAT("+CIPSTART=\"UDP\",\"" + host + "\"," + String(port));
   String response = checkResponse(1000, true);
   response += checkResponse(10000, true);
   
   return (response.indexOf("CONNECT OK") != -1);
 }
 
 /**
  * Send data over TCP/UDP connection
  */
 bool SIM800L::sendData(String data) {
   // Start data sending mode
   sendAT("+CIPSEND");
   String response = checkResponse(5000, true);
   
   if (response.indexOf(">") == -1) return false;
   
   // Send the data
   _serial.print(data);
   _serial.write(26); // Ctrl+Z to end the data input
   
   response = checkResponse(10000, true);
   return (response.indexOf("SEND OK") != -1);
 }
 
 /**
  * Receive data from TCP/UDP connection
  */
 String SIM800L::receiveData(unsigned long timeout) {
   String data = "";
   unsigned long startTime = millis();
   
   while ((millis() - startTime) < timeout) {
     if (_serial.available()) {
       char c = _serial.read();
       data += c;
       
       // Check for the data received indicator
       if (data.indexOf("+IPD,") != -1) {
         // Wait for the rest of the data
         delay(500);
         
         // Read all available data
         while (_serial.available()) {
           data += (char)_serial.read();
         }
         
         break;
       }
     }
     delay(10);
   }
   
   return data;
 }
 
 /**
  * Close TCP/UDP connection
  */
 bool SIM800L::closeConnection() {
   sendAT("+CIPCLOSE");
   String response = checkResponse(5000, true);
   
   sendAT("+CIPSHUT");
   response = checkResponse(5000, true);
   
   return (response.indexOf("SHUT OK") != -1);
 }
 
 




















 /**
 * Fixed SMS sending method with proper confirmation detection
 */
bool SIM800L::txSMS() {
    // Clear any pending serial data
    resetBufferState();
    
    LOG_INFO("tx_sms to: " + _txBuffNum);
    
    // Make sure we're in text mode
    sendAT("+CMGF=1");
    if (!checkResponse(1000, true)) {
      LOG_ERROR("Failed to set text mode");
      return false;
    }
    
    // Extra buffer clear before critical operation
    delay(100);
    while (_serial.available()) {
      _serial.read();
    }
    
    // Send command with proper formatting
    _serial.print("AT+CMGS=\"");
    _serial.print(_txBuffNum);
    _serial.println("\"");
    
    // Wait for '>' prompt with improved buffer handling
    unsigned long start = millis();
    bool promptFound = false;
    String response = "";
    
    while ((millis() - start) < 5000) {
      if (_serial.available()) {
        char c = _serial.read();
        response += c;
         #if PRINT_RAW_AT != 0
        Serial.print(c);
        #endif
        
        if (c == '>') {
          promptFound = true;
          // Continue reading any additional buffered data for a short time
          delay(100);
          while (_serial.available()) {
            c = _serial.read();
            response += c;
             #if PRINT_RAW_AT != 0
            Serial.print(c);
            #endif
          }
          break;
        }
      }
      delay(10);
    }
    
    if (!promptFound) {
      LOG_ERROR("Failed to get '>' prompt");
      abortSMSAndReset();
      return false;
    }
    
    // Add a bit more delay after prompt to ensure modem is ready
    delay(300);
    
    // Send message content with clear termination
    _serial.print(_txBuffMsg);
    delay(300);  // Increased delay before Ctrl+Z
    _serial.write(26);  // Ctrl+Z
    
    // Wait for send confirmation with improved handling
    start = millis();
    bool confirmed = false;
    response = "";
    int notificationCount = 0;
    
    while ((millis() - start) < 20000) {  // Extended timeout
      while (_serial.available()) {
        char c = _serial.read();
        response += c;
         #if PRINT_RAW_AT != 0
        Serial.print(c);
        #endif
      }
      
      // Count notifications that might be interfering
      if (response.indexOf("+CMTI:") != -1) {
        notificationCount++;
        // Replace the found notification to continue counting new ones
        response.replace("+CMTI:", "##COUNTED##");
        LOG_INFO("\nSMS notification during send, count: " + String(notificationCount));
      }
      
      // *** CRITICAL FIX: Improved confirmation detection that handles no newline ***
      if (response.indexOf("+CMGS:") != -1) {
        confirmed = true;
        LOG_INFO("SMS sent successfully");
        break;
      }
      
      if (response.indexOf("+CMS ERROR:") != -1) {
        LOG_ERROR("SMS send failed with CMS ERROR");
        break;
      }
      
      delay(10);
    }
    
    // Special handling for interrupted sends
    if (!confirmed && notificationCount > 0) {
      LOG_INFO("Send interrupted by " + String(notificationCount) + " notifications");
      
      // Extended waiting period proportional to notification count
      int extraWait = notificationCount * 1000;
      LOG_INFO("Waiting " + String(extraWait) + "ms for delayed confirmation");
      delay(extraWait);
      
      // Check for delayed confirmation
      response = "";
      while (_serial.available()) {
        char c = _serial.read();
        response += c;
         #if PRINT_RAW_AT != 0
        Serial.print(c);
        #endif
      }
      
      // *** CRITICAL FIX: Better detection of delayed confirmation ***
      if (response.indexOf("+CMGS:") != -1) {
        LOG_INFO("Delayed SMS confirmation received");
        confirmed = true;
      }
      
      // Last resort verification - ALWAYS do this after interrupted sends
      if (!confirmed) {
        // Give the modem time to finish any operations
        delay(2000);
        
        // Check if message was actually sent
        if (checkIfSMSWasSent()) {
          LOG_INFO("Message was sent despite missing confirmation");
          confirmed = true;
        }
      }
    }
    
    return confirmed;
  }
  
  /**
   * Enhanced check for successful send with better detection
   */
  bool SIM800L::checkIfSMSWasSent() {
    
    LOG_INFO("Verifying if SMS was actually sent...");
    
    // Clear buffer before checking
    resetBufferState();
    
    // Try checking last sent message reference
    sendAT("+CMGF=1"); 
    checkResponse(1000, true);
    
    // For SIMCOM modules, this might show the last message status
    sendAT("+CMSS?");
    String response = checkResponse(1000, false);
    
    if (response.indexOf("+CMGS:") != -1) {
      
      LOG_INFO("Found send confirmation in response!");
      return true;
    }
    
    // Check if our number is in recent messages
    sendAT("+CMGL=\"ALL\"");
    response = checkResponse(5000, true);
    
    if (response.indexOf(_txBuffNum) != -1) {
      
      LOG_INFO("Found our number in message list - SMS was sent");
      return true;
    }
    
    // As a last resort, check sent items
    sendAT("+CPMS=\"SM\"");
    checkResponse(1000, true);
    
    sendAT("+CMGL=\"STO SENT\"");
    response = checkResponse(2000, true);
    
    if (response.indexOf(_txBuffNum) != -1) {
      
      LOG_INFO("Found our number in sent items");
      return true;
    }
    
    // Also check unsent/queued messages
    sendAT("+CMGL=\"STO UNSENT\"");
    response = checkResponse(2000, true);
    
    if (response.indexOf(_txBuffMsg.substring(0, min((unsigned int)10, _txBuffMsg.length()))) != -1) {
      
      LOG_INFO("Found message in unsent queue");
      return false;  // It's still in the unsent queue
    }
    
    return false;
  }
  
  /**
   * Enhanced SMS handler with duplicate prevention
   */
  void SIM800L::handleTxSmsLoop() {
    static int backoffDelay = 2000; // Start with 2 seconds
    
    //static unsigned long lastSuccessTime = 0;
    
    
    
    if (_smsLoaded && ((millis() - _lastTxTry) > backoffDelay)) {
      LOG_INFO("\nSIM: Attempting to send SMS (backoff: " + String(backoffDelay) + "ms)");
      
      bool success = txSMS();
      
      if (success) {
        // Success - clear message and reset counters
        //lastSuccessTime = millis();
        
        _txBuffNum = "";
        _txBuffMsg = "";
        _smsLoaded = false;
        _counterCommFailures = 0;
        backoffDelay = 2000; // Reset backoff
        LOG_INFO("SMS sent successfully");
      } else {
        _counterCommFailures++;
        LOG_ERROR("SMS send failed, attempts: " + String(_counterCommFailures));
        
        // Exponential backoff - double the delay up to 1 minute max
        backoffDelay = min(backoffDelay * 2, 60000);
        
        // Even after failure, verify if it might have been sent
        if (_counterCommFailures > 0) {
          // Always check if sent anyway
          if (checkIfSMSWasSent()) {
            LOG_INFO("SMS was actually sent despite failure! Clearing queue.");
            
            //lastSuccessTime = millis();
            
            _txBuffNum = "";
            _txBuffMsg = "";
            _smsLoaded = false;
            _counterCommFailures = 0;
            backoffDelay = 2000; // Reset backoff
          }
        }
        
        if (_counterCommFailures > 4) {
          // Clear after several failures
          LOG_ERROR("Multiple failures, clearing SMS buffer");
          _txBuffNum = "";
          _txBuffMsg = "";
          _smsLoaded = false;
          backoffDelay = 2000; // Reset backoff
        }
        
        if (_counterCommFailures > MAX_TX_FAILURES) {
          LOG_ERROR("Too many tx failures. Forcing modem reset");
          _modemState = STATE_RESET;
          backoffDelay = 2000; // Reset backoff
        }
      }
      
      _lastTxTry = millis();
    }
  }
  



    /**
   * Emergency procedure to abort an SMS that's stuck
   */
   void SIM800L::abortSMSAndReset() {

    LOG_ERROR("EMERGENCY: Aborting stuck SMS");
    // Try to cancel the SMS command in progress

    _serial.write(27);  // ESC character

    delay(500);
    // Send a few line breaks to clear any partial command

    _serial.println();
    _serial.println();

    delay(500);

    // Clear anything in the buffer

    while (_serial.available()) {
      _serial.read();
    }
    // Try to get back to a sane state

    sendAT("");
    checkResponse(1000, false);


    // Force text mode

    sendAT("+CMGF=1");
    checkResponse(1000, false);
  }

  

  /**
   * Reset the buffer state to ensure clean communications
   */
  void SIM800L::resetBufferState() {

    // Clear serial buffer
    while (_serial.available()) {
      _serial.read();
    }
    // Send a break followed by a simple AT command to reset command parser
    _serial.println();
    delay(100);
    sendAT("");

    String response = checkResponse(1000, false);

    // If no response, try more aggressive recovery

    if (response.indexOf("OK") == -1) {

      LOG_ERROR("Modem not responding, trying recovery");
      
      // Send multiple breaks
      _serial.println();
      _serial.println();
      _serial.println();

      delay(500);
      
      // Try again
      sendAT("");
      checkResponse(1000, false);

    }

  }

  

 /**
  * Turn off network light
  */
 void SIM800L::turnOffNetlight() {

   _serial.println("AT+CNETLIGHT=0");
 }

 

 /**
  * Turn on network light
  */

 void SIM800L::turnOnNetlight() {
   _serial.println("AT+CNETLIGHT=1");
 }