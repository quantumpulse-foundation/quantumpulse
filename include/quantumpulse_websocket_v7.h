#ifndef QUANTUMPULSE_WEBSOCKET_V7_H
#define QUANTUMPULSE_WEBSOCKET_V7_H

#include "quantumpulse_logging_v7.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace QuantumPulse::WebSocket {

// WebSocket configuration
struct WSConfig {
  static constexpr int DEFAULT_PORT = 8081;
  static constexpr int MAX_CLIENTS = 1000;
  static constexpr int BUFFER_SIZE = 65536;
  static constexpr int PING_INTERVAL_SEC = 30;
};

// WebSocket message types
enum class MessageType {
  TEXT = 0x01,
  BINARY = 0x02,
  CLOSE = 0x08,
  PING = 0x09,
  PONG = 0x0A
};

// Event types for broadcasting
enum class EventType {
  NEW_BLOCK,
  NEW_TRANSACTION,
  PRICE_UPDATE,
  MINING_STATUS,
  PEER_CONNECTED,
  PEER_DISCONNECTED
};

// WebSocket client
struct WSClient {
  int socket{-1};
  std::string address;
  time_t connectedAt{0};
  bool isAuthenticated{false};
  std::vector<EventType> subscriptions;
};

// Base64 encoding for WebSocket handshake
inline std::string base64Encode(const unsigned char *data, size_t len) {
  static const char *chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((len + 2) / 3) * 4);

  for (size_t i = 0; i < len; i += 3) {
    unsigned int n = (static_cast<unsigned int>(data[i]) << 16);
    if (i + 1 < len)
      n |= (static_cast<unsigned int>(data[i + 1]) << 8);
    if (i + 2 < len)
      n |= static_cast<unsigned int>(data[i + 2]);

    result += chars[(n >> 18) & 0x3F];
    result += chars[(n >> 12) & 0x3F];
    result += (i + 1 < len) ? chars[(n >> 6) & 0x3F] : '=';
    result += (i + 2 < len) ? chars[n & 0x3F] : '=';
  }
  return result;
}

// WebSocket Server
class WebSocketServer final {
public:
  explicit WebSocketServer(int port = WSConfig::DEFAULT_PORT) noexcept
      : port_(port), running_(false), serverSocket_(-1) {
    Logging::Logger::getInstance().info(
        "WebSocket server initialized on port " + std::to_string(port),
        "WebSocket", 0);
  }

  ~WebSocketServer() noexcept { stop(); }

  // Non-copyable
  WebSocketServer(const WebSocketServer &) = delete;
  WebSocketServer &operator=(const WebSocketServer &) = delete;

