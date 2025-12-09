#ifndef QUANTUMPULSE_ATOMIC_SWAP_V7_H
#define QUANTUMPULSE_ATOMIC_SWAP_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace QuantumPulse::AtomicSwap {

// Swap states
enum class SwapState { INITIATED, HASH_LOCKED, REDEEMED, REFUNDED, EXPIRED };

// Atomic Swap Contract
struct SwapContract {
  std::string swapId;
  std::string initiator;   // Party A
  std::string participant; // Party B
  double qpAmount;         // QP amount
  std::string otherChain;  // BTC, ETH, etc.
  double otherAmount;      // Amount on other chain
  std::string hashLock;    // SHA3-512 hash
  std::string preimage;    // Secret (only known to initiator initially)
  int64_t lockTime;        // HTLC timeout
  int64_t createdAt;
  SwapState state;
};

// Atomic Swap Manager
class AtomicSwapManager final {
public:
  AtomicSwapManager() noexcept {
    Logging::Logger::getInstance().info("Atomic Swap Manager initialized",
                                        "AtomicSwap", 0);
  }

  // Initiate a new atomic swap
  std::string initiateSwap(const std::string &initiator,
                           const std::string &participant, double qpAmount,
                           const std::string &otherChain, double otherAmount,
                           int lockHours = 24) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    // Generate secret and hash
    Crypto::CryptoManager cm;
    std::string preimage =
        cm.sha3_512_v11(initiator + std::to_string(std::time(nullptr)) +
                            std::to_string(qpAmount),
                        0);
    std::string hashLock = cm.sha3_512_v11(preimage, 0); // H(preimage)

    SwapContract swap;
    swap.swapId = generateSwapId();
    swap.initiator = initiator;
    swap.participant = participant;
    swap.qpAmount = qpAmount;
    swap.otherChain = otherChain;
    swap.otherAmount = otherAmount;
    swap.hashLock = hashLock;
    swap.preimage = preimage; // Initiator knows this
    swap.lockTime = std::time(nullptr) + (lockHours * 3600);
    swap.createdAt = std::time(nullptr);
    swap.state = SwapState::INITIATED;

    swaps_[swap.swapId] = swap;

    Logging::Logger::getInstance().info("Swap initiated: " + swap.swapId +
                                            " for " + std::to_string(qpAmount) +
                                            " QP",
                                        "AtomicSwap", 0);

    return swap.swapId;
  }

  // Participant creates their side of swap
  bool participateSwap(const std::string &swapId,
                       const std::string &participant,
                       double otherAmount) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!swaps_.count(swapId))
      return false;

    auto &swap = swaps_[swapId];
    if (swap.state != SwapState::INITIATED)
      return false;
    if (swap.participant != participant)
      return false;

    swap.state = SwapState::HASH_LOCKED;

    Logging::Logger::getInstance().info("Swap participated: " + swapId,
                                        "AtomicSwap", 0);

    return true;
  }

  // Redeem with preimage (reveals secret)
  bool redeemSwap(const std::string &swapId,
                  const std::string &preimage) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!swaps_.count(swapId))
      return false;

    auto &swap = swaps_[swapId];
    if (swap.state != SwapState::HASH_LOCKED)
      return false;

    // Verify preimage
    Crypto::CryptoManager cm;
    std::string hash = cm.sha3_512_v11(preimage, 0);

    if (hash != swap.hashLock) {
      Logging::Logger::getInstance().warning(
          "Invalid preimage for swap: " + swapId, "AtomicSwap", 0);
      return false;
    }

    // Check timeout
    if (std::time(nullptr) > swap.lockTime) {
      swap.state = SwapState::EXPIRED;
      return false;
    }

    swap.state = SwapState::REDEEMED;
    swap.preimage = preimage; // Now participant knows it too

    Logging::Logger::getInstance().info("Swap redeemed: " + swapId,
                                        "AtomicSwap", 0);

    return true;
  }

  // Refund after timeout
  bool refundSwap(const std::string &swapId,
                  const std::string &requester) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!swaps_.count(swapId))
      return false;

    auto &swap = swaps_[swapId];

    // Only initiator can refund
    if (swap.initiator != requester)
      return false;

    // Must be expired
    if (std::time(nullptr) <= swap.lockTime) {
      Logging::Logger::getInstance().warning("Swap not expired yet: " + swapId,
                                             "AtomicSwap", 0);
      return false;
    }

    swap.state = SwapState::REFUNDED;

    Logging::Logger::getInstance().info("Swap refunded: " + swapId,
                                        "AtomicSwap", 0);

    return true;
  }

  // Get swap details
  SwapContract getSwap(const std::string &swapId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (swaps_.count(swapId)) {
      auto swap = swaps_.at(swapId);
      // Hide preimage from non-initiator
      swap.preimage = "HIDDEN";
      return swap;
    }
    return SwapContract{};
  }

  // Get hashlock only (for participant to use on other chain)
  std::string getHashLock(const std::string &swapId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (swaps_.count(swapId)) {
      return swaps_.at(swapId).hashLock;
    }
    return "";
  }

  // Get preimage (only after redeem, for initiator to claim on other chain)
  std::string getPreimage(const std::string &swapId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (swaps_.count(swapId) &&
        swaps_.at(swapId).state == SwapState::REDEEMED) {
      return swaps_.at(swapId).preimage;
    }
    return "";
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, SwapContract> swaps_;
  std::atomic<int> swapCounter_{0};

  std::string generateSwapId() {
    return "SWAP_" + std::to_string(++swapCounter_) + "_" +
           std::to_string(std::time(nullptr));
  }
};

} // namespace QuantumPulse::AtomicSwap

#endif // QUANTUMPULSE_ATOMIC_SWAP_V7_H
