#ifndef QUANTUMPULSE_MINING_V7_H
#define QUANTUMPULSE_MINING_V7_H

#include "quantumpulse_logging_v7.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>

namespace QuantumPulse::Mining {

// Mining configuration constants
struct MiningConfig {
  static constexpr double MAX_MINABLE_COINS = 3000000.0;
  static constexpr double INITIAL_REWARD = 50.0;
  static constexpr double MIN_REWARD = 0.0005;
  static constexpr uint64_t HALVING_INTERVAL = 210000;
  static constexpr int MAX_DIFFICULTY = 512;
  static constexpr int MIN_DIFFICULTY = 1;
  static constexpr int DEFAULT_DIFFICULTY = 4;
  static constexpr int MAX_NONCE_ATTEMPTS = 10000000;
};

// Thread-safe mining manager
class MiningManager final {
public:
  MiningManager() noexcept
      : totalMinedCoins_(0),
        currentDifficulty_(MiningConfig::DEFAULT_DIFFICULTY) {
    Logging::Logger::getInstance().info(
        "MiningManager initialized - PoW hybrid ready", "Mining", 0);
  }

  // Non-copyable, non-movable
  MiningManager(const MiningManager &) = delete;
  MiningManager &operator=(const MiningManager &) = delete;
  MiningManager(MiningManager &&) = delete;
  MiningManager &operator=(MiningManager &&) = delete;

  // Mine block with PoW - returns success and sets hash/nonce
  [[nodiscard]] bool mineBlock(const std::string &data, int difficulty,
                               int &nonce, std::string &hash,
                               int shardId) noexcept {
    std::lock_guard<std::mutex> lock(miningMutex_);

    if (!checkMiningLimit()) {
      Logging::Logger::getInstance().info(
          "Mining limit reached - 3,000,000 coins mined", "Mining", shardId);
      return false;
    }

    // Simulate PoW mining
    std::string target(difficulty, '0');
    auto startTime = std::chrono::high_resolution_clock::now();

    for (nonce = 0; nonce < MiningConfig::MAX_NONCE_ATTEMPTS; ++nonce) {
      // Simulated hash calculation
      hash = generateHash(data, nonce, shardId);

      if (hash.substr(0, difficulty) == target) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        Logging::Logger::getInstance().info(
            "Block mined in " + std::to_string(duration.count()) +
                "ms, nonce=" + std::to_string(nonce),
            "Mining", shardId);

        return true;
      }
    }

    Logging::Logger::getInstance().warning("Mining timeout - max nonce reached",
                                           "Mining", shardId);
    return false;
  }

  // Check if mining limit reached
  [[nodiscard]] bool checkMiningLimit() const noexcept {
    return getTotalMinedCoins() < MiningConfig::MAX_MINABLE_COINS;
  }

  // Add mined coins with cap enforcement
  void addMinedCoins(double amount) noexcept {
    std::lock_guard<std::mutex> lock(miningMutex_);

    int64_t amountSatoshis = static_cast<int64_t>(amount * 100000000);
    int64_t current = totalMinedCoins_.load(std::memory_order_relaxed);
    int64_t maxSatoshis =
        static_cast<int64_t>(MiningConfig::MAX_MINABLE_COINS * 100000000);

    if (current + amountSatoshis > maxSatoshis) {
      totalMinedCoins_.store(maxSatoshis, std::memory_order_relaxed);
      Logging::Logger::getInstance().info(
          "Mining cap reached at 3,000,000 coins", "Mining", 0);
    } else {
      totalMinedCoins_.fetch_add(amountSatoshis, std::memory_order_relaxed);
    }
  }

  // Get total mined coins
  [[nodiscard]] double getTotalMinedCoins() const noexcept {
    return static_cast<double>(
               totalMinedCoins_.load(std::memory_order_relaxed)) /
           100000000.0;
  }

  // Adjust difficulty based on block time
  void adjustDifficulty(double actualBlockTime,
                        double targetBlockTime) noexcept {
    std::lock_guard<std::mutex> lock(miningMutex_);

    if (targetBlockTime <= 0)
      return;

    double ratio = actualBlockTime / targetBlockTime;

    if (ratio < 0.5) {
      currentDifficulty_ =
          std::min(currentDifficulty_ + 2, MiningConfig::MAX_DIFFICULTY);
    } else if (ratio < 0.8) {
      currentDifficulty_ =
          std::min(currentDifficulty_ + 1, MiningConfig::MAX_DIFFICULTY);
    } else if (ratio > 2.0) {
      currentDifficulty_ =
          std::max(currentDifficulty_ - 2, MiningConfig::MIN_DIFFICULTY);
    } else if (ratio > 1.5) {
      currentDifficulty_ =
          std::max(currentDifficulty_ - 1, MiningConfig::MIN_DIFFICULTY);
    }

    Logging::Logger::getInstance().info("Difficulty adjusted to " +
                                            std::to_string(currentDifficulty_),
                                        "Mining", 0);
  }

  // Get current difficulty
  [[nodiscard]] int getDifficulty() const noexcept {
    return currentDifficulty_;
  }

  // Calculate block reward with halving
  [[nodiscard]] static double
  calculateBlockReward(uint64_t blockHeight) noexcept {
    uint64_t halvings = blockHeight / MiningConfig::HALVING_INTERVAL;
    double reward = MiningConfig::INITIAL_REWARD /
                    std::pow(2.0, static_cast<double>(halvings));
    return std::max(reward, MiningConfig::MIN_REWARD);
  }

private:
  std::atomic<int64_t> totalMinedCoins_;
  int currentDifficulty_;
  mutable std::mutex miningMutex_;

  // Generate simulated hash
  [[nodiscard]] std::string generateHash(const std::string &data, int nonce,
                                         int shardId) const noexcept {
    // Simulated hash - in production would use real crypto
    size_t hash = std::hash<std::string>{}(data + std::to_string(nonce) +
                                           std::to_string(shardId));

    std::string hashStr;
    hashStr.reserve(64);
    for (int i = 0; i < 64; ++i) {
      hashStr += "0123456789abcdef"[(hash >> (i % 16)) & 0xF];
    }
    return hashStr;
  }
};

} // namespace QuantumPulse::Mining

#endif // QUANTUMPULSE_MINING_V7_H
