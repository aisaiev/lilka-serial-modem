#include "ftp_server.h"
#include "storage.h"
#include <FtpServer.h>
#include <SD.h>
#include <WiFi.h>
#include <lilka/fileutils.h>

static FtpServer ftpSrv;
FtpServerManager ftpServer;

void FtpServerManager::begin() {
  enabled = storage.getFtpEnabled();
  started = false;
  lastStartAttempt = 0;
}

void FtpServerManager::setEnabled(bool newEnabled) {
  enabled = newEnabled;
  storage.setFtpEnabled(newEnabled);
  if (!enabled) {
    stopServer();
  } else if (WiFi.status() == WL_CONNECTED) {
    startServer();
  }
}

void FtpServerManager::loop() {
  bool desiredEnabled = storage.getFtpEnabled();
  if (desiredEnabled != enabled) {
    enabled = desiredEnabled;
    if (!enabled) {
      stopServer();
    }
  }

  if (!enabled) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!started) {
    startServer();
  }

  if (started) {
    ftpSrv.handleFTP();
  }
}

void FtpServerManager::startServer() {
  if (started) {
    return;
  }

  uint32_t now = millis();
  if ((now - lastStartAttempt) < 5000) {
    return;
  }
  lastStartAttempt = now;

  ftpSrv.begin("lilka", "lilka", "Welcome to Lilka FTP server");
  started = true;
  Serial.println("[FTP] Server started");
}

void FtpServerManager::stopServer() {
  if (!started) {
    return;
  }

  ftpSrv.end();
  started = false;
  Serial.println("[FTP] Server stopped");
}
