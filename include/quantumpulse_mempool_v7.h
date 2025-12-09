#ifndef QUANTUMPULSE_MEMPOOL_V7_H
#define QUANTUMPULSE_MEMPOOL_V7_H

#include "quantumpulse_utxo_v7.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <queue>
#include <string>

namespace QuantumPulse::Mempool {

// Mempool entry
struct MempoolEntry {
  UTXO::Transaction tx;
  int64_t entryTime;
  double feeRate;      // Fee per vbyte
  int ancestorCount;   // Number of ancestor txs
  int descendantCount; // Number of descendant txs
  double modifiedFee;  // Fee after reordering
  int height;          // Block height when added
};

// Transaction Mempool (Bitcoin Core-like)
class TransactionMempool final {
public:
  TransactionMempool(size_t maxSize = 300 * 1000000) noexcept // 300 MB default
      : maxSize_(maxSize) {
    Logging::Logger::getInstance().info("Mempool initialized (max " +
                                            std::to_string(maxSize / 1000000) +
                                            " MB)",
                                        "Mempool", 0);
  }

  // Add transaction to mempool
  bool addTransaction(const UTXO::Transaction &tx) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (transactions_.count(tx.txid)) {
      return false; // Already in mempool
    }

    // Check mempool size
    if (currentSize_ + tx.size > maxSize_) {
      evictLowFeeTxs(tx.size);
    }

    MempoolEntry entry;
    entry.tx = tx;
    entry.entryTime = std::time(nullptr);
    entry.feeRate = tx.fee / tx.vsize;
    entry.ancestorCount = 0;
    entry.descendantCount = 0;
    entry.modifiedFee = tx.fee;
    entry.height = currentHeight_;

    transactions_[tx.txid] = entry;
    currentSize_ += tx.size;

    // Add to fee-sorted index
    feeIndex_.push({tx.txid, entry.feeRate});

    Logging::Logger::getInstance().info(
        "TX added to mempool: " + tx.txid.substr(0, 16) + "...", "Mempool", 0);

    return true;
  }

  // Remove transaction (after mining)
  bool removeTransaction(const std::string &txid) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!transactions_.count(txid)) {
      return false;
    }

    currentSize_ -= transactions_[txid].tx.size;
    transactions_.erase(txid);

    return true;
  }

  // Get transactions for block template (highest fee first)
  std::vector<UTXO::Transaction>
  getBlockTemplate(size_t maxWeight = 4000000) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<UTXO::Transaction> result;
    size_t currentWeight = 0;

    // Sort by fee rate
    std::vector<MempoolEntry> sorted;
    for (const auto &[txid, entry] : transactions_) {
      sorted.push_back(entry);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const MempoolEntry &a, const MempoolEntry &b) {
                return a.feeRate > b.feeRate;
              });

    for (const auto &entry : sorted) {
      if (currentWeight + entry.tx.weight <= maxWeight) {
        result.push_back(entry.tx);
        currentWeight += entry.tx.weight;
      }
    }

    return result;
  }

  // Get transaction from mempool
  std::optional<UTXO::Transaction>
  getTransaction(const std::string &txid) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (transactions_.count(txid)) {
      return transactions_.at(txid).tx;
    }
    return std::nullopt;
  }

  // Check if transaction in mempool
  bool hasTransaction(const std::string &txid) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return transactions_.count(txid) > 0;
  }

  // Get mempool info
  size_t getSize() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return transactions_.size();
  }

  size_t getBytes() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSize_;
  }

  double getTotalFee() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (const auto &[txid, entry] : transactions_) {
      total += entry.tx.fee;
    }
    return total;
  }

  // Get mempool stats
  std::map<std::string, double> getStats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, double> stats;
    stats["size"] = transactions_.size();
    stats["bytes"] = currentSize_;
    stats["usage"] = (double)currentSize_ / maxSize_ * 100;
    stats["maxmempool"] = maxSize_;
    return stats;
  }

  void setHeight(int height) noexcept { currentHeight_ = height; }

private:
  mutable std::mutex mutex_;
  std::map<std::string, MempoolEntry> transactions_;
  size_t maxSize_;
  size_t currentSize_{0};
  int currentHeight_{0};

  struct FeeEntry {
    std::string txid;
    double feeRate;
    bool operator<(const FeeEntry &other) const {
      return feeRate < other.feeRate;
    }
  };
  std::priority_queue<FeeEntry> feeIndex_;

  void evictLowFeeTxs(size_t needed) {
    // Evict lowest fee transactions until we have space
    std::vector<std::pair<std::string, double>> sorted;
    for (const auto &[txid, entry] : transactions_) {
      sorted.push_back({txid, entry.feeRate});
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) { return a.second < b.second; });

    size_t freed = 0;
    for (const auto &[txid, _] : sorted) {
      if (freed >= needed)
        break;
      freed += transactions_[txid].tx.size;
      transactions_.erase(txid);
    }
  }
};

} // namespace QuantumPulse::Mempool

#endif // QUANTUMPULSE_MEMPOOL_V7_H