  // Start server
  bool start() noexcept {
    if (running_.load())
      return false;

    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0)
      return false;

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverSocket_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
      close(serverSocket_);
      return false;
    }

    if (listen(serverSocket_, WSConfig::MAX_CLIENTS) < 0) {
      close(serverSocket_);
      return false;
    }

    running_.store(true);
    acceptThread_ = std::thread(&WebSocketServer::acceptLoop, this);
    pingThread_ = std::thread(&WebSocketServer::pingLoop, this);

    Logging::Logger::getInstance().info(
        "WebSocket server started at ws://localhost:" + std::to_string(port_),
        "WebSocket", 0);
    return true;
  }

  // Stop server
  void stop() noexcept {
    if (!running_.load())
      return;

    running_.store(false);

    if (serverSocket_ >= 0) {
      shutdown(serverSocket_, SHUT_RDWR);
      close(serverSocket_);
      serverSocket_ = -1;
    }

    // Close all clients
    {
      std::lock_guard<std::mutex> lock(clientsMutex_);
      for (auto &[id, client] : clients_) {
        if (client.socket >= 0) {
          close(client.socket);
        }
      }
      clients_.clear();
    }

    if (acceptThread_.joinable())
      acceptThread_.join();
    if (pingThread_.joinable())
      pingThread_.join();
  }

  // Broadcast message to all clients
  void broadcast(EventType event, const std::string &data) noexcept {
    std::string message = formatEvent(event, data);

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto &[id, client] : clients_) {
      bool subscribed = client.subscriptions.empty();
      for (const auto &sub : client.subscriptions) {
        if (sub == event) {
          subscribed = true;
          break;
        }
      }

      if (subscribed) {
        sendMessage(client.socket, message);
      }
    }

    broadcastCount_++;
  }

  // Broadcast to specific event subscribers
  void broadcastNewBlock(const std::string &blockHash, int height) noexcept {
    std::stringstream ss;
    ss << "{\"hash\":\"" << blockHash << "\",\"height\":" << height << "}";
    broadcast(EventType::NEW_BLOCK, ss.str());
  }

  void broadcastPriceUpdate(double price) noexcept {
    broadcast(EventType::PRICE_UPDATE,
              "{\"price\":" + std::to_string(static_cast<int64_t>(price)) +
                  "}");
  }

  void broadcastTransaction(const std::string &txId, double amount) noexcept {
    std::stringstream ss;
    ss << "{\"txId\":\"" << txId << "\",\"amount\":" << amount << "}";
    broadcast(EventType::NEW_TRANSACTION, ss.str());
  }

  // Get stats
  [[nodiscard]] size_t getClientCount() const noexcept {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.size();
  }

  [[nodiscard]] size_t getBroadcastCount() const noexcept {
    return broadcastCount_;
  }

  [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

private:
  int port_;
  std::atomic<bool> running_;
  int serverSocket_;
  std::thread acceptThread_;
  std::thread pingThread_;
  std::map<int, WSClient> clients_;
  mutable std::mutex clientsMutex_;
  size_t broadcastCount_{0};
  int nextClientId_{0};

  void acceptLoop() noexcept {
    while (running_.load()) {
      sockaddr_in clientAddr{};
      socklen_t len = sizeof(clientAddr);

      int clientSocket = accept(
          serverSocket_, reinterpret_cast<sockaddr *>(&clientAddr), &len);

      if (clientSocket < 0) {
        if (running_.load())
          continue;
        break;
      }

      std::thread(&WebSocketServer::handleClient, this, clientSocket).detach();
    }
  }

  void handleClient(int clientSocket) noexcept {
    // Receive HTTP upgrade request
    char buffer[WSConfig::BUFFER_SIZE];
    ssize_t bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
      close(clientSocket);
      return;
    }

    buffer[bytes] = '\0';

    // Parse WebSocket key
    std::string key = extractWebSocketKey(buffer);
    if (key.empty()) {
      close(clientSocket);
      return;
    }

    // Send upgrade response
    std::string acceptKey = computeAcceptKey(key);
    std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " +
                           acceptKey + "\r\n\r\n";

    send(clientSocket, response.c_str(), response.length(), 0);

    // Add client
    int clientId;
    {
      std::lock_guard<std::mutex> lock(clientsMutex_);
      clientId = nextClientId_++;
      clients_[clientId] = {clientSocket, "", std::time(nullptr), false, {}};
    }

    Logging::Logger::getInstance().info("WebSocket client connected: " +
                                            std::to_string(clientId),
                                        "WebSocket", 0);

    // Handle messages
    while (running_.load()) {
      bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
      if (bytes <= 0)
        break;

      // Parse WebSocket frame (simplified)
      if (bytes >= 2) {
        uint8_t opcode = buffer[0] & 0x0F;

        if (opcode == 0x08) { // Close
          break;
        } else if (opcode == 0x09) { // Ping
          buffer[0] = 0x8A;          // Pong
          send(clientSocket, buffer, bytes, 0);
        }
      }
    }

    // Remove client
    {
      std::lock_guard<std::mutex> lock(clientsMutex_);
      clients_.erase(clientId);
    }

    close(clientSocket);

    Logging::Logger::getInstance().info("WebSocket client disconnected: " +
                                            std::to_string(clientId),
                                        "WebSocket", 0);
  }

  void pingLoop() noexcept {
    while (running_.load()) {
      std::this_thread::sleep_for(
          std::chrono::seconds(WSConfig::PING_INTERVAL_SEC));

      std::lock_guard<std::mutex> lock(clientsMutex_);
      for (auto &[id, client] : clients_) {
        // Send ping frame
        char ping[2] = {static_cast<char>(0x89), 0x00};
        send(client.socket, ping, 2, 0);
      }
    }
  }

  [[nodiscard]] std::string
  extractWebSocketKey(const char *request) const noexcept {
    const char *keyHeader = "Sec-WebSocket-Key: ";
    const char *pos = strstr(request, keyHeader);
    if (!pos)
      return "";

    pos += strlen(keyHeader);
    const char *end = strstr(pos, "\r\n");
    if (!end)
      return "";

    return std::string(pos, end - pos);
  }

  [[nodiscard]] std::string
  computeAcceptKey(const std::string &key) const noexcept {
    std::string combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(combined.c_str()),
         combined.length(), hash);

    return base64Encode(hash, SHA_DIGEST_LENGTH);
  }

  [[nodiscard]] std::string
  formatEvent(EventType event, const std::string &data) const noexcept {
    std::string eventName;
    switch (event) {
    case EventType::NEW_BLOCK:
      eventName = "new_block";
      break;
    case EventType::NEW_TRANSACTION:
      eventName = "new_transaction";
      break;
    case EventType::PRICE_UPDATE:
      eventName = "price_update";
      break;
    case EventType::MINING_STATUS:
      eventName = "mining_status";
      break;
    case EventType::PEER_CONNECTED:
      eventName = "peer_connected";
      break;
    case EventType::PEER_DISCONNECTED:
      eventName = "peer_disconnected";
      break;
    }

    return "{\"event\":\"" + eventName + "\",\"data\":" + data + "}";
  }

  void sendMessage(int socket, const std::string &message) noexcept {
    if (socket < 0 || message.empty())
      return;

    std::vector<char> frame;
    frame.push_back(static_cast<char>(0x81)); // Text frame, FIN

    if (message.length() <= 125) {
      frame.push_back(static_cast<char>(message.length()));
    } else if (message.length() <= 65535) {
      frame.push_back(126);
      frame.push_back(static_cast<char>((message.length() >> 8) & 0xFF));
      frame.push_back(static_cast<char>(message.length() & 0xFF));
    } else {
      frame.push_back(127);
      for (int i = 7; i >= 0; --i) {
        frame.push_back(
            static_cast<char>((message.length() >> (8 * i)) & 0xFF));
      }
    }

    frame.insert(frame.end(), message.begin(), message.end());
    send(socket, frame.data(), frame.size(), 0);
  }
};

} // namespace QuantumPulse::WebSocket

#endif // QUANTUMPULSE_WEBSOCKET_V7_H
