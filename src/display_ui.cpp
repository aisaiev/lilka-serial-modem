#include "display_ui.h"
#include "config.h"

DisplayUI displayUI;

DisplayUI::DisplayUI()
    : lastUpdate(0), needsRedraw(true), canvas(nullptr), wifiConnected(false),
      modemConnected(false), pppActive(false), pppNegotiating(false),
      baudRate(DEFAULT_BAUD), fileTransferActive(false),
      fileTransferIsUpload(true), fileTransferFilename("") {}

void DisplayUI::begin() {
  // Create canvas for double buffering
  canvas = new lilka::Canvas(lilka::display.width(), lilka::display.height());
  lilka::display.fillScreen(lilka::colors::Black);
  showWelcome();
  delay(750);
  needsRedraw = true;
}

void DisplayUI::update() {
  unsigned long now = millis();
  if (now - lastUpdate < STATUS_UPDATE_INTERVAL && !needsRedraw) {
    return;
  }

  lastUpdate = now;
  needsRedraw = false;
  showStatus();
}

void DisplayUI::showWelcome() {
  if (!canvas)
    return;

  int16_t x1, y1;
  uint16_t w, h;

  canvas->fillScreen(lilka::colors::Black);
  canvas->setTextColor(lilka::colors::Gainsboro);
  canvas->setFont(u8g2_font_Terminal_tr);
  canvas->setTextSize(2);

  canvas->getTextBounds("LILKA SERIAL", 0, 0, &x1, &y1, &w, &h);
  canvas->setCursor((canvas->width() - w) / 2, 110);
  canvas->println("LILKA SERIAL");

  canvas->getTextBounds("MODEM", 0, 0, &x1, &y1, &w, &h);
  canvas->setCursor((canvas->width() - w) / 2, 150);
  canvas->println("MODEM");

  lilka::display.drawCanvas(canvas);

  canvas->setFont(FONT_10x20);
}

void DisplayUI::showStatus() {
  if (!canvas)
    return;

  int16_t x1, y1;
  uint16_t w, h;

  canvas->fillScreen(lilka::colors::Black);

  drawStatusBar();
  drawConnectionInfo();

  // Button hints at bottom - centered
  canvas->setTextSize(1);
  canvas->setTextColor(lilka::colors::Cool_gray);
  canvas->getTextBounds("Select=Menu C=Reset", 0, 0, &x1, &y1, &w, &h);
  canvas->setCursor((canvas->width() - w) / 2, canvas->height() - 5);
  canvas->print("Select=Menu C=Reset");

  // Push entire canvas to display in one operation (no flicker!)
  lilka::display.drawCanvas(canvas);
}

void DisplayUI::drawStatusBar() {
  int16_t x1, y1;
  uint16_t w, h;

  // Top status bar - account for rounded corners
  const int TOP_MARGIN = 12;
  const int SIDE_MARGIN = 15;
  const int BAR_HEIGHT = 20;

  canvas->fillRect(0, 0, canvas->width(), BAR_HEIGHT,
                   lilka::colors::Blue_sapphire);

  canvas->setTextSize(1);

  // WiFi status
  canvas->setFont(u8g2_font_open_iconic_embedded_2x_t);
  canvas->setCursor(canvas->width() - SIDE_MARGIN - 35, TOP_MARGIN + 6);
  if (wifiConnected) {
    canvas->setTextColor(lilka::colors::Green);
  } else {
    canvas->setTextColor(lilka::colors::Red);
  }
  canvas->print((char)0x50);
  canvas->setFont(FONT_10x20);
}

