
/**
 * SIM800L Remote Monitoring System
 * 
 * This example demonstrates a complete remote monitoring system that:
 * 1. Periodically sends sensor data to a server via UDP
 * 2. Allows remote control via SMS commands
 * 3. Sends status updates and alerts via SMS
 * 
 * It showcases both the UDP and SMS capabilities of the SIM800L library.
 */


#include "configSIM800L.h"
#include "StatefulGSMLib.h"

// Uncomment to enable debug output
// #define DEBUG_MONITORING 1

// For demonstration, we'll simulate sensors
// In a real application, include the appropriate sensor libraries
float readTemperature() { return random(1800, 3000) / 100.0; }  // Simulated temperature (18-30°C)
float readHumidity() { return random(4000, 9000) / 100.0; }     // Simulated humidity (40-90%)
float readBatteryVoltage() { return random(350, 420) / 100.0; } // Simulated battery (3.5-4.2V)

// Create hardware serial instance
HardwareSerial SerialGSM(1);  // UART1

// Create SIM800L instance
SIM800L sim800(SerialGSM);

// Server details
const char* UDP_SERVER = "monitoring.example.com";  // Replace with your server
const int UDP_PORT = 7700;                          // Replace with your port

// Timing variables
unsigned long lastDataSend = 0;
const unsigned long DATA_INTERVAL = 15 * 60 * 1000;  // 15 minutes between data transmissions
unsigned long lastStatusCheck = 0;
const unsigned long STATUS_INTERVAL = 60 * 1000;     // Check status every minute

// Alert thresholds
const float HIGH_TEMP_THRESHOLD = 28.0;
const float LOW_BATTERY_THRESHOLD = 3.6;

// SMS variables
const String AUTHORIZED_NUMBER = TARGET_PHONE;  // From config.h
bool alertSent = false;

// UDP connection state
bool udpConnected = false;

// Setup function
void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(1000);
  
  #if DEBUG_MONITORING
  Serial.println("\n===== SIM800L Remote Monitoring System =====");
  #endif
  
  // Initialize the SIM800L module
  sim800.begin(
    MODEM_BAUD_RATE,
    MODEM_RX_PIN,
    MODEM_TX_PIN,
    MODEM_PWRKEY_PIN,
    MODEM_RST_PIN,
    MODEM_PWR_EXT_PIN
  );
  
  #if DEBUG_MONITORING
  Serial.println("SIM800L initialized");
  #endif
  
  // Allow time for modem to register on the network
  delay(10000);
  
  // Send startup notification via SMS
  sim800.sendSMS(AUTHORIZED_NUMBER, "Remote Monitoring System starting up");
}

// Main loop
void loop() {
  // Run the SIM800L state machine
  sim800.loop();
  
  // Check system status periodically
  if (millis() - lastStatusCheck >= STATUS_INTERVAL) {
    checkSystemStatus();
    lastStatusCheck = millis();
  }
  
  // Send data to server periodically
  if (millis() - lastDataSend >= DATA_INTERVAL) {
    sendSensorData();
    lastDataSend = millis();
  }
  
  // Handle incoming SMS
  if (sim800.sms_available) {
    handleSmsCommand();
  }
  
  // Handle remote UDP commands (if any)
  handleUdpMessages();
  
  // Small delay to prevent tight looping
  delay(100);
}

// Check overall system status
void checkSystemStatus() {
  float temperature = readTemperature();
  float battery = readBatteryVoltage();
  int signal = sim800.getSignalStrength();
  
  #if DEBUG_MONITORING
  Serial.println("===== System Status =====");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println("°C");
  Serial.print("Battery: "); Serial.print(battery); Serial.println("V");
  Serial.print("Signal: "); Serial.println(signal);
  #endif
  
  // Check for alert conditions
  if (temperature > HIGH_TEMP_THRESHOLD && !alertSent) {
    String alertMsg = "ALERT: High temperature detected: " + String(temperature, 1) + "°C";
    sim800.sendSMS(AUTHORIZED_NUMBER, alertMsg);
    alertSent = true;
    
    #if DEBUG_MONITORING
    Serial.println("Temperature alert sent");
    #endif
  } else if (temperature < HIGH_TEMP_THRESHOLD - 2.0) {
    // Reset alert when temperature drops 2 degrees below threshold
    alertSent = false;
  }
  
  // Check battery level
  if (battery < LOW_BATTERY_THRESHOLD) {
    String batteryMsg = "WARNING: Low battery: " + String(battery, 2) + "V";
    sim800.sendSMS(AUTHORIZED_NUMBER, batteryMsg);
    
    #if DEBUG_MONITORING
    Serial.println("Battery warning sent");
    #endif
  }
  
  // Check signal quality
  if (signal < 10) {
    #if DEBUG_MONITORING
    Serial.println("Weak signal detected");
    #endif
  }
}

