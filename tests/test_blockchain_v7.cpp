/**
 * QuantumPulse Blockchain Unit Tests v7.0
 *
 * Simple test framework without external dependencies (like gtest)
 */

#include "quantumpulse_blockchain_v7.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>

#define TEST(name) void test_##name()
#define RUN_TEST(name)                                                         \
  do {                                                                         \
    std::cout << "Running " << #name << "... ";                                \
    try {                                                                      \
      test_##name();                                                           \
      std::cout << "PASSED" << std::endl;                                      \
      passed++;                                                                \
    } catch (const std::exception &e) {                                        \
      std::cout << "FAILED: " << e.what() << std::endl;                        \
      failed++;                                                                \
    }                                                                          \
  } while (0)

#define EXPECT_TRUE(cond)                                                      \
  if (!(cond))                                                                 \
  throw std::runtime_error("Expected true: " #cond)
#define EXPECT_FALSE(cond)                                                     \
  if (cond)                                                                    \
  throw std::runtime_error("Expected false: " #cond)
#define EXPECT_EQ(a, b)                                                        \
  if ((a) != (b))                                                              \
  throw std::runtime_error("Expected equal: " #a " == " #b)
#define EXPECT_GE(a, b)                                                        \
  if ((a) < (b))                                                               \
  throw std::runtime_error("Expected >= : " #a " >= " #b)
#define EXPECT_GT(a, b)                                                        \
  if ((a) <= (b))                                                              \
  throw std::runtime_error("Expected > : " #a " > " #b)
#define EXPECT_LT(a, b)                                                        \
  if ((a) >= (b))                                                              \
  throw std::runtime_error("Expected < : " #a " < " #b)
#define EXPECT_NE(a, b)                                                        \
  if ((a) == (b))                                                              \
  throw std::runtime_error("Expected != : " #a " != " #b)
#define EXPECT_NO_THROW(expr)                                                  \
  try {                                                                        \
    expr;                                                                      \
  } catch (...) {                                                              \
    throw std::runtime_error("Unexpected throw: " #expr);                      \
  }

int passed = 0;
int failed = 0;

// Test: Price never below minimum ($600,000)
TEST(PriceNeverBelowMin) {
  QuantumPulse::Blockchain::Blockchain bc;
  double price1 = bc.adjustCoinPrice(599999, 0, 0);
  EXPECT_GE(price1, 600000.0);

  double price2 = bc.adjustCoinPrice(600000, 1, 0);
  EXPECT_GE(price2, 600000.0);

  double price3 = bc.adjustCoinPrice(1000000, 100, 0);
  EXPECT_GT(price3, 600000.0);
}

// Test: Data leak prevention
TEST(DataLeakPrevention) {
  QuantumPulse::AI::AIManager ai;
  EXPECT_TRUE(ai.preventDataLeak("secret data", 0));
  EXPECT_TRUE(ai.preventDataLeak("password123", 0));
  EXPECT_TRUE(ai.preventDataLeak("my api_key here", 0));
  EXPECT_FALSE(ai.preventDataLeak("normal transaction data", 0));
}

// Test: No overflow in price calculation
TEST(NoOverflowInPrice) {
  QuantumPulse::Blockchain::Blockchain bc;
  EXPECT_NO_THROW(
      bc.adjustCoinPrice(std::numeric_limits<double>::max() / 3, 1000000, 0));
}

// Test: Mining performance (should complete quickly)
TEST(MiningPerformance) {
  QuantumPulse::Mining::MiningManager mm;

  auto start = std::chrono::high_resolution_clock::now();
  int nonce = 0;
  std::string hash;
  [[maybe_unused]] bool mined =
      mm.mineBlock("test_data", 2, nonce, hash, 0); // Low difficulty for speed
  auto end = std::chrono::high_resolution_clock::now();

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  EXPECT_LT(duration.count(), 5000); // Should complete within 5 seconds
}

// Test: Crypto hashing
TEST(CryptoHashing) {
  QuantumPulse::Crypto::CryptoManager crypto;

  std::string hash1 = crypto.sha3_512_v11("test_data", 0);
  std::string hash2 = crypto.sha3_512_v11("test_data", 0);
  std::string hash3 = crypto.sha3_512_v11("different_data", 0);

  EXPECT_EQ(hash1, hash2); // Same input should give same hash
  EXPECT_NE(hash1, hash3); // Different input should give different hash
  EXPECT_FALSE(hash1.empty());
}

// Test: Key pair generation
TEST(KeyPairGeneration) {
  QuantumPulse::Crypto::CryptoManager crypto;

  auto keyPair = crypto.generateKeyPair(0);
  EXPECT_FALSE(keyPair.publicKey.empty());
  EXPECT_FALSE(keyPair.privateKey.empty());
  EXPECT_EQ(keyPair.multiSignatures.size(), 10); // Should have 10 signatures
}

// Test: Transaction validation
TEST(TransactionValidation) {
  QuantumPulse::Crypto::CryptoManager crypto;

  std::string signature = crypto.signTransaction("tx_data", "private_key", 0);
  EXPECT_FALSE(signature.empty());

  bool verified = crypto.verifyTransaction("tx_id", signature, "sender", 0);
  EXPECT_TRUE(verified);
}

// Test: ZK Proof
TEST(ZKProof) {
  QuantumPulse::Crypto::CryptoManager crypto;

  std::string proof = crypto.zkStarkProve_v11("secret_data", 0);
  EXPECT_FALSE(proof.empty());
  EXPECT_TRUE(proof.find("zk_proof_v11_") != std::string::npos);

  bool verified = crypto.zkStarkVerify_v11(proof, 0);
  EXPECT_TRUE(verified);
}

// Test: Blockchain initialization
TEST(BlockchainInit) {
  QuantumPulse::Blockchain::Blockchain bc;

  EXPECT_GE(bc.getChainLength(),
            16); // Should have genesis blocks for active shards
  EXPECT_TRUE(bc.validateChain());
  EXPECT_TRUE(bc.checkMiningLimit()); // Should be able to mine initially
}

// Test: Premined account privacy (balance hidden from public)
TEST(PreminedAccountBalance) {
  QuantumPulse::Blockchain::Blockchain bc;

  // FOUNDER_WALLET no longer exists - now uses stealth addresses
  // Public queries should NOT reveal premined balance
  auto balance = bc.getBalance("FOUNDER_WALLET", "any_token");
  EXPECT_FALSE(balance.has_value()); // Should be hidden (privacy feature)

  // Non-existent accounts should also return nullopt
  auto randomBalance = bc.getBalance("random_address", "");
  EXPECT_FALSE(randomBalance.has_value());
}

// Test: Block reward calculation
TEST(BlockRewardCalculation) {
  QuantumPulse::Blockchain::Blockchain bc;

  double reward0 = bc.calculateBlockReward(0);
  EXPECT_EQ(reward0, 50.0);

  double reward1 = bc.calculateBlockReward(210000);
  EXPECT_EQ(reward1, 25.0);

  double reward2 = bc.calculateBlockReward(420000);
  EXPECT_EQ(reward2, 12.5);
}

// Test: AI bug scanning
TEST(AIBugScanning) {
  QuantumPulse::AI::AIManager ai;

  // Empty code should be flagged
  EXPECT_TRUE(ai.scanForBugs("", 0));

  // Normal code should pass (may vary based on model)
  bool result = ai.scanForBugs("int main() { return 0; }", 0);
  // Just check it doesn't crash
  (void)result;
}

// Test: Self-healing code
TEST(SelfHealingCode) {
  QuantumPulse::AI::AIManager ai;

  std::string buggyCode = "ptr = nullptr; strcpy(buffer, input);";
  std::string fixedCode = ai.selfHealCode(buggyCode, 0);

  EXPECT_NE(buggyCode, fixedCode);
  EXPECT_TRUE(fixedCode.find("strncpy") != std::string::npos ||
              fixedCode.find("FIXED") != std::string::npos);
}

// Test: Sharding
TEST(Sharding) {
  QuantumPulse::Sharding::ShardingManager sm;

  EXPECT_TRUE(sm.validateShard(0));
  EXPECT_TRUE(sm.validateShard(15)); // ACTIVE_SHARDS = 16, so 0-15 are valid
  EXPECT_FALSE(sm.validateShard(-1));
  EXPECT_FALSE(sm.validateShard(16)); // 16 is invalid (ACTIVE_SHARDS=16)
  EXPECT_EQ(sm.getShardCount(), 16);  // ACTIVE_SHARDS = 16
}

// Test: Upgrade manager
TEST(UpgradeManager) {
  QuantumPulse::Upgrades::UpgradeManager um;

  EXPECT_EQ(um.getVersion(), "7.0.0");

  um.applyUpdate("security_patch_1");
  EXPECT_EQ(um.getVersion(), "7.0.1");

  [[maybe_unused]] bool rolled = um.rollback();
  EXPECT_EQ(um.getVersion(), "7.0.0");

  EXPECT_TRUE(um.checkCompatibility());
}

// Test: Multi-signature validation
TEST(MultiSignature) {
  QuantumPulse::Crypto::CryptoManager crypto;

  std::vector<std::string> validSigs(10, "signature");
  EXPECT_TRUE(crypto.validateMultiSignature(validSigs, 0));

  std::vector<std::string> invalidSigs(5, "signature");
  EXPECT_FALSE(crypto.validateMultiSignature(invalidSigs, 0));
}

int main() {
  std::cout << "\n";
  std::cout
      << "╔══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║     QuantumPulse Unit Tests v7.0                             ║\n";
  std::cout
      << "╚══════════════════════════════════════════════════════════════╝\n\n";

  RUN_TEST(PriceNeverBelowMin);
  RUN_TEST(DataLeakPrevention);
  RUN_TEST(NoOverflowInPrice);
  RUN_TEST(CryptoHashing);
  RUN_TEST(KeyPairGeneration);
  RUN_TEST(TransactionValidation);
  RUN_TEST(ZKProof);
  RUN_TEST(BlockchainInit);
  RUN_TEST(PreminedAccountBalance);
  RUN_TEST(BlockRewardCalculation);
  RUN_TEST(AIBugScanning);
  RUN_TEST(SelfHealingCode);
  RUN_TEST(Sharding);
  RUN_TEST(UpgradeManager);
  RUN_TEST(MultiSignature);
  RUN_TEST(MiningPerformance);

  std::cout << "\n";
  std::cout
      << "══════════════════════════════════════════════════════════════\n";
  std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
  std::cout
      << "══════════════════════════════════════════════════════════════\n\n";

  return failed > 0 ? 1 : 0;
}
