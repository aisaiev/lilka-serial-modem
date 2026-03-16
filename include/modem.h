#ifndef MODEM_H
#define MODEM_H

#include "ppp.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>

class ModemController {
public:
  ModemController();
  void begin();
  void loop();

  // Command handling
  void sendResult(int resultCode);
  void sendString(const String &msg);

  // Connection control
  void dialTo(const String &address);
  void hangUp();

  // Connection state
  bool isConnected() const { return callConnected; }
  bool isCommandMode() const { return cmdMode; }
  bool isPPPActive() const { return ppp.isPPPActive(); }
  bool isPPPNegotiating() const {
    return ppp.isEnabled() && !ppp.isPPPActive();
  }
  String getConnectedHost() const { return connectedHost; }
  unsigned long getConnectTime() const { return connectTime; }
  String getConnectionDuration() const;

  // Settings access
  bool getEcho() const { return echo; }
  bool getVerbose() const { return verbose; }
  bool getQuiet() const { return quiet; }
  int getBaudRate() const;

private:
  // State
  bool cmdMode;
  bool callConnected;
  bool echo;
  bool verbose;
  bool quiet;
  bool telnet;
  bool autoAnswer;

  String cmdBuffer;
  String connectedHost;
  unsigned long connectTime;
  unsigned long lastRingMs;
  int ringCount;
  bool xmodemSenderStarted;

  // PPP protocol
  PPPController ppp;

  // Command handlers
  void handleATCommand(const String &cmd);
  void dialOut(const String &host, int port);
  void answerCall();
  void handleHTTPGet(const String &url);

  // Data transfer
  void transferSerialToTCP();
  void transferTCPToSerial();
  void handleTelnetControlCode(uint8_t firstByte);

  // Helper methods
  String toUpperCase(const String &str);
  bool parseDial(const String &cmd, String &host, int &port);
  void waitForSpace();
  void playRingTone();

  // File system operations
  void handleListFiles();
  void handleFileUploadYMODEM();
  void handleFileDownloadYMODEM(const String &filename);
  void handleFileUploadXMODEM(const String &filename);
  void handleFileDownloadXMODEM(const String &filename);
  void listDirectory(File dir, int level);
  String formatBytes(uint64_t bytes);

  // YMODEM protocol helpers
  uint16_t calcCRC16(const uint8_t *data, size_t len);
  uint8_t calcChecksum(const uint8_t *data, size_t len);
  bool ymodemReceive(const String &filepath, uint32_t expectedSize,
                     bool useCrc);
  bool ymodemSend(const String &filepath);

  // XMODEM protocol helpers
  bool xmodemReceive(const String &filepath, uint32_t expectedSize);
  bool xmodemSend(const String &filepath);
};

extern ModemController modem;

#endif // MODEM_H
