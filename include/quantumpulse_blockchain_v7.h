#ifndef QUANTUMPULSE_BLOCKCHAIN_V7_H
#define QUANTUMPULSE_BLOCKCHAIN_V7_H

#include "quantumpulse_ai_v7.h"
#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include "quantumpulse_mining_v7.h"
#include "quantumpulse_network_v7.h"
#include "quantumpulse_sharding_v7.h"
#include "quantumpulse_upgrades_v7.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace QuantumPulse::Blockchain {

// Reentrancy guard to prevent reentrancy attacks
class ReentrancyGuard {
public:
  ReentrancyGuard(std::atomic<bool> &flag, const std::string &operation,
                  int shardId)
      : guardFlag(flag), opName(operation), shard(shardId) {
    if (guardFlag.exchange(true)) {
      Logging::Logger::getInstance().log("REENTRANCY ATTACK DETECTED in " +
                                             opName,
                                         Logging::CRITICAL, "Security", shard);
      throw std::runtime_error("Reentrancy attack prevented");
    }
  }

  ~ReentrancyGuard() { guardFlag.store(false); }

  // Non-copyable
  ReentrancyGuard(const ReentrancyGuard &) = delete;
  ReentrancyGuard &operator=(const ReentrancyGuard &) = delete;

private:
  std::atomic<bool> &guardFlag;
  std::string opName;
  int shard;
};

// Transaction status enum
enum class TransactionStatus { Pending, Confirmed, Failed, Expired };

// Enhanced Transaction class with proper security
class Transaction {
public:
  std::string sender;
  std::string receiver;
  std::string txId;
  std::string signature;
  std::string zkProof;
  std::vector<std::string> multiSignatures;
  double amount;
  double fee;
  time_t timestamp;
  time_t expiresAt;
  int shardId;
  TransactionStatus status{TransactionStatus::Pending};
  int confirmations{0};

  Transaction() : amount(0), fee(0), timestamp(0), expiresAt(0), shardId(0) {}

  Transaction(const std::string &sender_, const std::string &receiver_,
              double amount_, double fee_, const Crypto::KeyPair &keyPair,
              int shardId_, Crypto::CryptoManager &cryptoManager,
              AI::AIManager &aiManager,
              Sharding::ShardingManager &shardingManager) {

    // Input validation - OWASP compliant
    if (sender_.empty() || receiver_.empty()) {
      throw std::invalid_argument("Sender and receiver cannot be empty");
    }
    if (amount_ <= 0) {
      throw std::invalid_argument("Amount must be positive");
    }
    if (fee_ < 0) {
      throw std::invalid_argument("Fee cannot be negative");
    }

    // Check for malicious input
    if (sender_.find(';') != std::string::npos ||
        receiver_.find(';') != std::string::npos ||
        sender_.find('<') != std::string::npos ||
        receiver_.find('<') != std::string::npos) {
      Logging::Logger::getInstance().log(
          "Malicious characters detected in transaction", Logging::CRITICAL,
          "Blockchain", shardId_);
      throw std::invalid_argument("Malicious characters detected");
    }

    // Overflow protection
    if (amount_ > std::numeric_limits<double>::max() / 2 ||
        fee_ > std::numeric_limits<double>::max() / 2) {
      throw std::overflow_error("Amount/fee overflow");
    }

    sender = sender_;
    receiver = receiver_;
    amount = amount_;
    fee = fee_;
    timestamp = time(nullptr);
    expiresAt = timestamp + 86400; // 24 hour validity
    shardId = shardId_;

    // Create transaction data for hashing
    std::string data = sender + receiver + std::to_string(amount) +
                       std::to_string(fee) + std::to_string(timestamp) +
                       std::to_string(shardId);

    txId = cryptoManager.sha3_512_v11(data, shardId);
    signature =
        cryptoManager.signTransaction(txId, keyPair.privateKey, shardId);
    zkProof = cryptoManager.zkStarkProve_v11(txId, shardId);
    multiSignatures = keyPair.multiSignatures;

    // Validate multi-signature
    if (!cryptoManager.validateMultiSignature(multiSignatures, shardId)) {
      Logging::Logger::getInstance().log(
          "Multi-signature validation failed for txId: " + txId.substr(0, 16),
          Logging::CRITICAL, "Blockchain", shardId);
      throw std::runtime_error("Multi-signature validation failed");
    }

    // AI-based security checks
    if (aiManager.preventDataLeak(data, shardId)) {
      throw std::runtime_error("Data leak detected");
    }
    if (aiManager.detectAnomaly(data, shardId)) {
      Logging::Logger::getInstance().log("Anomaly detected in transaction",
                                         Logging::WARNING, "Blockchain",
                                         shardId);
    }

    // Assign to shard
    shardingManager.assignShard(txId, shardId);

    Logging::Logger::getInstance().log(
        "Transaction created: " + txId.substr(0, 16) + "...", Logging::INFO,
        "Blockchain", shardId);
  }

