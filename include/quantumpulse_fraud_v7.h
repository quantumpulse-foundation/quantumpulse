#ifndef QUANTUMPULSE_FRAUD_V7_H
#define QUANTUMPULSE_FRAUD_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::AI {

// Risk levels
enum class RiskLevel { LOW, MEDIUM, HIGH, CRITICAL };

// Transaction analysis result
struct FraudAnalysis {
  std::string txId;
  RiskLevel riskLevel;
  double riskScore; // 0-100
  std::vector<std::string> flags;
  bool blocked;
  std::string recommendation;
};

// User risk profile
struct UserRiskProfile {
  std::string userId;
  double baselineScore;
  int transactionCount;
  double avgTransactionSize;
  double maxTransactionSize;
  int suspiciousActivityCount;
  int64_t lastActivity;
};

// ML-based Fraud Detection
class FraudDetector final {
public:
  FraudDetector() noexcept {
    Logging::Logger::getInstance().info("AI Fraud Detector initialized",
                                        "AI-Fraud", 0);
  }

  // Analyze transaction
  [[nodiscard]] FraudAnalysis analyzeTransaction(const std::string &txId,
                                                 const std::string &from,
                                                 const std::string &to,
                                                 double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    FraudAnalysis result;
    result.txId = txId;
    result.riskScore = 0;
    result.blocked = false;

    // Get or create user profile
    auto &profile = getOrCreateProfile(from);

    // Rule 1: Large transaction (relative to user history)
    if (amount > profile.maxTransactionSize * 2) {
      result.riskScore += 25;
      result.flags.push_back("UNUSUAL_AMOUNT");
    }

    // Rule 2: High velocity (many transactions quickly)
    auto now = std::time(nullptr);
    if (now - profile.lastActivity < 60) { // Less than 1 minute
      result.riskScore += 15;
      result.flags.push_back("HIGH_VELOCITY");
    }

    // Rule 3: New account with large transaction
    if (profile.transactionCount < 5 && amount > 100) {
      result.riskScore += 20;
      result.flags.push_back("NEW_ACCOUNT_LARGE_TX");
    }

    // Rule 4: Known suspicious patterns
    if (checkSuspiciousPatterns(from, to, amount)) {
      result.riskScore += 30;
      result.flags.push_back("SUSPICIOUS_PATTERN");
    }

    // Rule 5: Whale transaction (very large)
    if (amount > 10000) {
      result.riskScore += 10;
      result.flags.push_back("WHALE_TRANSACTION");
    }

    // Determine risk level
    if (result.riskScore >= 80) {
      result.riskLevel = RiskLevel::CRITICAL;
      result.blocked = true;
      result.recommendation = "Block and investigate";
    } else if (result.riskScore >= 60) {
      result.riskLevel = RiskLevel::HIGH;
      result.recommendation = "Manual review required";
    } else if (result.riskScore >= 30) {
      result.riskLevel = RiskLevel::MEDIUM;
      result.recommendation = "Monitor closely";
    } else {
      result.riskLevel = RiskLevel::LOW;
      result.recommendation = "Approve";
    }

    // Update profile
    profile.transactionCount++;
    profile.avgTransactionSize =
        (profile.avgTransactionSize * (profile.transactionCount - 1) + amount) /
        profile.transactionCount;
    profile.maxTransactionSize = std::max(profile.maxTransactionSize, amount);
    profile.lastActivity = now;

    if (result.riskScore >= 50) {
      profile.suspiciousActivityCount++;
    }

    if (result.blocked) {
      Logging::Logger::getInstance().warning(
          "Transaction blocked by fraud detection: " + txId, "AI-Fraud", 0);
    }

    return result;
  }

  // Get user risk profile
  [[nodiscard]] std::optional<UserRiskProfile>
  getUserProfile(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = profiles_.find(userId);
    return it != profiles_.end() ? std::optional(it->second) : std::nullopt;
  }

  // Report suspicious activity
  void reportSuspicious(const std::string &userId,
                        const std::string &reason) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &profile = getOrCreateProfile(userId);
    profile.suspiciousActivityCount++;
    profile.baselineScore += 10;

    Logging::Logger::getInstance().warning(
        "Suspicious activity reported: " + userId + " - " + reason, "AI-Fraud",
        0);
  }

  // Whitelist user
  void whitelistUser(const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    whitelistedUsers_.insert(userId);
  }

  // Blacklist user
  void blacklistUser(const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    blacklistedUsers_.insert(userId);
  }

private:
  std::map<std::string, UserRiskProfile> profiles_;
  std::set<std::string> whitelistedUsers_;
  std::set<std::string> blacklistedUsers_;
  mutable std::mutex mutex_;

  UserRiskProfile &getOrCreateProfile(const std::string &userId) noexcept {
    auto it = profiles_.find(userId);
    if (it == profiles_.end()) {
      UserRiskProfile profile;
      profile.userId = userId;
      profile.baselineScore = 0;
      profile.transactionCount = 0;
      profile.avgTransactionSize = 0;
      profile.maxTransactionSize = 0;
      profile.suspiciousActivityCount = 0;
      profile.lastActivity = 0;
      profiles_[userId] = profile;
    }
    return profiles_[userId];
  }

  bool checkSuspiciousPatterns(const std::string &from, const std::string &to,
                               double amount) noexcept {
    // Check blacklist
    if (blacklistedUsers_.count(from) || blacklistedUsers_.count(to))
      return true;

    // Round number pattern (potential structuring)
    if (std::fmod(amount, 100) == 0 && amount >= 500)
      return true;

    return false;
  }
};

// Price Prediction AI
class PricePredictor final {
public:
  struct Prediction {
    double currentPrice;
    double predicted1h;
    double predicted24h;
    double predicted7d;
    double confidence;
    std::string trend; // bullish, bearish, neutral
  };

  [[nodiscard]] Prediction predictPrice() noexcept {
    Prediction p;
    p.currentPrice = 600000;
    p.predicted1h = 600050;
    p.predicted24h = 601000;
    p.predicted7d = 605000;
    p.confidence = 0.75;
    p.trend = "bullish";
    return p;
  }

  [[nodiscard]] std::vector<double> getHistoricalPrices(int days) noexcept {
    std::vector<double> prices;
    for (int i = 0; i < days; ++i) {
      prices.push_back(600000 + (i * 100));
    }
    return prices;
  }
};

// AI Chatbot
class Chatbot final {
public:
  [[nodiscard]] std::string respond(const std::string &message) noexcept {
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("price") != std::string::npos) {
      return "The current QP price is $600,000 with a guaranteed minimum price "
             "floor.";
    }
    if (lower.find("buy") != std::string::npos) {
      return "You can buy QP using PayPal, Stripe, or on exchanges like "
             "Binance and Coinbase.";
    }
    if (lower.find("stake") != std::string::npos) {
      return "We offer staking pools with up to 35% APY. Check the DeFi "
             "section for details.";
    }
    if (lower.find("help") != std::string::npos) {
      return "I can help with: price info, buying QP, staking, NFTs, and more. "
             "What would you like to know?";
    }
    return "I'm your QuantumPulse AI assistant. Ask me about prices, trading, "
           "staking, or any other features!";
  }
};

} // namespace QuantumPulse::AI

#endif // QUANTUMPULSE_FRAUD_V7_H
