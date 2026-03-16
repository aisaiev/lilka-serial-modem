#include "ppp.h"
#include "network.h"
#include "storage.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// PPP logging levels:
// 0 = DISABLED
// 1 = ERROR only (connection failures, critical errors)
// 2 = INFO (ERROR + connection status, important events)
// 3 = DEBUG (INFO + detailed packet info, verbose)
#define PPP_LOG_LEVEL 0

// ERROR: critical failures only
#if PPP_LOG_LEVEL >= 1
#define PPPERR(msg)                                                            \
  do {                                                                         \
    Serial.print(msg);                                                         \
  } while (0)
#define PPPERRLN(msg)                                                          \
  do {                                                                         \
    Serial.println(msg);                                                       \
  } while (0)
#else
#define PPPERR(msg)                                                            \
  do {                                                                         \
  } while (0)
#define PPPERRLN(msg)                                                          \
  do {                                                                         \
  } while (0)
#endif

// INFO: Important connection events
#if PPP_LOG_LEVEL >= 2
#define PPPINFO(msg)                                                           \
  do {                                                                         \
    Serial.print(msg);                                                         \
  } while (0)
#define PPPINFOLN(msg)                                                         \
  do {                                                                         \
    Serial.println(msg);                                                       \
  } while (0)
#else
#define PPPINFO(msg)                                                           \
  do {                                                                         \
  } while (0)
#define PPPINFOLN(msg)                                                         \
  do {                                                                         \
  } while (0)
#endif

// DEBUG: Verbose packet details
#if PPP_LOG_LEVEL >= 3
#define PPPDBG(msg)                                                            \
  do {                                                                         \
    Serial.print(msg);                                                         \
  } while (0)
#define PPPDBGLN(msg)                                                          \
  do {                                                                         \
    Serial.println(msg);                                                       \
  } while (0)
#else
#define PPPDBG(msg)                                                            \
  do {                                                                         \
  } while (0)
#define PPPDBGLN(msg)                                                          \
  do {                                                                         \
  } while (0)
#endif

PPPController ppp;

static WiFiUDP udp;
static uint16_t lastSourcePort = 0;
static IPAddress lastDestIP(0, 0, 0, 0);

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_CONNECTION_HANDSHAKE_TIMEOUT 15000UL
#define TCP_CONNECTION_IDLE_TIMEOUT 60000UL

// FCS lookup table for PPP frame check sequence (CRC-16-CCITT)
static const uint16_t fcstab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48,
    0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 0x1081, 0x0108,
    0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 0x9cc9, 0x8d40, 0xbfdb,
    0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 0x2102, 0x308b, 0x0210, 0x1399,
    0x6726, 0x76af, 0x4434, 0x55bd, 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e,
    0xfae7, 0xc87c, 0xd9f5, 0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e,
    0x54b5, 0x453c, 0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd,
    0xc974, 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 0x5285,
    0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 0xdecd, 0xcf44,
    0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 0x6306, 0x728f, 0x4014,
    0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5,
    0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 0x7387, 0x620e, 0x5095, 0x411c, 0x35a3,
    0x242a, 0x16b1, 0x0738, 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862,
    0x9af9, 0x8b70, 0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e,
    0xf0b7, 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 0x18c1,
    0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 0xa50a, 0xb483,
    0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 0x2942, 0x38cb, 0x0a50,
    0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 0xb58b, 0xa402, 0x9699, 0x8710,
    0xf3af, 0xe226, 0xd0bd, 0xc134, 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7,
    0x6e6e, 0x5cf5, 0x4d7c, 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1,
    0xa33a, 0xb2b3, 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72,
    0x3efb, 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 0xe70e,
    0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 0x6b46, 0x7acf,
    0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 0xf78f, 0xe606, 0xd49d,
    0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c,
    0x3de3, 0x2c6a, 0x1ef1, 0x0f78};

PPPController::PPPController()
    : enabled(false), pppStarted(false), state(PPP_DEAD), identifier(0),
      mru(1500), accm(0xFFFFFFFF), magicNumber(0), clientIP(0), primaryDNS(0),
      secondaryDNS(0), framePos(0), inFrame(false), escapeNext(false),
      clientIPAddress(192, 168, 7, 2), serialReadCacheLen(0),
      serialReadOverflow(false) {}

void PPPController::begin() {
  // Generate random magic number for loop detection
  magicNumber = esp_random();

  // Default configuration
  mru = 1500;
  accm = 0xFFFFFFFF; // Escape all control characters initially
}

void PPPController::enable() {
  enabled = true;
  pppStarted = false; // Don't start PPP until first frame seen
  state = PPP_ESTABLISH;
  framePos = 0;
  inFrame = false;
  escapeNext = false;

  PPPERRLN("===== PPP Session Started =====");
  PPPINFOLN("[PPP] Mode enabled, waiting for 0x7E");
  PPPINFOLN(
      "[PPP] Modem will respond to AT commands until PPP traffic detected");

  // TODO: modem handshake sound

  // Don't send LCP Configure-Request yet - wait for client to initiate
}

void PPPController::disable() {
  if (state != PPP_DEAD) {
    // Send LCP Terminate-Request
    uint8_t termData[4] = {0};
    sendLCPPacket(LCP_TERM_REQ, getNextIdentifier(), termData, 0);
  }

  // Clean up all network connections
  lastSourcePort = 0;
  udp.stop();
  for (size_t i = 0; i < MAX_TCP_CONNECTIONS; ++i) {
    releaseConnection(&tcpConnections[i]);
  }

  // Reset PPP state
  enabled = false;
  pppStarted = false;
  state = PPP_DEAD;

  PPPINFOLN("[PPP] Mode disabled");
}

void PPPController::loop() {
  if (!enabled)
    return;

  // Handle PPP state machine timeouts and retransmissions
  // TODO: Implement state machine timers
}

void PPPController::processSerialData() {
  if (!enabled)
    return;
  flushSerialCache();

  // Read and process serial data byte by byte
  while (Serial1.available()) {
    uint8_t byte = Serial1.read();
    processFrameByte(byte);
  }
}

