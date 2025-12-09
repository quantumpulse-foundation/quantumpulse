#ifndef QUANTUMPULSE_GAS_V7_H
#define QUANTUMPULSE_GAS_V7_H

#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <map>
#include <string>

namespace QuantumPulse::Gas {

// Gas pricing (Ethereum-like)
struct GasPrice {
  double baseFee;     // Base fee (burns)
  double priorityFee; // Miner tip
  double maxFee;      // Maximum fee willing to pay

  double effectiveFee() const {
    return std::min(baseFee + priorityFee, maxFee);
  }
};

// Gas limits for operations
struct GasLimits {
  static constexpr uint64_t TRANSFER = 21000;        // Simple transfer
  static constexpr uint64_t CONTRACT_CREATE = 53000; // Deploy contract
  static constexpr uint64_t CONTRACT_CALL = 21000;   // Base for contract call
  static constexpr uint64_t STORAGE_SET = 20000;     // Set storage slot
  static constexpr uint64_t STORAGE_CLEAR = 5000;    // Clear storage slot
  static constexpr uint64_t LOG_TOPIC = 375;         // Per log topic
  static constexpr uint64_t LOG_DATA = 8;            // Per log byte
  static constexpr uint64_t MEMORY = 3;              // Per word memory
  static constexpr uint64_t COPY = 3;                // Per word copy
  static constexpr uint64_t BLOCK_GAS_LIMIT = 30000000; // Block limit
};

// EIP-1559 style gas manager
class GasManager final {
public:
  GasManager() noexcept {
    baseFee_ = 1.0; // 1 Gwei equivalent
    Logging::Logger::getInstance().info("Gas Manager initialized", "Gas", 0);
  }

  // Calculate gas for operation
  uint64_t calculateGas(const std::string &operation,
                        size_t dataSize = 0) const noexcept {
    if (operation == "transfer") {
      return GasLimits::TRANSFER;
    }
    if (operation == "deploy") {
      return GasLimits::CONTRACT_CREATE + dataSize * 200;
    }
    if (operation == "call") {
      return GasLimits::CONTRACT_CALL + dataSize * 16;
    }
    if (operation == "storage_write") {
      return GasLimits::STORAGE_SET;
    }
    return GasLimits::TRANSFER; // Default
  }

  // Estimate gas for transaction
  uint64_t estimateGas(const std::string &to, const std::string &data,
                       double value) const noexcept {
    uint64_t gas = GasLimits::TRANSFER;

    // Add data cost
    for (char c : data) {
      gas += (c == 0) ? 4 : 16; // Zero bytes cheaper
    }

    // Contract creation
    if (to.empty()) {
      gas += GasLimits::CONTRACT_CREATE;
    }

    return gas;
  }

  // Calculate transaction fee
  double calculateFee(uint64_t gasUsed, const GasPrice &price) const noexcept {
    return gasUsed * price.effectiveFee() / 1e9; // Convert from Gwei
  }

  // Update base fee (EIP-1559 algorithm)
  void updateBaseFee(uint64_t gasUsed, uint64_t gasTarget) noexcept {
    double utilizationRatio = (double)gasUsed / gasTarget;

    if (utilizationRatio > 1.0) {
      // Block over target, increase base fee
      baseFee_ *= (1.0 + 0.125 * (utilizationRatio - 1.0));
    } else {
      // Block under target, decrease base fee
      baseFee_ *= (1.0 - 0.125 * (1.0 - utilizationRatio));
    }

    // Minimum base fee
    baseFee_ = std::max(baseFee_, 0.1);
  }

  // Get current base fee
  double getBaseFee() const noexcept { return baseFee_; }

  // Suggest gas price
  GasPrice suggestGasPrice(const std::string &speed = "medium") const noexcept {
    GasPrice price;
    price.baseFee = baseFee_;

    if (speed == "slow") {
      price.priorityFee = 0.1;
      price.maxFee = baseFee_ * 1.1;
    } else if (speed == "medium") {
      price.priorityFee = 1.0;
      price.maxFee = baseFee_ * 1.5;
    } else { // fast
      price.priorityFee = 2.0;
      price.maxFee = baseFee_ * 2.0;
    }

    return price;
  }

  // Get gas statistics
  std::map<std::string, double> getStats() const noexcept {
    return {{"baseFee", baseFee_},
            {"slowGasPrice", baseFee_ + 0.1},
            {"mediumGasPrice", baseFee_ + 1.0},
            {"fastGasPrice", baseFee_ + 2.0},
            {"blockGasLimit", GasLimits::BLOCK_GAS_LIMIT}};
  }

private:
  double baseFee_;
};

} // namespace QuantumPulse::Gas

#endif // QUANTUMPULSE_GAS_V7_H
