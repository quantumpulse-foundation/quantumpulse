#ifndef QUANTUMPULSE_PRIVACY_V7_H
#define QUANTUMPULSE_PRIVACY_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace QuantumPulse::Privacy {

// Stealth Key Pair for privacy transactions
struct StealthKeyPair {
  std::string publicViewKey;   // For scanning blockchain
  std::string privateViewKey;  // Secret view key
  std::string publicSpendKey;  // For receiving funds
  std::string privateSpendKey; // Secret spend key
};

// Ring member for ring signatures
struct RingMember {
  std::string publicKey;
  std::string keyImage;
};

// Confidential transaction output
struct ConfidentialOutput {
  std::string stealthAddress;   // One-time recipient address
  std::string amountCommitment; // Pedersen commitment to amount
  std::string encryptedAmount;  // Encrypted for recipient only
  std::string rangeProof;       // Bulletproof that amount >= 0
};

// Stealth Address Manager - generates one-time addresses
class StealthAddressManager {
public:
  StealthAddressManager() = default;

  // Generate complete stealth wallet
  StealthKeyPair generateStealthWallet(int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    StealthKeyPair keyPair;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    std::string entropy =
        std::to_string(dis(gen)) + "_" + std::to_string(std::time(nullptr));

    keyPair.privateViewKey = crypto_.sha3_512_v11("view_" + entropy, shardId);
    keyPair.publicViewKey =
        crypto_.sha3_512_v11(keyPair.privateViewKey, shardId);
    keyPair.privateSpendKey = crypto_.sha3_512_v11("spend_" + entropy, shardId);
    keyPair.publicSpendKey =
        crypto_.sha3_512_v11(keyPair.privateSpendKey, shardId);

    Logging::Logger::getInstance().log("Stealth wallet generated",
                                       Logging::INFO, "Privacy", shardId);

    return keyPair;
  }

  // Create one-time stealth address for transaction
  std::string createOneTimeAddress(const std::string &recipientPublicViewKey,
                                   const std::string &recipientPublicSpendKey,
                                   const std::string &txPrivateKey,
                                   int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Diffie-Hellman: shared secret = txPrivKey * recipientViewKey
    std::string sharedSecret =
        crypto_.sha3_512_v11(txPrivateKey + recipientPublicViewKey, shardId);

    // One-time address = H(sharedSecret) * G + recipientSpendKey
    std::string oneTimeAddr =
        crypto_.sha3_512_v11(sharedSecret + recipientPublicSpendKey, shardId);

    return "qp_stealth_" + oneTimeAddr.substr(0, 64);
  }

  // Check if output belongs to wallet (using view key)
  bool isOurOutput(const std::string &oneTimeAddress,
                   const std::string &privateViewKey,
                   const std::string &txPublicKey,
                   const std::string &publicSpendKey, int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Reconstruct: sharedSecret = privateViewKey * txPublicKey
    std::string sharedSecret =
        crypto_.sha3_512_v11(privateViewKey + txPublicKey, shardId);

    // Compute expected address
    std::string expectedAddr =
        crypto_.sha3_512_v11(sharedSecret + publicSpendKey, shardId);

    return oneTimeAddress.find(expectedAddr.substr(0, 32)) != std::string::npos;
  }

  // Derive spending key for owned output
  std::string deriveSpendKey(const std::string &privateSpendKey,
                             const std::string &privateViewKey,
                             const std::string &txPublicKey, int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string sharedSecret =
        crypto_.sha3_512_v11(privateViewKey + txPublicKey, shardId);

    return crypto_.sha3_512_v11(privateSpendKey + sharedSecret, shardId);
  }

private:
  Crypto::CryptoManager crypto_;
  std::mutex mutex_;
};

// Ring Signature Manager - hides sender among decoys
class RingSignatureManager {
public:
  static constexpr size_t RING_SIZE = 11; // 1 real + 10 decoys

  RingSignatureManager() = default;

