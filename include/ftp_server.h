#ifndef LILKA_OLDNET_MODEM_FTP_SERVER_H
#define LILKA_OLDNET_MODEM_FTP_SERVER_H

#include <Arduino.h>

class FtpServerManager {
public:
  void begin();
  void loop();
  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled; }

private:
  void startServer();
  void stopServer();

  bool enabled = false;
  bool started = false;
  uint32_t lastStartAttempt = 0;
};

extern FtpServerManager ftpServer;

#endif // LILKA_OLDNET_MODEM_FTP_SERVER_H