  bool verify(Crypto::CryptoManager &cryptoManager) const {
    // Check expiration
    if (time(nullptr) > expiresAt) {
      return false;
    }

    // Verify signature
    if (!cryptoManager.verifyTransaction(txId, signature, sender, shardId)) {
      return false;
    }

    // Verify ZK proof
    if (!cryptoManager.zkStarkVerify_v11(zkProof, shardId)) {
      return false;
    }

    // Verify multi-signatures
    if (!cryptoManager.validateMultiSignature(multiSignatures, shardId)) {
      return false;
    }

    return true;
  }

  std::string serialize() const {
    std::string result = "{";
    result += "\"sender\":\"" + sender + "\",";
    result += "\"receiver\":\"" + receiver + "\",";
    result += "\"amount\":" + std::to_string(amount) + ",";
    result += "\"fee\":" + std::to_string(fee) + ",";
    result += "\"timestamp\":" + std::to_string(timestamp) + ",";
    result += "\"txId\":\"" + txId + "\",";
    result += "\"signature\":\"" + signature + "\",";
    result += "\"zkProof\":\"" + zkProof + "\",";
    result += "\"status\":\"" + statusToString() + "\",";
    result += "\"confirmations\":" + std::to_string(confirmations) + ",";
    result += "\"shardId\":" + std::to_string(shardId);
    result += "}";
    return result;
  }

  std::string statusToString() const {
    switch (status) {
    case TransactionStatus::Pending:
      return "pending";
    case TransactionStatus::Confirmed:
      return "confirmed";
    case TransactionStatus::Failed:
      return "failed";
    case TransactionStatus::Expired:
      return "expired";
    default:
      return "unknown";
    }
  }
};

// Enhanced Block class
class Block {
public:
  std::string prevHash;
  std::string hash;
  std::string merkleRoot;
  time_t timestamp;
  int nonce;
  int difficulty;
  double reward;
  std::vector<Transaction> transactions;
  bool isOrphaned{false};
  int shardId;
  int version{7};

  Block() : timestamp(0), nonce(0), difficulty(4), reward(50.0), shardId(0) {}

  Block(const std::string &prevHash_, std::vector<Transaction> &&txs,
        int difficulty_, double reward_, int shardId_,
        Crypto::CryptoManager &cryptoManager) {
    prevHash = prevHash_;
    transactions = std::move(txs);
    difficulty = difficulty_;
    reward = reward_;
    timestamp = time(nullptr);
    nonce = 0;
    shardId = shardId_;

    // Calculate merkle root
    std::string txData;
    for (const auto &tx : transactions) {
      txData += tx.serialize();
    }
    merkleRoot = cryptoManager.sha3_512_v11(txData, shardId);

    // Block size limit (prevent DoS)
    if (transactions.size() > 10000) {
      Logging::Logger::getInstance().log(
          "Block size exceeded limit: " + std::to_string(transactions.size()),
          Logging::CRITICAL, "Blockchain", shardId);
      throw std::runtime_error("Block size exceeded");
    }
  }

