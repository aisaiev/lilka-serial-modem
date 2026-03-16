#include "modem.h"
#include "config.h"
#include "display_ui.h"
#include "ftp_server.h"
#include "network.h"
#include "storage.h"
#include <lilka.h>

// Modem logging levels:
// 0 = DISABLED
// 1 = INFO
#define MODEM_LOG_LEVEL 0

// INFO: Important events
#if MODEM_LOG_LEVEL >= 1
#define MDMINFO(msg) Serial.print(msg)
#define MDMINFOLN(msg) Serial.println(msg)
#else
#define MDMINFO(msg)                                                           \
  do {                                                                         \
  } while (0)
#define MDMINFOLN(msg)                                                         \
  do {                                                                         \
  } while (0)
#endif

ModemController modem;

static unsigned long calcSerialTimeoutMs(int baud, size_t bytes) {
  if (baud <= 0 || bytes == 0)
    return 2000;
  const unsigned long bitsPerByte = 11; // start + data + stop + margin
  unsigned long ms = (bytes * bitsPerByte * 1000UL) / (unsigned long)baud;
  return ms + 1000; // safety margin
}

static const String resultCodes[] = {"OK",          "CONNECT", "RING",
                                     "NO CARRIER",  "ERROR",   "",
                                     "NO DIALTONE", "BUSY",    "NO ANSWER"};

ModemController::ModemController()
    : cmdMode(true), callConnected(false), echo(true), verbose(true),
      quiet(false), telnet(true), autoAnswer(false), connectTime(0),
      lastRingMs(0), ringCount(0), xmodemSenderStarted(false) {}

void ModemController::begin() {
  // Load settings from storage
  echo = storage.getEcho();
  verbose = storage.getVerbose();
  quiet = storage.getQuiet();
  telnet = storage.getTelnet();
  autoAnswer = storage.getAutoAnswer();

  // Initialize PPP controller
  ppp.begin();

  cmdBuffer.reserve(MAX_CMD_LENGTH);
}

void ModemController::loop() {
  // PPP mode takes priority - but only if PPP traffic has started
  if (ppp.isPPPActive()) {
    ppp.processSerialData();
    ppp.processNetworkData();
    ppp.loop(); // Poll TCP connections and forward data
    return;
  }

  // Auto-detect PPP frames (0x7E) and enable PPP mode automatically
  // This supports "direct mode" where Psion starts PPP without AT commands
  if (Serial1.available()) {
    uint8_t firstByte = Serial1.peek();
    if (firstByte == 0x7E) {
      if (!ppp.isEnabled()) {
        MDMINFOLN("[Modem] PPP auto-enabled");
        ppp.enable();
        cmdMode = false;
        callConnected = true;
      }
      ppp.processSerialData();
      return;
    }
  }

  if (cmdMode) {
    // Command mode - process AT commands
    while (Serial1.available()) {
      char c = Serial1.read();

      // Echo if enabled
      if (echo) {
        Serial1.write(c);
      }

      // Process character
      if (c == 13 || c == 10) {
        if (cmdBuffer.length() > 0) {
          MDMINFO("[CMD] ");
          MDMINFOLN(cmdBuffer);
          handleATCommand(cmdBuffer);
          cmdBuffer = "";
        }
      } else if (c == 8 || c == 127) { // Backspace
        if (cmdBuffer.length() > 0) {
          cmdBuffer.remove(cmdBuffer.length() - 1);
        }
      } else if (cmdBuffer.length() < MAX_CMD_LENGTH) {
        cmdBuffer += c;
      }
    }

    // Check for incoming connections
    if (network.serverHasClient()) {
      if (autoAnswer) {
        answerCall();
      } else {
        // Ring the phone
        unsigned long now = millis();
        if (now - lastRingMs > RING_INTERVAL) {
          sendResult(MODEM_RING);
          if (storage.getBuzzerEnabled()) {
            playRingTone();
          }
          lastRingMs = now;
          ringCount++;

          if (ringCount > 3) {
            // Reject after 3 rings
            network.rejectIncoming();
            ringCount = 0;
          }
        }
      }
    }
  } else {
    // Connected mode - pass through data with escape sequence detection
    static int plusCount = 0;
    static unsigned long plusTime = 0;

    // Transfer serial to TCP, checking for +++ escape and handling telnet
    if (Serial1.available() && network.isClientConnected()) {
      // If telnet mode, reserve half buffer for escaping 0xFF bytes
      int max_buf_size = telnet ? 64 : 128;
      uint8_t buffer[128];
      size_t len = Serial1.readBytes(
          buffer, min((int)Serial1.available(), max_buf_size));

      // Check for +++ escape sequence in the buffer
      for (size_t i = 0; i < len; i++) {
        if (buffer[i] == '+') {
          plusCount++;
          if (plusCount >= 3) {
            plusTime = millis();
          }
        } else {
          plusCount = 0;
        }
      }

      // Escape 0xFF bytes for telnet (double them)
      if (telnet) {
        for (int i = len - 1; i >= 0; i--) {
          if (buffer[i] == 0xFF) {
            // Shift bytes right to make space
            for (int j = 127; j > i; j--) {
              buffer[j] = buffer[j - 1];
            }
            len++;
            if (len >= 128)
              break; // Buffer full
          }
        }
      }

      // Write buffer to TCP
      network.writeToClient(buffer, len);
    }

    // Check if we should enter command mode (1 second after +++)
    if (plusCount >= 3 && (millis() - plusTime) > 1000) {
      cmdMode = true;
      plusCount = 0;
      sendResult(MODEM_OK);
    }

    // Transfer TCP to serial with telnet handling
    if (telnet) {
      // Handle telnet protocol byte by byte
      while (network.clientAvailable()) {
        uint8_t rxByte;
        if (network.readFromClient(&rxByte, 1) == 1) {
          // Check for telnet IAC (0xFF)
          if (rxByte == 0xFF) {
            handleTelnetControlCode(rxByte);
          } else {
            // Normal data byte
            Serial1.write(rxByte);
          }
        }
      }
    } else {
      // Raw mode - no telnet processing
      transferTCPToSerial();
    }

    // Check if connection is still alive (skip for PPP mode)
    if (!ppp.isEnabled() && !network.isClientConnected()) {
      hangUp();
    }
  }
}

