#ifndef CONFIG_H
#define CONFIG_H

// Version information
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_BUILD "20260316"

// Lilka extension header pins
#define MODEM_TX_PIN 43 // TX pin on extension header
#define MODEM_RX_PIN 44 // RX pin on extension header

// Serial configuration
#define DEFAULT_BAUD 9600
#define MAX_CMD_LENGTH 256

// Network configuration
#define LISTEN_PORT 23
#define RING_INTERVAL 3000 // ms between RING messages

// Result codes (prefixed with MODEM_ to avoid conflicts with system headers)
enum ModemResultCodes {
  MODEM_OK = 0,
  MODEM_CONNECT = 1,
  MODEM_RING = 2,
  MODEM_NOCARRIER = 3,
  MODEM_ERROR = 4,
  MODEM_NONE = 5,
  MODEM_NODIALTONE = 6,
  MODEM_BUSY = 7,
  MODEM_NOANSWER = 8
};

// Baud rates supported
const int BAUD_RATES[] = {300,   1200,  2400,  4800,  9600,
                          19200, 38400, 57600, 115200};
const int BAUD_RATES_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

// Telnet protocol codes
#define TELNET_IAC 0xff
#define TELNET_DONT 0xfe
#define TELNET_DO 0xfd
#define TELNET_WONT 0xfc
#define TELNET_WILL 0xfb

// Display settings
#define STATUS_UPDATE_INTERVAL 1000 // Update display every second

#endif // CONFIG_H