void PPPController::processFrameByte(uint8_t byte) {
  // Handle flag byte (0x7E) - frame delimiter
  if (byte == 0x7E) {
    if (!pppStarted) {
      // First PPP frame detected - now fully activate PPP mode
      pppStarted = true;
      PPPINFOLN("[PPP] First frame flag detected, PPP fully active");

      // Send our LCP Configure-Request now
      uint8_t options[20];
      size_t optLen = 0;

      // MRU option
      options[optLen++] = LCP_OPT_MRU;
      options[optLen++] = 4;
      options[optLen++] = (mru >> 8) & 0xFF;
      options[optLen++] = mru & 0xFF;

      // Magic Number option
      options[optLen++] = LCP_OPT_MAGIC;
      options[optLen++] = 6;
      options[optLen++] = (magicNumber >> 24) & 0xFF;
      options[optLen++] = (magicNumber >> 16) & 0xFF;
      options[optLen++] = (magicNumber >> 8) & 0xFF;
      options[optLen++] = magicNumber & 0xFF;

      sendLCPPacket(LCP_CONF_REQ, getNextIdentifier(), options, optLen);
    }

    if (inFrame && framePos > 0) {
      // End of frame - process it
      processCompleteFrame();
    }
    // Start new frame
    inFrame = true;
    framePos = 0;
    escapeNext = false;
    return;
  }

  if (!inFrame) {
    // Not in a frame, ignore byte
    return;
  }

  // Handle escape byte (0x7D)
  if (byte == 0x7D && !escapeNext) {
    escapeNext = true;
    return;
  }

  // Unescape if needed
  if (escapeNext) {
    byte ^= 0x20;
    escapeNext = false;
  }

  // Add byte to frame buffer
  if (framePos < sizeof(frameBuffer)) {
    frameBuffer[framePos++] = byte;
  } else {
    // Frame too large, discard
    inFrame = false;
    framePos = 0;
  }
}

void PPPController::processCompleteFrame() {
  // Minimum frame: Address(1) + Control(1) + Protocol(2) + FCS(2) = 6 bytes
  PPPDBG("[PPP] Frame len=");
  PPPDBGLN(String(framePos));

  if (framePos < 6) {
    PPPDBGLN("[PPP] Frame too short");
    return; // Frame too short
  }

  // Verify Address and Control bytes
  if (frameBuffer[0] != 0xFF || frameBuffer[1] != 0x03) {
    return; // Invalid frame
  }

  // Calculate and verify FCS
  uint16_t receivedFCS =
      frameBuffer[framePos - 2] | (frameBuffer[framePos - 1] << 8);
  uint16_t calculatedFCS = calculateFCS(frameBuffer, framePos - 2);

  if (receivedFCS != calculatedFCS) {
    PPPERRLN("[PPP] FCS error");
    return; // FCS mismatch
  }

  // Extract protocol
  uint16_t protocol = (frameBuffer[2] << 8) | frameBuffer[3];

  // Dispatch to protocol handler
  dispatchFrame(protocol, &frameBuffer[4], framePos - 6);
}

void PPPController::dispatchFrame(uint16_t protocol, const uint8_t *data,
                                  size_t len) {
  switch (protocol) {
  case PPP_LCP:
    handleLCPPacket(data, len);
    break;
  case PPP_IPCP:
    handleIPCPPacket(data, len);
    break;
  case PPP_IP:
    handleIPPacket(data, len);
    break;
  default:
    // Unknown protocol - send Protocol-Reject
    PPPERR("[PPP] Unknown protocol 0x");
    PPPERR(String(protocol, HEX));
    PPPERRLN("");
    break;
  }
}

void PPPController::processNetworkData() {
  if (!enabled || state != PPP_OPENED)
    return;
  for (size_t i = 0; i < MAX_TCP_CONNECTIONS; ++i) {
    TCPConnection *conn = &tcpConnections[i];
    if (!conn->active)
      continue;
    const size_t MAX_TCP_DATA = 1460;
    uint8_t buffer[MAX_TCP_DATA];

    while (conn->socket.available() > 0) {
      int len = conn->socket.read(buffer, MAX_TCP_DATA);
      if (len <= 0) {
        break;
      }
      PPPDBG("[PPP] TCP RX: ");
      PPPDBGLN(String(len));
      sendTCPPacketToSerial(conn, TCP_FLAG_ACK | TCP_FLAG_PSH, buffer, len,
                            conn->serverSeq, conn->clientSeq);
      conn->serverSeq += len;
      conn->lastActivity = millis();
    }

    if (!conn->socket.connected()) {
      // Remote closed connection - notify PPP client after draining data
      sendTCPPacketToSerial(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, nullptr, 0,
                            conn->serverSeq, conn->clientSeq);
      conn->serverSeq++;
      releaseConnection(conn);
      continue;
    }
  }

  // Check for UDP responses (only if we have a pending UDP port)
  if (lastSourcePort > 0) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0 && packetSize <= 1400) {
      PPPDBG("[PPP] UDP RX: ");
      PPPDBGLN(String(packetSize));

      uint8_t udpPayload[1500];
      int len = udp.read(udpPayload, sizeof(udpPayload));

      if (len > 0) {
        // Get remote IP and port
        IPAddress remoteIP = udp.remoteIP();
        uint16_t remotePort = udp.remotePort();

        PPPDBG("[PPP] UDP from ");
        PPPDBG(remoteIP.toString());
        PPPDBG(":");
        PPPDBG(String(remotePort));
        PPPDBG(" len=");
        PPPDBGLN(String(len));

        // Build IP + UDP packet to send back to Windows
        uint8_t response[1500];
        size_t totalLen = 20 + 8 + len; // IP + UDP + payload

        // IP header
        response[0] = 0x45; // Version 4, header length 5
        response[1] = 0x00; // TOS
        response[2] = (totalLen >> 8) & 0xFF;
        response[3] = totalLen & 0xFF;
        response[4] = 0x00; // ID
        response[5] = 0x00;
        response[6] = 0x00; // Flags
        response[7] = 0x00;
        response[8] = 64;    // TTL
        response[9] = 17;    // Protocol: UDP
        response[10] = 0x00; // Checksum (will calculate)
        response[11] = 0x00;

        // Source IP: Remote server that responded
        response[12] = remoteIP[0];
        response[13] = remoteIP[1];
        response[14] = remoteIP[2];
        response[15] = remoteIP[3];

        // Dest IP: Client (192.168.7.2)
        response[16] = 192;
        response[17] = 168;
        response[18] = 7;
        response[19] = 2;

        // Calculate IP header checksum
        uint32_t sum = 0;
        for (int i = 0; i < 20; i += 2) {
          if (i != 10) {
            sum += ((uint16_t)response[i] << 8) | response[i + 1];
          }
        }
        while (sum >> 16)
          sum = (sum & 0xFFFF) + (sum >> 16);
        uint16_t ipChecksum = ~sum;
        response[10] = (ipChecksum >> 8) & 0xFF;
        response[11] = ipChecksum & 0xFF;

        // UDP header
        response[20] = (remotePort >> 8) & 0xFF; // Source port
        response[21] = remotePort & 0xFF;
        response[22] = (lastSourcePort >> 8) & 0xFF; // Dest port
        response[23] = lastSourcePort & 0xFF;
        response[24] = ((8 + len) >> 8) & 0xFF; // UDP length
        response[25] = (8 + len) & 0xFF;
        response[26] = 0x00; // Checksum (optional for UDP)
        response[27] = 0x00;

        // Copy UDP payload
        memcpy(&response[28], udpPayload, len);

        // Send to Windows
        forwardIPToSerial(response, totalLen);
        PPPDBGLN("[PPP] UDP TX");
      }
    }
  }

  unsigned long now = millis();
  for (size_t i = 0; i < MAX_TCP_CONNECTIONS; ++i) {
    TCPConnection *conn = &tcpConnections[i];
    if (!conn->active)
      continue;
    unsigned long timeout = conn->handshakeComplete
                                ? TCP_CONNECTION_IDLE_TIMEOUT
                                : TCP_CONNECTION_HANDSHAKE_TIMEOUT;
    if (now - conn->lastActivity > timeout) {
      PPPDBGLN("[PPP] TCP idle timeout");
      releaseConnection(conn);
    }
  }
}