void ModemController::handleATCommand(const String &cmd) {
  String upCmd = toUpperCase(cmd);
  upCmd.trim();

  // Handle chained commands like ATE0V1Q0
  // Check if command contains multiple single-letter commands after AT
  if (upCmd.startsWith("AT") && upCmd.length() > 2) {
    // Check for common chained patterns: E0/E1, V0/V1, Q0/Q1
    bool isChained = false;
    String remaining = upCmd.substring(2);

    // Simple heuristic: if we see E followed by 0/1 and then another letter,
    // it's chained
    if (remaining.length() >= 3) {
      for (int i = 0; i < remaining.length() - 2; i++) {
        char c = remaining.charAt(i);
        char next = remaining.charAt(i + 1);
        char after = remaining.charAt(i + 2);

        // Pattern: letter + digit + letter (e.g., E0V, V1Q, Q0E)
        if (isAlpha(c) && isDigit(next) && isAlpha(after)) {
          isChained = true;
          break;
        }
      }
    }

    if (isChained) {
      MDMINFOLN("[Modem] Chained command detected, splitting...");

      String segment = "AT";
      char prevChar = 0;
      for (int i = 2; i < upCmd.length(); i++) {
        char c = upCmd.charAt(i);

        bool startNewSegment = false;
        if ((c == '&' || c == '+') && segment.length() > 2) {
          startNewSegment = true;
        } else if (isAlpha(c) && isDigit(prevChar) && segment.length() > 2) {
          startNewSegment = true;
        }

        if (startNewSegment) {
          MDMINFO("[Modem] Executing sub-command: ");
          MDMINFOLN(segment);
          handleATCommand(segment);
          segment = "AT";
          prevChar = 0;
        }

        segment += c;
        prevChar = c;
      }

      if (segment.length() > 2) {
        MDMINFO("[Modem] Executing final sub-command: ");
        MDMINFOLN(segment);
        handleATCommand(segment);
      }
      return;
    }
  }

  // Basic AT
  if (upCmd == "AT") {
    sendResult(MODEM_OK);
  }
  // Help
  else if (upCmd == "AT?" || upCmd == "ATHELP") {
    Serial1.println("AT COMMAND SUMMARY:");
    Serial1.println("DIAL HOST...............: ATDTHOST:PORT");
    Serial1.println("SPEED DIAL..............: ATDSN (N=0-9)");
    Serial1.println("ANSWER CALL.............: ATA");
    Serial1.println("AUTO ANSWER.............: ATS0=N (N=0,1)");
    Serial1.println("HANGUP..................: ATH");
    Serial1.println("DEVICE INFO.............: ATI");
    Serial1.println("WIFI OFF/ON.............: ATC0 / ATC1");
    Serial1.println("SHOW IP ADDRESS.........: ATIP");
    Serial1.println("ECHO OFF/ON.............: ATE0 / ATE1");
    Serial1.println("VERBOSE OFF/ON..........: ATV0 / ATV1");
    Serial1.println("QUIET MODE OFF/ON.......: ATQ0 / ATQ1");
    Serial1.println("TELNET OFF/ON...........: ATNET0 / ATNET1");
    Serial1.println("BUZZER ON/OFF...........: AT$BUZZ=1/0");
    Serial1.println("FTP SERVER ON/OFF.......: AT$FTP=1/0");
    Serial1.println("SET BAUD RATE...........: AT$SB=RATE");
    waitForSpace();
    Serial1.println("SET WIFI SSID...........: AT$SSID=WIFISSID");
    Serial1.println("SET WIFI PASSWORD.......: AT$PASS=WIFIPASSWORD");
    Serial1.println("SET SPEED DIAL..........: AT&ZN=HOST:PORT (N=0-9)");
    Serial1.println("SET BUSY MSG............: AT$BM=BUSY MESSAGE");
    Serial1.println("RELOAD SETTINGS.........: ATZ");
    Serial1.println("SHOW SETTINGS...........: AT&V");
    Serial1.println("FACTORY DEFAULTS........: AT&F");
    Serial1.println("HTTP GET................: ATGET<URL>");
    Serial1.println("ENTER CMD MODE..........: +++");
    Serial1.println("EXIT CMD MODE...........: ATO");
    Serial1.println("LIST FILES..............: AT$LS");
    Serial1.println("XMODEM UPLOAD FILE......: AT$XUL=/FILENAME");
    Serial1.println("XMODEM DOWNLOAD FILE....: AT$XDL=/FILEPATH");
    Serial1.println("YMODEM UPLOAD FILE......: AT$YUL");
    Serial1.println("YMODEM DOWNLOAD FILE....: AT$YDL=/FILEPATH");
    sendResult(MODEM_OK);
  }
  // Information
  else if (upCmd == "ATI") {
    Serial1.println("VENDOR.....: Anderson & friends");
    Serial1.println("MODEL......: Lilka v2");
    Serial1.print("VERSION....: ");
    Serial1.print(VERSION_MAJOR);
    Serial1.print(".");
    Serial1.print(VERSION_MINOR);
    Serial1.print(" (");
    Serial1.print(VERSION_BUILD);
    Serial1.println(")");

    // ESP32 info
    Serial1.print("CHIP MODEL.: ");
    Serial1.println(ESP.getChipModel());
    Serial1.print("CPU FREQ...: ");
    Serial1.print(ESP.getCpuFreqMHz());
    Serial1.println(" MHz");
    Serial1.print("FREE MEMORY: ");
    Serial1.print(ESP.getFreeHeap() / 1024);
    Serial1.println(" KB");
    Serial1.print("FLASH SIZE.: ");
    Serial1.print(ESP.getFlashChipSize() / (1024 * 1024));
    Serial1.println(" MB");

    // WiFi status
    if (WiFi.status() == WL_CONNECTED) {
      Serial1.println("WIFI STATUS: CONNECTED");
      Serial1.print("SSID.......: ");
      Serial1.println(WiFi.SSID());
      Serial1.print("MAC ADDRESS: ");
      Serial1.println(WiFi.macAddress());
      Serial1.print("IP ADDRESS.: ");
      Serial1.println(WiFi.localIP());
      Serial1.print("GATEWAY....: ");
      Serial1.println(WiFi.gatewayIP());
      Serial1.print("SUBNET MASK: ");
      Serial1.println(WiFi.subnetMask());
    } else {
      Serial1.println("WIFI STATUS: NOT CONNECTED");
    }

    // Server port
    Serial1.print("SERVER PORT: ");
    Serial1.println(storage.getServerPort());

    // Web config URL
    if (WiFi.status() == WL_CONNECTED) {
      Serial1.print("WEB CONFIG.: HTTP://");
      Serial1.println(WiFi.localIP());
    }

    // Call status
    Serial1.print("CALL STATUS: ");
    if (callConnected) {
      Serial1.print("CONNECTED TO ");
      Serial1.print(connectedHost);
      Serial1.print(" (");
      Serial1.print(getConnectionDuration());
      Serial1.println(")");
    } else {
      Serial1.println("NOT CONNECTED");
    }

    sendResult(MODEM_OK);
  }
  // Show IP
  else if (upCmd == "ATIP") {
    if (WiFi.status() == WL_CONNECTED) {
      Serial1.print("\r\nIP: ");
      Serial1.println(WiFi.localIP());
      Serial1.print("SSID: ");
      Serial1.println(WiFi.SSID());
    } else {
      Serial1.println("\r\nNOT CONNECTED");
    }
    sendResult(MODEM_OK);
  }
  // Show settings
  else if (upCmd == "AT&V") {
    Serial1.print("BAUD: ");
    Serial1.println(storage.getBaudRate());
    Serial1.print("SSID: ");
    Serial1.println(storage.getSSID());

    // Flags on one line
    Serial1.print("E");
    Serial1.print(storage.getEcho() ? 1 : 0);
    Serial1.print(" Q");
    Serial1.print(storage.getQuiet() ? 1 : 0);
    Serial1.print(" V");
    Serial1.print(storage.getVerbose() ? 1 : 0);
    Serial1.print(" NET");
    Serial1.print(storage.getTelnet() ? 1 : 0);
    Serial1.print(" S0:");
    Serial1.print(storage.getAutoAnswer() ? 1 : 0);
    Serial1.print(" BUZZ");
    Serial1.println(storage.getBuzzerEnabled() ? 1 : 0);
    Serial1.print("BUSY MSG: ");
    Serial1.println(storage.getBusyMsg());

    Serial1.println("SPEED DIAL:");
    for (int i = 0; i < 10; i++) {
      Serial1.print(i);
      Serial1.print(": ");
      Serial1.println(storage.getSpeedDial(i));
    }

    sendResult(MODEM_OK);
  }
  // Dial - direct dial
  else if (upCmd.startsWith("ATDT")) {
    String number = cmd.substring(4);
    number.trim();

    // Special number 777 triggers PPP mode
    if (number == "777") {
      MDMINFOLN("[Modem] Dial 777 detected - activating PPP mode");
      Serial1.println("\r\nDIALING PPP SERVER...");
      delay(500);

      // Enable PPP mode
      ppp.enable();

      // Send CONNECT response
      sendResult(MODEM_CONNECT);
      cmdMode = false;      // Enter data mode
      callConnected = true; // Mark as connected for PPP
    } else {
      // Normal dial
      dialTo(number);
    }
  }
  // Dial with modifiers (ATD, ATDP, ATDW, ATDWT, etc.)
  else if (upCmd.startsWith("ATDS")) {
    int num = upCmd.substring(4, 5).toInt();
    String address = storage.getSpeedDial(num);
    if (address.length() > 0) {
      dialTo(address);
    } else {
      sendResult(MODEM_ERROR);
    }
  } else if (upCmd.startsWith("ATD")) {
    String number = cmd.substring(3);
    // Strip dial modifiers: W (wait for dial tone), T (tone), P (pulse), ,
    // (pause)
    number.replace("W", "");
    number.replace("w", "");
    number.replace("T", "");
    number.replace("t", "");
    number.replace("P", "");
    number.replace("p", "");
    number.replace(",", "");
    number.trim();

    // Special number 777 triggers PPP mode
    if (number == "777") {
      MDMINFOLN("[Modem] Dial 777 detected - activating PPP mode");
      Serial1.println("\r\nDIALING PPP SERVER...");
      delay(500);

      // Enable PPP mode
      ppp.enable();

      // Send CONNECT response
      sendResult(MODEM_CONNECT);
      cmdMode = false;      // Enter data mode
      callConnected = true; // Mark as connected for PPP
    } else {
      // Normal dial
      dialTo(number);
    }
  }
  // Answer
  else if (upCmd == "ATA") {
    if (network.serverHasClient()) {
      answerCall();
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Hang up
  else if (upCmd == "ATH" || upCmd == "ATH0") {
    if (callConnected) {
      hangUp();
    } else {
      sendResult(MODEM_OK);
    }
  }
  // Return to online mode (exit command mode)
  else if (upCmd == "ATO") {
    if (callConnected) {
      sendResult(MODEM_CONNECT);
      cmdMode = false;
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Echo control
  else if (upCmd.startsWith("ATE")) {
    String param = upCmd.substring(3);
    if (param == "0") {
      echo = false;
      storage.setEcho(false);
      sendResult(MODEM_OK);
    } else if (param == "1") {
      echo = true;
      storage.setEcho(true);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Verbose control
  else if (upCmd.startsWith("ATV")) {
    String param = upCmd.substring(3);
    if (param == "0") {
      verbose = false;
      storage.setVerbose(false);
      sendResult(MODEM_OK);
    } else if (param == "1") {
      verbose = true;
      storage.setVerbose(true);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Quiet control
  else if (upCmd.startsWith("ATQ")) {
    String param = upCmd.substring(3);
    if (param == "0") {
      quiet = false;
      storage.setQuiet(false);
      sendResult(MODEM_OK);
    } else if (param == "1") {
      quiet = true;
      storage.setQuiet(true);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Telnet control
  else if (upCmd.startsWith("ATNET")) {
    String param = upCmd.substring(5);
    if (param == "0") {
      telnet = false;
      storage.setTelnet(false);
      sendResult(MODEM_OK);
    } else if (param == "1") {
      telnet = true;
      storage.setTelnet(true);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // DTR control (later commands may chain AT&DN)
  else if (upCmd.startsWith("AT&D")) {
    String param = upCmd.substring(4);
    if (param == "0" || param == "1" || param == "2") {
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // DCD control
  else if (upCmd.startsWith("AT&C")) {
    String param = upCmd.substring(4);
    if (param == "0" || param == "1") {
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Speaker mode (M0-M3) - accept but ignore
  else if (upCmd.startsWith("ATM")) {
    String param = upCmd.substring(3);
    if (param.length() == 1 && param[0] >= '0' && param[0] <= '3') {
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Speaker volume (L0-L3) - accept but ignore
  else if (upCmd.startsWith("ATL")) {
    String param = upCmd.substring(3);
    if (param.length() == 1 && param[0] >= '0' && param[0] <= '3') {
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Extended result codes (X0-X4) - accept but ignore
  else if (upCmd.startsWith("ATX")) {
    String param = upCmd.substring(3);
    if (param.length() == 1 && param[0] >= '0' && param[0] <= '4') {
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // S8 register (comma dial pause) - accept but ignore
  else if (upCmd.startsWith("ATS8=")) {
    sendResult(MODEM_OK);
  } else if (upCmd == "ATS8?") {
    Serial1.print("\r\n");
    Serial1.println("004"); // 4 seconds
    sendResult(MODEM_OK);
  }
  // Auto answer
  else if (upCmd.startsWith("ATS0=")) {
    String param = upCmd.substring(5);
    if (param == "0") {
      autoAnswer = false;
      storage.setAutoAnswer(false);
      sendResult(MODEM_OK);
    } else if (param == "1") {
      autoAnswer = true;
      storage.setAutoAnswer(true);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Query auto answer
  else if (upCmd == "ATS0?") {
    Serial1.print("\r\n");
    Serial1.println(autoAnswer ? "001" : "000");
    sendResult(MODEM_OK);
  }
  // Query S7 register (connection timeout in seconds)
  else if (upCmd == "ATS7?") {
    Serial1.print("\r\n");
    Serial1.println("060"); // 60 seconds timeout
    sendResult(MODEM_OK);
  }
  // Set S7 register (accept but ignore - we have our own timeout)
  else if (upCmd.startsWith("ATS7=")) {
    sendResult(MODEM_OK);
  }
  // WiFi control
  else if (upCmd == "ATC0") {
    network.disconnectWiFi();
    sendResult(MODEM_OK);
  } else if (upCmd == "ATC1") {
    network.connectWiFi();
    sendResult(MODEM_OK);
  }
  // Set SSID
  else if (upCmd.startsWith("AT$SSID=")) {
    String ssid = cmd.substring(8);
    storage.setSSID(ssid);
    sendResult(MODEM_OK);
  }
  // Set password
  else if (upCmd.startsWith("AT$PASS=")) {
    String pass = cmd.substring(8);
    storage.setPassword(pass);
    sendResult(MODEM_OK);
  }
  // Set baud rate
  else if (upCmd.startsWith("AT$SB=")) {
    String baudStr = cmd.substring(6);
    int baud = baudStr.toInt();

    // Validate baud rate (common rates)
    if (baud == 300 || baud == 1200 || baud == 2400 || baud == 4800 ||
        baud == 9600 || baud == 19200 || baud == 38400 || baud == 57600 ||
        baud == 115200) {

      Serial1.print("\r\nSWITCHING TO ");
      Serial1.print(baud);
      Serial1.println(" BAUD IN 2 SECONDS");
      delay(2000);

      storage.setBaudRate(baud);
      Serial1.end();
      delay(100);
      Serial1.begin(baud, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
      delay(100);

      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Set busy message
  else if (upCmd.startsWith("AT$BM=")) {
    String msg = cmd.substring(6);
    msg.trim();
    if (msg.length() > 0 && msg.length() <= 100) {
      storage.setBusyMsg(msg);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Query busy message
  else if (upCmd == "AT$BM?") {
    Serial1.print("\r\nBUSY MSG: ");
    Serial1.println(storage.getBusyMsg());
    sendResult(MODEM_OK);
  }
  // Enable/disable buzzer
  else if (upCmd.startsWith("AT$BUZZ=")) {
    int enabled = cmd.substring(8).toInt();
    if (enabled == 0 || enabled == 1) {
      storage.setBuzzerEnabled(enabled == 1);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Query buzzer status
  else if (upCmd == "AT$BUZZ?") {
    Serial1.print("\r\nBUZZER: ");
    Serial1.println(storage.getBuzzerEnabled() ? "ON" : "OFF");
    sendResult(MODEM_OK);
  }
  // FTP server control
  else if (upCmd.startsWith("AT$FTP=")) {
    String param = upCmd.substring(7);
    if (param == "0") {
      ftpServer.setEnabled(false);
      sendResult(MODEM_OK);
    } else if (param == "1") {
      ftpServer.setEnabled(true);
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  } else if (upCmd == "AT$FTP?") {
    Serial1.print("\r\nFTP: ");
    Serial1.println(storage.getFtpEnabled() ? "ON" : "OFF");
    sendResult(MODEM_OK);
  }
  // List files on SD card
  else if (upCmd == "AT$LS") {
    handleListFiles();
  }
  // Upload file via XMODEM
  else if (upCmd == "AT$XUL" || upCmd.startsWith("AT$XUL=")) {
    String filename;
    if (upCmd.startsWith("AT$XUL=")) {
      filename = cmd.substring(7);
      filename.trim();
    }
    handleFileUploadXMODEM(filename);
  }
  // Download file via XMODEM
  else if (upCmd.startsWith("AT$XDL=")) {
    String filename = cmd.substring(7);
    filename.trim();
    handleFileDownloadXMODEM(filename);
  }
  // Upload file via YMODEM
  else if (upCmd == "AT$YUL") {
    handleFileUploadYMODEM();
  }
  // Download file via YMODEM
  else if (upCmd.startsWith("AT$YDL=")) {
    String filename = cmd.substring(7);
    filename.trim();
    handleFileDownloadYMODEM(filename);
  }
  // Set speed dial
  else if (upCmd.startsWith("AT&Z")) {
    // Parse AT&ZN=host:port
    if (cmd.length() > 5 && cmd.charAt(4) >= '0' && cmd.charAt(4) <= '9') {
      int index = cmd.charAt(4) - '0';
      int eqPos = cmd.indexOf('=');
      if (eqPos != -1 && eqPos < cmd.length() - 1) {
        String address = cmd.substring(eqPos + 1);
        address.trim();
        storage.setSpeedDial(index, address);
        sendResult(MODEM_OK);
      } else {
        sendResult(MODEM_ERROR);
      }
    } else {
      sendResult(MODEM_ERROR);
    }
  }
  // Reload settings
  else if (upCmd == "ATZ") {
    storage.loadSettings();
    begin(); // Reload settings
    sendResult(MODEM_OK);
  }
  // Support for +++ATZ command (some modems use this)
  else if (upCmd == "+++ATZ") {
    sendResult(MODEM_OK);
  }
  // Factory defaults
  else if (upCmd == "AT&F") {
    storage.resetToDefaults();
    begin(); // Reload settings
    sendResult(MODEM_OK);
  }
  // HTTP GET request
  else if (upCmd.startsWith("ATGET")) {
    handleHTTPGet(cmd.substring(5));
  }
  // Fax class query (for Psion compatibility)
  else if (upCmd == "AT+FCLASS=?") {
    // Report we support class 0 (data mode only)
    Serial1.print("\r\n0");
    sendResult(MODEM_OK);
  } else if (upCmd == "AT+FCLASS?") {
    // Report current class is 0 (data mode)
    Serial1.print("\r\n0");
    sendResult(MODEM_OK);
  } else if (upCmd.startsWith("AT+FCLASS=")) {
    String param = upCmd.substring(10);
    param.trim();
    // Accept class 0 (data mode), reject others
    if (param == "0") {
      sendResult(MODEM_OK);
    } else {
      sendResult(MODEM_ERROR);
    }
  } else {
    sendResult(MODEM_ERROR);
  }
}

void ModemController::dialTo(const String &address) {
  String host;
  int port;
  if (parseDial(address, host, port)) {
    dialOut(host, port);
  } else {
    sendResult(MODEM_ERROR);
  }
}

bool ModemController::parseDial(const String &address, String &host,
                                int &port) {
  int colonPos = address.indexOf(':');
  if (colonPos != -1) {
    host = address.substring(0, colonPos);
    port = address.substring(colonPos + 1).toInt();
  } else {
    host = address;
    port = 23; // Default telnet port
  }
  host.trim();
  return host.length() > 0 && port > 0 && port < 65536;
}

void ModemController::dialOut(const String &host, int port) {
  if (callConnected) {
    sendResult(MODEM_ERROR);
    return;
  }

  Serial1.print("\r\nDIALING ");
  Serial1.print(host);
  Serial1.print(":");
  Serial1.println(port);

  if (network.connectToHost(host, port)) {
    connectedHost = host + ":" + String(port);
    connectTime = millis();
    callConnected = true;
    cmdMode = false;
    ringCount = 0;
    sendResult(MODEM_CONNECT);
  } else {
    sendResult(MODEM_ERROR);
  }
}

void ModemController::hangUp() {
  network.disconnect();
  callConnected = false;
  cmdMode = true;
  connectTime = 0;
  connectedHost = "";
  sendResult(MODEM_NOCARRIER);
}

void ModemController::answerCall() {
  if (network.acceptIncoming()) {
    connectedHost = "INCOMING";
    connectTime = millis();
    callConnected = true;
    cmdMode = false;
    ringCount = 0;
    sendResult(MODEM_CONNECT);
  } else {
    sendResult(MODEM_ERROR);
  }
}

void ModemController::transferSerialToTCP() {
  if (Serial1.available() && network.isClientConnected()) {
    uint8_t buffer[128];
    int len = Serial1.readBytes(buffer, sizeof(buffer));
    network.writeToClient(buffer, len);
  }
}

void ModemController::transferTCPToSerial() {
  if (network.clientAvailable()) {
    uint8_t buffer[128];
    int len = network.readFromClient(buffer, sizeof(buffer));
    if (len > 0) {
      Serial1.write(buffer, len);
    }
  }
}

void ModemController::sendResult(int resultCode) {
  if (quiet)
    return;

  // Debug logging
  MDMINFO("[TX] Result: ");
  MDMINFO(resultCodes[resultCode]);
  if (resultCode == MODEM_CONNECT) {
    MDMINFO(" ");
    MDMINFO(String(getBaudRate()));
  }
  MDMINFOLN("");

  Serial1.print("\r\n");
  if (verbose) {
    Serial1.print(resultCodes[resultCode]);
    if (resultCode == MODEM_CONNECT) {
      Serial1.print(" ");
      Serial1.print(getBaudRate());
    } else if (resultCode == MODEM_NOCARRIER) {
      Serial1.print(" (");
      Serial1.print(getConnectionDuration());
      Serial1.print(")");
    }
  } else {
    Serial1.print(resultCode);
  }
  Serial1.print("\r\n");
}

void ModemController::sendString(const String &msg) {
  MDMINFO("[TX] String: ");
  MDMINFOLN(msg);

  Serial1.print("\r\n");
  Serial1.print(msg);
  Serial1.print("\r\n");
}

String ModemController::getConnectionDuration() const {
  if (connectTime == 0)
    return "00:00:00";

  unsigned long duration = (millis() - connectTime) / 1000;
  int hours = duration / 3600;
  int minutes = (duration % 3600) / 60;
  int seconds = duration % 60;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

int ModemController::getBaudRate() const { return storage.getBaudRate(); }

String ModemController::toUpperCase(const String &str) {
  String result = str;
  result.toUpperCase();
  return result;
}

void ModemController::waitForSpace() {
  Serial1.print("PRESS SPACE");
  char c = 0;
  while (c != 0x20) {
    if (Serial1.available() > 0) {
      c = Serial1.read();
    }
  }
  Serial1.print("\r");
}

void ModemController::handleTelnetControlCode(uint8_t firstByte) {
  // First byte is 0xFF (IAC - Interpret As Command)
  uint8_t secondByte;
  if (network.readFromClient(&secondByte, 1) != 1)
    return;

  if (secondByte == 0xFF) {
    // Escaped 0xFF - output literal 0xFF
    Serial1.write(0xFF);
    return;
  }

  // Read third byte (option code)
  uint8_t optionByte;
  if (network.readFromClient(&optionByte, 1) != 1)
    return;

  // Handle telnet commands
  if (secondByte == TELNET_DO) {
    // Server asks us to do an option - respond we won't
    uint8_t response[3] = {TELNET_IAC, TELNET_WONT, optionByte};
    network.writeToClient(response, 3);
  } else if (secondByte == TELNET_WILL) {
    // Server will do an option - acknowledge with DO
    uint8_t response[3] = {TELNET_IAC, TELNET_DO, optionByte};
    network.writeToClient(response, 3);
  } else if (secondByte == TELNET_DONT || secondByte == TELNET_WONT) {
    // Server says don't/won't - just acknowledge, no response needed
  }
}

void ModemController::handleHTTPGet(const String &url) {
  if (callConnected) {
    sendResult(MODEM_ERROR);
    return;
  }

  // Parse URL: http://host:port/path or http://host/path
  String workUrl = url;
  workUrl.trim();

  // Remove http:// prefix if present
  if (workUrl.startsWith("http://")) {
    workUrl = workUrl.substring(7);
  } else if (workUrl.startsWith("HTTP://")) {
    workUrl = workUrl.substring(7);
  }

  // Find port and path separators
  int portIndex = workUrl.indexOf(':');
  int pathIndex = workUrl.indexOf('/');

  String host;
  int port = 80;
  String path = "/";

  // Determine host, port, and path
  if (pathIndex < 0) {
    // No path specified
    pathIndex = workUrl.length();
  }

  if (portIndex < 0 || portIndex > pathIndex) {
    // No port specified
    host = workUrl.substring(0, pathIndex);
  } else {
    // Port specified
    host = workUrl.substring(0, portIndex);
    port = workUrl.substring(portIndex + 1, pathIndex).toInt();
  }

  if (pathIndex < workUrl.length()) {
    path = workUrl.substring(pathIndex);
  }

  // Validate
  if (host.length() == 0) {
    sendResult(MODEM_ERROR);
    return;
  }

  Serial1.print("\r\nHTTP GET: http://");
  Serial1.print(host);
  if (port != 80) {
    Serial1.print(":");
    Serial1.print(port);
  }
  Serial1.println(path);
  Serial1.println("Connecting...");

  // Connect to server
  if (!network.connectToHost(host, port)) {
    Serial1.println("\r\nCONNECTION FAILED");
    sendResult(MODEM_ERROR);
    return;
  }

  // Build HTTP request
  String request = "GET ";
  request += path;
  request += " HTTP/1.1\r\nHost: ";
  request += host;
  request += "\r\nConnection: close";
  request += "\r\nUser-Agent: Lilka-Modem/1.0";
  request += "\r\n\r\n";

  // Send HTTP request
  network.writeToClient((const uint8_t *)request.c_str(), request.length());

  sendResult(MODEM_CONNECT);

  // Enter data mode to stream response
  connectedHost = host + ":" + String(port);
  connectTime = millis();
  callConnected = true;
  cmdMode = false;
  ringCount = 0;

  // Note: The response will be streamed through normal data transfer
  // Connection will auto-close when server closes (Connection: close)
}

void ModemController::playRingTone() {
  // Vintage electromechanical phone ring imitation
  // ~425 Hz with 20 Hz warble and slight frequency jitter

  // First ring burst
  for (int i = 0; i < 28; i++) {         // ~700ms (28 * 25ms)
    uint16_t freq = (i % 2) ? 420 : 430; // Alternate for jitter effect
    lilka::buzzer.play(freq);
    delay(25); // ~20 Hz modulation
  }
  lilka::buzzer.stop();

  // Pause between rings
  delay(180);

  // Second ring burst
  for (int i = 0; i < 28; i++) { // ~700ms
    uint16_t freq = (i % 2) ? 420 : 430;
    lilka::buzzer.play(freq);
    delay(25);
  }
  lilka::buzzer.stop();
}

void ModemController::handleListFiles() {
  if (!SD.begin()) {
    Serial1.println("\r\nSD CARD NOT AVAILABLE");
    sendResult(MODEM_ERROR);
    return;
  }

  Serial1.println("\r\n=== SD CARD FILES ===");
  listDirectory(SD.open("/"), 0);
  Serial1.println("=====================");

  // Show SD card usage
  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();
  Serial1.print("USED: ");
  Serial1.print(formatBytes(used));
  Serial1.print(" / ");
  Serial1.println(formatBytes(total));

  sendResult(MODEM_OK);
}

void ModemController::listDirectory(File dir, int level) {
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File file = dir.openNextFile();
  while (file) {
    // Indent based on level
    for (int i = 0; i < level; i++) {
      Serial1.print("  ");
    }

    if (file.isDirectory()) {
      Serial1.print("[DIR] ");
      Serial1.println(file.name());
      // Recursively list subdirectories
      listDirectory(file, level + 1);
    } else {
      Serial1.print(file.name());
      Serial1.print(" (");
      Serial1.print(formatBytes(file.size()));
      Serial1.println(")");
    }

    file.close();
    file = dir.openNextFile();
  }
  dir.close();
}

String ModemController::formatBytes(uint64_t bytes) {
  if (bytes < 1024)
    return String(bytes) + " B";
  if (bytes < 1024 * 1024)
    return String(bytes / 1024.0, 1) + " KB";
  if (bytes < 1024 * 1024 * 1024)
    return String(bytes / (1024.0 * 1024), 1) + " MB";
  return String(bytes / (1024.0 * 1024 * 1024), 2) + " GB";
}

void ModemController::handleFileUploadYMODEM() {
  if (!SD.begin()) {
    Serial1.println("\r\nSD CARD NOT AVAILABLE");
    sendResult(MODEM_ERROR);
    return;
  }

  // Show file transfer status on display
  displayUI.setFileTransferStatus(true, true, "Waiting for file...");

  // Save current echo state and enter binary mode
  bool savedEcho = echo;
  echo = false;

  Serial1.println("\r\nREADY FOR YMODEM UPLOAD");
  Serial1.println("START TRANSFER NOW...");
  Serial1.flush();

  // Clear any stale data in serial buffer (wait for in-flight bytes)
  delay(50);
  while (Serial1.available()) {
    Serial1.read();
  }

  // Set timeout for readBytes (allow for 1K blocks)
  Serial1.setTimeout(calcSerialTimeoutMs(getBaudRate(), 1024));

  // Use CRC mode for YMODEM
  bool useCrc = true;

  // Wait for first block (block 0 with filename)
  unsigned long startTime = millis();
  unsigned long lastCTime = millis() - 500;
  bool readyToReceive = false;
  int checkCount = 0;

  while (true) {
    unsigned long elapsed = millis() - startTime;

    // Send CRC request periodically until the sender responds
    if (!readyToReceive && millis() - lastCTime > 1000) {
      Serial1.write('C');
      Serial1.flush();
      lastCTime = millis();
    }

    if (elapsed > 180000) { // 180 second timeout
      MDMINFOLN("[Modem] Transfer timeout");
      Serial1.println("\r\nTRANSFER TIMEOUT");
      echo = savedEcho;
      displayUI.setFileTransferStatus(false, true, ""); // Clear transfer status
      sendResult(MODEM_ERROR);
      return;
    }

    checkCount++;

    // Yield to prevent watchdog timeout
    if (checkCount % 10 == 0) {
      yield();
    }

    if (Serial1.available()) {
      if (!readyToReceive) {
        // Sender started - begin receiving without consuming data
        readyToReceive = true;
      }

      uint8_t c = Serial1.peek();

      // SOH = start of 128-byte block, STX = start of 1K block
      if (c == 0x01 || c == 0x02) {
        size_t blockDataLen = (c == 0x02) ? 1024 : 128;

        // Read block 0 to get filename
        uint8_t header[3];
        size_t bytesRead = Serial1.readBytes(header, 3);

        if (bytesRead != 3) {
          MDMINFOLN("[Modem] Invalid header");
          Serial1.write(0x18); // CAN - cancel
          Serial1.println("\r\nERROR: INVALID HEADER");
          echo = savedEcho;
          displayUI.setFileTransferStatus(false, true,
                                          ""); // Clear transfer status
          sendResult(MODEM_ERROR);
          return;
        }

        uint8_t blockNum = header[1];
        uint8_t blockNumInv = header[2];

        if (blockNum != 0 || blockNumInv != 0xFF) {
          MDMINFOLN("[Modem] Expected block 0");
          Serial1.write(0x18); // CAN
          Serial1.println("\r\nERROR: EXPECTED BLOCK 0");
          echo = savedEcho;
          sendResult(MODEM_ERROR);
          return;
        }

        // Read data block
        if (blockDataLen > 1024) {
          MDMINFOLN("[Modem] Block size too large");
          Serial1.write(0x18); // CAN
          Serial1.println("\r\nERROR: BLOCK TOO LARGE");
          echo = savedEcho;
          sendResult(MODEM_ERROR);
          return;
        }

        uint8_t data[1024];
        Serial1.setTimeout(calcSerialTimeoutMs(getBaudRate(), blockDataLen));
        size_t dataRead = Serial1.readBytes(data, blockDataLen);

        if (dataRead != blockDataLen) {
          MDMINFOLN("[Modem] Read timeout on data block");
          Serial1.write(0x18); // CAN
          Serial1.println("\r\nERROR: READ TIMEOUT");
          echo = savedEcho;
          sendResult(MODEM_ERROR);
          return;
        }

        if (useCrc) {
          // Read CRC (2 bytes)
          uint8_t crcBytes[2];
          if (Serial1.readBytes(crcBytes, 2) != 2) {
            MDMINFOLN("[Modem] CRC read failed");
            Serial1.write(0x18); // CAN
            Serial1.println("\r\nERROR: CRC READ FAILED");
            echo = savedEcho;
            sendResult(MODEM_ERROR);
            return;
          }

          // Verify CRC
          uint16_t receivedCRC = (crcBytes[0] << 8) | crcBytes[1];
          uint16_t calculatedCRC = calcCRC16(data, blockDataLen);

          if (receivedCRC != calculatedCRC) {
            MDMINFO("[Modem] CRC mismatch: recv=0x");
            MDMINFO(String(receivedCRC, HEX));
            MDMINFO(" calc=0x");
            MDMINFO(String(calculatedCRC, HEX));
            MDMINFOLN("");
            Serial1.write(0x15); // NAK
            Serial1.println("\r\nERROR: CRC MISMATCH");
            echo = savedEcho;
            sendResult(MODEM_ERROR);
            return;
          }
        } else {
          // Checksum (1 byte)
          uint8_t checksumByte = 0;
          if (Serial1.readBytes(&checksumByte, 1) != 1) {
            MDMINFOLN("[Modem] Checksum read failed");
            Serial1.write(0x18); // CAN
            Serial1.println("\r\nERROR: CHECKSUM READ FAILED");
            echo = savedEcho;
            sendResult(MODEM_ERROR);
            return;
          }

          uint8_t calculated = calcChecksum(data, blockDataLen);
          if (checksumByte != calculated) {
            MDMINFO("[Modem] Checksum mismatch: recv=0x");
            MDMINFO(String(checksumByte, HEX));
            MDMINFO(" calc=0x");
            MDMINFO(String(calculated, HEX));
            MDMINFOLN("");
            Serial1.write(0x15); // NAK
            Serial1.println("\r\nERROR: CHECKSUM MISMATCH");
            echo = savedEcho;
            sendResult(MODEM_ERROR);
            return;
          }
        }

        // Extract filename and size from block 0
        String filename = "";
        for (size_t i = 0; i < blockDataLen && data[i] != 0; i++) {
          filename += (char)data[i];
        }

        String sizeStr = "";
        size_t idx = filename.length() + 1;
        for (size_t i = idx; i < blockDataLen; i++) {
          if (data[i] == 0x00 || data[i] == 0x20) {
            break;
          }
          sizeStr += (char)data[i];
        }
        uint32_t expectedSize =
            (sizeStr.length() > 0) ? (uint32_t)sizeStr.toInt() : 0;

        if (filename.length() == 0) {
          MDMINFOLN("[Modem] No filename in block 0");
          Serial1.write(0x18); // CAN
          Serial1.println("\r\nERROR: NO FILENAME");
          sendResult(MODEM_ERROR);
          return;
        }

        // Ensure filename starts with /
        String filepath = filename;
        if (!filepath.startsWith("/")) {
          filepath = "/" + filepath;
        }

        MDMINFO("[Modem] Receiving file: ");
        MDMINFOLN(filepath);
        Serial1.print("\r\nFILENAME: ");
        Serial1.println(filepath);
        if (expectedSize > 0) {
          Serial1.print("SIZE: ");
          Serial1.print(expectedSize);
          Serial1.println(" BYTES");
        }

        // Update display with actual filename
        displayUI.setFileTransferStatus(true, true, filename);

        // Send ACK for block 0
        Serial1.write(0x06); // ACK
        Serial1.flush();

        // Request data blocks (CRC mode)
        Serial1.write('C');
        Serial1.flush();

        // Now receive the file data (block 1 should follow immediately)
        xmodemSenderStarted = true;
        if (ymodemReceive(filepath, expectedSize, useCrc)) {
          // Clear any stale serial input
          delay(100);
          while (Serial1.available()) {
            Serial1.read();
          }
          Serial1.flush();
          delay(50);
          MDMINFOLN("[Modem] Upload complete");
          Serial1.print("\r\nUPLOAD COMPLETE\r\n");
          Serial1.flush();
          echo = savedEcho; // Restore echo
          displayUI.setFileTransferStatus(false, true,
                                          ""); // Clear transfer status
          sendResult(MODEM_OK);
        } else {
          delay(100);
          while (Serial1.available()) {
            Serial1.read();
          }
          Serial1.flush();
          delay(50);
          MDMINFOLN("[Modem] Upload failed");
          Serial1.print("\r\nUPLOAD FAILED\r\n");
          Serial1.flush();
          echo = savedEcho; // Restore echo
          displayUI.setFileTransferStatus(false, true,
                                          ""); // Clear transfer status
          sendResult(MODEM_ERROR);
        }
        xmodemSenderStarted = false;
        return;
      } else if (c == 0x18) { // CAN - cancel
        Serial1.read();       // consume it
        MDMINFOLN("[Modem] Transfer cancelled by sender");
        Serial1.println("\r\nTRANSFER CANCELLED");
        echo = savedEcho; // Restore echo
        displayUI.setFileTransferStatus(false, true,
                                        ""); // Clear transfer status
        sendResult(MODEM_ERROR);
        return;
      } else {
        Serial1.read(); // consume unexpected byte
      }
    }

    // Small delay to prevent tight loop, check every 50ms
    delay(50);
  }

  // Timeout - restore echo
  echo = savedEcho;
}

void ModemController::handleFileDownloadYMODEM(const String &filename) {
  if (!SD.begin()) {
    Serial1.println("\r\nSD CARD NOT AVAILABLE");
    sendResult(MODEM_ERROR);
    return;
  }

  String filepath = filename;
  if (!filepath.startsWith("/")) {
    filepath = "/" + filepath;
  }

  if (!SD.exists(filepath)) {
    Serial1.println("\r\nFILE NOT FOUND");
    sendResult(MODEM_ERROR);
    return;
  }

  File file = SD.open(filepath, FILE_READ);
  if (!file || file.isDirectory()) {
    Serial1.println("\r\nCANNOT OPEN FILE");
    sendResult(MODEM_ERROR);
    return;
  }

  Serial1.print("\r\nREADY TO SEND: ");
  Serial1.println(filepath);
  Serial1.print("SIZE: ");
  Serial1.print(file.size());
  Serial1.println(" BYTES");
  Serial1.println("START YMODEM RECEIVE NOW...");
  Serial1.flush();

  // Show download status on display - extract filename from path
  int lastSlash = filepath.lastIndexOf('/');
  String displayName = filepath;
  if (lastSlash >= 0) {
    displayName = filepath.substring(lastSlash + 1);
  }
  displayUI.setFileTransferStatus(true, false, displayName);

  file.close();

  // Wait for 'C' from receiver to start transfer
  unsigned long startTime = millis();
  bool receivedC = false;

  while (!receivedC) {
    unsigned long elapsed = millis() - startTime;
    if (elapsed > 60000) { // 60 second timeout
      Serial1.println("\r\nTRANSFER TIMEOUT");
      displayUI.setFileTransferStatus(false, false,
                                      ""); // Clear transfer status
      sendResult(MODEM_ERROR);
      return;
    }

    if (Serial1.available()) {
      uint8_t c = Serial1.read();
      if (c == 'C') {
        receivedC = true;
        break;
      } else if (c == 0x18) { // CAN - cancel
        Serial1.println("\r\nTRANSFER CANCELLED");
        displayUI.setFileTransferStatus(false, false,
                                        ""); // Clear transfer status
        sendResult(MODEM_ERROR);
        return;
      }
    }

    // Feed watchdog and yield to prevent WDT reset
    delay(100);
    yield();
  }

  // Send the file
  if (ymodemSend(filepath)) {
    Serial1.println("\r\nDOWNLOAD COMPLETE");
    displayUI.setFileTransferStatus(false, false, ""); // Clear transfer status
    sendResult(MODEM_OK);
  } else {
    Serial1.println("\r\nDOWNLOAD FAILED");
    displayUI.setFileTransferStatus(false, false, ""); // Clear transfer status
    sendResult(MODEM_ERROR);
  }
}

// CRC-16 calculation for YMODEM (CRC-CCITT)
uint16_t ModemController::calcCRC16(const uint8_t *data, size_t len) {
  uint16_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

uint8_t ModemController::calcChecksum(const uint8_t *data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum = (uint8_t)(sum + data[i]);
  }
  return sum;
}

// YMODEM receive implementation
bool ModemController::ymodemReceive(const String &filepath,
                                    uint32_t expectedSize, bool useCrc) {
  File file = SD.open(filepath, FILE_WRITE);
  if (!file) {
    MDMINFOLN("[Modem] Failed to open file for writing");
    Serial1.write(0x18); // CAN
    return false;
  }

  uint8_t expectedBlock = 1;
  int consecutiveErrors = 0;
  unsigned long lastBlockTime = millis();
  unsigned long startTime = millis();
  unsigned long lastCTime = millis() - 500;
  bool started = false;
  int blocksReceived = 0;
  uint32_t bytesWritten = 0;

  // Request first data block immediately
  if (useCrc) {
    Serial1.write('C');
  } else {
    Serial1.write(0x15); // NAK
  }
  Serial1.flush();
  lastCTime = millis();

  while (true) {
    // Yield every iteration to prevent watchdog timeout
    yield();

    // Timeout check (first block vs subsequent blocks)
    if (!started) {
      if (millis() - startTime > 180000) {
        MDMINFOLN("[Modem] Start timeout waiting for first data block");
        Serial1.write(0x18); // CAN
        file.close();
        return false;
      }
    } else if (millis() - lastBlockTime > 30000) {
      MDMINFO("[Modem] Block timeout after ");
      MDMINFO(String(blocksReceived));
      MDMINFOLN(" blocks");
      Serial1.write(0x18); // CAN
      file.close();
      return false;
    }

    // Re-send CRC request periodically until first data block arrives
    if (!started && millis() - lastCTime > 500) {
      Serial1.write('C');
      Serial1.flush();
      lastCTime = millis();
    }

    if (Serial1.available()) {
      uint8_t header = Serial1.read();

      // EOT - End of transmission
      if (header == 0x04) {
        MDMINFO("[Modem] EOT received after ");
        MDMINFO(String(blocksReceived));
        MDMINFOLN(" blocks");
        Serial1.write(0x06); // ACK
        Serial1.flush();
        file.flush(); // Flush file buffer
        file.close();

        // Wait for final null block 0
        delay(1000);
        Serial1.write('C');

        unsigned long nullTimeout = millis();
        while (millis() - nullTimeout < 3000) {
          if (Serial1.available() && Serial1.peek() == 0x01) {
            // Read and ACK the null block
            uint8_t nullBlock[133];
            Serial1.readBytes(nullBlock, 133);
            Serial1.write(0x06); // ACK
            break;
          }
        }

        return true;
      }

      // CAN - Cancel
      if (header == 0x18) {
        MDMINFO("[Modem] CAN received after ");
        MDMINFO(String(blocksReceived));
        MDMINFOLN(" blocks");
        file.close();
        return false;
      }

      // SOH - 128-byte block, STX - 1K block
      if (header == 0x01 || header == 0x02) {
        size_t blockSize = (header == 0x02) ? 1024 : 128;
        Serial1.setTimeout(calcSerialTimeoutMs(getBaudRate(), blockSize));

        uint8_t blockInfo[2];
        if (Serial1.readBytes(blockInfo, 2) != 2) {
          MDMINFO("[Modem] Block info read failed (block ");
          MDMINFO(String(expectedBlock));
          MDMINFOLN(")");
          Serial1.write(0x15); // NAK
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            MDMINFOLN("[Modem] Too many consecutive errors, aborting");
            Serial1.write(0x18); // CAN
            file.close();
            return false;
          }
          continue;
        }

        uint8_t blockNum = blockInfo[0];
        uint8_t blockNumInv = blockInfo[1];

        // Verify block number complement
        if (blockNum != (uint8_t)(~blockNumInv)) {
          MDMINFO("[Modem] Block complement error: ");
          MDMINFO(String(blockNum));
          MDMINFO(" != ~");
          MDMINFO(String(blockNumInv));
          MDMINFOLN("");
          Serial1.write(0x15); // NAK
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            MDMINFOLN("[Modem] Too many errors, aborting");
            Serial1.write(0x18); // CAN
            file.close();
            return false;
          }
          continue;
        }

        // Read data block
        uint8_t data[1024];
        if (blockSize > sizeof(data)) {
          Serial1.write(0x18); // CAN
          file.close();
          return false;
        }

        if (Serial1.readBytes(data, blockSize) != blockSize) {
          Serial1.write(0x15); // NAK
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18); // CAN
            file.close();
            return false;
          }
          continue;
        }

        if (useCrc) {
          // Read CRC
          uint8_t crcBytes[2];
          if (Serial1.readBytes(crcBytes, 2) != 2) {
            Serial1.write(0x15); // NAK
            consecutiveErrors++;
            if (consecutiveErrors > 10) {
              Serial1.write(0x18); // CAN
              file.close();
              return false;
            }
            continue;
          }

          // Verify CRC
          uint16_t receivedCRC = (crcBytes[0] << 8) | crcBytes[1];
          uint16_t calculatedCRC = calcCRC16(data, blockSize);

          if (receivedCRC != calculatedCRC) {
            Serial1.write(0x15); // NAK
            consecutiveErrors++;
            if (consecutiveErrors > 10) {
              Serial1.write(0x18); // CAN
              file.close();
              return false;
            }
            continue;
          }
        } else {
          // Read checksum
          uint8_t checksumByte = 0;
          if (Serial1.readBytes(&checksumByte, 1) != 1) {
            Serial1.write(0x15); // NAK
            consecutiveErrors++;
            if (consecutiveErrors > 10) {
              Serial1.write(0x18); // CAN
              file.close();
              return false;
            }
            continue;
          }

          uint8_t calculated = calcChecksum(data, blockSize);
          if (checksumByte != calculated) {
            Serial1.write(0x15); // NAK
            consecutiveErrors++;
            if (consecutiveErrors > 10) {
              Serial1.write(0x18); // CAN
              file.close();
              return false;
            }
            continue;
          }
        }

        // Check if this is the expected block
        if (blockNum == expectedBlock) {
          // Write to file (respect expected size if known)
          size_t toWrite = blockSize;
          if (expectedSize > 0 && bytesWritten < expectedSize) {
            uint32_t remaining = expectedSize - bytesWritten;
            if (remaining < toWrite) {
              toWrite = remaining;
            }
          }

          if (toWrite > 0) {
            file.write(data, toWrite);
            bytesWritten += toWrite;
          }
          blocksReceived++;
          expectedBlock++;
          consecutiveErrors = 0;
          lastBlockTime = millis();
          started = true;

          // Flush periodically to improve throughput
          if ((blocksReceived % 8) == 0) {
            file.flush();
          }

          // Send ACK
          Serial1.write(0x06);
          Serial1.flush();
        } else if (blockNum == (uint8_t)(expectedBlock - 1)) {
          // Duplicate block - just ACK it
          Serial1.write(0x06);
          Serial1.flush();
        } else {
          // Wrong block number
          Serial1.write(0x15); // NAK
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18); // CAN
            file.close();
            return false;
          }
        }
      }
    }
  }
}

// YMODEM send implementation
bool ModemController::ymodemSend(const String &filepath) {
  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    return false;
  }

  size_t fileSize = file.size();

  // Extract just the filename (without path)
  String filename = filepath;
  int lastSlash = filename.lastIndexOf('/');
  if (lastSlash >= 0) {
    filename = filename.substring(lastSlash + 1);
  }

  // Send block 0 with filename and size
  uint8_t block0[133];
  block0[0] = 0x01; // SOH
  block0[1] = 0x00; // Block 0
  block0[2] = 0xFF; // Block 0 complement

  // Clear data area
  memset(&block0[3], 0, 128);

  // Add filename
  filename.toCharArray((char *)&block0[3], 128);

  // Add file size after filename (after null terminator)
  int nameLen = filename.length();
  String sizeStr = String(fileSize);
  sizeStr.toCharArray((char *)&block0[3 + nameLen + 1], 128 - nameLen - 1);

  // Calculate CRC for block 0
  uint16_t crc0 = calcCRC16(&block0[3], 128);
  block0[131] = crc0 >> 8;
  block0[132] = crc0 & 0xFF;

  // Send block 0
  Serial1.write(block0, 133);
  Serial1.flush();

  // Wait for ACK
  unsigned long timeout = millis();
  bool gotAck = false;
  while (millis() - timeout < 10000) {
    if (Serial1.available()) {
      uint8_t response = Serial1.read();
      if (response == 0x06) { // ACK
        gotAck = true;
        break;
      } else if (response == 0x18) { // CAN
        file.close();
        return false;
      }
    }
  }

  if (!gotAck) {
    file.close();
    return false;
  }

  // Wait for 'C' to start data transfer
  timeout = millis();
  bool gotC = false;
  while (millis() - timeout < 10000) {
    if (Serial1.available()) {
      uint8_t c = Serial1.read();
      if (c == 'C') {
        gotC = true;
        break;
      } else if (c == 0x18) { // CAN
        file.close();
        return false;
      }
    }
  }

  if (!gotC) {
    file.close();
    return false;
  }

  // Send data blocks
  uint8_t blockNum = 1;
  uint8_t buffer[128];

  while (file.available()) {
    yield(); // Feed watchdog
    size_t bytesRead = file.read(buffer, 128);

    // Pad with zeros if less than 128 bytes
    if (bytesRead < 128) {
      memset(&buffer[bytesRead], 0, 128 - bytesRead);
    }

    // Build block
    uint8_t block[133];
    block[0] = 0x01; // SOH
    block[1] = blockNum;
    block[2] = ~blockNum;
    memcpy(&block[3], buffer, 128);

    // Calculate CRC
    uint16_t crc = calcCRC16(buffer, 128);
    block[131] = crc >> 8;
    block[132] = crc & 0xFF;

    // Send block with retries
    int retries = 0;
    bool blockAcked = false;

    while (retries < 10 && !blockAcked) {
      // Clear any stale responses
      while (Serial1.available()) {
        Serial1.read();
      }

      Serial1.write(block, 133);
      Serial1.flush();

      // Wait indefinitely for ACK or NAK - critical for high baud rates
      // Processing delays at high speeds can exceed any reasonable timeout
      while (!blockAcked && retries < 10) {
        yield();  // Prevent watchdog
        delay(1); // Small delay to prevent busy loop

        if (Serial1.available()) {
          uint8_t response = Serial1.read();
          if (response == 0x06) { // ACK
            blockAcked = true;
            break;
          } else if (response == 0x15) { // NAK
            retries++;
            MDMINFO("[Modem] Block ");
            MDMINFO(String((int)blockNum));
            MDMINFO(" NAK, retry ");
            MDMINFO(String(retries));
            MDMINFOLN("/10");
            break;
          } else if (response == 0x18) { // CAN
            MDMINFOLN("[Modem] Transfer cancelled by receiver");
            file.close();
            return false;
          }
        }
      }
    }

    if (!blockAcked) {
      MDMINFO("[Modem] Block ");
      MDMINFO(String((int)blockNum));
      MDMINFOLN(" send failed after retries");
      file.close();
      return false;
    }

    blockNum++;
  }

  file.close();

  // Send EOT
  Serial1.write(0x04); // EOT
  Serial1.flush();

  // Wait for ACK
  timeout = millis();
  while (millis() - timeout < 10000) {
    yield();
    if (Serial1.available()) {
      uint8_t response = Serial1.read();
      if (response == 0x06) { // ACK
        // Wait for final 'C'
        timeout = millis();
        while (millis() - timeout < 3000) {
          yield();
          if (Serial1.available() && Serial1.read() == 'C') {
            // Send null block 0 to end session
            uint8_t nullBlock[133];
            nullBlock[0] = 0x01;
            nullBlock[1] = 0x00;
            nullBlock[2] = 0xFF;
            memset(&nullBlock[3], 0, 128);
            uint16_t nullCrc = calcCRC16(&nullBlock[3], 128);
            nullBlock[131] = nullCrc >> 8;
            nullBlock[132] = nullCrc & 0xFF;
            Serial1.write(nullBlock, 133);
            Serial1.flush();

            // Wait for final ACK
            delay(100);
            return true;
          }
        }
        return true;
      }
    }
  }

  return false;
}

// XMODEM file upload handler
void ModemController::handleFileUploadXMODEM(const String &filename) {
  if (!SD.begin()) {
    Serial1.println("\r\nSD CARD NOT AVAILABLE");
    sendResult(MODEM_ERROR);
    return;
  }

  Serial1.println("\r\nREADY FOR XMODEM UPLOAD");
  Serial1.println("START TRANSFER NOW...");
  Serial1.flush();

  String resolvedName = filename.length() > 0 ? filename : "xmodem.bin";
  String filepath = resolvedName;
  if (!filepath.startsWith("/")) {
    filepath = "/" + filepath;
  }

  if (SD.exists(filepath)) {
    int suffix = 1;
    String base = resolvedName;
    String ext = "";
    int dot = resolvedName.lastIndexOf('.');
    if (dot > 0) {
      base = resolvedName.substring(0, dot);
      ext = resolvedName.substring(dot);
    }
    while (SD.exists("/" + base + "-" + String(suffix) + ext) &&
           suffix < 1000) {
      suffix++;
    }
    filepath = "/" + base + "-" + String(suffix) + ext;
  }

  displayUI.setFileTransferStatus(true, true, "Waiting for file...");

  xmodemSenderStarted = false;
  if (xmodemReceive(filepath, 0)) {
    Serial1.println("\r\nUPLOAD COMPLETE");
    displayUI.setFileTransferStatus(false, true, "");
    sendResult(MODEM_OK);
  } else {
    Serial1.println("\r\nUPLOAD FAILED");
    displayUI.setFileTransferStatus(false, true, "");
    sendResult(MODEM_ERROR);
  }
  xmodemSenderStarted = false;
}

// XMODEM file download handler
void ModemController::handleFileDownloadXMODEM(const String &filename) {
  if (!SD.begin()) {
    Serial1.println("\r\nSD CARD NOT AVAILABLE");
    sendResult(MODEM_ERROR);
    return;
  }

  String filepath = filename;
  if (!filepath.startsWith("/")) {
    filepath = "/" + filepath;
  }

  if (!SD.exists(filepath)) {
    Serial1.println("\r\nFILE NOT FOUND");
    sendResult(MODEM_ERROR);
    return;
  }

  File file = SD.open(filepath, FILE_READ);
  if (!file || file.isDirectory()) {
    Serial1.println("\r\nCANNOT OPEN FILE");
    sendResult(MODEM_ERROR);
    return;
  }

  Serial1.print("\r\nREADY TO SEND: ");
  Serial1.println(filepath);
  Serial1.print("SIZE: ");
  Serial1.print(file.size());
  Serial1.println(" BYTES");
  Serial1.println("START XMODEM RECEIVE NOW...");
  Serial1.flush();

  int lastSlash = filepath.lastIndexOf('/');
  String displayName = filepath;
  if (lastSlash >= 0) {
    displayName = filepath.substring(lastSlash + 1);
  }
  displayUI.setFileTransferStatus(true, false, displayName);

  file.close();

  unsigned long startTime = millis();
  bool receivedStart = false;

  while (!receivedStart) {
    if (millis() - startTime > 60000) {
      Serial1.println("\r\nTRANSFER TIMEOUT");
      displayUI.setFileTransferStatus(false, false, "");
      sendResult(MODEM_ERROR);
      return;
    }

    if (Serial1.available()) {
      uint8_t c = Serial1.read();
      if (c == 'C' || c == 0x15) {
        receivedStart = true;
        break;
      } else if (c == 0x18) {
        Serial1.println("\r\nTRANSFER CANCELLED");
        displayUI.setFileTransferStatus(false, false, "");
        sendResult(MODEM_ERROR);
        return;
      }
    }

    delay(100);
    yield();
  }

  if (xmodemSend(filepath)) {
    Serial1.println("\r\nDOWNLOAD COMPLETE");
    displayUI.setFileTransferStatus(false, false, "");
    sendResult(MODEM_OK);
  } else {
    Serial1.println("\r\nDOWNLOAD FAILED");
    displayUI.setFileTransferStatus(false, false, "");
    sendResult(MODEM_ERROR);
  }
}

// XMODEM receive - simpler than YMODEM, no block 0
bool ModemController::xmodemReceive(const String &filepath,
                                    uint32_t expectedSize) {
  bool savedEcho = echo;
  echo = false;

  if (!xmodemSenderStarted) {
    delay(50);
    while (Serial1.available()) {
      Serial1.read();
    }

    Serial1.setTimeout(calcSerialTimeoutMs(getBaudRate(), 128));

    for (int i = 0; i < 3; i++) {
      delay(1000);
      Serial1.write('C');
      Serial1.flush();
    }
  } else {
    Serial1.setTimeout(calcSerialTimeoutMs(getBaudRate(), 128));
  }

  File file = SD.open(filepath, FILE_WRITE);
  if (!file) {
    MDMINFOLN("[Modem] Failed to open file for writing");
    Serial1.write(0x18);
    echo = savedEcho;
    return false;
  }

  uint8_t expectedBlock = 1;
  int consecutiveErrors = 0;
  unsigned long lastBlockTime = 0;
  int blocksReceived = 0;
  bool started = xmodemSenderStarted;
  uint32_t bytesWritten = 0;

  unsigned long startTime = millis();
  unsigned long lastCTime = millis();

  while (true) {
    yield();

    if (!started) {
      if (millis() - startTime > 60000) {
        MDMINFOLN("[Modem] Start timeout waiting for first block");
        Serial1.write(0x18);
        file.close();
        echo = savedEcho;
        return false;
      }
    } else {
      if (millis() - lastBlockTime > 30000) {
        MDMINFO("[Modem] Block timeout after ");
        MDMINFO(String(blocksReceived));
        MDMINFOLN(" blocks");
        Serial1.write(0x18);
        file.close();
        echo = savedEcho;
        return false;
      }
    }

    if (!started && millis() - lastCTime > 3000 && !Serial1.available()) {
      Serial1.write('C');
      Serial1.flush();
      lastCTime = millis();
    }

    if (Serial1.available()) {
      uint8_t header = Serial1.read();

      if (header == 0x04) {
        if (!started) {
          continue;
        }
        MDMINFO("[Modem] EOT received after ");
        MDMINFO(String(blocksReceived));
        MDMINFOLN(" blocks");
        Serial1.write(0x06);
        Serial1.flush();
        file.flush();
        file.close();
        echo = savedEcho;
        return true;
      }

      if (header == 0x18) {
        MDMINFO("[Modem]CAN received after ");
        MDMINFO(String(blocksReceived));
        MDMINFOLN(" blocks");
        file.close();
        echo = savedEcho;
        return false;
      }

      if (header == 0x01 || header == 0x02) {
        size_t blockSize = (header == 0x02) ? 1024 : 128;
        Serial1.setTimeout(calcSerialTimeoutMs(getBaudRate(), blockSize));

        uint8_t blockInfo[2];
        if (Serial1.readBytes(blockInfo, 2) != 2) {
          Serial1.write(0x15);
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18);
            file.close();
            echo = savedEcho;
            return false;
          }
          continue;
        }

        uint8_t blockNum = blockInfo[0];
        uint8_t blockNumInv = blockInfo[1];

        if (blockNum != (uint8_t)(~blockNumInv)) {
          Serial1.write(0x15);
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18);
            file.close();
            echo = savedEcho;
            return false;
          }
          continue;
        }

        uint8_t data[1024];
        if (blockSize > sizeof(data)) {
          Serial1.write(0x18);
          file.close();
          echo = savedEcho;
          return false;
        }

        if (Serial1.readBytes(data, blockSize) != blockSize) {
          Serial1.write(0x15);
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18);
            file.close();
            echo = savedEcho;
            return false;
          }
          continue;
        }

        uint8_t crcBytes[2];
        if (Serial1.readBytes(crcBytes, 2) != 2) {
          Serial1.write(0x15);
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18);
            file.close();
            echo = savedEcho;
            return false;
          }
          continue;
        }

        uint16_t receivedCRC = (crcBytes[0] << 8) | crcBytes[1];
        uint16_t calculatedCRC = calcCRC16(data, blockSize);

        if (receivedCRC != calculatedCRC) {
          Serial1.write(0x15);
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18);
            file.close();
            echo = savedEcho;
            return false;
          }
          continue;
        }

        if (blockNum == expectedBlock) {
          size_t toWrite = blockSize;
          if (expectedSize > 0 && bytesWritten < expectedSize) {
            uint32_t remaining = expectedSize - bytesWritten;
            if (remaining < toWrite) {
              toWrite = remaining;
            }
          }
          if (toWrite > 0) {
            file.write(data, toWrite);
            bytesWritten += toWrite;
          }
          blocksReceived++;
          expectedBlock++;
          consecutiveErrors = 0;
          lastBlockTime = millis();
          started = true;

          file.flush();

          Serial1.write(0x06);
          Serial1.flush();
        } else if (blockNum == (uint8_t)(expectedBlock - 1)) {
          Serial1.write(0x06);
          Serial1.flush();
        } else {
          Serial1.write(0x15);
          consecutiveErrors++;
          if (consecutiveErrors > 10) {
            Serial1.write(0x18);
            file.close();
            echo = savedEcho;
            return false;
          }
        }
      }
    }
  }
}

// XMODEM send - simpler than YMODEM, no block 0, starts at block 1
bool ModemController::xmodemSend(const String &filepath) {
  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    return false;
  }

  size_t fileSize = file.size();
  MDMINFO("[Modem] Sending ");
  MDMINFO(String((unsigned long)fileSize));
  MDMINFOLN(" bytes via XMODEM");

  uint8_t blockNum = 1;
  uint8_t buffer[128];

  while (file.available()) {
    yield();

    size_t bytesRead = file.read(buffer, 128);

    if (bytesRead < 128) {
      memset(&buffer[bytesRead], 0x1A, 128 - bytesRead);
    }

    uint8_t block[133];
    block[0] = 0x01;
    block[1] = blockNum;
    block[2] = ~blockNum;
    memcpy(&block[3], buffer, 128);

    uint16_t crc = calcCRC16(buffer, 128);
    block[131] = crc >> 8;
    block[132] = crc & 0xFF;

    int retries = 0;
    bool blockAcked = false;

    while (retries < 10 && !blockAcked) {
      while (Serial1.available()) {
        Serial1.read();
      }

      Serial1.write(block, 133);
      Serial1.flush();

      while (!blockAcked && retries < 10) {
        yield();
        delay(1);

        if (Serial1.available()) {
          uint8_t response = Serial1.read();
          if (response == 0x06) {
            blockAcked = true;
            break;
          } else if (response == 0x15) {
            retries++;
            MDMINFO("[Modem] Block ");
            MDMINFO(String((int)blockNum));
            MDMINFO(" NAK, retry ");
            MDMINFO(String(retries));
            MDMINFOLN("/10");
            break;
          } else if (response == 0x18) {
            MDMINFOLN("[Modem] Transfer cancelled by receiver");
            file.close();
            return false;
          }
        }
      }
    }

    if (!blockAcked) {
      MDMINFO("[Modem] Block ");
      MDMINFO(String((int)blockNum));
      MDMINFOLN(" send failed after retries");
      file.close();
      return false;
    }

    blockNum++;
  }

  file.close();

  Serial1.write(0x04);
  Serial1.flush();

  unsigned long timeout = millis();
  while (millis() - timeout < 10000) {
    yield();
    if (Serial1.available()) {
      uint8_t response = Serial1.read();
      if (response == 0x06) {
        return true;
      }
    }
  }

  return false;
}
