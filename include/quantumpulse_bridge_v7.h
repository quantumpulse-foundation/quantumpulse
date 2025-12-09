#ifndef QUANTUMPULSE_BRIDGE_V7_H
#define QUANTUMPULSE_BRIDGE_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::Bridge {

// Supported chains
enum class Chain { QUANTUMPULSE, ETHEREUM, BITCOIN, BSC, POLYGON, SOLANA };

// Bridge transfer
struct BridgeTransfer {
  std::string transferId;
  std::string userId;
  Chain sourceChain;
  Chain destChain;
  double amount;
  std::string sourceAddress;
  std::string destAddress;
  std::string status; // pending, confirmed, completed, failed
  std::string sourceTxHash;
  std::string destTxHash;
  int64_t timestamp;
  int confirmations;
};

// Wrapped token info
struct WrappedToken {
  Chain chain;
  std::string contractAddress;
  std::string symbol;
  double totalSupply;
};

// Cross-chain Bridge
class CrossChainBridge final {
public:
  CrossChainBridge() noexcept {
    initializeWrappedTokens();
    Logging::Logger::getInstance().info("Cross-chain Bridge initialized",
                                        "Bridge", 0);
  }

  // Initiate bridge transfer
  [[nodiscard]] std::string bridgeOut(const std::string &userId,
                                      double amountQP, Chain destChain,
                                      const std::string &destAddress) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    BridgeTransfer transfer;
    transfer.transferId = "bridge_" + std::to_string(nextTransferId_++);
    transfer.userId = userId;
    transfer.sourceChain = Chain::QUANTUMPULSE;
    transfer.destChain = destChain;
    transfer.amount = amountQP;
    transfer.destAddress = destAddress;
    transfer.status = "pending";
    transfer.timestamp = std::time(nullptr);
    transfer.confirmations = 0;

    transfers_[transfer.transferId] = transfer;

    Logging::Logger::getInstance().info(
        "Bridge out initiated: " + std::to_string(amountQP) + " QP to " +
            getChainName(destChain),
        "Bridge", 0);

    return transfer.transferId;
  }

  // Bridge back to QuantumPulse
  [[nodiscard]] std::string bridgeIn(const std::string &userId, double amount,
                                     Chain sourceChain,
                                     const std::string &sourceTxHash) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    BridgeTransfer transfer;
    transfer.transferId = "bridge_" + std::to_string(nextTransferId_++);
    transfer.userId = userId;
    transfer.sourceChain = sourceChain;
    transfer.destChain = Chain::QUANTUMPULSE;
    transfer.amount = amount;
    transfer.sourceTxHash = sourceTxHash;
    transfer.status = "pending";
    transfer.timestamp = std::time(nullptr);

    transfers_[transfer.transferId] = transfer;

    Logging::Logger::getInstance().info(
        "Bridge in initiated: " + std::to_string(amount) + " from " +
            getChainName(sourceChain),
        "Bridge", 0);

    return transfer.transferId;
  }

  // Confirm transfer (called by validators)
  bool confirmTransfer(const std::string &transferId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = transfers_.find(transferId);
    if (it == transfers_.end())
      return false;

    it->second.confirmations++;
    if (it->second.confirmations >= REQUIRED_CONFIRMATIONS) {
      it->second.status = "completed";
    } else {
      it->second.status = "confirmed";
    }
    return true;
  }

  // Get transfer status
  [[nodiscard]] std::optional<BridgeTransfer>
  getTransfer(const std::string &transferId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transfers_.find(transferId);
    return it != transfers_.end() ? std::optional(it->second) : std::nullopt;
  }

  // Get user transfers
  [[nodiscard]] std::vector<BridgeTransfer>
  getUserTransfers(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BridgeTransfer> result;
    for (const auto &[id, transfer] : transfers_) {
      if (transfer.userId == userId)
        result.push_back(transfer);
    }
    return result;
  }

  // Get bridge fees
  [[nodiscard]] double getBridgeFee(Chain destChain) const noexcept {
    switch (destChain) {
    case Chain::ETHEREUM:
      return 0.001; // 0.1%
    case Chain::BITCOIN:
      return 0.002; // 0.2%
    case Chain::BSC:
      return 0.0005; // 0.05%
    case Chain::POLYGON:
      return 0.0003; // 0.03%
    case Chain::SOLANA:
      return 0.0004; // 0.04%
    default:
      return 0;
    }
  }

  // Get supported chains
  [[nodiscard]] std::vector<std::pair<Chain, std::string>>
  getSupportedChains() const noexcept {
    return {{Chain::ETHEREUM, "Ethereum"},
            {Chain::BITCOIN, "Bitcoin"},
            {Chain::BSC, "BNB Smart Chain"},
            {Chain::POLYGON, "Polygon"},
            {Chain::SOLANA, "Solana"}};
  }

  // Get wrapped token info
  [[nodiscard]] std::optional<WrappedToken>
  getWrappedToken(Chain chain) const noexcept {
    auto it = wrappedTokens_.find(chain);
    return it != wrappedTokens_.end() ? std::optional(it->second)
                                      : std::nullopt;
  }

private:
  static constexpr int REQUIRED_CONFIRMATIONS = 12;

  std::map<std::string, BridgeTransfer> transfers_;
  std::map<Chain, WrappedToken> wrappedTokens_;
  mutable std::mutex mutex_;
  int nextTransferId_{1};

  void initializeWrappedTokens() noexcept {
    wrappedTokens_[Chain::ETHEREUM] = {Chain::ETHEREUM, "0x1234...abcd", "wQP",
                                       0};
    wrappedTokens_[Chain::BSC] = {Chain::BSC, "0x5678...efgh", "wQP", 0};
    wrappedTokens_[Chain::POLYGON] = {Chain::POLYGON, "0x9abc...ijkl", "wQP",
                                      0};
  }

  [[nodiscard]] std::string getChainName(Chain chain) const noexcept {
    switch (chain) {
    case Chain::QUANTUMPULSE:
      return "QuantumPulse";
    case Chain::ETHEREUM:
      return "Ethereum";
    case Chain::BITCOIN:
      return "Bitcoin";
    case Chain::BSC:
      return "BSC";
    case Chain::POLYGON:
      return "Polygon";
    case Chain::SOLANA:
      return "Solana";
    default:
      return "Unknown";
    }
  }
};

} // namespace QuantumPulse::Bridge

#endif // QUANTUMPULSE_BRIDGE_V7_H
