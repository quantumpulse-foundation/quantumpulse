#ifndef QUANTUMPULSE_DEFI_V7_H
#define QUANTUMPULSE_DEFI_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::DeFi {

// Staking pool
struct StakingPool {
  std::string poolId;
  std::string name;
  double apy; // Annual Percentage Yield
  double totalStaked;
  double minStake;
  int lockDays;
  bool active;
};

// User stake
struct UserStake {
  std::string stakeId;
  std::string userId;
  std::string poolId;
  double amount;
  int64_t startTime;
  int64_t unlockTime;
  double earnedRewards;
};

// Lending position
struct LendingPosition {
  std::string positionId;
  std::string userId;
  double collateralQP;
  double borrowedUSD;
  double interestRate;
  double healthFactor;
  int64_t openTime;
};

// Staking Protocol
class StakingProtocol final {
public:
  StakingProtocol() noexcept {
    initializePools();
    Logging::Logger::getInstance().info("Staking Protocol initialized", "DeFi",
                                        0);
  }

  // Stake QP
  [[nodiscard]] std::string stake(const std::string &userId,
                                  const std::string &poolId,
                                  double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto poolIt = pools_.find(poolId);
    if (poolIt == pools_.end() || amount < poolIt->second.minStake) {
      return "";
    }

    UserStake stake;
    stake.stakeId = "stake_" + std::to_string(nextStakeId_++);
    stake.userId = userId;
    stake.poolId = poolId;
    stake.amount = amount;
    stake.startTime = std::time(nullptr);
    stake.unlockTime = stake.startTime + (poolIt->second.lockDays * 86400);
    stake.earnedRewards = 0;

    stakes_[stake.stakeId] = stake;
    poolIt->second.totalStaked += amount;

    Logging::Logger::getInstance().info(
        "Staked " + std::to_string(amount) + " QP in " + poolId, "DeFi", 0);

    return stake.stakeId;
  }

  // Unstake
  bool unstake(const std::string &stakeId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stakes_.find(stakeId);
    if (it == stakes_.end())
      return false;

    if (std::time(nullptr) < it->second.unlockTime)
      return false;

    auto poolIt = pools_.find(it->second.poolId);
    if (poolIt != pools_.end()) {
      poolIt->second.totalStaked -= it->second.amount;
    }

    stakes_.erase(it);
    return true;
  }

  // Claim rewards
  double claimRewards(const std::string &stakeId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = stakes_.find(stakeId);
    if (it == stakes_.end())
      return 0;

    double rewards = calculateRewards(it->second);
    it->second.earnedRewards = 0;
    return rewards;
  }

  // Get pools
  [[nodiscard]] std::vector<StakingPool> getPools() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StakingPool> result;
    for (const auto &[id, pool] : pools_) {
      result.push_back(pool);
    }
    return result;
  }

  // Get user stakes
  [[nodiscard]] std::vector<UserStake>
  getUserStakes(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserStake> result;
    for (const auto &[id, stake] : stakes_) {
      if (stake.userId == userId)
        result.push_back(stake);
    }
    return result;
  }

private:
  std::map<std::string, StakingPool> pools_;
  std::map<std::string, UserStake> stakes_;
  mutable std::mutex mutex_;
  int nextStakeId_{1};

  void initializePools() noexcept {
    pools_["flexible"] = {"flexible", "Flexible Staking", 5.0, 0, 1, 0, true};
    pools_["30day"] = {"30day", "30 Day Lock", 12.0, 0, 10, 30, true};
    pools_["90day"] = {"90day", "90 Day Lock", 20.0, 0, 50, 90, true};
    pools_["365day"] = {"365day", "1 Year Lock", 35.0, 0, 100, 365, true};
  }

  double calculateRewards(const UserStake &stake) const noexcept {
    auto poolIt = pools_.find(stake.poolId);
    if (poolIt == pools_.end())
      return 0;

    double daysStaked = (std::time(nullptr) - stake.startTime) / 86400.0;
    double dailyRate = poolIt->second.apy / 365.0 / 100.0;
    return stake.amount * dailyRate * daysStaked;
  }
};

// Lending Protocol
class LendingProtocol final {
public:
  LendingProtocol() noexcept {
    Logging::Logger::getInstance().info("Lending Protocol initialized", "DeFi",
                                        0);
  }

  // Deposit collateral and borrow
  [[nodiscard]] std::string borrow(const std::string &userId,
                                   double collateralQP,
                                   double borrowUSD) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Max LTV 50% (collateral value must be 2x borrow)
    double collateralValue = collateralQP * 600000;
    if (borrowUSD > collateralValue * 0.5)
      return "";

    LendingPosition pos;
    pos.positionId = "loan_" + std::to_string(nextLoanId_++);
    pos.userId = userId;
    pos.collateralQP = collateralQP;
    pos.borrowedUSD = borrowUSD;
    pos.interestRate = 8.0; // 8% APR
    pos.healthFactor = collateralValue / borrowUSD;
    pos.openTime = std::time(nullptr);

    positions_[pos.positionId] = pos;

    Logging::Logger::getInstance().info(
        "Loan opened: " + std::to_string(borrowUSD) + " USD", "DeFi", 0);

    return pos.positionId;
  }

  // Repay loan
  bool repay(const std::string &positionId, double amountUSD) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = positions_.find(positionId);
    if (it == positions_.end())
      return false;

    it->second.borrowedUSD -= amountUSD;
    if (it->second.borrowedUSD <= 0) {
      positions_.erase(it);
    }
    return true;
  }

  // Get positions
  [[nodiscard]] std::vector<LendingPosition>
  getUserPositions(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LendingPosition> result;
    for (const auto &[id, pos] : positions_) {
      if (pos.userId == userId)
        result.push_back(pos);
    }
    return result;
  }

private:
  std::map<std::string, LendingPosition> positions_;
  mutable std::mutex mutex_;
  int nextLoanId_{1};
};

// Yield Aggregator
class YieldAggregator final {
public:
  struct YieldFarm {
    std::string name;
    double apy;
    double tvl;
    std::string protocol;
  };

  [[nodiscard]] std::vector<YieldFarm> getBestYields() const noexcept {
    return {{"QP-USDT LP", 45.0, 10000000, "QuantumSwap"},
            {"QP Staking", 35.0, 50000000, "QuantumStake"},
            {"QP-ETH LP", 38.0, 8000000, "QuantumSwap"},
            {"Auto-Compound QP", 42.0, 25000000, "QuantumVault"}};
  }
};

} // namespace QuantumPulse::DeFi

#endif // QUANTUMPULSE_DEFI_V7_H