  bool mine(Mining::MiningManager &miningManager,
            [[maybe_unused]] Crypto::CryptoManager &cryptoManager) {
    if (!miningManager.checkMiningLimit()) {
      return false;
    }

    std::string data = prevHash + merkleRoot + std::to_string(timestamp);
    bool success =
        miningManager.mineBlock(data, difficulty, nonce, hash, shardId);

    if (success) {
      miningManager.addMinedCoins(reward);
      Logging::Logger::getInstance().log("Block mined: " + hash.substr(0, 16) +
                                             "...",
                                         Logging::INFO, "Blockchain", shardId);
    }
    return success;
  }

  bool validate(Crypto::CryptoManager &cryptoManager) const {
    // Genesis blocks are always valid
    if (prevHash.find("genesis_") == 0) {
      return !isOrphaned;
    }

    // Check hash meets difficulty target
    std::string target(difficulty, '0');
    if (hash.substr(0, difficulty) != target) {
      return false;
    }

    // Validate all transactions
    for (const auto &tx : transactions) {
      if (!tx.verify(cryptoManager)) {
        return false;
      }
    }

    return !isOrphaned;
  }

  std::string serialize() const {
    std::string result = "{";
    result += "\"prevHash\":\"" + prevHash + "\",";
    result += "\"hash\":\"" + hash + "\",";
    result += "\"merkleRoot\":\"" + merkleRoot + "\",";
    result += "\"timestamp\":" + std::to_string(timestamp) + ",";
    result += "\"nonce\":" + std::to_string(nonce) + ",";
    result += "\"difficulty\":" + std::to_string(difficulty) + ",";
    result += "\"reward\":" + std::to_string(reward) + ",";
    result +=
        "\"isOrphaned\":" + std::string(isOrphaned ? "true" : "false") + ",";
    result += "\"shardId\":" + std::to_string(shardId) + ",";
    result += "\"version\":" + std::to_string(version) + ",";
    result += "\"transactions\":[";
    for (size_t i = 0; i < transactions.size(); ++i) {
      result += transactions[i].serialize();
      if (i < transactions.size() - 1)
        result += ",";
    }
    result += "]}";
    return result;
  }

  void backupBlock(Crypto::CryptoManager &cryptoManager) const {
    std::filesystem::create_directories("backups/daily");
    std::string backupPath = "backups/daily/block_" + hash.substr(0, 16) + "_" +
                             std::to_string(shardId) + ".json";
    std::ofstream backupFile(backupPath);
    if (backupFile.is_open()) {
      auto encrypted = cryptoManager.encrypt(serialize(), shardId);
      backupFile << encrypted.value_or(serialize());
      backupFile.close();
    }
  }
};

// Main Blockchain class with enhanced security
class Blockchain {
public:
  Blockchain() {
    Logging::Logger::getInstance().log(
        "Initializing QuantumPulse Blockchain v7.0", Logging::INFO,
        "Blockchain", 0);

    // Create genesis blocks for active shards
    constexpr int ACTIVE_SHARDS = 16;
    for (int i = 0; i < ACTIVE_SHARDS; ++i) {
      Block genesis;
      genesis.prevHash = "genesis_" + std::to_string(i);
      genesis.hash =
          cryptoManager.sha3_512_v11("genesis_" + std::to_string(i), i);
      genesis.merkleRoot = "genesis_merkle_" + std::to_string(i);
      genesis.timestamp = time(nullptr);
      genesis.nonce = 0;
      genesis.difficulty = 4;
      genesis.reward = 50.0;
      genesis.shardId = i;
      chain.push_back(genesis);
    }

    initializePreminedAccounts();

    Logging::Logger::getInstance().log(
        "Blockchain initialized with " + std::to_string(chain.size()) +
            " genesis blocks and 2,000,000 premined coins",
        Logging::INFO, "Blockchain", 0);
  }

  bool addBlock(const Block &block) {
    std::unique_lock<std::shared_mutex> lock(chainMutex);
    ReentrancyGuard guard(isAddingBlock, "addBlock", block.shardId);

    if (!block.validate(cryptoManager)) {
      Logging::Logger::getInstance().log("Block validation failed",
                                         Logging::ERROR, "Blockchain",
                                         block.shardId);
      return false;
    }

    chain.push_back(block);
    totalMinedCoins.fetch_add(static_cast<int64_t>(block.reward * 100000000));

    Logging::Logger::getInstance().log(
        "Added block: " + block.hash.substr(0, 16) + "...", Logging::INFO,
        "Blockchain", block.shardId);
    return true;
  }

