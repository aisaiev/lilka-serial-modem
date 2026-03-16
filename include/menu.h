#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <lilka.h>

class MenuSystem {
public:
  MenuSystem();
  void begin();
  void show();
  bool isActive() const { return active; }
  void update(); // Update and handle menu logic

private:
  bool active;
  lilka::Canvas *canvas;

  // Menu handlers
  void showMainMenu();
  void showWiFiScanMenu();
  void showSpeedDialMenu();
  void showSettingsMenu();
  void showAboutDialog();

  // WiFi operations
  void connectToWiFi(const String &ssid, const String &password);
  void scanAndSelectWiFi();

  // Speed dial operations
  void editSpeedDial(int index);

  // Settings operations
  void editBaudRate();
  void toggleEcho();
  void toggleVerbose();
  void toggleTelnet();
  void toggleAutoAnswer();
  void toggleFtp();
  void toggleBuzzer();
  void editBusyMessage();
  void editServerPort();

  // Helper functions
  String formatBaudRate(int baud);
  String formatOnOff(bool value);
  String formatBusyMsg(String msg);
};

extern MenuSystem menu;

#endif // MENU_H
