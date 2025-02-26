
#include "configSIM800L.h"
#include "StatefulGSMLib.h"

// Create a hardware serial for the modem
HardwareSerial HSerial1(1);

// Create SIM800L instance
SIM800L sim800(HSerial1);

// Example server details for TCP/UDP connections
const char* SERVER_HOST = "example.com";
const int SERVER_PORT = 80;
bool finished = false;



void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  Serial.println("Start");
  delay(10);

  // Initialize SIM800L module
  sim800.begin(MODEM_BAUD_RATE, MODEM_RX_PIN, MODEM_TX_PIN, MODEM_PWRKEY_PIN, MODEM_RST_PIN, MODEM_PWR_EXT_PIN);

  Serial.println("Init done");
}


void loop() {

  // Run the SIM800L state machine
  sim800.loop();  // tries to be non-blocking but can still block for up to 5 seconds during normal function.

  if ((sim800.state() == STATE_READY) && (!finished)) {
    delay(10000);  //wait for the network to settle
    //if (fetch_http()) finished = true;
    delay(10000);
  }
}






/**
 * Fetch data from example.com via HTTP and print it on serial
 * Uses the already initialized TCP connection
 */
bool fetch_http() {

  bool success = false;

  Serial.println("Connecting...");
  bool connected = sim800.initTCP(SERVER_HOST, SERVER_PORT);
  if (connected)
  {
    Serial.println("Fetching HTTP content from example.com...");

    // Construct HTTP GET request
    String httpRequest =
      "GET / HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Connection: close\r\n"
      "\r\n";

    // Send the HTTP request
    bool sendSuccess = sim800.sendData(httpRequest);

    if (sendSuccess)
    {
      Serial.println("HTTP request sent successfully.");

      // Wait for and receive the response (with a 15-second timeout)
      String httpResponse = sim800.receiveData(15000);

      // Check if we got a response
      if (httpResponse.length() > 0)
      {
        // Print full response to serial
        Serial.println("\n----- HTTP Response -----");
        Serial.println(httpResponse);
        Serial.println("----- End Response -----\n");

        // Extract HTTP status code
        int statusCodeStart = httpResponse.indexOf("HTTP/1.1");
        if (statusCodeStart >= 0)
        {
          String statusLine = httpResponse.substring(statusCodeStart, httpResponse.indexOf("\r\n", statusCodeStart));
          Serial.print("Status: ");
          Serial.println(statusLine);
        }
        success = true;
        Serial.println("HTTP fetch completed successfully");
      }
      else
      {
        Serial.println("No HTTP response received or timeout occurred");
      }
    } 
    else
    {
      Serial.println("Failed to send HTTP request");
    }
  }
  else
  {
    Serial.println("Connection failed.");
  }


  // Close the connection
  if (sim800.closeConnection()) // usually will give error but not a real problem
  {
    Serial.println("Connection closed properly");
  }
  else
  {
    Serial.println("Warning: Connection may not have closed properly");
  }
  return success;
}