// Send sensor data to UDP server
void sendSensorData() {
  #if DEBUG_MONITORING
  Serial.println("Preparing to send sensor data...");
  #endif
  
  // Check if UDP is connected, if not, try to connect
  if (!udpConnected) {
    udpConnected = sim800.initUDP(UDP_SERVER, UDP_PORT);
    
    if (!udpConnected) {
      #if DEBUG_MONITORING
      Serial.println("Failed to connect to UDP server");
      #endif
      return;
    }
    
    #if DEBUG_MONITORING
    Serial.println("UDP connection established");
    #endif
  }
  
  // Collect sensor data
  float temperature = readTemperature();
  float humidity = readHumidity();
  float battery = readBatteryVoltage();
  int signal = sim800.getSignalStrength();
  
  // Create JSON data packet
  String dataPacket = "{\"id\":\"DEVICE-001\",";
  dataPacket += "\"temp\":" + String(temperature, 2) + ",";
  dataPacket += "\"humid\":" + String(humidity, 2) + ",";
  dataPacket += "\"batt\":" + String(battery, 2) + ",";
  dataPacket += "\"sig\":" + String(signal) + ",";
  dataPacket += "\"uptime\":" + String(millis() / 1000) + "}";
  
  #if DEBUG_MONITORING
  Serial.print("Sending: ");
  Serial.println(dataPacket);
  #endif
  
  // Send the data
  if (sim800.sendData(dataPacket)) {
    #if DEBUG_MONITORING
    Serial.println("Data sent successfully");
    #endif
  } else {
    #if DEBUG_MONITORING
    Serial.println("Failed to send data, resetting connection");
    #endif
    
    // Reset connection on failure
    sim800.closeConnection();
    udpConnected = false;
  }
}

// Handle incoming SMS commands
void handleSmsCommand() {
  String sender = sim800.receivedNumber;
  String message = sim800.receivedMessage;
  message.toUpperCase();  // Convert to uppercase for easier command parsing
  
  #if DEBUG_MONITORING
  Serial.println("SMS received from: " + sender);
  Serial.println("Message: " + message);
  #endif
  
  // Only process commands from authorized numbers
  if (sender == AUTHORIZED_NUMBER) {
    
    if (message == "STATUS") {
      // Send status report
      float temperature = readTemperature();
      float humidity = readHumidity();
      float battery = readBatteryVoltage();
      int signal = sim800.getSignalStrength();
      
      String statusMsg = "Status Report:\n";
      statusMsg += "Temp: " + String(temperature, 1) + "C\n";
      statusMsg += "Humidity: " + String(humidity, 1) + "%\n";
      statusMsg += "Battery: " + String(battery, 2) + "V\n";
      statusMsg += "Signal: " + String(signal) + "\n";
      statusMsg += "Uptime: " + String(millis() / 60000) + " min";
      
      sim800.sendSMS(AUTHORIZED_NUMBER, statusMsg);
      
      #if DEBUG_MONITORING
      Serial.println("Status SMS sent");
      #endif
    } 
    else if (message == "RESET") {
      // Reset the device
      sim800.sendSMS(AUTHORIZED_NUMBER, "System will reset in 5 seconds");
      
      #if DEBUG_MONITORING
      Serial.println("Reset command received, resetting in 5 seconds");
      #endif
      
      delay(5000);
      ESP.restart();
    } 
    else if (message.startsWith("INTERVAL ")) {
      // Change data sending interval
      String intervalStr = message.substring(9);
      int newInterval = intervalStr.toInt();
      
      if (newInterval >= 1 && newInterval <= 60) {
        // Convert from minutes to milliseconds
        // Note: In a real implementation, use a non-const variable or EEPROM
        // DATA_INTERVAL = newInterval * 60 * 1000;
        
        sim800.sendSMS(AUTHORIZED_NUMBER, "Data interval set to " + String(newInterval) + " minutes");
        
        #if DEBUG_MONITORING
        Serial.println("Interval changed to " + String(newInterval) + " minutes");
        #endif
      } else {
        sim800.sendSMS(AUTHORIZED_NUMBER, "Invalid interval. Use 1-60 minutes");
      }
    } 
    else if (message == "SENDNOW") {
      // Force immediate data transmission
      if (sim800.state() == STATE_READY)
      {
        sim800.sendSMS(AUTHORIZED_NUMBER, "Sending data now");
        sendSensorData();
      }
    } 
    else {
      // Unknown command
      sim800.sendSMS(AUTHORIZED_NUMBER, "Unknown command. Available commands: STATUS, RESET, INTERVAL [1-60], SENDNOW");
    }
  } else {
    #if DEBUG_MONITORING
    Serial.println("SMS from unauthorized number ignored");
    #endif
  }
  
  // Reset SMS available flag
  sim800.sms_available = false;
}

// Handle incoming UDP messages
void handleUdpMessages() {
  if (!udpConnected) return;
  
  // Check for incoming UDP data with a short timeout
  String receivedData = sim800.receiveData(100);
  
  if (receivedData.length() > 0 && receivedData.indexOf("+IPD") != -1) {
    #if DEBUG_MONITORING
    Serial.println("UDP data received: " + receivedData);
    #endif
    
    // Extract the actual data payload
    int dataLengthStart = receivedData.indexOf("+IPD,") + 5;
    int dataLengthEnd = receivedData.indexOf(":", dataLengthStart);
    
    if (dataLengthStart != -1 && dataLengthEnd != -1) {
      int dataLength = receivedData.substring(dataLengthStart, dataLengthEnd).toInt();
      String payload = receivedData.substring(dataLengthEnd + 1, dataLengthEnd + 1 + dataLength);
      
      #if DEBUG_MONITORING
      Serial.println("Extracted payload: " + payload);
      #endif
      
      // Process commands from server (similar to SMS commands)
      payload.toUpperCase();
      
      if (payload == "STATUS") {
        // The next sendSensorData() will happen immediately
        lastDataSend = 0;
      } 
      else if (payload == "REBOOT") {
        #if DEBUG_MONITORING
        Serial.println("Remote reboot command received");
        #endif
        
        delay(1000);
        ESP.restart();
      }
      // Add more commands as needed
    }
  }
}