void PPPController::sendPPPFrame(uint16_t protocol, const uint8_t *data,
                                 size_t len) {
  // PPP frame format:
  // Flag (0x7E) | Address (0xFF) | Control (0x03) | Protocol (2 bytes) | Data |
  // FCS (2 bytes) | Flag (0x7E)

  uint8_t frame[1600];
  size_t pos = 0;

  // Address and Control (uncompressed for now)
  frame[pos++] = 0xFF;
  frame[pos++] = 0x03;

  // Protocol
  frame[pos++] = (protocol >> 8) & 0xFF;
  frame[pos++] = protocol & 0xFF;

  // Data
  if (data && len > 0) {
    memcpy(&frame[pos], data, len);
    pos += len;
  }

  // Calculate FCS over Address, Control, Protocol, and Data
  uint16_t fcs = calculateFCS(frame, pos);
  frame[pos++] = fcs & 0xFF;
  frame[pos++] = (fcs >> 8) & 0xFF;

  // Send flags unescaped, escape everything between flags
  Serial1.write(0x7E);
  escapeSerialData(frame, pos);
  Serial1.write(0x7E);
}

void PPPController::sendLCPPacket(uint8_t code, uint8_t id, const uint8_t *data,
                                  size_t len) {
  uint8_t packet[1500];
  packet[0] = code;
  packet[1] = id;
  uint16_t length = 4 + len;
  packet[2] = (length >> 8) & 0xFF;
  packet[3] = length & 0xFF;

  if (data && len > 0) {
    memcpy(&packet[4], data, len);
  }

  sendPPPFrame(PPP_LCP, packet, length);
}

void PPPController::sendIPCPPacket(uint8_t code, uint8_t id,
                                   const uint8_t *data, size_t len) {
  uint8_t packet[1500];
  packet[0] = code;
  packet[1] = id;
  uint16_t length = 4 + len;
  packet[2] = (length >> 8) & 0xFF;
  packet[3] = length & 0xFF;

  if (data && len > 0) {
    memcpy(&packet[4], data, len);
  }

  sendPPPFrame(PPP_IPCP, packet, length);
}

void PPPController::handleLCPPacket(const uint8_t *data, size_t len) {
  if (len < 4)
    return;

  uint8_t code = data[0];
  uint8_t id = data[1];
  uint16_t length = (data[2] << 8) | data[3];

  if (length > len)
    return;

  const uint8_t *payload = &data[4];
  size_t payloadLen = length - 4;

  switch (code) {
  case LCP_CONF_REQ:
    handleLCPConfigureRequest(id, payload, payloadLen);
    break;
  case LCP_CONF_ACK:
    handleLCPConfigureAck(id, payload, payloadLen);
    break;
  case LCP_TERM_REQ:
    handleLCPTerminateRequest(id);
    break;
  case LCP_ECHO_REQ:
    handleLCPEchoRequest(id, payload, payloadLen);
    break;
  default:
    // Unknown code - send Code-Reject
    sendLCPPacket(LCP_CODE_REJ, getNextIdentifier(), data, length);
    break;
  }
}

