#ifndef QUANTUMPULSE_WALLET_V7_H
#define QUANTUMPULSE_WALLET_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace QuantumPulse::Wallet {

// Wallet configuration
struct WalletConfig {
  static constexpr const char *WALLET_DIR = "wallets";
  static constexpr const char *WALLET_EXT = ".qpw";
  static constexpr int ENCRYPTION_ROUNDS = 10000;
};

// Transaction record for history
struct TransactionRecord {
  std::string txId;
  std::string from;
  std::string to;
  double amount;
  double fee;
  time_t timestamp;
  std::string status; // pending, confirmed, failed

  [[nodiscard]] std::string serialize() const noexcept {
    std::stringstream ss;
    ss << txId << "|" << from << "|" << to << "|" << amount << "|" << fee << "|"
       << timestamp << "|" << status;
    return ss.str();
  }

  static TransactionRecord deserialize(const std::string &data) noexcept {
    TransactionRecord record;
    std::istringstream ss(data);
    std::string token;

    if (std::getline(ss, token, '|'))
      record.txId = token;
    if (std::getline(ss, token, '|'))
      record.from = token;
    if (std::getline(ss, token, '|'))
      record.to = token;
    if (std::getline(ss, token, '|'))
      record.amount = std::stod(token);
    if (std::getline(ss, token, '|'))
      record.fee = std::stod(token);
    if (std::getline(ss, token, '|'))
      record.timestamp = std::stoll(token);
    if (std::getline(ss, token, '|'))
      record.status = token;

    return record;
  }
};

// Wallet class
class Wallet final {
public:
  explicit Wallet(const std::string &name) noexcept
      : name_(name), balance_(0.0), isLocked_(true) {
    std::filesystem::create_directories(WalletConfig::WALLET_DIR);
    walletPath_ = std::string(WalletConfig::WALLET_DIR) + "/" + name +
                  WalletConfig::WALLET_EXT;
  }

  // Create new wallet
  [[nodiscard]] bool create(const std::string &password) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    if (std::filesystem::exists(walletPath_)) {
      Logging::Logger::getInstance().warning("Wallet already exists: " + name_,
                                             "Wallet", 0);
      return false;
    }

    // Generate key pair
    keyPair_ = cryptoManager_.generateKeyPair(0);
    password_ = cryptoManager_.sha3_512_v11(password, 0);
    createdAt_ = std::time(nullptr);
    balance_ = 0.0;
    isLocked_ = false;

    if (!save()) {
      return false;
    }