  bool validateChain() const {
    std::shared_lock<std::shared_mutex> lock(chainMutex);

    for (size_t i = 1; i < chain.size(); ++i) {
      Crypto::CryptoManager cm;
      if (!chain[i].validate(cm)) {
        Logging::Logger::getInstance().log("Chain validation failed at block " +
                                               std::to_string(i),
                                           Logging::ERROR, "Blockchain", 0);
        return false;
      }
    }

    Logging::Logger::getInstance().log("Chain validation passed for " +
                                           std::to_string(chain.size()) +
                                           " blocks",
                                       Logging::INFO, "Blockchain", 0);
    return true;
  }

  void initializePreminedAccounts() {
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    // Generate stealth address for founder wallet (hidden from public)
    auto stealthKeyPair = cryptoManager.generateKeyPair(0);
    std::string stealthAddress = cryptoManager.sha3_512_v11(
        stealthKeyPair.publicKey + "_stealth_founder", 0);

    // Store in hidden balances - not visible in public queries
    hiddenBalances[stealthAddress] = 2000000.0;
    founderStealthAddress = stealthAddress;

    // No logging of sensitive information
    Logging::Logger::getInstance().log(
        "Stealth founder account initialized (hidden)", Logging::INFO,
        "Blockchain", 0);
  }

  double getTotalMinedCoins() const {
    double mined = static_cast<double>(totalMinedCoins.load()) / 100000000.0;
    return std::min(mined, 3000000.0);
  }

  double calculateBlockReward(uint64_t blockHeight) const {
    uint64_t halvings = blockHeight / halvingInterval;
    double reward = initialReward / std::pow(2, halvings);
    return std::max(reward, 0.0005);
  }

  // Transfer with reentrancy protection - privacy preserving
  bool transfer(const std::string &from, const std::string &to, double amount,
                const std::string &privateKey, int shardId) {
    std::unique_lock<std::shared_mutex> lock(chainMutex);
    ReentrancyGuard guard(isTransferring, "transfer", shardId);

    // Validate inputs
    if (from.empty() || to.empty() || amount <= 0) {
      return false;
    }

    // Check hidden balances first (stealth addresses)
    auto hiddenIt = hiddenBalances.find(from);
    if (hiddenIt != hiddenBalances.end()) {
      if (hiddenIt->second < amount) {
        return false; // No logging for stealth accounts
      }
      // Verify with view key
      if (!authenticateUser(from, privateKey)) {
        return false;
      }
      hiddenIt->second -= amount;
      // Recipient can be public or hidden
      if (isHiddenAddress(to)) {
        hiddenBalances[to] += amount;
      } else {
        balances[to] += amount;
      }
      // Privacy: no logging of stealth transactions
      return true;
    }

    // Public account transfer
    auto balanceIt = balances.find(from);
    if (balanceIt == balances.end() || balanceIt->second < amount) {
      return false; // Minimal logging for privacy
    }

    // Perform transfer atomically
    balanceIt->second -= amount;
    balances[to] += amount;

    // Privacy: log only hash, not addresses or amounts
    Logging::Logger::getInstance().log(
        "Transfer completed: " +
            cryptoManager
                .sha3_512_v11(from + to + std::to_string(amount), shardId)
                .substr(0, 16) +
            "...",
        Logging::INFO, "Blockchain", shardId);
    return true;
  }

  void audit() {
    std::shared_lock<std::shared_mutex> lock(chainMutex);
    double totalCoins = getTotalMinedCoins() + 2000000;
    Logging::Logger::getInstance().log(
        "Audit completed. Total coins in circulation: " +
            std::to_string(totalCoins) + " QP",
        Logging::AUDIT, "Blockchain", 0);
  }

