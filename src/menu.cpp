#include "menu.h"
#include "config.h"
#include "ftp_server.h"
#include "modem.h"
#include "network.h"
#include "storage.h"
#include <WiFi.h>

MenuSystem menu;

MenuSystem::MenuSystem() : active(false), canvas(nullptr) {}

void MenuSystem::begin() {
  // Create canvas for double buffering
  canvas = new lilka::Canvas(lilka::display.width(), lilka::display.height());
}

void MenuSystem::show() {
  active = true;
  showMainMenu();
  active = false;
}

void MenuSystem::update() {
  // This is called from main loop when menu is active
  // Menu logic is now handled in show() which blocks until menu exits
}

void MenuSystem::showMainMenu() {
  bool menuActive = true;

  while (menuActive) {
    lilka::Menu mainMenu("Serial Modem");
    mainMenu.addItem("WiFi Networks", NULL, lilka::colors::White);
    mainMenu.addItem("Speed Dial", NULL, lilka::colors::White);
    mainMenu.addItem("Settings", NULL, lilka::colors::White);
    mainMenu.addItem("About", NULL, lilka::colors::White);
    mainMenu.addItem("Exit", NULL, lilka::colors::White);

    while (!mainMenu.isFinished()) {
      mainMenu.update();
      mainMenu.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }

    int16_t index = mainMenu.getCursor();

    switch (index) {
    case 0: // WiFi Networks
      showWiFiScanMenu();
      break;
    case 1: // Speed Dial
      showSpeedDialMenu();
      break;
    case 2: // Settings
      showSettingsMenu();
      break;
    case 3: // About
      showAboutDialog();
      break;
    case 4: // Exit
      menuActive = false;
      break;
    }
  }
}