void PPPController::handleLCPConfigureRequest(uint8_t id,
                                              const uint8_t *options,
                                              size_t optLen) {
  // Parse LCP options and build response
  uint8_t ackOptions[256];
  uint8_t nakOptions[256];
  uint8_t rejOptions[256];
  size_t ackLen = 0, nakLen = 0, rejLen = 0;

  size_t pos = 0;
  while (pos < optLen) {
    if (pos + 2 > optLen)
      break; // Need at least type and length

    uint8_t type = options[pos];
    uint8_t len = options[pos + 1];

    if (len < 2 || pos + len > optLen)
      break; // Invalid length

    bool accept = true;
    bool nak = false;

    switch (type) {
    case LCP_OPT_MRU: // Maximum Receive Unit
      if (len == 4) {
        uint16_t peerMRU = (options[pos + 2] << 8) | options[pos + 3];
        if (peerMRU >= 296 && peerMRU <= 1500) {
          // Accept MRU
          PPPINFO("[LCP] Peer MRU=");
          PPPINFOLN(String(peerMRU));
        } else {
          // NAK with our preferred MRU
          nak = true;
          nakOptions[nakLen++] = type;
          nakOptions[nakLen++] = 4;
          nakOptions[nakLen++] = (mru >> 8) & 0xFF;
          nakOptions[nakLen++] = mru & 0xFF;
          accept = false;
        }
      }
      break;

    case LCP_OPT_ACCM: // Async Control Character Map
      if (len == 6) {
        // Accept any ACCM
        PPPINFOLN("[LCP] ACCM accepted");
      }
      break;

    case LCP_OPT_MAGIC: // Magic Number
      if (len == 6) {
        uint32_t peerMagic = ((uint32_t)options[pos + 2] << 24) |
                             ((uint32_t)options[pos + 3] << 16) |
                             ((uint32_t)options[pos + 4] << 8) |
                             (uint32_t)options[pos + 5];
        if (peerMagic == magicNumber) {
          // Loop detected!
          PPPERRLN("[LCP] Loop detected!");
          nak = true;
          nakOptions[nakLen++] = type;
          nakOptions[nakLen++] = 6;
          uint32_t newMagic = esp_random();
          nakOptions[nakLen++] = (newMagic >> 24) & 0xFF;
          nakOptions[nakLen++] = (newMagic >> 16) & 0xFF;
          nakOptions[nakLen++] = (newMagic >> 8) & 0xFF;
          nakOptions[nakLen++] = newMagic & 0xFF;
          accept = false;
        } else {
          PPPINFO("[LCP] Peer Magic=0x");
          PPPINFO(String(peerMagic, HEX));
          PPPINFOLN("");
        }
      }
      break;

    case LCP_OPT_PFC:  // Protocol Field Compression
    case LCP_OPT_ACFC: // Address/Control Field Compression
      // Accept these options
      PPPINFO("[LCP] ");
      PPPINFO(type == LCP_OPT_PFC ? "PFC" : "ACFC");
      PPPINFOLN(" accepted");
      break;

    case LCP_OPT_AUTH: // Authentication
      // Reject authentication for now
      PPPINFOLN("[LCP] Authentication rejected");
      memcpy(&rejOptions[rejLen], &options[pos], len);
      rejLen += len;
      accept = false;
      break;

    default:
      // Unknown option - reject it
      PPPINFO("[LCP] Unknown option ");
      PPPINFO(String(type));
      PPPINFOLN(" rejected");
      memcpy(&rejOptions[rejLen], &options[pos], len);
      rejLen += len;
      accept = false;
      break;
    }

    if (accept && !nak) {
      // Copy to ACK options
      memcpy(&ackOptions[ackLen], &options[pos], len);
      ackLen += len;
    }

    pos += len;
  }

  // Send response in priority order: REJ > NAK > ACK
  if (rejLen > 0) {
    sendLCPPacket(LCP_CONF_REJ, id, rejOptions, rejLen);
  } else if (nakLen > 0) {
    sendLCPPacket(LCP_CONF_NAK, id, nakOptions, nakLen);
  } else {
    sendLCPPacket(LCP_CONF_ACK, id, ackOptions, ackLen);
    if (state == PPP_ESTABLISH) {
      state = PPP_NETWORK;
      PPPINFOLN("[PPP] LCP negotiation complete");
      PPPINFOLN("[PPP] Starting IPCP negotiation");

      // Send our IPCP Configure-Request
      uint8_t ipcpOptions[20];
      size_t ipcpLen = 0;

      // IP Address option - request 192.168.7.1 for ourselves (server/modem
      // side)
      ipcpOptions[ipcpLen++] = IPCP_OPT_IP_ADDRESS;
      ipcpOptions[ipcpLen++] = 6;
      ipcpOptions[ipcpLen++] = 192;
      ipcpOptions[ipcpLen++] = 168;
      ipcpOptions[ipcpLen++] = 7;
      ipcpOptions[ipcpLen++] = 1; // Server uses .1, client will get .2

      PPPINFOLN("[IPCP] Requesting server IP 192.168.7.1");

      sendIPCPPacket(IPCP_CONF_REQ, getNextIdentifier(), ipcpOptions, ipcpLen);
    }
  }
}

void PPPController::handleLCPConfigureAck(uint8_t id, const uint8_t *options,
                                          size_t optLen) {
  // Our configuration was accepted
  PPPINFOLN("[LCP] Configuration accepted");
}

void PPPController::handleLCPTerminateRequest(uint8_t id) {
  sendLCPPacket(LCP_TERM_ACK, id, nullptr, 0);

  // Clean up all network connections
  lastSourcePort = 0;
  udp.stop();
  for (size_t i = 0; i < MAX_TCP_CONNECTIONS; ++i) {
    releaseConnection(&tcpConnections[i]);
  }

  // Reset PPP state
  state = PPP_DEAD;
  enabled = false;
  pppStarted = false;

  PPPINFOLN("[PPP] Connection terminated");
}

void PPPController::handleLCPEchoRequest(uint8_t id, const uint8_t *data,
                                         size_t len) {
  // Send Echo-Reply with same data
  sendLCPPacket(LCP_ECHO_REPLY, id, data, len);
}

void PPPController::handleIPCPPacket(const uint8_t *data, size_t len) {
  if (len < 4)
    return;

  uint8_t code = data[0];
  uint8_t id = data[1];
  uint16_t length = (data[2] << 8) | data[3];

  if (length > len)
    return;

  size_t payloadLen = 0;
  if (length > 4) {
    payloadLen = length - 4;
  }
  const uint8_t *payload = &data[4];

  switch (code) {
  case IPCP_CONF_REQ:
    handleIPCPConfigureRequest(id, payload, payloadLen);
    break;
  case IPCP_CONF_ACK:
    handleIPCPConfigureAck(id, payload, payloadLen);
    break;
  default:
    break;
  }
}

