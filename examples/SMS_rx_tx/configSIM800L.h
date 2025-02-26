
#define CONFIG_CUSTOM 1  // overriding the default configuration with this configuration


// Debug flags
#define SERIAL_LOG_LEVEL     1  // Set to 0 to disable printing messages on Serial, set to 1 for important messages, set to 2 for verbose.
#define PRINT_RAW_AT         0  // Set to 0 to disable AT command logging


// Timings (in milliseconds)
#define SMS_CHECK_INTERVAL   60000    // 1 minute
#define NETWORK_HEALTH_CHECK 120000   // 2 minutes
#define NETWORK_RESET_TIMEOUT 900000  // 15 minutes
#define MODEM_RESET_WAIT     7000     // Wait after modem reset
#define MODEM_REGULAR_RESET  30000    // Regular modem reset interval

// Retry configuration, will restart the SIM800 after this many failures.
#define MAX_AT_RETRIES       10
#define MAX_NETWORK_RETRIES  30   
#define MAX_SMS_CHECK_PER_CYCLE 3
#define MAX_TX_FAILURES      10




// Authorized phone number - only accept commands from this number
#define TARGET_PHONE            "+447777123456"

// Hardware configuration
#define MODEM_RX_PIN         26  // ESP32 pin connected to SIM800L TX
#define MODEM_TX_PIN         27  // ESP32 pin connected to SIM800L RX
#define MODEM_RST_PIN        5   // Reset pin
#define MODEM_PWRKEY_PIN     4   // Power key pin
#define MODEM_PWR_EXT_PIN    23  // External power control pin
#define MODEM_BAUD_RATE      9600