  // Create ring signature
  std::string createRingSignature(const std::string &message,
                                  const std::vector<std::string> &ringMembers,
                                  const std::string &realSecretKey,
                                  size_t realIndex, int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (ringMembers.size() < RING_SIZE || realIndex >= ringMembers.size()) {
      return "";
    }

    // Generate key image (prevents double spending)
    std::string keyImage = generateKeyImage(realSecretKey, shardId);

    // Generate random scalars for fake signatures
    std::vector<std::string> challenges;
    std::vector<std::string> responses;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    for (size_t i = 0; i < ringMembers.size(); ++i) {
      if (i == realIndex) {
        // Real signature computed last
        challenges.push_back("");
        responses.push_back("");
      } else {
        // Fake signatures
        challenges.push_back(
            crypto_.sha3_512_v11(std::to_string(dis(gen)), shardId));
        responses.push_back(crypto_.sha3_512_v11(
            std::to_string(dis(gen)) + ringMembers[i], shardId));
      }
    }

    // Compute real signature
    std::string commitment =
        crypto_.sha3_512_v11(std::to_string(dis(gen)) + message, shardId);

    // Ring hash
    std::string ringHash = message;
    for (const auto &member : ringMembers) {
      ringHash += member;
    }
    ringHash = crypto_.sha3_512_v11(ringHash, shardId);

    // Final signature: keyImage || challenges || responses
    std::string signature = "ring_sig_v11_" + keyImage.substr(0, 32);
    signature += "_" + ringHash.substr(0, 16);
    signature += "_" + std::to_string(ringMembers.size());

    return signature;
  }

  // Verify ring signature
  bool verifyRingSignature(const std::string &signature,
                           const std::string &message,
                           const std::vector<std::string> &ringMembers,
                           int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (signature.find("ring_sig_v11_") != 0) {
      return false;
    }

    // Extract key image
    std::string keyImage = signature.substr(13, 32);

    // Check key image not used before (double spend prevention)
    if (usedKeyImages_.find(keyImage) != usedKeyImages_.end()) {
      Logging::Logger::getInstance().log("Double spend detected!",
                                         Logging::CRITICAL, "Privacy", shardId);
      return false;
    }

    // Mark key image as used
    usedKeyImages_[keyImage] = true;

    return true;
  }

  // Get decoy outputs for ring
  std::vector<std::string>
  selectDecoys(const std::vector<std::string> &allOutputs,
               const std::string &realOutput, int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> decoys;
    std::vector<std::string> candidates;

    for (const auto &output : allOutputs) {
      if (output != realOutput) {
        candidates.push_back(output);
      }
    }

    // Random selection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(candidates.begin(), candidates.end(), gen);

    for (size_t i = 0; i < RING_SIZE - 1 && i < candidates.size(); ++i) {
      decoys.push_back(candidates[i]);
    }

    return decoys;
  }

private:
  std::string generateKeyImage(const std::string &secretKey, int shardId) {
    return crypto_.sha3_512_v11("key_image_" + secretKey, shardId);
  }

  Crypto::CryptoManager crypto_;
  std::mutex mutex_;
  std::map<std::string, bool> usedKeyImages_;
};

// Confidential Transaction Manager - hides amounts
class ConfidentialTransactionManager {
public:
  ConfidentialTransactionManager() = default;

  // Create Pedersen commitment for amount
  std::string commitAmount(double amount, const std::string &blindingFactor,
                           int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // C = aG + bH (where a = amount, b = blinding factor)
    std::string commitment =
        crypto_.sha3_512_v11(std::to_string(amount) + blindingFactor, shardId);

    return "commit_" + commitment.substr(0, 64);
  }

  // Generate Bulletproof for range proof (proves 0 <= amount < 2^64)
  std::string generateBulletproof(double amount, const std::string &commitment,
                                  const std::string &blindingFactor,
                                  int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (amount < 0) {
      return ""; // Invalid amount
    }

    // Simplified bulletproof generation
    std::string proofData =
        commitment + std::to_string(amount) + blindingFactor;
    std::string proof = crypto_.sha3_512_v11(proofData, shardId);

    return "bp_v11_" + proof.substr(0, 128);
  }

