/**
 * @file StatefulGSMLib.h
 * @brief SIM800L GSM Module class definition
 */

 #ifndef SIM800L_H
 #define SIM800L_H
 
 #include <Arduino.h>
 #include "StatefulGSMLibconfig.h"
 
 /**
  * @brief States for the SIM800L state machine
  */
 enum SIM800L_State {
   STATE_RESET = 0,
   STATE_POST_RESET = 1,
   STATE_CHECK_AT = 2,
   STATE_CHECK_SIM = 3,
   STATE_CHECK_NETWORK = 4,
   STATE_INITIALIZE = 5,
   STATE_READY = 6
 };
 
 /**
  * @brief Class to manage SIM800L GSM/GPRS module
  */
 class SIM800L {
 public:
   /**
    * @brief Constructor
    * @param serial Serial interface for the modem
    */
   SIM800L(HardwareSerial &serial);
   
   /**
    * @brief Initialize the modem
    */
   void begin(unsigned long baudrate, int rx_pin, int tx_pin, int pwr_key_pin, int rst_pin, int pwr_ext_pin);
   
   /**
    * @brief Main state machine loop, call this in the main loop
    */
   void loop();
   int state();
   /**
    * @brief Send SMS message
    * @param number Recipient phone number
    * @param message Message content
    */
   void sendSMS(String number, String message);
   
   /**
    * @brief Get signal strength
    * @return Signal strength (0-31, 99=unknown)
    */
   int getSignalStrength();
   
   
   /**
    * @brief Initialize TCP connection
    * @param host Server host address
    * @param port Server port number
    * @return true if initialization successful
    */
   bool initTCP(String host, int port);
   
   /**
    * @brief Initialize UDP connection
    * @param host Server host address
    * @param port Server port number
    * @return true if initialization successful
    */
   bool initUDP(String host, int port);
   
   /**
    * @brief Send data over TCP/UDP connection
    * @param data Data to send
    * @return true if send successful
    */
   bool sendData(String data);
   
   /**
    * @brief Receive data from TCP/UDP connection
    * @param timeout Timeout in milliseconds
    * @return Received data as string
    */
   String receiveData(unsigned long timeout);
   
   /**
    * @brief Close TCP/UDP connection
    * @return true if close successful
    */
   bool closeConnection();
   
   // SMS data and flags
   String receivedNumber;
   String receivedMessage;
   bool sms_available;    // Flag indicating new SMS is available for processing
   
    String lastErrorMessage; // Store last error message for debugging

 
 private:
   // Hardware interfaces
   HardwareSerial &_serial;
   //int _rxPin;
   //int _txPin;
   int _pwr_key_pin; // sim800 internal power switch
   int _pwr_ext_pin=-1; // external power switch using a transistor 
   int _rst_pin=-1;


   
   // State tracking
   SIM800L_State _modemState;
   bool _unreadSMS;
   bool _atAckOK;
   bool _smsLoaded;
   
   // Counters
   uint8_t _counterATDead;
   uint8_t _counterNoNetwork;
   uint8_t _counterCommFailures;
   uint16_t _modemResetCounts;
   int _signalStrength;
   
   // Timers
   unsigned long _lastSimReset;
   unsigned long _lastAliveCheck;
   unsigned long _lastNetworkOK;
   unsigned long _regularTimer;
   unsigned long _networkHealthTime;
   unsigned long _lastTxTry;
   
   // SMS buffers
   String _txBuffMsg;
   String _txBuffNum;
   
   // Private methods
   void resetModem();
   bool checkATAlive();
   bool checkSimAvailable();
   bool hasNetwork();
   int getRSSI();
   bool initialSettings();
   bool initializeTxSmsSettings();
   bool checkSMSFifo();
   
   void sendAT(String command);
   String checkResponse(unsigned long wait, bool returnAtOK);
   int extractParam(String response, String confirmHeader, int paramNum);
   String extractSMSCNumber(String response);
   
   bool txSMS();
   void handleTxSmsLoop();
   // Methods for SMS handling
   bool checkIfSMSWasSent();
   void resetBufferState();
   void abortSMSAndReset();
   
   void turnOffNetlight();
   void turnOnNetlight();
 };
 
 #endif // SIM800L_H