void DisplayUI::drawConnectionInfo() {
  const int SIDE_MARGIN = 15;
  int y = 45; // Start below status bar with more space
  canvas->setTextSize(1);

  // WiFi Section
  canvas->setTextColor(lilka::colors::Cyan_process);
  canvas->setCursor(SIDE_MARGIN, y);
  canvas->println("WiFi Status:");
  y += 18;

  const int INDENT = 0;
  canvas->setTextColor(lilka::colors::Gainsboro);
  canvas->setCursor(SIDE_MARGIN + INDENT, y);
  if (wifiConnected) {
    canvas->print("SSID: ");
    canvas->println(wifiSSID);
    y += 18;
    canvas->setCursor(SIDE_MARGIN + INDENT, y);
    canvas->print("IP: ");
    canvas->println(wifiIP);
  } else {
    canvas->println("Not connected");
  }

  y += 28;

  // Modem Section
  canvas->setTextColor(lilka::colors::Cyan_process);
  canvas->setCursor(SIDE_MARGIN, y);
  canvas->println("Modem Status:");
  y += 18;

  canvas->setTextColor(lilka::colors::Gainsboro);
  canvas->setCursor(SIDE_MARGIN + INDENT, y);
  if (fileTransferActive) {
    canvas->setTextColor(lilka::colors::Arylide_yellow);
    if (fileTransferIsUpload) {
      canvas->println("UPLOADING FILE...");
    } else {
      canvas->println("DOWNLOADING FILE...");
    }
    y += 18;
    canvas->setTextColor(lilka::colors::Gainsboro);
    canvas->setCursor(SIDE_MARGIN + INDENT, y);
    canvas->setTextWrap(false);
    canvas->println(fileTransferFilename);
  } else if (pppActive) {
    canvas->setTextColor(lilka::colors::Kelly_green);
    canvas->println("PPP MODE ACTIVE");
    y += 18;
    canvas->setTextColor(lilka::colors::Gainsboro);
    canvas->setCursor(SIDE_MARGIN + INDENT, y);
    canvas->println("Protocol: Point-to-Point");
  } else if (pppNegotiating) {
    canvas->setTextColor(lilka::colors::Arylide_yellow);
    canvas->println("NEGOTIATING PPP...");
    y += 18;
    canvas->setTextColor(lilka::colors::Gainsboro);
    canvas->setCursor(SIDE_MARGIN + INDENT, y);
    canvas->println("Waiting for client");
  } else if (modemConnected) {
    canvas->setTextColor(lilka::colors::Kelly_green);
    canvas->print("CONNECTED");
    y += 18;
    canvas->setTextColor(lilka::colors::Gainsboro);
    canvas->setCursor(SIDE_MARGIN + INDENT, y);
    canvas->print("Host: ");
    canvas->setTextWrap(false);
    canvas->println(modemHost);
    y += 18;
    canvas->setCursor(SIDE_MARGIN + INDENT, y);
    canvas->print("Time: ");
    canvas->println(modemDuration);
  } else {
    canvas->setTextColor(lilka::colors::Arylide_yellow);
    canvas->println("Ready (Command Mode)");
  }

  y += 28;

  // Serial Settings
  canvas->setTextColor(lilka::colors::Cyan_process);
  canvas->setCursor(SIDE_MARGIN, y);
  canvas->println("Serial Port:");
  y += 18;

  canvas->setTextColor(lilka::colors::Gainsboro);
  canvas->setCursor(SIDE_MARGIN + INDENT, y);
  canvas->print("Baud Rate: ");
  canvas->print(baudRate);
  canvas->println(" bps");
}

void DisplayUI::setWiFiStatus(bool connected, const String &ssid,
                              const String &ip) {
  if (wifiConnected != connected || wifiSSID != ssid || wifiIP != ip) {
    wifiConnected = connected;
    wifiSSID = ssid;
    wifiIP = ip;
    needsRedraw = true;
  }
}

void DisplayUI::setModemStatus(bool connected, bool isPPP, bool isNegotiating,
                               const String &host, const String &duration) {
  if (modemConnected != connected || pppActive != isPPP ||
      pppNegotiating != isNegotiating || modemHost != host ||
      modemDuration != duration) {
    modemConnected = connected;
    pppActive = isPPP;
    pppNegotiating = isNegotiating;
    modemHost = host;
    modemDuration = duration;
    needsRedraw = true;
  }
}

void DisplayUI::setSerialBaud(int baud) {
  if (baudRate != baud) {
    baudRate = baud;
    needsRedraw = true;
  }
}

void DisplayUI::setFileTransferStatus(bool active, bool isUpload,
                                      const String &filename) {
  if (fileTransferActive != active || fileTransferIsUpload != isUpload ||
      fileTransferFilename != filename) {
    fileTransferActive = active;
    fileTransferIsUpload = isUpload;
    fileTransferFilename = filename;
    needsRedraw = true;
    // Force immediate update during file transfer
    if (active) {
      showStatus();
    }
  }
}

void DisplayUI::clear() {
  lilka::display.fillScreen(lilka::colors::Black);
  needsRedraw = true;
}