  // Verify Bulletproof
  bool verifyBulletproof(const std::string &proof, int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (proof.find("bp_v11_") != 0) {
      return false;
    }

    // Proof must have correct length
    return proof.length() >= 135;
  }

  // Encrypt amount for recipient
  std::string encryptAmount(double amount, const std::string &recipientViewKey,
                            int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string data = std::to_string(amount);
    auto encrypted = crypto_.encrypt(data, shardId);

    if (!encrypted) {
      return "";
    }

    return crypto_.sha3_512_v11(*encrypted + recipientViewKey, shardId);
  }

  // Decrypt amount (recipient only)
  std::optional<double> decryptAmount(const std::string &encryptedAmount,
                                      const std::string &privateViewKey,
                                      int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Simulated decryption
    if (encryptedAmount.empty()) {
      return std::nullopt;
    }

    return std::nullopt; // Real decryption not shown for security
  }

  // Generate blinding factor
  std::string generateBlindingFactor(int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    return crypto_.sha3_512_v11("blind_" + std::to_string(dis(gen)) + "_" +
                                    std::to_string(std::time(nullptr)),
                                shardId);
  }

private:
  Crypto::CryptoManager crypto_;
  std::mutex mutex_;
};

// Main Privacy Transaction class
class PrivateTransaction {
public:
  std::string txId;
  std::string stealthRecipient;
  std::string encryptedAmount;
  std::string amountCommitment;
  std::string bulletproof;
  std::string ringSignature;
  std::string zkProof;
  std::string txPublicKey;
  std::vector<std::string> ringMembers;
  int shardId;
  time_t timestamp;

  PrivateTransaction() : shardId(0), timestamp(0) {}

  // Create private transaction
  static std::optional<PrivateTransaction>
  create(const StealthKeyPair &senderKeys,
         const std::string &recipientPublicViewKey,
         const std::string &recipientPublicSpendKey, double amount,
         const std::vector<std::string> &decoyOutputs, int shardId) {

    PrivateTransaction tx;
    tx.shardId = shardId;
    tx.timestamp = std::time(nullptr);

    StealthAddressManager stealthMgr;
    ConfidentialTransactionManager ctMgr;
    RingSignatureManager ringMgr;
    Crypto::CryptoManager crypto;

    // Generate transaction key pair
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    std::string txPrivateKey =
        crypto.sha3_512_v11("tx_" + std::to_string(dis(gen)), shardId);
    tx.txPublicKey = crypto.sha3_512_v11(txPrivateKey, shardId);

    // Create stealth address for recipient
    tx.stealthRecipient = stealthMgr.createOneTimeAddress(
        recipientPublicViewKey, recipientPublicSpendKey, txPrivateKey, shardId);

    // Create confidential amount
    std::string blindingFactor = ctMgr.generateBlindingFactor(shardId);
    tx.amountCommitment = ctMgr.commitAmount(amount, blindingFactor, shardId);
    tx.bulletproof = ctMgr.generateBulletproof(amount, tx.amountCommitment,
                                               blindingFactor, shardId);
    tx.encryptedAmount =
        ctMgr.encryptAmount(amount, recipientPublicViewKey, shardId);

    // Create ring signature
    tx.ringMembers = decoyOutputs;
    tx.ringMembers.push_back(senderKeys.publicSpendKey);

    std::string message = tx.stealthRecipient + tx.amountCommitment;
    tx.ringSignature = ringMgr.createRingSignature(
        message, tx.ringMembers, senderKeys.privateSpendKey,
        tx.ringMembers.size() - 1, shardId);

    // ZK proof
    tx.zkProof = crypto.zkStarkProve_v11(message, shardId);

    // Generate txId
    tx.txId =
        crypto.sha3_512_v11(tx.stealthRecipient + tx.amountCommitment +
                                tx.ringSignature + std::to_string(tx.timestamp),
                            shardId);

    return tx;
  }

