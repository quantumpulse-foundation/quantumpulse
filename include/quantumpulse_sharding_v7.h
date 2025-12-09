#ifndef QUANTUMPULSE_SHARDING_V7_H
#define QUANTUMPULSE_SHARDING_V7_H

#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace QuantumPulse::Sharding {

// Sharding configuration
struct ShardingConfig {
  static constexpr int MAX_SHARDS = 2048;
  static constexpr int ACTIVE_SHARDS = 16;
};

// Thread-safe sharding manager
class ShardingManager final {
public:
  ShardingManager() noexcept : shardCount_(ShardingConfig::ACTIVE_SHARDS) {
    Logging::Logger::getInstance().info("ShardingManager initialized with " +
                                            std::to_string(shardCount_) +
                                            " active shards",
                                        "Sharding", 0);
  }

  // Non-copyable
  ShardingManager(const ShardingManager &) = delete;
  ShardingManager &operator=(const ShardingManager &) = delete;

  // Assign data to shard
  void assignShard(std::string_view dataId, int shardId) noexcept {
    std::lock_guard<std::mutex> lock(shardMutex_);

    int effectiveShardId = shardId;
    if (shardId < 0 || shardId >= shardCount_) {
      effectiveShardId = calculateShardId(dataId);
    }

    shardAssignments_[std::string(dataId)] = effectiveShardId;
  }

  // Sync all shards
  void syncShards() noexcept {
    std::lock_guard<std::mutex> lock(shardMutex_);

    Logging::Logger::getInstance().info(
        "Syncing " + std::to_string(shardCount_) + " shards", "Sharding", 0);

    syncCount_++;
  }

  // Validate shard ID
  [[nodiscard]] bool validateShard(int shardId) const noexcept {
    return shardId >= 0 && shardId < shardCount_;
  }

  // Get shard for data
  [[nodiscard]] int getShardForData(std::string_view dataId) const noexcept {
    std::lock_guard<std::mutex> lock(shardMutex_);

    auto it = shardAssignments_.find(std::string(dataId));
    if (it != shardAssignments_.end()) {
      return it->second;
    }

    return calculateShardId(dataId);
  }

  // Getters
  [[nodiscard]] int getShardCount() const noexcept { return shardCount_; }

  [[nodiscard]] int getMaxShards() const noexcept {
    return ShardingConfig::MAX_SHARDS;
  }

  [[nodiscard]] size_t getAssignmentCount() const noexcept {
    std::lock_guard<std::mutex> lock(shardMutex_);
    return shardAssignments_.size();
  }

  // Rebalance shards
  void rebalanceShards() noexcept {
    std::lock_guard<std::mutex> lock(shardMutex_);

    Logging::Logger::getInstance().info(
        "Rebalancing shards for optimal performance", "Sharding", 0);

    rebalanceCount_++;
  }

private:
  int shardCount_;
  std::unordered_map<std::string, int> shardAssignments_;
  size_t syncCount_{0};
  size_t rebalanceCount_{0};
  mutable std::mutex shardMutex_;

  [[nodiscard]] static int calculateShardId(std::string_view dataId) noexcept {
    size_t hash = std::hash<std::string_view>{}(dataId);
    return static_cast<int>(hash % ShardingConfig::ACTIVE_SHARDS);
  }
};

} // namespace QuantumPulse::Sharding

#endif // QUANTUMPULSE_SHARDING_V7_H
