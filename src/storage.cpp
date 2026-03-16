#include "storage.h"
#include "config.h"
#include <WiFi.h>

ModemStorage storage;

ModemStorage::ModemStorage() {}

void ModemStorage::begin() { prefs.begin("modem", false); }

bool ModemStorage::loadSettings() {
  // Initialize with defaults if first run
  if (!prefs.isKey("initialized")) {
    resetToDefaults();
    return false;
  }
  return true;
}

String ModemStorage::getSSID() { return prefs.getString("ssid", ""); }

void ModemStorage::setSSID(const String &ssid) {
  prefs.putString("ssid", ssid);
}

String ModemStorage::getPassword() { return prefs.getString("password", ""); }

void ModemStorage::setPassword(const String &password) {
  prefs.putString("password", password);
}

int ModemStorage::getBaudRate() {
  return prefs.getInt("baudrate", DEFAULT_BAUD);
}

void ModemStorage::setBaudRate(int baud) { prefs.putInt("baudrate", baud); }

bool ModemStorage::getEcho() { return prefs.getBool("echo", true); }

void ModemStorage::setEcho(bool echo) { prefs.putBool("echo", echo); }

bool ModemStorage::getVerbose() { return prefs.getBool("verbose", true); }

void ModemStorage::setVerbose(bool verbose) {
  prefs.putBool("verbose", verbose);
}

bool ModemStorage::getQuiet() { return prefs.getBool("quiet", false); }

void ModemStorage::setQuiet(bool quiet) { prefs.putBool("quiet", quiet); }

bool ModemStorage::getTelnet() { return prefs.getBool("telnet", true); }

void ModemStorage::setTelnet(bool telnet) { prefs.putBool("telnet", telnet); }

bool ModemStorage::getAutoAnswer() {
  return prefs.getBool("autoanswer", false);
}

void ModemStorage::setAutoAnswer(bool autoAnswer) {
  prefs.putBool("autoanswer", autoAnswer);
}

bool ModemStorage::getFtpEnabled() { return prefs.getBool("ftp", false); }

void ModemStorage::setFtpEnabled(bool enabled) {
  prefs.putBool("ftp", enabled);
}

int ModemStorage::getServerPort() {
  return prefs.getInt("serverport", LISTEN_PORT);
}

void ModemStorage::setServerPort(int port) { prefs.putInt("serverport", port); }

String ModemStorage::getBusyMsg() {
  return prefs.getString(
      "busymsg", "SORRY, SYSTEM IS CURRENTLY BUSY. PLEASE TRY AGAIN LATER.");
}

void ModemStorage::setBusyMsg(const String &msg) {
  prefs.putString("busymsg", msg);
}

bool ModemStorage::getBuzzerEnabled() { return prefs.getBool("buzzer", true); }

void ModemStorage::setBuzzerEnabled(bool enabled) {
  prefs.putBool("buzzer", enabled);
}

String ModemStorage::getSpeedDial(int index) {
  if (index < 0 || index > 9)
    return "";
  char key[12];
  snprintf(key, sizeof(key), "dial%d", index);
  // Check if key exists to avoid error logs
  if (!prefs.isKey(key))
    return "";
  return prefs.getString(key, "");
}

void ModemStorage::setSpeedDial(int index, const String &address) {
  if (index < 0 || index > 9)
    return;
  char key[12];
  snprintf(key, sizeof(key), "dial%d", index);
  prefs.putString(key, address);
}

void ModemStorage::resetToDefaults() {
  prefs.clear();
  prefs.putBool("initialized", true);
  prefs.putString("ssid", "");
  prefs.putString("password", "");
  prefs.putInt("baudrate", DEFAULT_BAUD);
  prefs.putBool("echo", true);
  prefs.putBool("verbose", true);
  prefs.putBool("quiet", false);
  prefs.putBool("telnet", true);
  prefs.putBool("autoanswer", false);
  prefs.putBool("ftp", false);
  prefs.putInt("serverport", LISTEN_PORT);
  prefs.putString("busymsg",
                  "SORRY, SYSTEM IS CURRENTLY BUSY. PLEASE TRY AGAIN LATER.");
  prefs.putBool("buzzer", true);

  // Default speed dials
  prefs.putString("dial0", "theoldnet.com:23");
  prefs.putString("dial1", "bbs.retrocampus.com:23");
  prefs.putString("dial2", "bbs.eotd.com:23");
}
