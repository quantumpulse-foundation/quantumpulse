#ifndef QUANTUMPULSE_PORTFOLIO_V7_H
#define QUANTUMPULSE_PORTFOLIO_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::Analytics {

// Asset holding
struct Asset {
  std::string symbol;
  double amount;
  double avgBuyPrice;
  double currentPrice;
  double value;
  double pnl;
  double pnlPercent;
};

// Transaction for tax
struct TaxTransaction {
  std::string txId;
  std::string type; // buy, sell, transfer, stake_reward
  std::string asset;
  double amount;
  double priceUSD;
  double feeUSD;
  int64_t timestamp;
  double costBasis;
  double proceeds;
  double gain;
};

// Portfolio Tracker
class PortfolioTracker final {
public:
  PortfolioTracker() noexcept {
    Logging::Logger::getInstance().info("Portfolio Tracker initialized",
                                        "Analytics", 0);
  }

  // Add asset
  void addAsset(const std::string &userId, const std::string &symbol,
                double amount, double buyPrice) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto &portfolio = portfolios_[userId];
    auto it = portfolio.assets.find(symbol);

    if (it == portfolio.assets.end()) {
      Asset asset;
      asset.symbol = symbol;
      asset.amount = amount;
      asset.avgBuyPrice = buyPrice;
      portfolio.assets[symbol] = asset;
    } else {
      // Update average buy price
      double totalValue =
          it->second.amount * it->second.avgBuyPrice + amount * buyPrice;
      it->second.amount += amount;
      it->second.avgBuyPrice = totalValue / it->second.amount;
    }
  }

  // Remove asset
  void removeAsset(const std::string &userId, const std::string &symbol,
                   double amount) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto &portfolio = portfolios_[userId];
    auto it = portfolio.assets.find(symbol);

    if (it != portfolio.assets.end()) {
      it->second.amount -= amount;
      if (it->second.amount <= 0) {
        portfolio.assets.erase(it);
      }
    }
  }

  // Get portfolio
  [[nodiscard]] std::vector<Asset>
  getPortfolio(const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Asset> result;
    auto it = portfolios_.find(userId);
    if (it == portfolios_.end())
      return result;

    for (auto &[symbol, asset] : it->second.assets) {
      asset.currentPrice = getPrice(symbol);
      asset.value = asset.amount * asset.currentPrice;
      asset.pnl = (asset.currentPrice - asset.avgBuyPrice) * asset.amount;
      asset.pnlPercent = ((asset.currentPrice / asset.avgBuyPrice) - 1) * 100;
      result.push_back(asset);
    }

    return result;
  }

  // Get total value
  [[nodiscard]] double getTotalValue(const std::string &userId) noexcept {
    auto portfolio = getPortfolio(userId);
    double total = 0;
    for (const auto &asset : portfolio) {
      total += asset.value;
    }
    return total;
  }

  // Get total PnL
  [[nodiscard]] double getTotalPnL(const std::string &userId) noexcept {
    auto portfolio = getPortfolio(userId);
    double total = 0;
    for (const auto &asset : portfolio) {
      total += asset.pnl;
    }
    return total;
  }

private:
  struct UserPortfolio {
    std::map<std::string, Asset> assets;
  };

  std::map<std::string, UserPortfolio> portfolios_;
  std::mutex mutex_;

  double getPrice(const std::string &symbol) noexcept {
    if (symbol == "QP")
      return 600000;
    if (symbol == "BTC")
      return 45000;
    if (symbol == "ETH")
      return 2500;
    return 1;
  }
};

// Tax Report Generator
class TaxReportGenerator final {
public:
  // Add transaction
  void addTransaction(const std::string &userId,
                      const TaxTransaction &tx) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    transactions_[userId].push_back(tx);
  }

  // Generate tax report
  [[nodiscard]] std::string generateReport(const std::string &userId,
                                           int year) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::stringstream report;
    report << "# QuantumPulse Tax Report " << year << "\n\n";
    report << "User: " << userId << "\n";
    report << "Generated: " << std::time(nullptr) << "\n\n";

    double totalGains = 0;
    double totalLosses = 0;
    int txCount = 0;

    auto it = transactions_.find(userId);
    if (it != transactions_.end()) {
      report << "## Transactions\n\n";
      report << "| Date | Type | Asset | Amount | Price | Gain/Loss |\n";
      report << "|------|------|-------|--------|-------|----------|\n";

      for (const auto &tx : it->second) {
        if (tx.type == "sell") {
          report << "| " << tx.timestamp << " | " << tx.type << " | "
                 << tx.asset << " | " << tx.amount << " | $" << tx.priceUSD
                 << " | $" << tx.gain << " |\n";

          if (tx.gain > 0)
            totalGains += tx.gain;
          else
            totalLosses += std::abs(tx.gain);
          txCount++;
        }
      }
    }

    report << "\n## Summary\n\n";
    report << "- Total Gains: $" << totalGains << "\n";
    report << "- Total Losses: $" << totalLosses << "\n";
    report << "- Net: $" << (totalGains - totalLosses) << "\n";
    report << "- Taxable Transactions: " << txCount << "\n";

    return report.str();
  }

  // Export to CSV
  [[nodiscard]] std::string exportCSV(const std::string &userId,
                                      int year) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::stringstream csv;
    csv << "Date,Type,Asset,Amount,Price,Cost Basis,Proceeds,Gain/Loss\n";

    auto it = transactions_.find(userId);
    if (it != transactions_.end()) {
      for (const auto &tx : it->second) {
        csv << tx.timestamp << "," << tx.type << "," << tx.asset << ","
            << tx.amount << "," << tx.priceUSD << "," << tx.costBasis << ","
            << tx.proceeds << "," << tx.gain << "\n";
      }
    }

    return csv.str();
  }

private:
  std::map<std::string, std::vector<TaxTransaction>> transactions_;
  std::mutex mutex_;
};

// Whale Alert
class WhaleAlert final {
public:
  struct Alert {
    std::string txId;
    std::string from;
    std::string to;
    double amount;
    double valueUSD;
    int64_t timestamp;
  };

  // Check if transaction is whale
  [[nodiscard]] bool isWhaleTransaction(double amount) const noexcept {
    return amount >= WHALE_THRESHOLD;
  }

  // Record alert
  void recordAlert(const Alert &alert) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    alerts_.push_back(alert);
    if (alerts_.size() > 1000)
      alerts_.erase(alerts_.begin());

    Logging::Logger::getInstance().info(
        "🐋 WHALE ALERT: " + std::to_string(alert.amount) + " QP ($" +
            std::to_string(alert.valueUSD) + ")",
        "WhaleAlert", 0);
  }

  // Get recent alerts
  [[nodiscard]] std::vector<Alert>
  getRecentAlerts(int count = 50) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Alert> result;
    int start = std::max(0, static_cast<int>(alerts_.size()) - count);
    for (size_t i = start; i < alerts_.size(); ++i) {
      result.push_back(alerts_[i]);
    }
    return result;
  }

private:
  static constexpr double WHALE_THRESHOLD = 100; // 100 QP = $60M
  std::vector<Alert> alerts_;
  mutable std::mutex mutex_;
};

} // namespace QuantumPulse::Analytics

#endif // QUANTUMPULSE_PORTFOLIO_V7_H
