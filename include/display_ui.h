#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <lilka.h>

class DisplayUI {
public:
  DisplayUI();
  void begin();
  void update();
  void showStatus();
  void showWelcome();
  void clear();

  // Status information
  void setWiFiStatus(bool connected, const String &ssid, const String &ip);
  void setModemStatus(bool connected, bool pppActive, bool pppNegotiating,
                      const String &host, const String &duration);
  void setSerialBaud(int baud);
  void setFileTransferStatus(bool active, bool isUpload,
                             const String &filename);

private:
  unsigned long lastUpdate;
  bool needsRedraw;
  lilka::Canvas *canvas;

  // Status data
  bool wifiConnected;
  String wifiSSID;
  String wifiIP;
  bool modemConnected;
  bool pppActive;
  bool pppNegotiating;
  String modemHost;
  String modemDuration;
  int baudRate;
  bool fileTransferActive;
  bool fileTransferIsUpload;
  String fileTransferFilename;

  void drawStatusBar();
  void drawConnectionInfo();
};

extern DisplayUI displayUI;

#endif // DISPLAY_UI_H
