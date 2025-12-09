#ifndef QUANTUMPULSE_ERC20_V7_H
#define QUANTUMPULSE_ERC20_V7_H

#include "quantumpulse_logging_v7.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::ERC20 {

// ERC-20 Token Events
struct TokenEvent {
  std::string eventType; // Transfer, Approval, Mint, Burn
  std::string from;
  std::string to;
  double amount;
  int64_t timestamp;
  std::string txHash;
  int64_t blockNumber;
};

// ERC-20 Token Contract (Ethereum-compatible)
class Token final {
public:
  Token(const std::string &name, const std::string &symbol, uint8_t decimals,
        double totalSupply, const std::string &owner) noexcept
      : name_(name), symbol_(symbol), decimals_(decimals), owner_(owner) {

    balances_[owner] = totalSupply;
    totalSupply_ = totalSupply;

    Logging::Logger::getInstance().info(
        "Token created: " + name + " (" + symbol + ")", "ERC20", 0);
  }

  // ERC-20 Standard Functions

  // Get token name
  std::string name() const noexcept { return name_; }

  // Get token symbol
  std::string symbol() const noexcept { return symbol_; }

  // Get decimals
  uint8_t decimals() const noexcept { return decimals_; }

  // Get total supply
  double totalSupply() const noexcept { return totalSupply_; }

  // Get balance
  double balanceOf(const std::string &account) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (balances_.count(account)) {
      return balances_.at(account);
    }
    return 0.0;
  }

  // Transfer tokens
  bool transfer(const std::string &from, const std::string &to,
                double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (amount <= 0)
      return false;
    if (!balances_.count(from) || balances_[from] < amount)
      return false;

    balances_[from] -= amount;
    balances_[to] += amount;

    // Emit event
    TokenEvent event;
    event.eventType = "Transfer";
    event.from = from;
    event.to = to;
    event.amount = amount;
    event.timestamp = std::time(nullptr);
    events_.push_back(event);

    return true;
  }

  // Approve spender
  bool approve(const std::string &owner, const std::string &spender,
               double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    allowances_[owner][spender] = amount;

    // Emit event
    TokenEvent event;
    event.eventType = "Approval";
    event.from = owner;
    event.to = spender;
    event.amount = amount;
    event.timestamp = std::time(nullptr);
    events_.push_back(event);

    return true;
  }

  // Get allowance
  double allowance(const std::string &owner,
                   const std::string &spender) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (allowances_.count(owner) && allowances_.at(owner).count(spender)) {
      return allowances_.at(owner).at(spender);
    }
    return 0.0;
  }

  // Transfer from (using allowance)
  bool transferFrom(const std::string &spender, const std::string &from,
                    const std::string &to, double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (amount <= 0)
      return false;

    // Check allowance
    if (!allowances_.count(from) || !allowances_[from].count(spender) ||
        allowances_[from][spender] < amount) {
      return false;
    }

    // Check balance
    if (!balances_.count(from) || balances_[from] < amount) {
      return false;
    }

    // Execute transfer
    balances_[from] -= amount;
    balances_[to] += amount;
    allowances_[from][spender] -= amount;

    // Emit event
    TokenEvent event;
    event.eventType = "Transfer";
    event.from = from;
    event.to = to;
    event.amount = amount;
    event.timestamp = std::time(nullptr);
    events_.push_back(event);

    return true;
  }

  // Mint new tokens (owner only)
  bool mint(const std::string &caller, const std::string &to,
            double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (caller != owner_)
      return false;
    if (amount <= 0)
      return false;

    balances_[to] += amount;
    totalSupply_ += amount;

    // Emit event
    TokenEvent event;
    event.eventType = "Mint";
    event.from = "0x0";
    event.to = to;
    event.amount = amount;
    event.timestamp = std::time(nullptr);
    events_.push_back(event);

    return true;
  }

  // Burn tokens
  bool burn(const std::string &from, double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (amount <= 0)
      return false;
    if (!balances_.count(from) || balances_[from] < amount)
      return false;

    balances_[from] -= amount;
    totalSupply_ -= amount;

    // Emit event
    TokenEvent event;
    event.eventType = "Burn";
    event.from = from;
    event.to = "0x0";
    event.amount = amount;
    event.timestamp = std::time(nullptr);
    events_.push_back(event);

    return true;
  }

  // Get events
  std::vector<TokenEvent> getEvents(int limit = 100) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    int start = std::max(0, (int)events_.size() - limit);
    return std::vector<TokenEvent>(events_.begin() + start, events_.end());
  }

  // Get all holders
  std::vector<std::pair<std::string, double>> getHolders() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::pair<std::string, double>> holders;
    for (const auto &[addr, bal] : balances_) {
      if (bal > 0) {
        holders.push_back({addr, bal});
      }
    }
    return holders;
  }

private:
  std::string name_;
  std::string symbol_;
  uint8_t decimals_;
  double totalSupply_;
  std::string owner_;

  mutable std::mutex mutex_;
  std::map<std::string, double> balances_;
  std::map<std::string, std::map<std::string, double>> allowances_;
  std::vector<TokenEvent> events_;
};

// Token Factory (create new tokens)
class TokenFactory final {
public:
  static TokenFactory &getInstance() noexcept {
    static TokenFactory instance;
    return instance;
  }

  // Create new token
  std::string createToken(const std::string &name, const std::string &symbol,
                          uint8_t decimals, double totalSupply,
                          const std::string &owner) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    Crypto::CryptoManager cm;
    std::string address =
        "0x" +
        cm.sha3_512_v11(name + symbol + std::to_string(std::time(nullptr)), 0)
            .substr(0, 40);

    tokens_[address] =
        std::make_unique<Token>(name, symbol, decimals, totalSupply, owner);

    return address;
  }

  // Get token by address
  Token *getToken(const std::string &address) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (tokens_.count(address)) {
      return tokens_[address].get();
    }
    return nullptr;
  }

  // List all tokens
  std::vector<std::string> listTokens() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    for (const auto &[addr, _] : tokens_) {
      result.push_back(addr);
    }
    return result;
  }

private:
  TokenFactory() = default;
  mutable std::mutex mutex_;
  std::map<std::string, std::unique_ptr<Token>> tokens_;
};

} // namespace QuantumPulse::ERC20

#endif // QUANTUMPULSE_ERC20_V7_H