void PPPController::handleIPCPConfigureRequest(uint8_t id,
                                               const uint8_t *options,
                                               size_t optLen) {
  // Parse IPCP options and assign IP address to client
  uint8_t ackOptions[256];
  uint8_t nakOptions[256];
  size_t ackLen = 0, nakLen = 0;

  // Get WiFi gateway IP for DNS
  IPAddress gateway = WiFi.gatewayIP();
  uint32_t dnsIP = ((uint32_t)gateway[0] << 24) | ((uint32_t)gateway[1] << 16) |
                   ((uint32_t)gateway[2] << 8) | (uint32_t)gateway[3];

  size_t pos = 0;
  while (pos < optLen) {
    if (pos + 2 > optLen)
      break;

    uint8_t type = options[pos];
    uint8_t len = options[pos + 1];

    if (len < 2 || pos + len > optLen)
      break;

    bool accept = true;
    bool nak = false;

    switch (type) {
    case IPCP_OPT_IP_ADDRESS: // IP Address
      if (len == 6) {
        uint32_t requestedIP = ((uint32_t)options[pos + 2] << 24) |
                               ((uint32_t)options[pos + 3] << 16) |
                               ((uint32_t)options[pos + 4] << 8) |
                               (uint32_t)options[pos + 5];

        if (requestedIP == 0) {
          // Client requesting IP - assign 192.168.7.2
          nak = true;
          nakOptions[nakLen++] = type;
          nakOptions[nakLen++] = 6;
          nakOptions[nakLen++] = 192;
          nakOptions[nakLen++] = 168;
          nakOptions[nakLen++] = 7;
          nakOptions[nakLen++] = 2;
          clientIP = (192 << 24) | (168 << 16) | (7 << 8) | 2;
          clientIPAddress = IPAddress(192, 168, 7, 2);
          accept = false;
          PPPINFOLN("[IPCP] Assigned IP 192.168.7.2");
        } else {
          clientIP = requestedIP;
          clientIPAddress =
              IPAddress((requestedIP >> 24) & 0xFF, (requestedIP >> 16) & 0xFF,
                        (requestedIP >> 8) & 0xFF, requestedIP & 0xFF);
          PPPINFO("[IPCP] Client IP ");
          PPPINFO(String((requestedIP >> 24) & 0xFF));
          PPPINFO(".");
          PPPINFO(String((requestedIP >> 16) & 0xFF));
          PPPINFO(".");
          PPPINFO(String((requestedIP >> 8) & 0xFF));
          PPPINFO(".");
          PPPINFO(String(requestedIP & 0xFF));
          PPPINFOLN("");
        }
      }
      break;

    case IPCP_OPT_PRIMARY_DNS: // Primary DNS
      if (len == 6) {
        uint32_t requestedDNS = ((uint32_t)options[pos + 2] << 24) |
                                ((uint32_t)options[pos + 3] << 16) |
                                ((uint32_t)options[pos + 4] << 8) |
                                (uint32_t)options[pos + 5];

        if (requestedDNS == 0) {
          // Client requesting DNS - provide gateway IP
          nak = true;
          nakOptions[nakLen++] = type;
          nakOptions[nakLen++] = 6;
          nakOptions[nakLen++] = (dnsIP >> 24) & 0xFF;
          nakOptions[nakLen++] = (dnsIP >> 16) & 0xFF;
          nakOptions[nakLen++] = (dnsIP >> 8) & 0xFF;
          nakOptions[nakLen++] = dnsIP & 0xFF;
          primaryDNS = dnsIP;
          accept = false;
          PPPINFO("[IPCP] Primary DNS ");
          PPPINFO(String(gateway[0]));
          PPPINFO(".");
          PPPINFO(String(gateway[1]));
          PPPINFO(".");
          PPPINFO(String(gateway[2]));
          PPPINFO(".");
          PPPINFO(String(gateway[3]));
          PPPINFOLN("");
        } else {
          primaryDNS = requestedDNS;
        }
      }
      break;

    case IPCP_OPT_SECONDARY_DNS: // Secondary DNS
      if (len == 6) {
        uint32_t requestedDNS = ((uint32_t)options[pos + 2] << 24) |
                                ((uint32_t)options[pos + 3] << 16) |
                                ((uint32_t)options[pos + 4] << 8) |
                                (uint32_t)options[pos + 5];

        if (requestedDNS == 0) {
          // Provide same as primary for now
          nak = true;
          nakOptions[nakLen++] = type;
          nakOptions[nakLen++] = 6;
          nakOptions[nakLen++] = (dnsIP >> 24) & 0xFF;
          nakOptions[nakLen++] = (dnsIP >> 16) & 0xFF;
          nakOptions[nakLen++] = (dnsIP >> 8) & 0xFF;
          nakOptions[nakLen++] = dnsIP & 0xFF;
          secondaryDNS = dnsIP;
          accept = false;
        } else {
          secondaryDNS = requestedDNS;
        }
      }
      break;

    default:
      // Unknown option - accept it anyway
      break;
    }

    if (accept && !nak) {
      memcpy(&ackOptions[ackLen], &options[pos], len);
      ackLen += len;
    }

    pos += len;
  }

  // Send response
  if (nakLen > 0) {
    sendIPCPPacket(IPCP_CONF_NAK, id, nakOptions, nakLen);
  } else {
    sendIPCPPacket(IPCP_CONF_ACK, id, ackOptions, ackLen);
    if (state == PPP_NETWORK) {
      state = PPP_OPENED;
      PPPINFOLN("[PPP] IPCP negotiation complete");
      PPPINFOLN("[PPP] PPP connection established!");
      PPPINFOLN("[PPP] Client IP: 192.168.7.2");
      PPPINFOLN("[PPP] Ready for IP traffic");
    }
  }
}

void PPPController::handleIPCPConfigureAck(uint8_t id, const uint8_t *options,
                                           size_t optLen) {
  PPPINFOLN("[IPCP] Configuration accepted");
}

void PPPController::handleIPPacket(const uint8_t *data, size_t len) {
  // Forward IP packet to network
  forwardIPToNetwork(data, len);
}

