#include "network.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>

NetworkManager network;

NetworkManager::NetworkManager()
    : server(nullptr), wifiConnected(false), serverPort(LISTEN_PORT) {}

void NetworkManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("lilka-modem");

  serverPort = storage.getServerPort();
  if (serverPort > 0) {
    server = new WiFiServer(serverPort);
    server->begin();
  }

  // Auto-connect if credentials exist
  String ssid = storage.getSSID();
  if (ssid.length() > 0) {
    connectWiFi();
  }
}

void NetworkManager::loop() {
  // Monitor WiFi connection status
  wifiConnected = (WiFi.status() == WL_CONNECTED);
}

void NetworkManager::connectWiFi() {
  String ssid = storage.getSSID();
  String password = storage.getPassword();

  if (ssid.length() == 0) {
    Serial1.println("\r\nNO SSID CONFIGURED");
    Serial1.println("USE AT$SSID= TO SET SSID");
    return;
  }

  Serial1.print("\r\nCONNECTING TO ");
  Serial1.print(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial1.print(".");
    attempts++;
  }

  Serial1.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial1.print("CONNECTED TO ");
    Serial1.println(WiFi.SSID());
    Serial1.print("IP ADDRESS: ");
    Serial1.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial1.print("FAILED TO CONNECT TO ");
    Serial1.println(ssid);
  }
}

void NetworkManager::disconnectWiFi() {
  WiFi.disconnect();
  wifiConnected = false;
  Serial1.println("\r\nWIFI DISCONNECTED");
}

bool NetworkManager::isWiFiConnected() const { return wifiConnected; }

String NetworkManager::getSSID() const { return WiFi.SSID(); }

String NetworkManager::getIP() const { return WiFi.localIP().toString(); }

bool NetworkManager::connectToHost(const String &host, int port) {
  if (!wifiConnected) {
    Serial1.println("\r\nNOT CONNECTED TO WIFI");
    return false;
  }

  if (client.connected()) {
    client.stop();
  }

  if (client.connect(host.c_str(), port)) {
    client.setNoDelay(true); // Disable Nagle's algorithm
    return true;
  }

  return false;
}

void NetworkManager::disconnect() {
  if (client.connected()) {
    client.stop();
  }
}

bool NetworkManager::isClientConnected() const {
  // Cast away const to call connected() which is not const in WiFiClient
  return const_cast<WiFiClient &>(client).connected();
}

int NetworkManager::clientAvailable() { return client.available(); }

int NetworkManager::readFromClient(uint8_t *buffer, size_t size) {
  if (client.available()) {
    return client.read(buffer, size);
  }
  return 0;
}

void NetworkManager::writeToClient(const uint8_t *buffer, size_t size) {
  if (client.connected()) {
    client.write(buffer, size);
  }
}

bool NetworkManager::serverHasClient() {
  if (server && server->hasClient()) {
    return true;
  }
  return false;
}

bool NetworkManager::acceptIncoming() {
  if (server && server->hasClient()) {
    if (client.connected()) {
      client.stop();
    }
    client = server->available();
    if (client) {
      client.setNoDelay(true);
      return true;
    }
  }
  return false;
}

void NetworkManager::rejectIncoming() {
  if (server && server->hasClient()) {
    WiFiClient tempClient = server->available();
    String busyMsg = storage.getBusyMsg();
    tempClient.println(busyMsg);
    tempClient.flush();
    delay(10);
    tempClient.stop();
  }
}
