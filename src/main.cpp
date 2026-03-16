#include "config.h"
#include "display_ui.h"
#include "ftp_server.h"
#include "menu.h"
#include "modem.h"
#include "network.h"
#include "storage.h"
#include "webserver.h"
#include <Arduino.h>
#include <lilka.h>

void setup() {
  // Initialize Lilka hardware
  lilka::begin();

  // Give SD card time to initialize after boot
  delay(100);

  // Initialize display
  displayUI.begin();

  // Initialize storage
  storage.begin();
  storage.loadSettings();

  // Configure Serial1 for modem communication (TX/RX on extension header)
  int baudRate = storage.getBaudRate();

  // Increase buffer for high-speed file transfers
  Serial1.setRxBufferSize(4096);
  Serial1.setTxBufferSize(4096);
  Serial1.begin(baudRate, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

  // Initialize network
  network.begin();

  // Initialize modem controller
  modem.begin();

  // Initialize menu system
  menu.begin();

  // Initialize web server (starts on port 80 when WiFi connected)
  webServer.begin();

  // Initialize FTP server manager
  ftpServer.begin();

  // Send welcome message to serial
  Serial1.println("\r\n");
  Serial1.println("===========================");
  Serial1.println("  LILKA SERIAL MODEM");
  Serial1.println("  Build: " + String(VERSION_BUILD));
  Serial1.println("===========================");
  Serial1.println("Type AT? for help");
  Serial1.println();

  // Update display with initial status
  displayUI.setSerialBaud(baudRate);
  displayUI.showStatus();
}

void loop() {
  // Check button input first
  lilka::State state = lilka::controller.getState();

  // Button Select - Show menu (blocking call)
  if (state.select.justPressed) {
    menu.show();
    // Menu has closed, reload settings and update hardware
    storage.loadSettings();

    // Update Serial1 baud rate if it changed
    int newBaudRate = storage.getBaudRate();
    Serial1.updateBaudRate(newBaudRate);

    // Update display with new settings
    displayUI.setSerialBaud(newBaudRate);
    displayUI.showStatus();
    delay(100);
    return;
  }

  // Button C - Hang up / Reset
  if (state.c.justPressed) {
    if (modem.isConnected()) {
      Serial1.println("\r\n[BUTTON HANG UP]");
      modem.hangUp();
      delay(100);
    }
  }

  // Update network layer
  network.loop();

  // Update modem controller (handles serial I/O and commands)
  modem.loop();

  // Update FTP server
  ftpServer.loop();

  // Update web server (sends SSE status updates)
  webServer.loop();

  // Update display with current status
  displayUI.setWiFiStatus(network.isWiFiConnected(), network.getSSID(),
                          network.getIP());

  displayUI.setModemStatus(modem.isConnected(), modem.isPPPActive(),
                           modem.isPPPNegotiating(), modem.getConnectedHost(),
                           modem.getConnectionDuration());

  displayUI.setSerialBaud(storage.getBaudRate());

  displayUI.update();

  // Small delay to prevent tight loop
  delay(10);
}