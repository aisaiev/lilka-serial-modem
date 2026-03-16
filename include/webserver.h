#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

class WebServer {
public:
  WebServer();
  void begin();
  void loop();

private:
  AsyncWebServer *server;
  AsyncEventSource *events;
  unsigned long lastStatusUpdate;

  void setupRoutes();
  void sendStatusUpdate();

  // Route handlers
  void handleRoot(AsyncWebServerRequest *request);
  void handleGetStatus(AsyncWebServerRequest *request);
  void handleGetSettings(AsyncWebServerRequest *request);
  void handleGetSpeedDials(AsyncWebServerRequest *request);
  void handleSaveSettings(AsyncWebServerRequest *request);
  void handleReloadSettings(AsyncWebServerRequest *request);
  void handleDial(AsyncWebServerRequest *request);
  void handleHangup(AsyncWebServerRequest *request);
  void handleUpdateSpeedDial(AsyncWebServerRequest *request);
  void handleFactoryDefaults(AsyncWebServerRequest *request);

  // File manager handlers
  void handleListFiles(AsyncWebServerRequest *request);
  void handleUploadFile(AsyncWebServerRequest *request, String filename,
                        size_t index, uint8_t *data, size_t len, bool final);
  void handleDownloadFile(AsyncWebServerRequest *request);
  void handleDeleteFile(AsyncWebServerRequest *request);
  void handleCreateFolder(AsyncWebServerRequest *request);

  // Helper functions
  bool deleteDirectory(const String &path);
};

extern WebServer webServer;
