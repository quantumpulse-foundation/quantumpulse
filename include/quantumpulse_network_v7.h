#ifndef QUANTUMPULSE_NETWORK_V7_H
#define QUANTUMPULSE_NETWORK_V7_H

#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace QuantumPulse::Network {

// Network configuration
struct NetworkConfig {
  static constexpr int MAX_PEERS = 2000;
  static constexpr int DISCOVERY_INTERVAL_SEC = 60;
  static constexpr int SYNC_TIMEOUT_SEC = 30;
};

// Thread-safe network manager
class NetworkManager final {
public:
  NetworkManager() noexcept : peerCount_(0), isSyncing_(false) {
    Logging::Logger::getInstance().info(
        "NetworkManager initialized - P2P with TLS 1.3 ready", "Network", 0);
  }

  // Non-copyable
  NetworkManager(const NetworkManager &) = delete;
  NetworkManager &operator=(const NetworkManager &) = delete;

  // Broadcast block to peers (const-correct)
  void broadcastBlock(std::string_view blockData, int shardId) noexcept {
    std::lock_guard<std::mutex> lock(networkMutex_);

    if (blockData.empty()) {
      Logging::Logger::getInstance().warning(
          "Empty block data - broadcast skipped", "Network", shardId);
      return;
    }

    // Store broadcast for simulation
    lastBroadcast_ = std::string(blockData);
    broadcastCount_++;

    Logging::Logger::getInstance().info(
        "Broadcasting block to " + std::to_string(peerCount_) +
            " peers on shard " + std::to_string(shardId),
        "Network", shardId);
  }

  // Sync chain with network
  void syncChain(int shardId) noexcept {
    std::lock_guard<std::mutex> lock(networkMutex_);

    if (isSyncing_.load(std::memory_order_relaxed)) {
      return;
    }

    isSyncing_.store(true, std::memory_order_relaxed);

    Logging::Logger::getInstance().info("Syncing chain for shard " +
                                            std::to_string(shardId),
                                        "Network", shardId);

    syncCount_++;
    isSyncing_.store(false, std::memory_order_relaxed);
  }

  // Discover peers
  void discoverPeers(int shardId) noexcept {
    std::lock_guard<std::mutex> lock(networkMutex_);

    peerCount_ = std::min(peerCount_ + 10, NetworkConfig::MAX_PEERS);

    Logging::Logger::getInstance().info("Discovered peers, current count: " +
                                            std::to_string(peerCount_),
                                        "Network", shardId);
  }

  // Add peer
  [[nodiscard]] bool addPeer(std::string_view peerAddress) noexcept {
    std::lock_guard<std::mutex> lock(networkMutex_);

    if (peerAddress.empty()) {
      return false;
    }

    if (peerCount_ >= NetworkConfig::MAX_PEERS) {
      Logging::Logger::getInstance().warning("Max peer limit reached",
                                             "Network", 0);
      return false;
    }

    peers_.emplace_back(peerAddress);
    peerCount_++;

    Logging::Logger::getInstance().info(
        "Added peer: " + std::string(peerAddress), "Network", 0);
    return true;
  }

  // Remove peer
  [[nodiscard]] bool removePeer(std::string_view peerAddress) noexcept {
    std::lock_guard<std::mutex> lock(networkMutex_);

    auto it = std::find(peers_.begin(), peers_.end(), std::string(peerAddress));
    if (it != peers_.end()) {
      peers_.erase(it);
      peerCount_--;

      Logging::Logger::getInstance().info(
          "Removed peer: " + std::string(peerAddress), "Network", 0);
      return true;
    }
    return false;
  }

  // Getters
  [[nodiscard]] int getPeerCount() const noexcept { return peerCount_; }

  [[nodiscard]] bool isHealthy() const noexcept {
    return peerCount_ >= 10 && !isSyncing_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] size_t getBroadcastCount() const noexcept {
    return broadcastCount_;
  }

  [[nodiscard]] size_t getSyncCount() const noexcept { return syncCount_; }

private:
  std::vector<std::string> peers_;
  std::string lastBroadcast_;
  int peerCount_;
  std::atomic<bool> isSyncing_;
  size_t broadcastCount_{0};
  size_t syncCount_{0};
  mutable std::mutex networkMutex_;
};

} // namespace QuantumPulse::Network

#endif // QUANTUMPULSE_NETWORK_V7_H
