// QuantumPulse Mining Source Implementation v7.0
// PoW hybrid mining with difficulty adjustment

#include "quantumpulse_mining_v7.h"
#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <cmath>

namespace QuantumPulse::Mining {

// Constructor
MiningManager::MiningManager() : totalMinedCoins(0), currentDifficulty(4) {
  Logging::Logger::getInstance().log(
      "MiningManager initialized - PoW hybrid ready", Logging::INFO, "Mining",
      0);
}

// Mine block with PoW
bool MiningManager::mineBlock(const std::string &data, int difficulty,
                              int &nonce, std::string &hash, int shardId) {
  std::lock_guard<std::mutex> lock(miningMutex);

  if (!checkMiningLimit()) {
    Logging::Logger::getInstance().log(
        "Mining limit reached - 3,000,000 coins mined", Logging::INFO, "Mining",
        shardId);
    return false;
  }

  Crypto::CryptoManager crypto;
  std::string target(difficulty, '0');

  auto startTime = std::chrono::high_resolution_clock::now();
  int maxNonce = 10000000; // Prevent infinite loop

  for (nonce = 0; nonce < maxNonce; ++nonce) {
    std::string testData = data + std::to_string(nonce);
    hash = crypto.sha3_512_v11(testData, shardId);

    if (hash.substr(0, difficulty) == target) {
      auto endTime = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          endTime - startTime);

      Logging::Logger::getInstance().log(
          "Block mined in " + std::to_string(duration.count()) +
              "ms, nonce=" + std::to_string(nonce),
          Logging::INFO, "Mining", shardId);

      return true;
    }
  }

  Logging::Logger::getInstance().log("Mining failed - max nonce reached",
                                     Logging::ERROR, "Mining", shardId);
  return false;
}

// Check if mining limit reached
bool MiningManager::checkMiningLimit() const {
  return totalMinedCoins.load() < 3000000.0;
}

// Add mined coins (with limit check)
void MiningManager::addMinedCoins(double amount) {
  std::lock_guard<std::mutex> lock(miningMutex);

  double current = totalMinedCoins.load();
  double newTotal = current + amount;

  if (newTotal > 3000000.0) {
    totalMinedCoins.store(3000000.0);
    Logging::Logger::getInstance().log("Mining cap reached at 3,000,000 coins",
                                       Logging::INFO, "Mining", 0);
  } else {
    totalMinedCoins.store(newTotal);
  }
}

// Adjust difficulty based on block time
void MiningManager::adjustDifficulty(double actualBlockTime,
                                     double targetBlockTime) {
  std::lock_guard<std::mutex> lock(miningMutex);

  double ratio = actualBlockTime / targetBlockTime;

  if (ratio < 0.5) {
    currentDifficulty = std::min(currentDifficulty + 2, 512);
  } else if (ratio < 0.8) {
    currentDifficulty = std::min(currentDifficulty + 1, 512);
  } else if (ratio > 2.0) {
    currentDifficulty = std::max(currentDifficulty - 2, 1);
  } else if (ratio > 1.5) {
    currentDifficulty = std::max(currentDifficulty - 1, 1);
  }

  Logging::Logger::getInstance().log("Difficulty adjusted to " +
                                         std::to_string(currentDifficulty),
                                     Logging::INFO, "Mining", 0);
}

// Get current difficulty
int MiningManager::getDifficulty() const { return currentDifficulty; }

// Get total mined coins
double MiningManager::getTotalMinedCoins() const {
  return totalMinedCoins.load();
}

// Calculate block reward with halving
double calculateBlockReward(uint64_t blockHeight) {
  uint64_t halvingInterval = 210000;
  uint64_t halvings = blockHeight / halvingInterval;

  double reward = 50.0 / std::pow(2, halvings);
  return std::max(reward, 0.0005); // Minimum reward
}

} // namespace QuantumPulse::Mining