void PPPController::forwardIPToNetwork(const uint8_t *data, size_t len) {
  if (len < 20) {
    PPPERRLN("[PPP] IP packet too small");
    return;
  }

  // Parse IP header
  uint8_t version = (data[0] >> 4) & 0x0F;
  uint8_t headerLen = (data[0] & 0x0F) * 4;
  uint8_t protocol = data[9];

  // Extract destination IP (big-endian)
  uint32_t destIP = ((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) |
                    ((uint32_t)data[18] << 8) | (uint32_t)data[19];

  // Extract source port for UDP/TCP (if present)
  uint16_t srcPort = 0;
  uint16_t dstPort = 0;
  if (len > headerLen + 4 && (protocol == 6 || protocol == 17)) { // TCP or UDP
    srcPort = ((uint16_t)data[headerLen] << 8) | data[headerLen + 1];
    dstPort = ((uint16_t)data[headerLen + 2] << 8) | data[headerLen + 3];
  }

  PPPDBG("[PPP] IP v");
  PPPDBG(String(version));
  PPPDBG(" proto=");
  PPPDBG(String(protocol));
  PPPDBG(" dst=");
  PPPDBG(String(data[16]));
  PPPDBG(".");
  PPPDBG(String(data[17]));
  PPPDBG(".");
  PPPDBG(String(data[18]));
  PPPDBG(".");
  PPPDBGLN(String(data[19]));

  if (protocol == 6) {
    handleTCPPacket(data, len);
  } else if (protocol == 17) {
    PPPDBG(" UDP ");
    PPPDBG(String(srcPort));
    PPPDBG("->");
    PPPDBGLN(String(dstPort));

    // Handle UDP packet
    IPAddress destAddr(data[16], data[17], data[18], data[19]);

    // Send UDP payload (skip IP and UDP headers)
    if (len > headerLen + 8) { // UDP header is 8 bytes
      size_t payloadLen = len - headerLen - 8;
      PPPDBG("[PPP] Sending UDP to ");
      PPPDBG(destAddr.toString());
      PPPDBG(":");
      PPPDBG(String(dstPort));
      PPPDBG(" len=");
      PPPDBGLN(String(payloadLen));

      // Start listening on the source port if not already
      if (lastSourcePort != srcPort) {
        udp.begin(srcPort);
        PPPDBG("[PPP] UDP listening on port ");
        PPPDBGLN(String(srcPort));
      }

      udp.beginPacket(destAddr, dstPort);
      udp.write(&data[headerLen + 8], payloadLen);
      udp.endPacket();

      lastSourcePort = srcPort;
      lastDestIP = destAddr; // Store as IPAddress, not uint32_t
    }
  } else if (protocol == 1) {
    PPPDBGLN(" ICMP");

    // Handle ICMP Echo Request (ping) - reply as if we're the destination
    if (len > headerLen && data[headerLen] == 8) { // Type 8 = Echo Request
      PPPDBG("[PPP] ICMP Echo Request to ");
      PPPDBG(String(data[16]));
      PPPDBG(".");
      PPPDBG(String(data[17]));
      PPPDBG(".");
      PPPDBG(String(data[18]));
      PPPDBG(".");
      PPPDBGLN(String(data[19]));

      // Build ICMP Echo Reply packet
      uint8_t reply[1500];
      memcpy(reply, data, len); // Copy original packet

      // Change source IP to destination (reply FROM the pinged host)
      reply[12] = data[16];
      reply[13] = data[17];
      reply[14] = data[18];
      reply[15] = data[19];

      // Change dest IP to source (reply TO our client)
      reply[16] = data[12];
      reply[17] = data[13];
      reply[18] = data[14];
      reply[19] = data[15];

      // Change ICMP type from Echo Request (8) to Echo Reply (0)
      reply[headerLen] = 0;

      // Update TTL
      reply[8] = 64;

      // Recalculate IP header checksum
      reply[10] = 0;
      reply[11] = 0;
      uint32_t sum = 0;
      for (int i = 0; i < headerLen; i += 2) {
        sum += ((uint16_t)reply[i] << 8) | reply[i + 1];
      }
      while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
      uint16_t ipChecksum = ~sum;
      reply[10] = (ipChecksum >> 8) & 0xFF;
      reply[11] = ipChecksum & 0xFF;

      // Recalculate ICMP checksum
      reply[headerLen + 2] = 0;
      reply[headerLen + 3] = 0;
      sum = 0;
      for (size_t i = headerLen; i < len; i += 2) {
        if (i + 1 < len) {
          sum += ((uint16_t)reply[i] << 8) | reply[i + 1];
        } else {
          sum += (uint16_t)reply[i] << 8;
        }
      }
      while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
      uint16_t icmpChecksum = ~sum;
      reply[headerLen + 2] = (icmpChecksum >> 8) & 0xFF;
      reply[headerLen + 3] = icmpChecksum & 0xFF;

      // Send reply back to Windows
      forwardIPToSerial(reply, len);
      PPPDBGLN("[PPP] ICMP Echo Reply sent");
    }
  } else {
    PPPDBGLN("");
    PPPDBG("[PPP] Unsupported protocol: ");
    PPPDBGLN(String(protocol));
  }
}

PPPController::TCPConnection *PPPController::allocateConnection() {
  unsigned long now = millis();
  TCPConnection *oldest = nullptr;
  unsigned long oldestIdle = 0;

  for (size_t i = 0; i < MAX_TCP_CONNECTIONS; ++i) {
    TCPConnection *entry = &tcpConnections[i];
    if (!entry->active) {
      entry->active = true;
      entry->handshakeComplete = false;
      entry->synAckSent = false;
      entry->lastActivity = now;
      return entry;
    }

    unsigned long idle = now - entry->lastActivity;
    if (oldest == nullptr || idle > oldestIdle) {
      oldest = entry;
      oldestIdle = idle;
    }
  }

  if (oldest) {
    releaseConnection(oldest);
    oldest->lastActivity = now;
    oldest->active = true;
    oldest->handshakeComplete = false;
    oldest->synAckSent = false;
    return oldest;
  }

  return nullptr;
}

PPPController::TCPConnection *
PPPController::findConnection(const IPAddress &serverIP, uint16_t serverPort,
                              uint16_t clientPort) {
  for (size_t i = 0; i < MAX_TCP_CONNECTIONS; ++i) {
    TCPConnection *entry = &tcpConnections[i];
    if (!entry->active)
      continue;
    if (entry->serverPort == serverPort && entry->clientPort == clientPort &&
        entry->serverIP == serverIP) {
      return entry;
    }
  }
  return nullptr;
}

void PPPController::releaseConnection(TCPConnection *conn) {
  if (!conn)
    return;
  if (conn->socket.connected()) {
    conn->socket.stop();
  }
  conn->active = false;
  conn->handshakeComplete = false;
  conn->synAckSent = false;
  conn->serverPort = 0;
  conn->clientPort = 0;
  conn->clientSeq = 0;
  conn->serverSeq = 0;
  conn->serverIP = IPAddress(0, 0, 0, 0);
  conn->lastActivity = 0;
}

void PPPController::handleTCPPacket(const uint8_t *data, size_t len) {
  if (len < 40)
    return;

  uint8_t ipHeaderLen = (data[0] & 0x0F) * 4;
  if (len < ipHeaderLen + 20)
    return;

  const uint8_t *tcpPacket = &data[ipHeaderLen];
  uint8_t tcpHeaderLen = ((tcpPacket[12] >> 4) & 0x0F) * 4;
  if (tcpHeaderLen < 20 || len < ipHeaderLen + tcpHeaderLen)
    return;

  IPAddress serverAddr(data[16], data[17], data[18], data[19]);
  uint16_t srcPort = (tcpPacket[0] << 8) | tcpPacket[1];
  uint16_t dstPort = (tcpPacket[2] << 8) | tcpPacket[3];

  uint32_t seq = ((uint32_t)tcpPacket[4] << 24) |
                 ((uint32_t)tcpPacket[5] << 16) |
                 ((uint32_t)tcpPacket[6] << 8) | (uint32_t)tcpPacket[7];
  uint32_t ack = ((uint32_t)tcpPacket[8] << 24) |
                 ((uint32_t)tcpPacket[9] << 16) |
                 ((uint32_t)tcpPacket[10] << 8) | (uint32_t)tcpPacket[11];
  uint8_t flags = tcpPacket[13];

  size_t payloadLen = 0;
  if (len > ipHeaderLen + tcpHeaderLen) {
    payloadLen = len - ipHeaderLen - tcpHeaderLen;
  }
  const uint8_t *payload = tcpPacket + tcpHeaderLen;
  uint32_t clientNextSeq = seq + payloadLen;
  if (flags & TCP_FLAG_SYN)
    clientNextSeq++;
  if (flags & TCP_FLAG_FIN)
    clientNextSeq++;

  TCPConnection *conn = findConnection(serverAddr, dstPort, srcPort);
  unsigned long now = millis();
  if (conn) {
    unsigned long timeout = conn->handshakeComplete
                                ? TCP_CONNECTION_IDLE_TIMEOUT
                                : TCP_CONNECTION_HANDSHAKE_TIMEOUT;
    if (now - conn->lastActivity > timeout) {
      releaseConnection(conn);
      conn = nullptr;
    }
  }

  if (flags & TCP_FLAG_SYN) {
    if (conn) {
      conn->lastActivity = now;
      conn->clientSeq = clientNextSeq;
      if (!conn->handshakeComplete) {
        uint32_t synSeq = (conn->serverSeq > 0) ? conn->serverSeq - 1 : 1000;
        sendTCPPacketToSerial(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, nullptr, 0,
                              synSeq, conn->clientSeq, true);
        conn->synAckSent = true;
        return;
      }
      releaseConnection(conn);
      conn = nullptr;
    }

    conn = allocateConnection();
    if (!conn) {
      sendTCPReset(serverAddr, dstPort, srcPort, 0, clientNextSeq);
      return;
    }

    conn->serverIP = serverAddr;
    conn->serverPort = dstPort;
    conn->clientPort = srcPort;
    conn->clientSeq = clientNextSeq;
    conn->serverSeq = 1000;
    conn->lastActivity = now;
    conn->handshakeComplete = false;

    if (!conn->socket.connect(serverAddr, dstPort, 3000)) {
      PPPERRLN("[PPP] TCP connect failed");
      sendTCPReset(serverAddr, dstPort, srcPort, conn->serverSeq,
                   conn->clientSeq);
      releaseConnection(conn);
      return;
    }

    conn->synAckSent = true;
    PPPINFOLN("[PPP] TCP connected");
    sendTCPPacketToSerial(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, nullptr, 0,
                          conn->serverSeq, conn->clientSeq, true);
    conn->serverSeq++;
    return;
  }

  if (!conn || !conn->active) {
    sendTCPReset(serverAddr, dstPort, srcPort, 0, clientNextSeq);
    return;
  }

  conn->lastActivity = now;
  if (!conn->handshakeComplete && (flags & TCP_FLAG_ACK) &&
      ack >= conn->serverSeq) {
    conn->handshakeComplete = true;
  }
  if (flags & TCP_FLAG_RST) {
    releaseConnection(conn);
    return;
  }

  if (flags & TCP_FLAG_FIN) {
    sendTCPPacketToSerial(conn, TCP_FLAG_ACK, nullptr, 0, conn->serverSeq,
                          clientNextSeq);
    releaseConnection(conn);
    return;
  }

  if (payloadLen > 0) {
    conn->socket.write(payload, payloadLen);
    conn->clientSeq = clientNextSeq;
    conn->lastActivity = millis();
  } else if (clientNextSeq > conn->clientSeq) {
    conn->clientSeq = clientNextSeq;
  }

  if (ack > conn->serverSeq) {
    conn->serverSeq = ack;
  }
}

void PPPController::sendTCPPacketToSerial(TCPConnection *conn, uint8_t flags,
                                          const uint8_t *payload,
                                          size_t payloadLen, uint32_t seq,
                                          uint32_t ack, bool includeOptions) {
  if (!conn)
    return;

  const size_t tcpHeaderLen = includeOptions ? 24 : 20;
  size_t ipLen = 20 + tcpHeaderLen + payloadLen;
  if (ipLen > 1500) {
    PPPERRLN("[PPP] TCP packet too large");
    return;
  }

  uint8_t packet[1500] = {0};
  packet[0] = 0x45;
  packet[1] = 0x00;
  packet[2] = (ipLen >> 8) & 0xFF;
  packet[3] = ipLen & 0xFF;
  packet[4] = 0x00;
  packet[5] = 0x00;
  packet[6] = 0x40;
  packet[7] = 0x00;
  packet[8] = 64;
  packet[9] = 6;

  packet[12] = conn->serverIP[0];
  packet[13] = conn->serverIP[1];
  packet[14] = conn->serverIP[2];
  packet[15] = conn->serverIP[3];

  packet[16] = clientIPAddress[0];
  packet[17] = clientIPAddress[1];
  packet[18] = clientIPAddress[2];
  packet[19] = clientIPAddress[3];

  packet[20] = (conn->serverPort >> 8) & 0xFF;
  packet[21] = conn->serverPort & 0xFF;
  packet[22] = (conn->clientPort >> 8) & 0xFF;
  packet[23] = conn->clientPort & 0xFF;

  packet[24] = (seq >> 24) & 0xFF;
  packet[25] = (seq >> 16) & 0xFF;
  packet[26] = (seq >> 8) & 0xFF;
  packet[27] = seq & 0xFF;

  packet[28] = (ack >> 24) & 0xFF;
  packet[29] = (ack >> 16) & 0xFF;
  packet[30] = (ack >> 8) & 0xFF;
  packet[31] = ack & 0xFF;

  packet[32] = (tcpHeaderLen / 4) << 4;
  packet[33] = flags;
  packet[34] = 0x20;
  packet[35] = 0x00;

  if (includeOptions) {
    packet[40] = 0x02;
    packet[41] = 0x04;
    packet[42] = 0x05;
    packet[43] = 0xB4;
  }

  if (payloadLen > 0 && payload) {
    memcpy(&packet[20 + tcpHeaderLen], payload, payloadLen);
  }

  uint32_t sum = 0;
  for (int i = 0; i < 20; i += 2) {
    if (i == 10)
      continue;
    sum += ((uint16_t)packet[i] << 8) | packet[i + 1];
  }
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  uint16_t ipChecksum = ~sum;
  packet[10] = (ipChecksum >> 8) & 0xFF;
  packet[11] = ipChecksum & 0xFF;

  uint32_t tcpSum = 0;
  tcpSum += (packet[12] << 8) | packet[13];
  tcpSum += (packet[14] << 8) | packet[15];
  tcpSum += (packet[16] << 8) | packet[17];
  tcpSum += (packet[18] << 8) | packet[19];
  tcpSum += 6;
  tcpSum += tcpHeaderLen + payloadLen;

  size_t checksumOffset = 20 + 16;
  for (size_t i = 20; i < 20 + tcpHeaderLen + payloadLen; i += 2) {
    if (i == checksumOffset)
      continue;
    uint16_t word = 0;
    if (i + 1 < 20 + tcpHeaderLen + payloadLen) {
      word = ((uint16_t)packet[i] << 8) | packet[i + 1];
    } else {
      word = ((uint16_t)packet[i] << 8);
    }
    tcpSum += word;
  }

  while (tcpSum >> 16) {
    tcpSum = (tcpSum & 0xFFFF) + (tcpSum >> 16);
  }
  tcpSum = ~tcpSum;
  packet[checksumOffset] = (tcpSum >> 8) & 0xFF;
  packet[checksumOffset + 1] = tcpSum & 0xFF;

  forwardIPToSerial(packet, 20 + tcpHeaderLen + payloadLen);
}

void PPPController::sendTCPReset(const IPAddress &destIP, uint16_t destPort,
                                 uint16_t srcPort, uint32_t seq, uint32_t ack) {
  TCPConnection temp;
  temp.serverIP = destIP;
  temp.serverPort = destPort;
  temp.clientPort = srcPort;
  sendTCPPacketToSerial(&temp, TCP_FLAG_RST | TCP_FLAG_ACK, nullptr, 0, seq,
                        ack);
}

void PPPController::forwardIPToSerial(const uint8_t *data, size_t len) {
  // Encapsulate IP packet in PPP frame and send to serial
  sendPPPFrame(PPP_IP, data, len);
}

uint16_t PPPController::calculateFCS(const uint8_t *data, size_t len) {
  uint16_t fcs = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    fcs = (fcs >> 8) ^ fcstab[(fcs ^ data[i]) & 0xFF];
  }

  return fcs ^ 0xFFFF;
}