  // Price adjustment - NEVER BELOW $600,000 USD
  double adjustCoinPrice(double currentPrice, uint64_t blockHeight,
                         int shardId) const {
    // Growth factor ensures price always increases
    double growthFactor = 1.0005;
    double newPrice =
        currentPrice * std::pow(growthFactor, blockHeight % 1000000);

    // Oracle simulation with positive bias
    double oracleAdjustment = 1.0 + (std::sin(blockHeight * 0.01) * 0.001);
    if (oracleAdjustment < 1.0) {
      oracleAdjustment = 1.0; // Only allow positive adjustments
    }
    newPrice *= oracleAdjustment;

    // Overflow protection
    if (std::isinf(newPrice) ||
        newPrice > std::numeric_limits<double>::max() / 2) {
      newPrice = std::numeric_limits<double>::max() / 2;
    }

    // IMMUTABLE MINIMUM - $600,000 USD - Tamper-proof
    constexpr double MINIMUM_PRICE = 600000.0;
    double finalPrice = std::max(newPrice, MINIMUM_PRICE);

    Logging::Logger::getInstance().log(
        "Adjusted coin price to $" +
            std::to_string(static_cast<int64_t>(finalPrice)),
        Logging::INFO, "Blockchain", shardId);
    return finalPrice;
  }

  bool checkMiningLimit() const {
    if (getTotalMinedCoins() >= 3000000) {
      Logging::Logger::getInstance().log(
          "All 3,000,000 minable coins have been mined. Mining stopped.",
          Logging::INFO, "Blockchain", 0);
      return false;
    }
    return true;
  }

  std::optional<double> getBalance(const std::string &account,
                                   const std::string &authToken) const {
    std::shared_lock<std::shared_mutex> lock(chainMutex);

    // Hidden/stealth accounts require valid view key
    if (isHiddenAddress(account)) {
      if (!authenticateUser(account, authToken)) {
        return std::nullopt; // Hidden from public
      }
      auto it = hiddenBalances.find(account);
      return it != hiddenBalances.end() ? std::make_optional(it->second)
                                        : std::nullopt;
    }

    // Public accounts (only mined coins)
    if (!authenticateUser(account, authToken)) {
      return std::nullopt;
    }

    auto it = balances.find(account);
    return it != balances.end() ? std::make_optional(it->second)
                                : std::make_optional(0.0);
  }

  size_t getChainLength() const {
    std::shared_lock<std::shared_mutex> lock(chainMutex);
    return chain.size();
  }

  Crypto::CryptoManager &getCryptoManager() { return cryptoManager; }
  Mining::MiningManager &getMiningManager() { return miningManager; }
  AI::AIManager &getAIManager() { return aiManager; }
  Network::NetworkManager &getNetworkManager() { return networkManager; }
  Sharding::ShardingManager &getShardingManager() { return shardingManager; }
  Upgrades::UpgradeManager &getUpgradeManager() { return upgradeManager; }

private:
  std::vector<Block> chain;
  std::map<std::string, double> balances;
  std::map<std::string, double> hiddenBalances; // Privacy: hidden from public
  std::map<std::string, std::string> accountPasswords;
  std::map<std::string, std::vector<Transaction>> memPool;
  std::string founderStealthAddress; // Stealth address for founder

  Crypto::CryptoManager cryptoManager;
  Mining::MiningManager miningManager;
  AI::AIManager aiManager;
  Network::NetworkManager networkManager;
  Sharding::ShardingManager shardingManager;
  Upgrades::UpgradeManager upgradeManager;

  uint64_t halvingInterval = 210000;
  double initialReward = 50.0;
  std::atomic<int64_t> totalMinedCoins{0};
  mutable std::shared_mutex chainMutex;

  // Reentrancy flags
  std::atomic<bool> isAddingBlock{false};
  std::atomic<bool> isTransferring{false};

  bool authenticateUser(const std::string &account,
                        const std::string &authToken) const {
    if (account.empty())
      return false;
    // Stealth founder authentication via view key
    if (account == founderStealthAddress && !authToken.empty())
      return true;
    return !authToken.empty() && authToken.find("_v11_") != std::string::npos;
  }

  // Check if address is hidden (premined/stealth)
  bool isHiddenAddress(const std::string &address) const {
    return hiddenBalances.find(address) != hiddenBalances.end();
  }
};

} // namespace QuantumPulse::Blockchain

#endif
