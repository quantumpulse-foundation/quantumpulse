#ifndef QUANTUMPULSE_P2P_PROTOCOL_V7_H
#define QUANTUMPULSE_P2P_PROTOCOL_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace QuantumPulse::P2P {

// P2P Message Types (Bitcoin protocol compatible)
enum class MessageType {
  VERSION,     // Protocol version handshake
  VERACK,      // Version acknowledgment
  ADDR,        // Address propagation
  GETADDR,     // Request addresses
  INV,         // Inventory (blocks/txs)
  GETDATA,     // Request data
  NOTFOUND,    // Data not found
  GETBLOCKS,   // Request block hashes
  GETHEADERS,  // Request block headers
  TX,          // Transaction
  BLOCK,       // Block
  HEADERS,     // Block headers
  PING,        // Ping for connectivity
  PONG,        // Pong response
  MEMPOOL,     // Request mempool
  REJECT,      // Reject message
  SENDHEADERS, // Prefer headers announcement
  FEEFILTER,   // Fee filter
  SENDCMPCT,   // Compact blocks
  CMPCTBLOCK,  // Compact block
  GETBLOCKTXN, // Get block transactions
  BLOCKTXN     // Block transactions
};

// Peer information
struct PeerInfo {
  std::string address;
  int port;
  std::string userAgent;
  int version;
  int64_t connectedTime;
  int64_t lastSeen;
  int64_t bytesReceived;
  int64_t bytesSent;
  bool inbound;
  int startingHeight;
  double pingTime;
  int banScore;
  std::set<std::string> services;
};

// Network message
struct NetworkMessage {
  MessageType type;
  std::string payload;
  std::string checksum;
  size_t length;
  int64_t timestamp;
};

// Inventory item
struct InvItem {
  int type; // 1=TX, 2=Block
  std::string hash;
};

// P2P Network Manager (Bitcoin Core-like)
class NetworkManager final {
public:
  NetworkManager(int port = 8333, int maxPeers = 125) noexcept
      : port_(port), maxPeers_(maxPeers) {

    // Set network magic bytes
    networkMagic_ = {0xF9, 0xBE, 0xB4, 0xD9}; // Like Bitcoin mainnet

    Logging::Logger::getInstance().info(
        "P2P Network initialized on port " + std::to_string(port), "P2P", 0);
  }

  // Connect to peer
  bool connectPeer(const std::string &address, int port) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (peers_.size() >= maxPeers_) {
      return false;
    }

    std::string peerKey = address + ":" + std::to_string(port);
    if (peers_.count(peerKey)) {
      return false; // Already connected
    }

    PeerInfo peer;
    peer.address = address;
    peer.port = port;
    peer.connectedTime = std::time(nullptr);
    peer.lastSeen = peer.connectedTime;
    peer.bytesReceived = 0;
    peer.bytesSent = 0;
    peer.inbound = false;
    peer.version = 70015;
    peer.userAgent = "/QuantumPulse:7.0.0/";
    peer.pingTime = 0.0;
    peer.banScore = 0;

    peers_[peerKey] = peer;

    // Send version message
    sendVersion(peerKey);

    return true;
  }

  // Disconnect peer
  void disconnectPeer(const std::string &peerKey) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.erase(peerKey);
  }

  // Send message to peer
  void sendMessage(const std::string &peerKey,
                   const NetworkMessage &msg) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (peers_.count(peerKey)) {
      peers_[peerKey].bytesSent += msg.length;
      outboundQueue_.push({peerKey, msg});
    }
  }

  // Broadcast to all peers
  void broadcast(const NetworkMessage &msg) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &[peerKey, peer] : peers_) {
      peer.bytesSent += msg.length;
      outboundQueue_.push({peerKey, msg});
    }
  }

  // Announce transaction
  void announceTransaction(const std::string &txid) noexcept {
    NetworkMessage msg;
    msg.type = MessageType::INV;
    msg.payload = "1:" + txid; // Type 1 = TX
    msg.timestamp = std::time(nullptr);
    msg.length = msg.payload.length();

    broadcast(msg);
    announcedTxs_.insert(txid);
  }

  // Announce block
  void announceBlock(const std::string &blockHash, int height) noexcept {
    NetworkMessage msg;
    msg.type = MessageType::INV;
    msg.payload = "2:" + blockHash; // Type 2 = Block
    msg.timestamp = std::time(nullptr);
    msg.length = msg.payload.length();

    broadcast(msg);

    Logging::Logger::getInstance().info(
        "Block announced: " + blockHash.substr(0, 16) + "...", "P2P", 0);
  }

  // Request block
  void requestBlock(const std::string &peerKey,
                    const std::string &blockHash) noexcept {
    NetworkMessage msg;
    msg.type = MessageType::GETDATA;
    msg.payload = "2:" + blockHash;
    msg.timestamp = std::time(nullptr);
    msg.length = msg.payload.length();

    sendMessage(peerKey, msg);
  }

  // Get peer info
  std::vector<PeerInfo> getPeers() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PeerInfo> result;
    for (const auto &[key, peer] : peers_) {
      result.push_back(peer);
    }
    return result;
  }

  // Get peer count
  size_t getPeerCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_.size();
  }

  // Get network stats
  std::map<std::string, int64_t> getStats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t totalReceived = 0, totalSent = 0;
    for (const auto &[key, peer] : peers_) {
      totalReceived += peer.bytesReceived;
      totalSent += peer.bytesSent;
    }

    return {{"peers", peers_.size()},
            {"bytesReceived", totalReceived},
            {"bytesSent", totalSent},
            {"txAnnounced", announcedTxs_.size()},
            {"port", port_}};
  }

  // Add seed node
  void addSeedNode(const std::string &address, int port) noexcept {
    seedNodes_.push_back({address, port});
  }

  // Connect to seed nodes
  void connectToSeedNodes() noexcept {
    for (const auto &[address, port] : seedNodes_) {
      connectPeer(address, port);
    }
  }

  // Ban peer
  void banPeer(const std::string &peerKey, int banTime = 86400) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    bannedPeers_[peerKey] = std::time(nullptr) + banTime;
    peers_.erase(peerKey);

    Logging::Logger::getInstance().warning("Peer banned: " + peerKey, "P2P", 0);
  }

private:
  int port_;
  size_t maxPeers_;
  std::array<uint8_t, 4> networkMagic_;

  mutable std::mutex mutex_;
  std::map<std::string, PeerInfo> peers_;
  std::set<std::string> announcedTxs_;
  std::map<std::string, int64_t> bannedPeers_;
  std::vector<std::pair<std::string, int>> seedNodes_;

  std::queue<std::pair<std::string, NetworkMessage>> outboundQueue_;

  void sendVersion(const std::string &peerKey) {
    NetworkMessage msg;
    msg.type = MessageType::VERSION;
    msg.payload = "70015:/QuantumPulse:7.0.0/";
    msg.timestamp = std::time(nullptr);
    msg.length = msg.payload.length();

    sendMessage(peerKey, msg);
  }
};

} // namespace QuantumPulse::P2P

#endif // QUANTUMPULSE_P2P_PROTOCOL_V7_H