void PPPController::escapeSerialData(const uint8_t *data, size_t len) {
  // Write data to serial, escaping control characters while capturing any data
  // that arrives during transmission so we can process it later without
  // overflow.
  captureIncomingSerial();
  for (size_t i = 0; i < len; i++) {
    uint8_t c = data[i];

    // Check if character needs escaping based on ACCM
    bool needsEscape = false;
    if (c < 0x20) {
      needsEscape = (accm & (1 << c)) != 0;
    } else if (c == 0x7D || c == 0x7E) {
      needsEscape = true;
    }

    if (needsEscape) {
      Serial1.write(0x7D);
      captureIncomingSerial();
      Serial1.write(c ^ 0x20);
    } else {
      Serial1.write(c);
    }
    captureIncomingSerial();
  }
  captureIncomingSerial();
}

void PPPController::captureIncomingSerial() {
  while (Serial1.available()) {
    if (serialReadCacheLen < SERIAL_READ_BUFFER) {
      serialReadCache[serialReadCacheLen++] = Serial1.read();
    } else {
      serialReadOverflow = true;
      Serial1.read();
    }
  }
}

void PPPController::flushSerialCache() {
  if (serialReadCacheLen == 0) {
    return;
  }

  for (size_t i = 0; i < serialReadCacheLen; ++i) {
    processFrameByte(serialReadCache[i]);
  }
  serialReadCacheLen = 0;

  if (serialReadOverflow) {
    PPPERRLN("[PPP] Serial buffer overflow while transmitting");
    serialReadOverflow = false;
  }
}
