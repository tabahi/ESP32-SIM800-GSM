# StatefulGSMLib (2025)

This library provides a clean, well-structured interface for controlling a SIM800L GSM/GPRS module with an ESP32. It implements a state machine to handle the various states of modem operation, including initialization, network registration, and SMS handling.

## Features

- Well-organized OOP structure with clear class hierarchy
- Robust state machine for handling various modem states
- Error handling and recovery mechanisms
- SMS sending and receiving capabilities
- Network status monitoring
- Signal strength monitoring
- TCP/UDP communication support
- HTTP data fetching

## Hardware Requirements

- ESP32 development board
- SIM800L GSM/GPRS module
- SIM card with SMS capability
- Power supply capable of providing sufficient current for the SIM800L module


## Configuration

All pin assignments and timing parameters can be configured in `configSIM800L.h` in examples or `StatefulGSMLibconfig.h` for defaults. Update the `TARGET_PHONE` define to set the authorized phone number for SMS commands.

## Usage


### Getting started - Checking new SMS
```cpp
// Include necessary headers
#include "configSIM800L.h"
#include "StatefulGSMLib.h"

// Create a hardware serial for the modem
HardwareSerial HSerial1(1);

// Create SIM800L instance
SIM800L sim800(HSerial1);

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  
  // Initialize SIM800L module with your pin configuration
  sim800.begin(MODEM_BAUD_RATE, MODEM_RX_PIN, MODEM_TX_PIN, MODEM_PWRKEY_PIN, MODEM_RST_PIN, MODEM_PWR_EXT_PIN);
}

void loop() {
  // Run the SIM800L state machine
  sim800.loop();
  
  // Process any received SMS messages
  if (sim800.sms_available) {
    // Handle the received message
    String sender = sim800.receivedNumber;
    String message = sim800.receivedMessage;
    
    Serial.println("SMS from: " + sender);
    Serial.println("Message: " + message);
    
    // Clear the flag to avoid processing the same message repeatedly
    sim800.sms_available = false;
  }
}
```

### Sending an SMS
```cpp
// Assuming you have already initialized the modem as shown above

// Send an SMS to a specific number
void sendStatusMessage(String phoneNumber) {
  String statusMessage = "Device status: Signal strength = " + 
                         String(sim800.getSignalStrength()) + 
                         ", All systems operational.";
  
  // Queue the message for sending (the state machine will handle the actual sending)
  sim800.sendSMS(phoneNumber, statusMessage);
}

// Example usage in your code
if (buttonPressed) {
  sendStatusMessage(TARGET_PHONE);
}
```

### TCP connect and request
```cpp
// Assuming you have already initialized the modem as shown above

// Connect to a server and send a TCP request
bool sendDataToServer(String host, int port, String data) {
  // Initialize TCP connection
  if (sim800.initTCP(host, port)) {
    Serial.println("Connected to server");
    
    // Send data
    if (sim800.sendData(data)) {
      Serial.println("Data sent successfully");
      
      // Wait for response (10 second timeout)
      String response = sim800.receiveData(10000);
      Serial.println("Server response: " + response);
      
      // Close the connection
      sim800.closeConnection();
      return true;
    } else {
      Serial.println("Failed to send data");
    }
    
    // Ensure connection is closed
    sim800.closeConnection();
  } else {
    Serial.println("Failed to connect to server");
  }
  
  return false;
}

// Example usage
if (sim800.state() == STATE_READY) {
  sendDataToServer("example.com", 80, "Hello Server!");
}
```

### UDP connect and send
```cpp
// Assuming you have already initialized the modem as shown above

// Connect to a server and send a UDP datagram
bool sendUDPData(String host, int port, String data) {
  // Initialize UDP connection
  if (sim800.initUDP(host, port)) {
    Serial.println("UDP connection established");
    
    // Send data
    if (sim800.sendData(data)) {
      Serial.println("UDP data sent successfully");
      
      // For UDP, you might want to receive a response
      String response = sim800.receiveData(5000);
      if (response.length() > 0) {
        Serial.println("Received UDP response: " + response);
      }
      
      // Close the connection
      sim800.closeConnection();
      return true;
    } else {
      Serial.println("Failed to send UDP data");
    }
    
    // Ensure connection is closed
    sim800.closeConnection();
  } else {
    Serial.println("Failed to establish UDP connection");
  }
  
  return false;
}

// Example usage in your code
if (sim800.state() == STATE_READY && dataNeedsToSend) {
  sendUDPData("udpserver.com", 8080, "Status update: " + getSensorData());
}
```

### HTTP Request Example
```cpp
// Connect to a server and perform an HTTP GET request
bool performHTTPGet(String host, int port, String path) {
  // Initialize TCP connection
  if (sim800.initTCP(host, port)) {
    Serial.println("Connected to HTTP server");
    
    // Construct HTTP GET request
    String httpRequest = 
      "GET " + path + " HTTP/1.1\r\n" +
      "Host: " + host + "\r\n" +
      "Connection: close\r\n\r\n";
    
    // Send the HTTP request
    if (sim800.sendData(httpRequest)) {
      Serial.println("HTTP request sent");
      
      // Receive the HTTP response (with 15-second timeout)
      String httpResponse = sim800.receiveData(15000);
      
      // Process the response
      if (httpResponse.length() > 0) {
        Serial.println("HTTP Response received: ");
        Serial.println(httpResponse);
      } else {
        Serial.println("No response or timeout");
      }
      
      // Close the connection
      sim800.closeConnection();
      return true;
    }
    
    // Ensure connection is closed
    sim800.closeConnection();
  }
  
  return false;
}

// Example usage
if (sim800.state() == STATE_READY) {
  performHTTPGet("example.com", 80, "/api/data");
}
```

## State Machine

The SIM800L state machine goes through the following states:

1. **RESET**: Resets the modem hardware
2. **POST_RESET**: Waits after reset
3. **CHECK_AT**: Verifies AT command interface is working
4. **CHECK_SIM**: Checks if SIM card is available
5. **CHECK_NETWORK**: Checks for cellular network registration
6. **INITIALIZE**: Configures SMS settings
7. **READY**: Normal operation, handles SMS and maintains network connection

## Error Recovery

The library includes automatic error recovery:

- Automatically resets the modem if it becomes unresponsive
- Attempts to re-register to the network if connection is lost
- Monitors signal strength and network health
- Implements retry mechanisms for failed operations


## Wiring

This module has been tested with [LilyGo-T-Call-SIM800](https://github.com/Xinyuan-LilyGO/LilyGo-T-Call-SIM800) that has an ESP32-Wrover connected to these pins:


| ESP32 Pin | SIM800L Pin |
|-----------|-------------|
| GPIO 27   | RX          |
| GPIO 26   | TX          |
| GPIO 5    | RST         |
| GPIO 4    | PWRKEY      |
| GPIO 23   | VDD         |
| GND       | GND         |

## Handling Communication Errors

The library handles common communication errors with SIM800L modules:

- Automatically retries failed SMS transmissions with exponential backoff
- Monitors network registration status and re-registers when needed
- Verifies successful message sending with multiple confirmation methods
- Stores last error message for debugging (`sim800.lastErrorMessage`)

## Power Management

For reliable operation of the SIM800L module, keep in mind:

- The module requires a stable power supply capable of handling current spikes up to 2A
- Use appropriate capacitors (recommended 100μF) between VCC and GND
- Consider using a dedicated power regulator for the SIM800L module

## License

The Unlicense