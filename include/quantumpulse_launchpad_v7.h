#ifndef QUANTUMPULSE_LAUNCHPAD_V7_H
#define QUANTUMPULSE_LAUNCHPAD_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::Launchpad {

// Token sale status
enum class SaleStatus { UPCOMING, ACTIVE, COMPLETED, CANCELLED };

// Token sale (IDO/ICO)
struct TokenSale {
  std::string saleId;
  std::string projectName;
  std::string tokenSymbol;
  std::string description;
  double tokenPrice; // Price in QP
  double softCap;    // Minimum to raise
  double hardCap;    // Maximum to raise
  double totalRaised;
  double tokensForSale;
  int64_t startTime;
  int64_t endTime;
  SaleStatus status;
  std::map<std::string, double> contributions;
  bool isWhitelistOnly;
  std::set<std::string> whitelist;
};

// Launchpad Manager
class LaunchpadManager final {
public:
  LaunchpadManager() noexcept {
    Logging::Logger::getInstance().info("Launchpad initialized", "Launchpad",
                                        0);
  }

  // Create token sale
  [[nodiscard]] std::string createSale(const std::string &projectName,
                                       const std::string &tokenSymbol,
                                       double tokenPrice, double softCap,
                                       double hardCap, double tokensForSale,
                                       int durationDays) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    TokenSale sale;
    sale.saleId = "IDO-" + std::to_string(nextSaleId_++);
    sale.projectName = projectName;
    sale.tokenSymbol = tokenSymbol;
    sale.tokenPrice = tokenPrice;
    sale.softCap = softCap;
    sale.hardCap = hardCap;
    sale.tokensForSale = tokensForSale;
    sale.totalRaised = 0;
    sale.startTime = std::time(nullptr);
    sale.endTime = sale.startTime + (durationDays * 86400);
    sale.status = SaleStatus::ACTIVE;
    sale.isWhitelistOnly = false;

    sales_[sale.saleId] = sale;

    Logging::Logger::getInstance().info("Token sale created: " + sale.saleId +
                                            " - " + projectName,
                                        "Launchpad", 0);

    return sale.saleId;
  }

  // Contribute to sale
  [[nodiscard]] bool contribute(const std::string &saleId,
                                const std::string &contributor,
                                double amountQP) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sales_.find(saleId);
    if (it == sales_.end())
      return false;

    auto &sale = it->second;

    if (sale.status != SaleStatus::ACTIVE)
      return false;
    if (std::time(nullptr) > sale.endTime) {
      finalizeSale(saleId);
      return false;
    }

    // Whitelist check
    if (sale.isWhitelistOnly && sale.whitelist.count(contributor) == 0) {
      return false;
    }

    // Hard cap check
    if (sale.totalRaised + amountQP > sale.hardCap) {
      amountQP = sale.hardCap - sale.totalRaised;
      if (amountQP <= 0)
        return false;
    }

    sale.contributions[contributor] += amountQP;
    sale.totalRaised += amountQP;

    // Auto-finalize if hard cap reached
    if (sale.totalRaised >= sale.hardCap) {
      sale.status = SaleStatus::COMPLETED;
    }

    return true;
  }

  // Claim tokens
  [[nodiscard]] double claimTokens(const std::string &saleId,
                                   const std::string &contributor) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sales_.find(saleId);
    if (it == sales_.end())
      return 0;

    auto &sale = it->second;
    if (sale.status != SaleStatus::COMPLETED)
      return 0;

    auto contribIt = sale.contributions.find(contributor);
    if (contribIt == sale.contributions.end())
      return 0;

    double contributed = contribIt->second;
    double tokens = contributed / sale.tokenPrice;

    sale.contributions.erase(contribIt); // One-time claim
    return tokens;
  }

  // Finalize sale
  void finalizeSale(const std::string &saleId) noexcept {
    auto it = sales_.find(saleId);
    if (it == sales_.end())
      return;

    auto &sale = it->second;
    if (sale.totalRaised >= sale.softCap) {
      sale.status = SaleStatus::COMPLETED;
    } else {
      sale.status = SaleStatus::CANCELLED; // Refund enabled
    }
  }

  // Get active sales
  [[nodiscard]] std::vector<TokenSale> getActiveSales() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TokenSale> result;
    for (const auto &[id, sale] : sales_) {
      if (sale.status == SaleStatus::ACTIVE ||
          sale.status == SaleStatus::UPCOMING) {
        result.push_back(sale);
      }
    }
    return result;
  }

  // Add to whitelist
  void addToWhitelist(const std::string &saleId,
                      const std::string &address) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sales_.find(saleId);
    if (it != sales_.end()) {
      it->second.whitelist.insert(address);
    }
  }

private:
  std::map<std::string, TokenSale> sales_;
  mutable std::mutex mutex_;
  int nextSaleId_{1};
};

// Airdrop Manager
class AirdropManager final {
public:
  struct Airdrop {
    std::string airdropId;
    std::string tokenSymbol;
    double amountPerUser;
    std::map<std::string, bool> claimed;
    int64_t expiresAt;
  };

  [[nodiscard]] std::string createAirdrop(const std::string &symbol,
                                          double amountPerUser,
                                          int durationDays) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    Airdrop airdrop;
    airdrop.airdropId = "AIRDROP-" + std::to_string(nextId_++);
    airdrop.tokenSymbol = symbol;
    airdrop.amountPerUser = amountPerUser;
    airdrop.expiresAt = std::time(nullptr) + (durationDays * 86400);

    airdrops_[airdrop.airdropId] = airdrop;
    return airdrop.airdropId;
  }

  [[nodiscard]] double claim(const std::string &airdropId,
                             const std::string &user) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = airdrops_.find(airdropId);
    if (it == airdrops_.end())
      return 0;

    if (std::time(nullptr) > it->second.expiresAt)
      return 0;
    if (it->second.claimed[user])
      return 0;

    it->second.claimed[user] = true;
    return it->second.amountPerUser;
  }

private:
  std::map<std::string, Airdrop> airdrops_;
  std::mutex mutex_;
  int nextId_{1};
};

} // namespace QuantumPulse::Launchpad

#endif // QUANTUMPULSE_LAUNCHPAD_V7_H
