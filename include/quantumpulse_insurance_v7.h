#ifndef QUANTUMPULSE_INSURANCE_V7_H
#define QUANTUMPULSE_INSURANCE_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace QuantumPulse::Insurance {

// Policy type
enum class PolicyType { WALLET, SMART_CONTRACT, DEFI, NFT };

// Insurance policy
struct Policy {
  std::string policyId;
  std::string holder;
  PolicyType type;
  double coverageAmount;
  double premium;
  double deductible;
  int64_t startDate;
  int64_t endDate;
  bool isActive;
  bool hasClaim;
};

// Claim
struct Claim {
  std::string claimId;
  std::string policyId;
  double amount;
  std::string reason;
  bool approved;
  bool paid;
  int64_t filedAt;
};

// Insurance Protocol
class InsuranceProtocol final {
public:
  InsuranceProtocol() noexcept {
    Logging::Logger::getInstance().info("Insurance Protocol initialized",
                                        "Insurance", 0);
  }

  // Purchase policy
  [[nodiscard]] std::string purchasePolicy(const std::string &holder,
                                           PolicyType type,
                                           double coverageAmount,
                                           int durationDays) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    Policy policy;
    policy.policyId = "POL-" + std::to_string(nextPolicyId_++);
    policy.holder = holder;
    policy.type = type;
    policy.coverageAmount = coverageAmount;
    policy.premium = calculatePremium(coverageAmount, type, durationDays);
    policy.deductible = coverageAmount * 0.1; // 10% deductible
    policy.startDate = std::time(nullptr);
    policy.endDate = policy.startDate + (durationDays * 86400);
    policy.isActive = true;
    policy.hasClaim = false;

    policies_[policy.policyId] = policy;
    totalPremiums_ += policy.premium;

    return policy.policyId;
  }

  // File claim
  [[nodiscard]] std::string fileClaim(const std::string &policyId,
                                      double amount,
                                      const std::string &reason) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = policies_.find(policyId);
    if (it == policies_.end() || !it->second.isActive)
      return "";
    if (it->second.hasClaim)
      return ""; // One claim per policy
    if (amount > it->second.coverageAmount)
      amount = it->second.coverageAmount;

    Claim claim;
    claim.claimId = "CLM-" + std::to_string(nextClaimId_++);
    claim.policyId = policyId;
    claim.amount = amount - it->second.deductible;
    claim.reason = reason;
    claim.approved = false;
    claim.paid = false;
    claim.filedAt = std::time(nullptr);

    claims_[claim.claimId] = claim;
    it->second.hasClaim = true;

    return claim.claimId;
  }

  // Approve claim
  bool approveClaim(const std::string &claimId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = claims_.find(claimId);
    if (it == claims_.end())
      return false;

    it->second.approved = true;
    return true;
  }

  // Pay claim
  bool payClaim(const std::string &claimId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = claims_.find(claimId);
    if (it == claims_.end() || !it->second.approved)
      return false;

    it->second.paid = true;
    totalPayouts_ += it->second.amount;
    return true;
  }

  // Get policy
  [[nodiscard]] std::optional<Policy>
  getPolicy(const std::string &policyId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = policies_.find(policyId);
    return it != policies_.end() ? std::optional(it->second) : std::nullopt;
  }

  [[nodiscard]] double getTotalPremiums() const noexcept {
    return totalPremiums_;
  }
  [[nodiscard]] double getTotalPayouts() const noexcept {
    return totalPayouts_;
  }

private:
  std::map<std::string, Policy> policies_;
  std::map<std::string, Claim> claims_;
  mutable std::mutex mutex_;
  int nextPolicyId_{1};
  int nextClaimId_{1};
  double totalPremiums_{0};
  double totalPayouts_{0};

  double calculatePremium(double coverage, PolicyType type,
                          int days) const noexcept {
    double baseRate = 0;
    switch (type) {
    case PolicyType::WALLET:
      baseRate = 0.01;
      break;
    case PolicyType::SMART_CONTRACT:
      baseRate = 0.03;
      break;
    case PolicyType::DEFI:
      baseRate = 0.05;
      break;
    case PolicyType::NFT:
      baseRate = 0.02;
      break;
    }
    return coverage * baseRate * (days / 365.0);
  }
};

} // namespace QuantumPulse::Insurance

#endif // QUANTUMPULSE_INSURANCE_V7_H
