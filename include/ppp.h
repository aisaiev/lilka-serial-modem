#ifndef PPP_H
#define PPP_H

#include <Arduino.h>
#include <WiFi.h>

// PPP Protocol codes
#define PPP_IP 0x0021   // Internet Protocol
#define PPP_IPCP 0x8021 // IP Control Protocol
#define PPP_LCP 0xC021  // Link Control Protocol
#define PPP_PAP 0xC023  // Password Authentication Protocol
#define PPP_CHAP 0xC223 // Challenge Handshake Authentication Protocol

// LCP packet types
#define LCP_CONF_REQ 1     // Configure-Request
#define LCP_CONF_ACK 2     // Configure-Ack
#define LCP_CONF_NAK 3     // Configure-Nak
#define LCP_CONF_REJ 4     // Configure-Reject
#define LCP_TERM_REQ 5     // Terminate-Request
#define LCP_TERM_ACK 6     // Terminate-Ack
#define LCP_CODE_REJ 7     // Code-Reject
#define LCP_PROTO_REJ 8    // Protocol-Reject
#define LCP_ECHO_REQ 9     // Echo-Request
#define LCP_ECHO_REPLY 10  // Echo-Reply
#define LCP_DISCARD_REQ 11 // Discard-Request

// LCP configuration options
#define LCP_OPT_MRU 1   // Maximum-Receive-Unit
#define LCP_OPT_ACCM 2  // Async-Control-Character-Map
#define LCP_OPT_AUTH 3  // Authentication-Protocol
#define LCP_OPT_MAGIC 5 // Magic-Number
#define LCP_OPT_PFC 7   // Protocol-Field-Compression
#define LCP_OPT_ACFC 8  // Address-and-Control-Field-Compression

// IPCP packet types (same as LCP)
#define IPCP_CONF_REQ 1
#define IPCP_CONF_ACK 2
#define IPCP_CONF_NAK 3
#define IPCP_CONF_REJ 4
#define IPCP_TERM_REQ 5
#define IPCP_TERM_ACK 6

// IPCP configuration options
#define IPCP_OPT_IP_ADDRESS 3      // IP-Address
#define IPCP_OPT_PRIMARY_DNS 129   // Primary DNS Server Address
#define IPCP_OPT_SECONDARY_DNS 131 // Secondary DNS Server Address

// PPP States
enum PPPState {
  PPP_DEAD,         // Physical layer not ready
  PPP_ESTABLISH,    // LCP negotiation
  PPP_AUTHENTICATE, // Authentication phase
  PPP_NETWORK,      // NCP negotiation (IPCP)
  PPP_OPENED,       // Network layer protocol running
  PPP_TERMINATE     // Termination phase
};

class PPPController {
public:
  PPPController();
  void begin();
  void loop();

  // State management
  bool isEnabled() const { return enabled; }
  bool isPPPActive() const { return enabled && pppStarted; }
  void enable();
  void disable();
  PPPState getState() const { return state; }

  // Data handling
  void processSerialData();
  void processNetworkData();

private:
  struct TCPConnection {
    WiFiClient socket;
    IPAddress serverIP;
    uint16_t serverPort;
    uint16_t clientPort;
    uint32_t clientSeq;
    uint32_t serverSeq;
    bool active;
    bool handshakeComplete;
    bool synAckSent;
    unsigned long lastActivity;
  };

  static constexpr size_t MAX_TCP_CONNECTIONS = 4;

  bool enabled;
  bool pppStarted; // True once first PPP frame (0x7E) is seen
  PPPState state;
  uint8_t identifier; // Packet identifier counter

  // Configuration
  uint16_t mru;         // Maximum Receive Unit
  uint32_t accm;        // Async Control Character Map
  uint32_t magicNumber; // Magic number for loop detection

  // Client IP configuration
  uint32_t clientIP;
  uint32_t primaryDNS;
  uint32_t secondaryDNS;
  static constexpr size_t SERIAL_READ_BUFFER = 4096;
  uint8_t serialReadCache[SERIAL_READ_BUFFER];
  size_t serialReadCacheLen;
  bool serialReadOverflow;

  // Frame parsing state
  uint8_t frameBuffer[2048]; // Buffer for incoming frame
  size_t framePos;           // Current position in frame buffer
  bool inFrame;              // Currently receiving a frame
  bool escapeNext;           // Next byte is escaped

  // PPP packet handling
  void sendLCPPacket(uint8_t code, uint8_t id, const uint8_t *data, size_t len);
  void sendIPCPPacket(uint8_t code, uint8_t id, const uint8_t *data,
                      size_t len);
  void sendPPPFrame(uint16_t protocol, const uint8_t *data, size_t len);

  // LCP handlers
  void handleLCPPacket(const uint8_t *data, size_t len);
  void handleLCPConfigureRequest(uint8_t id, const uint8_t *options,
                                 size_t optLen);
  void handleLCPConfigureAck(uint8_t id, const uint8_t *options, size_t optLen);
  void handleLCPTerminateRequest(uint8_t id);
  void handleLCPEchoRequest(uint8_t id, const uint8_t *data, size_t len);

  // IPCP handlers
  void handleIPCPPacket(const uint8_t *data, size_t len);
  void handleIPCPConfigureRequest(uint8_t id, const uint8_t *options,
                                  size_t optLen);
  void handleIPCPConfigureAck(uint8_t id, const uint8_t *options,
                              size_t optLen);

  // IP packet forwarding
  void handleIPPacket(const uint8_t *data, size_t len);
  void forwardIPToNetwork(const uint8_t *data, size_t len);
  void forwardIPToSerial(const uint8_t *data, size_t len);

  // Frame processing
  void processFrameByte(uint8_t byte);
  void processCompleteFrame();
  void dispatchFrame(uint16_t protocol, const uint8_t *data, size_t len);

  // Utilities
  uint8_t getNextIdentifier() { return ++identifier; }
  uint16_t calculateFCS(const uint8_t *data, size_t len);
  void escapeSerialData(const uint8_t *data, size_t len);
  void captureIncomingSerial();
  void flushSerialCache();

  TCPConnection tcpConnections[MAX_TCP_CONNECTIONS];
  IPAddress clientIPAddress;
  TCPConnection *allocateConnection();
  TCPConnection *findConnection(const IPAddress &serverIP, uint16_t serverPort,
                                uint16_t clientPort);
  void releaseConnection(TCPConnection *conn);
  void handleTCPPacket(const uint8_t *data, size_t len);
  void sendTCPPacketToSerial(TCPConnection *conn, uint8_t flags,
                             const uint8_t *payload, size_t payloadLen,
                             uint32_t seq, uint32_t ack,
                             bool includeOptions = false);
  void sendTCPReset(const IPAddress &destIP, uint16_t destPort,
                    uint16_t srcPort, uint32_t seq, uint32_t ack);
};

extern PPPController ppp;

#endif // PPP_H