    Logging::Logger::getInstance().info(
        "Wallet created: " + name_ + " Address: " + getAddress(), "Wallet", 0);
    return true;
  }

  // Load existing wallet
  [[nodiscard]] bool load(const std::string &password) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    if (!std::filesystem::exists(walletPath_)) {
      Logging::Logger::getInstance().error("Wallet not found: " + name_,
                                           "Wallet", 0);
      return false;
    }

    std::ifstream file(walletPath_);
    if (!file.is_open()) {
      return false;
    }

    std::string line;
    std::getline(file, line); // Password hash
    std::string storedHash = line;

    std::string providedHash = cryptoManager_.sha3_512_v11(password, 0);
    if (!Crypto::SecureMemory::constantTimeCompare(storedHash, providedHash)) {
      Logging::Logger::getInstance().warning(
          "Invalid password for wallet: " + name_, "Wallet", 0);
      return false;
    }

    password_ = storedHash;

    std::getline(file, line); // Public key
    keyPair_.publicKey = line;

    std::getline(file, line); // Private key (encrypted in production)
    keyPair_.privateKey = line;

    std::getline(file, line); // Balance
    balance_ = std::stod(line);

    std::getline(file, line); // Created timestamp
    createdAt_ = std::stoll(line);

    // Load transaction history
    transactions_.clear();
    while (std::getline(file, line)) {
      if (!line.empty()) {
        transactions_.push_back(TransactionRecord::deserialize(line));
      }
    }

    file.close();
    isLocked_ = false;

    Logging::Logger::getInstance().info("Wallet loaded: " + name_, "Wallet", 0);
    return true;
  }

  // Save wallet
  [[nodiscard]] bool save() noexcept {
    std::ofstream file(walletPath_);
    if (!file.is_open()) {
      return false;
    }

    file << password_ << "\n";
    file << keyPair_.publicKey << "\n";
    file << keyPair_.privateKey << "\n";
    file << std::fixed << std::setprecision(8) << balance_ << "\n";
    file << createdAt_ << "\n";

    for (const auto &tx : transactions_) {
      file << tx.serialize() << "\n";
    }

    file.close();
    return true;
  }

  // Lock wallet
  void lock() noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);
    isLocked_ = true;
    Logging::Logger::getInstance().info("Wallet locked: " + name_, "Wallet", 0);
  }

  // Unlock wallet
  [[nodiscard]] bool unlock(const std::string &password) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    std::string providedHash = cryptoManager_.sha3_512_v11(password, 0);
    if (!Crypto::SecureMemory::constantTimeCompare(password_, providedHash)) {
      return false;
    }

    isLocked_ = false;
    return true;
  }

  // Get wallet address
  [[nodiscard]] std::string getAddress() const noexcept {
    return keyPair_.publicKey;
  }

  // Get balance
  [[nodiscard]] double getBalance() const noexcept { return balance_; }

  // Set balance (called by blockchain)
  void setBalance(double amount) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);
    balance_ = amount;
  }

  // Create transaction
  [[nodiscard]] std::string createTransaction(const std::string &to,
                                              double amount,
                                              double fee = 0.0001) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    if (isLocked_) {
      Logging::Logger::getInstance().warning(
          "Cannot create transaction - wallet locked", "Wallet", 0);
      return "";
    }

    if (amount <= 0) {
      Logging::Logger::getInstance().warning("Invalid amount", "Wallet", 0);
      return "";
    }

    if (balance_ < amount + fee) {
      Logging::Logger::getInstance().warning("Insufficient balance", "Wallet",
                                             0);
      return "";
    }

    // Create transaction record
    TransactionRecord tx;
    tx.txId = "tx_" + cryptoManager_
                          .sha3_512_v11(keyPair_.publicKey + to +
                                            std::to_string(amount) +
                                            std::to_string(std::time(nullptr)),
                                        0)
                          .substr(0, 32);
    tx.from = keyPair_.publicKey;
    tx.to = to;
    tx.amount = amount;
    tx.fee = fee;
    tx.timestamp = std::time(nullptr);
    tx.status = "pending";

    transactions_.push_back(tx);
    balance_ -= (amount + fee);

    save();

    Logging::Logger::getInstance().info("Transaction created: " + tx.txId,
                                        "Wallet", 0);
    return tx.txId;
  }

  // Receive coins
  void receive(const std::string &from, double amount,
               const std::string &txId) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    TransactionRecord tx;
    tx.txId = txId;
    tx.from = from;
    tx.to = keyPair_.publicKey;
    tx.amount = amount;
    tx.fee = 0;
    tx.timestamp = std::time(nullptr);
    tx.status = "confirmed";

    transactions_.push_back(tx);
    balance_ += amount;

    save();

    Logging::Logger::getInstance().info(
        "Received " + std::to_string(amount) + " QP", "Wallet", 0);
  }

  // Get transaction history
  [[nodiscard]] std::vector<TransactionRecord> getHistory() const noexcept {
    return transactions_;
  }

  // Export keys
  [[nodiscard]] std::string exportKeys(const std::string &password) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    std::string providedHash = cryptoManager_.sha3_512_v11(password, 0);
    if (!Crypto::SecureMemory::constantTimeCompare(password_, providedHash)) {
      return "";
    }

    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"address\": \"" << keyPair_.publicKey << "\",\n";
    ss << "  \"privateKey\": \"" << keyPair_.privateKey << "\",\n";
    ss << "  \"createdAt\": " << createdAt_ << "\n";
    ss << "}";

    return ss.str();
  }

  // Import keys
  [[nodiscard]] bool importKeys(const std::string &publicKey,
                                const std::string &privateKey,
                                const std::string &password) noexcept {
    std::lock_guard<std::mutex> lock(walletMutex_);

    keyPair_.publicKey = publicKey;
    keyPair_.privateKey = privateKey;
    password_ = cryptoManager_.sha3_512_v11(password, 0);
    createdAt_ = std::time(nullptr);
    balance_ = 0.0;
    isLocked_ = false;

    return save();
  }

  // Getters
  [[nodiscard]] std::string getName() const noexcept { return name_; }
  [[nodiscard]] bool isLocked() const noexcept { return isLocked_; }
  [[nodiscard]] time_t getCreatedAt() const noexcept { return createdAt_; }
  [[nodiscard]] size_t getTransactionCount() const noexcept {
    return transactions_.size();
  }

private:
  std::string name_;
  std::string walletPath_;
  std::string password_;
  Crypto::KeyPair keyPair_;
  double balance_;
  time_t createdAt_{0};
  bool isLocked_;
  std::vector<TransactionRecord> transactions_;
  Crypto::CryptoManager cryptoManager_;
  mutable std::mutex walletMutex_;
};

// Wallet Manager for multiple wallets
class WalletManager final {
public:
  WalletManager() noexcept {
    std::filesystem::create_directories(WalletConfig::WALLET_DIR);
    Logging::Logger::getInstance().info("WalletManager initialized", "Wallet",
                                        0);
  }

  // List all wallets
  [[nodiscard]] std::vector<std::string> listWallets() const noexcept {
    std::vector<std::string> wallets;

    for (const auto &entry :
         std::filesystem::directory_iterator(WalletConfig::WALLET_DIR)) {
      if (entry.path().extension() == WalletConfig::WALLET_EXT) {
        wallets.push_back(entry.path().stem().string());
      }
    }

    return wallets;
  }

  // Check if wallet exists
  [[nodiscard]] bool walletExists(const std::string &name) const noexcept {
    std::string path = std::string(WalletConfig::WALLET_DIR) + "/" + name +
                       WalletConfig::WALLET_EXT;
    return std::filesystem::exists(path);
  }

  // Delete wallet
  [[nodiscard]] bool deleteWallet(const std::string &name,
                                  const std::string &password) noexcept {
    Wallet wallet(name);
    if (!wallet.load(password)) {
      return false;
    }

    std::string path = std::string(WalletConfig::WALLET_DIR) + "/" + name +
                       WalletConfig::WALLET_EXT;
    return std::filesystem::remove(path);
  }

private:
};

} // namespace QuantumPulse::Wallet

#endif // QUANTUMPULSE_WALLET_V7_H
