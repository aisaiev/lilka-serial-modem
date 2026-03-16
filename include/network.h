#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>

class NetworkManager {
public:
  NetworkManager();
  void begin();
  void loop();

  // WiFi management
  void connectWiFi();
  void disconnectWiFi();
  bool isWiFiConnected() const;
  String getSSID() const;
  String getIP() const;

  // TCP Client
  bool connectToHost(const String &host, int port);
  void disconnect();
  bool isClientConnected() const;
  int clientAvailable();
  int readFromClient(uint8_t *buffer, size_t size);
  void writeToClient(const uint8_t *buffer, size_t size);

  // TCP Server
  bool serverHasClient();
  bool acceptIncoming();
  void rejectIncoming();

private:
  WiFiClient client;
  WiFiServer *server;
  bool wifiConnected;
  int serverPort;
};

extern NetworkManager network;

#endif // NETWORK_H
