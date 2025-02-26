/**
 * @file sms_rx_tx.ino
 * @brief Example program for SIM800L with ESP32
 * @details This program initializes and manages a SIM800L GSM module
 *          for sending and receiving SMS messages. Default settings for timeouts and retries overridden by configSIM800L.h
 */

#include "configSIM800L.h"
#include "StatefulGSMLib.h"

 
// Serial configuration

#define LED_GPIO 13
#define LED_ON HIGH
#define LED_OFF LOW

// Create a hardware serial for the modem
HardwareSerial HSerial1(1);

// Create SIM800L instance
SIM800L sim800(HSerial1);

unsigned long timer = 0;

/**
 * Setup function
 */
void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  Serial.println("Start");
  delay(10);

  // Initialize LED pin
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LED_OFF);

  // Initialize SIM800L module
  sim800.begin(MODEM_BAUD_RATE, MODEM_RX_PIN, MODEM_TX_PIN, MODEM_PWRKEY_PIN,  MODEM_RST_PIN, MODEM_PWR_EXT_PIN);

  Serial.println("Init done");
}

/**
 * Main loop
 */
void loop() {
  // Turn on LED to indicate activity
  digitalWrite(LED_GPIO, LED_ON);

  // Run the SIM800L state machine
  sim800.loop();

  // Process any received SMS messages
  if (sim800.sms_available) {
    processReceivedSMS();
  }


  // do something every 10 seconds without blocking.
  if ((millis()-timer) > 10000)
  {
    if (sim800.state() == STATE_READY)
    {
      // use GSM
    }
    else
    {
      Serial.print("GSM status: "); Serial.println(sim800.state());
    }
    timer = millis();
  }
  

  // Turn off LED
  digitalWrite(LED_GPIO, LED_OFF);
}






/**
 * Process received SMS message
 * This function handles SMS processing in the main program
 */
void processReceivedSMS() {
  if (sim800.receivedMessage.length() > 0) {
    
    String message = sim800.receivedMessage;
    
    sim800.receivedMessage = "";  // Clear the SMS rx buffer in case another SMS is received meanwhile
    sim800.sms_available = false; // must set it to false to avoid reading twice

    Serial.print("\nFrom: ");
    Serial.println(sim800.receivedNumber);
    Serial.print("Content: ");
    Serial.println(message);

    if (sim800.receivedNumber.indexOf(TARGET_PHONE) >= 0) {
      if ((message.indexOf("status") >= 0) || (message.indexOf("Status") >= 0))
      {
        Serial.println("CMD: Status");
        sim800.sendSMS(TARGET_PHONE, "Status: all good!"); // loads the sending buffer. Won't actually send it right now. The `sim800.loop();` will handle the actual transmission.

      }
      else if (message.indexOf("reboot") >= 0)
      {
        Serial.println("CMD: Reboot");
        sim800.sendSMS(TARGET_PHONE, "Rebooting system...");
        // Add your reboot code here
      }
      


      // Add more custom commands here
    }
    else
    {
      Serial.println("Unauthorized SMS number");
    }

  }
}