void MenuSystem::scanAndSelectWiFi() {
  // Ensure WiFi is in station mode and ready
  WiFi.mode(WIFI_STA);
  delay(100);

  // Show scanning progress
  lilka::ProgressDialog progress("WiFi Scan", "Scanning networks...");
  progress.setProgress(0);
  progress.draw(canvas);
  lilka::display.drawCanvas(canvas);

  // Start synchronous scan
  int n = WiFi.scanNetworks();

  progress.setProgress(100);
  progress.draw(canvas);
  lilka::display.drawCanvas(canvas);
  delay(500);

  if (n == 0) {
    lilka::Alert alert("WiFi Scan", "No networks found");
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
    return;
  }

  // Build menu with network list
  lilka::Menu wifiMenu("Select Network");
  for (int i = 0; i < n && i < 20; i++) {
    String item = WiFi.SSID(i);
    String postfix = String(WiFi.RSSI(i)) + "dBm";
    wifiMenu.addItem(item, 0, lilka::colors::White, postfix);
  }
  wifiMenu.addItem("< Back");

  while (!wifiMenu.isFinished()) {
    wifiMenu.update();
    wifiMenu.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  int16_t index = wifiMenu.getCursor();

  if (index >= n) {
    WiFi.scanDelete();
    return; // Back selected
  }

  // Save network info before deleting scan results
  String ssid = WiFi.SSID(index);
  String password = "";
  bool isSecured = (WiFi.encryptionType(index) != WIFI_AUTH_OPEN);

  // Delete scan results before showing input dialog
  WiFi.scanDelete();

  // If network is secured, ask for password
  if (isSecured) {
    lilka::InputDialog inputDialog("Enter Password");
    inputDialog.setMasked(true);
    while (!inputDialog.isFinished()) {
      inputDialog.update();
      inputDialog.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
    password = inputDialog.getValue();
  }

  connectToWiFi(ssid, password);
}

void MenuSystem::connectToWiFi(const String &ssid, const String &password) {
  // Connect without saving credentials first
  WiFi.begin(ssid.c_str(), password.c_str());

  lilka::ProgressDialog progress("WiFi Connect",
                                 "Connecting to " + ssid + "...");
  progress.setProgress(50);
  progress.draw(canvas);
  lilka::display.drawCanvas(canvas);

  // Wait for connection (with timeout)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    progress.setProgress(50 + (attempts * 2));
    progress.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Save credentials only after successful connection
    storage.setSSID(ssid);
    storage.setPassword(password);

    lilka::Alert alert("Success",
                       "Connected to " + ssid + "\nIP: " + network.getIP());
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  } else {
    WiFi.disconnect();
    lilka::Alert alert("Error", "Failed to connect to " + ssid);
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  }
}

void MenuSystem::showWiFiScanMenu() { scanAndSelectWiFi(); }

void MenuSystem::showSpeedDialMenu() {
  lilka::Menu sdMenu("Speed Dial");
  for (int i = 0; i < 10; i++) {
    String address = storage.getSpeedDial(i);
    String title =
        String(i) + ": " +
        (address.length() > 0 ? address.substring(0, 20) : "(empty)");
    sdMenu.addItem(title);
  }
  sdMenu.addItem("< Back");

  while (!sdMenu.isFinished()) {
    sdMenu.update();
    sdMenu.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  int16_t index = sdMenu.getCursor();
  if (index >= 10)
    return; // Back selected

  editSpeedDial(index);
}

void MenuSystem::editSpeedDial(int index) {
  String current = storage.getSpeedDial(index);

  lilka::InputDialog inputDialog("Speed Dial " + String(index));
  inputDialog.setValue(current);
  while (!inputDialog.isFinished()) {
    inputDialog.update();
    inputDialog.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  String newValue = inputDialog.getValue();
  storage.setSpeedDial(index, newValue);

  lilka::Alert alert("Saved", "Speed dial " + String(index) + " updated");
  while (!alert.isFinished()) {
    alert.update();
    alert.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }
}

void MenuSystem::showSettingsMenu() {
  bool settingsLoop = true;

  while (settingsLoop) {
    lilka::Menu settingsMenu("Settings");
    settingsMenu.addItem("Baud Rate", 0, lilka::colors::White,
                         formatBaudRate(storage.getBaudRate()));
    settingsMenu.addItem("Echo", 0, lilka::colors::White,
                         formatOnOff(storage.getEcho()));
    settingsMenu.addItem("Verbose", 0, lilka::colors::White,
                         formatOnOff(storage.getVerbose()));
    settingsMenu.addItem("Telnet", 0, lilka::colors::White,
                         formatOnOff(storage.getTelnet()));
    settingsMenu.addItem("Auto Answer", 0, lilka::colors::White,
                         formatOnOff(storage.getAutoAnswer()));
    settingsMenu.addItem("FTP Server", 0, lilka::colors::White,
                         formatOnOff(storage.getFtpEnabled()));
    settingsMenu.addItem("Buzzer", 0, lilka::colors::White,
                         formatOnOff(storage.getBuzzerEnabled()));
    settingsMenu.addItem("Busy Message", 0, lilka::colors::White,
                         formatBusyMsg(storage.getBusyMsg()));
    settingsMenu.addItem("Server Port", 0, lilka::colors::White,
                         String(storage.getServerPort()));
    settingsMenu.addItem("< Back");

    while (!settingsMenu.isFinished()) {
      settingsMenu.update();
      settingsMenu.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }

    int16_t index = settingsMenu.getCursor();

    switch (index) {
    case 0:
      editBaudRate();
      break;
    case 1:
      toggleEcho();
      break;
    case 2:
      toggleVerbose();
      break;
    case 3:
      toggleTelnet();
      break;
    case 4:
      toggleAutoAnswer();
      break;
    case 5:
      toggleFtp();
      break;
    case 6:
      toggleBuzzer();
      break;
    case 7:
      editBusyMessage();
      break;
    case 8:
      editServerPort();
      break;
    default:
      settingsLoop = false;
      break;
    }
  }
}

void MenuSystem::editBaudRate() {
  lilka::Menu baudMenu("Baud Rate");
  for (int i = 0; i < BAUD_RATES_COUNT; i++) {
    String rate = String(BAUD_RATES[i]);
    uint16_t color = (BAUD_RATES[i] == storage.getBaudRate())
                         ? lilka::colors::Green
                         : lilka::colors::White;
    baudMenu.addItem(rate, 0, color);
  }
  baudMenu.addItem("< Back");

  while (!baudMenu.isFinished()) {
    baudMenu.update();
    baudMenu.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  int16_t index = baudMenu.getCursor();
  if (index < BAUD_RATES_COUNT) {
    storage.setBaudRate(BAUD_RATES[index]);
    Serial1.updateBaudRate(BAUD_RATES[index]);
  }
}

void MenuSystem::toggleEcho() { storage.setEcho(!storage.getEcho()); }

void MenuSystem::toggleVerbose() { storage.setVerbose(!storage.getVerbose()); }

void MenuSystem::toggleTelnet() { storage.setTelnet(!storage.getTelnet()); }

void MenuSystem::toggleAutoAnswer() {
  storage.setAutoAnswer(!storage.getAutoAnswer());
}

void MenuSystem::toggleFtp() {
  bool enabled = !storage.getFtpEnabled();
  ftpServer.setEnabled(enabled);
}

void MenuSystem::toggleBuzzer() {
  storage.setBuzzerEnabled(!storage.getBuzzerEnabled());
}

void MenuSystem::editBusyMessage() {
  String current = storage.getBusyMsg();

  lilka::InputDialog inputDialog("Busy Message");
  inputDialog.setValue(current);
  while (!inputDialog.isFinished()) {
    inputDialog.update();
    inputDialog.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  String newValue = inputDialog.getValue();
  if (newValue.length() <= 100) {
    storage.setBusyMsg(newValue);

    lilka::Alert alert("Saved", "Busy message updated");
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  } else {
    lilka::Alert alert("Error", "Message too long (max 100 chars)");
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  }
}

void MenuSystem::editServerPort() {
  String current = String(storage.getServerPort());

  lilka::InputDialog inputDialog("Server Port");
  inputDialog.setValue(current);
  while (!inputDialog.isFinished()) {
    inputDialog.update();
    inputDialog.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }

  String newValue = inputDialog.getValue();
  int port = newValue.toInt();

  if (port == 80) {
    lilka::Alert alert("Error", "Port 80 reserved for web UI");
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  } else if (port > 0 && port <= 65535) {
    storage.setServerPort(port);

    lilka::Alert alert("Saved", "Server port updated to " + String(port) +
                                    "\nRestart modem to apply");
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  } else {
    lilka::Alert alert("Error", "Invalid port (1-65535)");
    while (!alert.isFinished()) {
      alert.update();
      alert.draw(canvas);
      lilka::display.drawCanvas(canvas);
    }
  }
}

void MenuSystem::showAboutDialog() {
  String message = "Version " + String(VERSION_MAJOR) + "." +
                   String(VERSION_MINOR) + "\nBuild: " + String(VERSION_BUILD) +
                   "\n\nHayes AT Compatible Serial Modem";

  lilka::Alert about("Serial Modem", message);
  while (!about.isFinished()) {
    about.update();
    about.draw(canvas);
    lilka::display.drawCanvas(canvas);
  }
}

// Helper formatting functions
String MenuSystem::formatBaudRate(int baud) { return String(baud); }

String MenuSystem::formatOnOff(bool value) { return value ? "ON" : "OFF"; }

String MenuSystem::formatBusyMsg(String msg) {
  if (msg.length() == 0)
    return "(empty)";
  if (msg.length() > 5)
    return msg.substring(0, 5) + "...";
  return msg;
}
