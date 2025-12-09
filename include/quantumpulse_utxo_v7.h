#ifndef QUANTUMPULSE_UTXO_V7_H
#define QUANTUMPULSE_UTXO_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace QuantumPulse::UTXO {

// Unspent Transaction Output (Bitcoin-like UTXO model)
struct UTXOutput {
  std::string txid;         // Transaction ID
  int vout;                 // Output index
  std::string address;      // Owner address
  double amount;            // Amount in QP
  std::string scriptPubKey; // Locking script
  int64_t blockHeight;      // Block where confirmed
  bool coinbase;            // Is coinbase output
  int confirmations;        // Number of confirmations
};

// Transaction Input
struct TxInput {
  std::string txid;      // Previous tx ID
  int vout;              // Previous output index
  std::string scriptSig; // Unlocking script
  std::string witness;   // SegWit witness data
  uint32_t sequence;     // Sequence number
};

// Transaction Output
struct TxOutput {
  double amount;            // Amount in QP
  std::string scriptPubKey; // Locking script
  std::string address;      // Recipient address
};

// Full Transaction (Bitcoin-compatible)
struct Transaction {
  std::string txid;           // Transaction ID (hash)
  std::string wtxid;          // Witness txid (SegWit)
  int version;                // Transaction version
  std::vector<TxInput> vin;   // Inputs
  std::vector<TxOutput> vout; // Outputs
  uint32_t locktime;          // Lock time
  int64_t timestamp;          // Creation time
  double fee;                 // Transaction fee
  int size;                   // Size in bytes
  int vsize;                  // Virtual size (SegWit)
  int weight;                 // Weight units
  bool confirmed;             // Is confirmed
  int confirmations;          // Confirmation count
};

// UTXO Set Manager (Bitcoin Core-like)
class UTXOSet final {
public:
  UTXOSet() noexcept {
    Logging::Logger::getInstance().info("UTXO Set initialized", "UTXO", 0);

    // Add genesis UTXO (pre-mined coins)
    addGenesisUTXO();
  }

  // Add UTXO
  void addUTXO(const UTXOutput &utxo) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = utxo.txid + ":" + std::to_string(utxo.vout);
    utxos_[key] = utxo;
    addressUtxos_[utxo.address].insert(key);
  }

  // Spend UTXO (remove from set)
  bool spendUTXO(const std::string &txid, int vout) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = txid + ":" + std::to_string(vout);

    if (!utxos_.count(key)) {
      return false; // UTXO not found
    }

    auto &utxo = utxos_[key];
    addressUtxos_[utxo.address].erase(key);
    utxos_.erase(key);

    return true;
  }

  // Get UTXO
  std::optional<UTXOutput> getUTXO(const std::string &txid,
                                   int vout) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = txid + ":" + std::to_string(vout);

    if (utxos_.count(key)) {
      return utxos_.at(key);
    }
    return std::nullopt;
  }

  // Get all UTXOs for address
  std::vector<UTXOutput>
  getAddressUTXOs(const std::string &address) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UTXOutput> result;

    if (addressUtxos_.count(address)) {
      for (const auto &key : addressUtxos_.at(address)) {
        if (utxos_.count(key)) {
          result.push_back(utxos_.at(key));
        }
      }
    }
    return result;
  }

  // Calculate balance from UTXOs
  double getBalance(const std::string &address) const noexcept {
    auto utxos = getAddressUTXOs(address);
    double balance = 0.0;
    for (const auto &utxo : utxos) {
      balance += utxo.amount;
    }
    return balance;
  }

  // Validate transaction inputs
  bool validateInputs(const Transaction &tx) const noexcept {
    double inputSum = 0.0;

    for (const auto &input : tx.vin) {
      auto utxo = getUTXO(input.txid, input.vout);
      if (!utxo) {
        return false; // Input not found in UTXO set
      }
      inputSum += utxo->amount;
    }

    double outputSum = 0.0;
    for (const auto &output : tx.vout) {
      outputSum += output.amount;
    }

    // Inputs must be >= outputs (difference is fee)
    return inputSum >= outputSum;
  }

  // Get UTXO count
  size_t getUTXOCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return utxos_.size();
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, UTXOutput> utxos_; // key = txid:vout
  std::map<std::string, std::set<std::string>> addressUtxos_;

  void addGenesisUTXO() {
    // Pre-mined coins for founder
    UTXOutput genesis;
    genesis.txid = "genesis_coinbase_000000000000000000000000000000000000";
    genesis.vout = 0;
    genesis.address = "Shankar-Lal-Khati";
    genesis.amount = 2000000.0; // 2 million pre-mined
    genesis.scriptPubKey =
        "OP_DUP OP_HASH160 <pubKeyHash> OP_EQUALVERIFY OP_CHECKSIG";
    genesis.blockHeight = 0;
    genesis.coinbase = true;
    genesis.confirmations = 999999;

    addUTXO(genesis);
  }
};

} // namespace QuantumPulse::UTXO

#endif // QUANTUMPULSE_UTXO_V7_H