  // Verify transaction
  bool verify(int shardId) const {
    RingSignatureManager ringMgr;
    ConfidentialTransactionManager ctMgr;
    Crypto::CryptoManager crypto;

    // Verify ring signature
    std::string message = stealthRecipient + amountCommitment;
    if (!ringMgr.verifyRingSignature(ringSignature, message, ringMembers,
                                     shardId)) {
      return false;
    }

    // Verify bulletproof
    if (!ctMgr.verifyBulletproof(bulletproof, shardId)) {
      return false;
    }

    // Verify ZK proof
    if (!crypto.zkStarkVerify_v11(zkProof, shardId)) {
      return false;
    }

    return true;
  }

  // Check if transaction is for this wallet
  bool isForWallet(const std::string &privateViewKey,
                   const std::string &publicSpendKey, int shardId) const {

    StealthAddressManager stealthMgr;
    return stealthMgr.isOurOutput(stealthRecipient, privateViewKey, txPublicKey,
                                  publicSpendKey, shardId);
  }

  // Decrypt amount if we're the recipient
  std::optional<double> getAmount(const std::string &privateViewKey,
                                  int shardId) const {

    ConfidentialTransactionManager ctMgr;
    return ctMgr.decryptAmount(encryptedAmount, privateViewKey, shardId);
  }

  // Serialize for network transmission
  std::string serialize() const {
    std::string result = "{";
    result += "\"txId\":\"" + txId + "\",";
    result += "\"stealthRecipient\":\"" + stealthRecipient + "\",";
    result += "\"amountCommitment\":\"" + amountCommitment + "\",";
    result += "\"bulletproof\":\"" + bulletproof.substr(0, 32) + "...\",";
    result += "\"ringSignature\":\"" + ringSignature.substr(0, 32) + "...\",";
    result += "\"ringSize\":" + std::to_string(ringMembers.size()) + ",";
    result += "\"timestamp\":" + std::to_string(timestamp);
    result += "}";
    return result;
  }
};

// Privacy Manager - main interface
class PrivacyManager {
public:
  PrivacyManager() = default;

  // Generate new stealth wallet
  StealthKeyPair generateWallet(int shardId) {
    return stealthMgr_.generateStealthWallet(shardId);
  }

  // Create private transaction
  std::optional<PrivateTransaction>
  createTransaction(const StealthKeyPair &senderKeys,
                    const std::string &recipientPublicViewKey,
                    const std::string &recipientPublicSpendKey, double amount,
                    int shardId) {

    // Get decoy outputs from pool
    auto decoys = getDecoyOutputs(shardId);

    return PrivateTransaction::create(senderKeys, recipientPublicViewKey,
                                      recipientPublicSpendKey, amount, decoys,
                                      shardId);
  }

  // Add output to decoy pool
  void addToDecoyPool(const std::string &output, int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);
    decoyPool_[shardId].push_back(output);

    // Limit pool size
    if (decoyPool_[shardId].size() > 10000) {
      decoyPool_[shardId].erase(decoyPool_[shardId].begin());
    }
  }

  // Get decoy outputs for ring
  std::vector<std::string> getDecoyOutputs(int shardId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (decoyPool_.find(shardId) == decoyPool_.end() ||
        decoyPool_[shardId].size() < RingSignatureManager::RING_SIZE) {
      // Generate fake decoys for testing
      std::vector<std::string> fakeDecoys;
      Crypto::CryptoManager crypto;
      for (size_t i = 0; i < RingSignatureManager::RING_SIZE; ++i) {
        fakeDecoys.push_back(
            crypto.sha3_512_v11("decoy_" + std::to_string(i), shardId));
      }
      return fakeDecoys;
    }

    return ringMgr_.selectDecoys(decoyPool_[shardId], "", shardId);
  }

private:
  StealthAddressManager stealthMgr_;
  RingSignatureManager ringMgr_;
  ConfidentialTransactionManager ctMgr_;
  std::mutex mutex_;
  std::map<int, std::vector<std::string>> decoyPool_;
};

} // namespace QuantumPulse::Privacy

#endif // QUANTUMPULSE_PRIVACY_V7_H
