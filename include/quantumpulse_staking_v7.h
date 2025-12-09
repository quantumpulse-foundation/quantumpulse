#ifndef QUANTUMPULSE_STAKING_V7_H
#define QUANTUMPULSE_STAKING_V7_H

#include "quantumpulse_logging_v7.h"
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::Staking {

// Staking Configuration
struct StakingConfig {
  static constexpr double MIN_STAKE_AMOUNT = 100.0;        // Minimum 100 QP
  static constexpr double APY_RATE = 12.0;                 // 12% annual yield
  static constexpr int MIN_LOCK_DAYS = 30;                 // 30 days minimum
  static constexpr int MAX_LOCK_DAYS = 365;                // 1 year maximum
  static constexpr double EARLY_WITHDRAWAL_PENALTY = 10.0; // 10% penalty
};

// Stake record
struct Stake {
  std::string stakeId;
  std::string walletAddress;
  double amount;
  int64_t startTime;
  int lockDays;
  double rewardRate; // Custom APY
  bool active;
  double earnedRewards;
};

// Staking Pool Manager
class StakingPool final {
public:
  StakingPool() noexcept {
    Logging::Logger::getInstance().info("Staking Pool initialized", "Staking",
                                        0);
  }

  // Create new stake
  std::string createStake(const std::string &wallet, double amount,
                          int lockDays) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (amount < StakingConfig::MIN_STAKE_AMOUNT) {
      Logging::Logger::getInstance().warning("Stake amount below minimum",
                                             "Staking", 0);
      return "";
    }

    if (lockDays < StakingConfig::MIN_LOCK_DAYS ||
        lockDays > StakingConfig::MAX_LOCK_DAYS) {
      return "";
    }

    Stake stake;
    stake.stakeId = generateStakeId();
    stake.walletAddress = wallet;
    stake.amount = amount;
    stake.startTime = std::time(nullptr);
    stake.lockDays = lockDays;
    stake.rewardRate = calculateAPY(lockDays);
    stake.active = true;
    stake.earnedRewards = 0.0;

    stakes_[stake.stakeId] = stake;
    totalStaked_ += amount;

    Logging::Logger::getInstance().info("New stake created: " + stake.stakeId +
                                            " for " + std::to_string(amount) +
                                            " QP",
                                        "Staking", 0);

    return stake.stakeId;
  }

  // Calculate current rewards
  double calculateRewards(const std::string &stakeId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!stakes_.count(stakeId))
      return 0.0;

    const auto &stake = stakes_.at(stakeId);
    if (!stake.active)
      return stake.earnedRewards;

    int64_t now = std::time(nullptr);
    double daysStaked = (now - stake.startTime) / 86400.0;
    double yearlyReward = stake.amount * (stake.rewardRate / 100.0);
    double earnedReward = yearlyReward * (daysStaked / 365.0);

    return earnedReward;
  }

  // Withdraw stake + rewards
  double withdrawStake(const std::string &stakeId,
                       bool forceEarly = false) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!stakes_.count(stakeId))
      return 0.0;

    auto &stake = stakes_[stakeId];
    if (!stake.active)
      return 0.0;

    int64_t now = std::time(nullptr);
    int daysStaked = (now - stake.startTime) / 86400;

    double rewards = calculateRewardsInternal(stake);
    double totalAmount = stake.amount + rewards;

    // Apply early withdrawal penalty
    if (daysStaked < stake.lockDays) {
      if (!forceEarly)
        return 0.0; // Not allowed without force
      double penalty =
          totalAmount * (StakingConfig::EARLY_WITHDRAWAL_PENALTY / 100.0);
      totalAmount -= penalty;
      Logging::Logger::getInstance().warning(
          "Early withdrawal penalty applied: " + std::to_string(penalty) +
              " QP",
          "Staking", 0);
    }

    stake.active = false;
    stake.earnedRewards = rewards;
    totalStaked_ -= stake.amount;

    return totalAmount;
  }

  // Get stake info
  Stake getStake(const std::string &stakeId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stakes_.count(stakeId)) {
      return stakes_.at(stakeId);
    }
    return Stake{};
  }

  // Get all stakes for wallet
  std::vector<Stake> getWalletStakes(const std::string &wallet) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Stake> result;
    for (const auto &[id, stake] : stakes_) {
      if (stake.walletAddress == wallet) {
        result.push_back(stake);
      }
    }
    return result;
  }

  // Get total staked
  double getTotalStaked() const noexcept { return totalStaked_.load(); }

  // Get total stakers
  size_t getTotalStakers() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return stakes_.size();
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, Stake> stakes_;
  std::atomic<double> totalStaked_{0.0};
  std::atomic<int> stakeCounter_{0};

  std::string generateStakeId() {
    return "STAKE_" + std::to_string(++stakeCounter_) + "_" +
           std::to_string(std::time(nullptr));
  }

  double calculateAPY(int lockDays) const noexcept {
    // Longer lock = higher APY (up to 20%)
    double baseAPY = StakingConfig::APY_RATE;
    double bonus = (lockDays - StakingConfig::MIN_LOCK_DAYS) *
                   0.02;                    // +0.02% per extra day
    return std::min(baseAPY + bonus, 20.0); // Max 20% APY
  }

  double calculateRewardsInternal(const Stake &stake) const noexcept {
    int64_t now = std::time(nullptr);
    double daysStaked = (now - stake.startTime) / 86400.0;
    double yearlyReward = stake.amount * (stake.rewardRate / 100.0);
    return yearlyReward * (daysStaked / 365.0);
  }
};

} // namespace QuantumPulse::Staking

#endif // QUANTUMPULSE_STAKING_V7_H
