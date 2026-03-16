#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

class ModemStorage {
public:
  ModemStorage();
  void begin();

  // Settings
  bool loadSettings();

  // WiFi credentials
  String getSSID();
  void setSSID(const String &ssid);
  String getPassword();
  void setPassword(const String &password);

  // Modem configuration
  int getBaudRate();
  void setBaudRate(int baud);
  bool getEcho();
  void setEcho(bool echo);
  bool getVerbose();
  void setVerbose(bool verbose);
  bool getQuiet();
  void setQuiet(bool quiet);
  bool getTelnet();
  void setTelnet(bool telnet);
  bool getAutoAnswer();
  void setAutoAnswer(bool autoAnswer);
  bool getFtpEnabled();
  void setFtpEnabled(bool enabled);
  int getServerPort();
  void setServerPort(int port);
  String getBusyMsg();
  void setBusyMsg(const String &msg);
  bool getBuzzerEnabled();
  void setBuzzerEnabled(bool enabled);

  // Speed dial
  String getSpeedDial(int index);
  void setSpeedDial(int index, const String &address);

  // Reset to defaults
  void resetToDefaults();

private:
  Preferences prefs;
};

extern ModemStorage storage;

#endif // STORAGE_H
