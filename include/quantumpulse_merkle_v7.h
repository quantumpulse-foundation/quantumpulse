#ifndef QUANTUMPULSE_MERKLE_V7_H
#define QUANTUMPULSE_MERKLE_V7_H

#include "quantumpulse_crypto_v7.h"
#include <string>
#include <vector>

namespace QuantumPulse::Merkle {

// Merkle Tree for block transactions (Bitcoin-like)
class MerkleTree final {
public:
  MerkleTree() = default;

  // Build merkle tree from transaction IDs
  std::string buildTree(const std::vector<std::string> &txids) noexcept {
    if (txids.empty()) {
      return sha3Hash("");
    }

    if (txids.size() == 1) {
      return txids[0];
    }

    std::vector<std::string> current = txids;

    // Duplicate last if odd number
    if (current.size() % 2 != 0) {
      current.push_back(current.back());
    }

    while (current.size() > 1) {
      std::vector<std::string> next;

      for (size_t i = 0; i < current.size(); i += 2) {
        std::string combined = current[i] + current[i + 1];
        next.push_back(sha3Hash(combined));
      }

      current = next;

      // Duplicate last if odd
      if (current.size() > 1 && current.size() % 2 != 0) {
        current.push_back(current.back());
      }
    }

    root_ = current[0];
    return root_;
  }

  // Get merkle root
  std::string getRoot() const noexcept { return root_; }

  // Generate merkle proof for transaction
  std::vector<std::pair<std::string, bool>>
  getProof(const std::vector<std::string> &txids, size_t index) noexcept {

    std::vector<std::pair<std::string, bool>> proof;

    if (txids.empty() || index >= txids.size()) {
      return proof;
    }

    std::vector<std::string> current = txids;
    if (current.size() % 2 != 0) {
      current.push_back(current.back());
    }

    size_t idx = index;

    while (current.size() > 1) {
      // Get sibling
      size_t siblingIdx = (idx % 2 == 0) ? idx + 1 : idx - 1;
      bool isRight = (idx % 2 == 0);

      proof.push_back({current[siblingIdx], isRight});

      // Move to parent level
      std::vector<std::string> next;
      for (size_t i = 0; i < current.size(); i += 2) {
        std::string combined = current[i] + current[i + 1];
        next.push_back(sha3Hash(combined));
      }

      current = next;
      idx = idx / 2;

      if (current.size() > 1 && current.size() % 2 != 0) {
        current.push_back(current.back());
      }
    }

    return proof;
  }

  // Verify merkle proof (SPV verification)
  bool verifyProof(const std::string &txid,
                   const std::vector<std::pair<std::string, bool>> &proof,
                   const std::string &root) const noexcept {
    std::string current = txid;

    for (const auto &[sibling, isRight] : proof) {
      if (isRight) {
        current = sha3Hash(current + sibling);
      } else {
        current = sha3Hash(sibling + current);
      }
    }

    return current == root;
  }

  // Compute witness commitment (SegWit)
  std::string
  computeWitnessCommitment(const std::vector<std::string> &wtxids) noexcept {

    std::string witnessRoot = buildTree(wtxids);
    std::string witnessReserved(64, '0'); // Witness reserved value

    return sha3Hash(witnessRoot + witnessReserved);
  }

private:
  std::string root_;
  mutable Crypto::CryptoManager crypto_;

  std::string sha3Hash(const std::string &data) const {
    return crypto_.sha3_512_v11(data, 0);
  }
};

// Block Header structure (Bitcoin-like)
struct BlockHeader {
  int version;
  std::string prevBlockHash;
  std::string merkleRoot;
  int64_t timestamp;
  uint32_t bits; // Difficulty target
  uint32_t nonce;

  // SegWit additions
  std::string witnessCommitment;

  std::string getHash() const {
    Crypto::CryptoManager cm;
    std::string data = std::to_string(version) + prevBlockHash + merkleRoot +
                       std::to_string(timestamp) + std::to_string(bits) +
                       std::to_string(nonce);
    // Double hash like Bitcoin
    std::string hash1 = cm.sha3_512_v11(data, 0);
    return cm.sha3_512_v11(hash1, 0);
  }
};

} // namespace QuantumPulse::Merkle

#endif // QUANTUMPULSE_MERKLE_V7_H
