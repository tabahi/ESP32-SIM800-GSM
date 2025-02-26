/**
 * @file config.h
 * @brief Configuration for SIM800L module with ESP32
 */

#if defined(CONFIG_CUSTOM)

#else
// Debug flags
#define SERIAL_LOG_LEVEL     1
#define PRINT_RAW_AT         0

// Timings (in milliseconds)
#define SMS_CHECK_INTERVAL   60000    // 1 minute
#define NETWORK_HEALTH_CHECK 120000   // 2 minutes
#define NETWORK_RESET_TIMEOUT 900000  // 15 minutes
#define MODEM_RESET_WAIT     7000     // Wait after modem reset
#define MODEM_REGULAR_RESET  30000    // Regular modem reset interval

// Counters and limits
#define MAX_AT_RETRIES       10
#define MAX_NETWORK_RETRIES  30
#define MAX_SMS_CHECK_PER_CYCLE 3
#define MAX_TX_FAILURES      10

#pragma message "Default configuration - StatefulGSMLibconfig.h"

#define CONFIG_CUSTOM
#endif // CONFIG_H

 