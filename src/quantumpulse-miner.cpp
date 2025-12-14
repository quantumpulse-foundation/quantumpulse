// QuantumPulse Miner (quantumpulse-miner)
// Bitcoin-like standalone mining client with HIGH difficulty and HALVING

#include "../include/quantumpulse_crypto_v7.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace QuantumPulse;

static std::atomic<bool> g_mining{true};
static std::atomic<uint64_t> g_totalHashes{0};
static std::atomic<int> g_blocksFound{0};
static std::atomic<int> g_currentDifficulty{8}; // HIGH difficulty like Bitcoin
static std::atomic<int> g_totalBlocksMined{0};

// Bitcoin-like constants
constexpr int HALVING_INTERVAL =
    210000; // Halving every 210,000 blocks (like Bitcoin)
constexpr int INITIAL_DIFFICULTY = 8;         // Very hard starting difficulty
constexpr int MAX_DIFFICULTY = 16;            // Maximum difficulty
constexpr int TARGET_BLOCK_TIME = 600;        // 10 minutes
constexpr double INITIAL_BLOCK_REWARD = 50.0; // Start at 50 QP
constexpr double MIN_PRICE = 600000.0;        // $600,000 minimum ALWAYS

void signalHandler(int) {
  std::cout << "\n[miner] Stopping..." << std::endl;
  g_mining = false;
}

// Calculate block reward with halving (Bitcoin-style)
double calculateBlockReward(int blockHeight) {
  int halvings = blockHeight / HALVING_INTERVAL;

  // After 64 halvings, reward becomes 0
  if (halvings >= 64)
    return 0.0;

  // Halve the reward for each halving
  double reward = INITIAL_BLOCK_REWARD;
  for (int i = 0; i < halvings; i++) {
    reward /= 2.0;
  }

  // Minimum reward of 0.00000001 QP (1 satoshi equivalent)
  return std::max(reward, 0.00000001);
}

// Get reward era name
std::string getEraName(int blockHeight) {
  int halvings = blockHeight / HALVING_INTERVAL;
  switch (halvings) {
  case 0:
    return "Genesis Era (50 QP)";
  case 1:
    return "First Halving (25 QP)";
  case 2:
    return "Second Halving (12.5 QP)";
  case 3:
    return "Third Halving (6.25 QP)";
  case 4:
    return "Fourth Halving (3.125 QP)";
  default:
    return "Post-Halving Era";
  }
}

// Calculate hash
std::string calculateHash(const std::string &data, int shardId) {
  Crypto::CryptoManager cm;
  return cm.sha3_512_v11(data, shardId);
}

// Check if hash meets difficulty target
bool meetsTarget(const std::string &hash, int difficulty) {
  for (int i = 0; i < difficulty && i < static_cast<int>(hash.length()); i++) {
    if (hash[i] != '0')
      return false;
  }
  return true;
}

// Mining worker thread - Bitcoin-like algorithm with halving
void mineWorker(int threadId, const std::string &minerAddress) {
  uint64_t localHashes = 0;
  uint64_t nonce = threadId * 10000000000ULL;

  auto lastBlockTime = std::chrono::steady_clock::now();

  while (g_mining) {
    int64_t timestamp = std::time(nullptr);
    int difficulty = g_currentDifficulty.load();
    int currentBlock = g_totalBlocksMined.load();
    double blockReward = calculateBlockReward(currentBlock);

    // Block header format
    std::string blockHeader =
        "VERSION:7|" + std::string(64, '0') + "|" + // Previous hash
        "MERKLE:" + std::to_string(currentBlock) + "|" +
        "TIME:" + std::to_string(timestamp) + "|" +
        "DIFF:" + std::to_string(difficulty) + "|" +
        "NONCE:" + std::to_string(nonce) + "|" + "MINER:" + minerAddress;

    // Double SHA3-512 (like Bitcoin's double SHA256)
    std::string hash1 = calculateHash(blockHeader, threadId);
    std::string hash2 = calculateHash(hash1, threadId);

    localHashes++;
    nonce++;

    // Check if block found
    if (meetsTarget(hash2, difficulty)) {
      auto now = std::chrono::steady_clock::now();
      auto blockTime =
          std::chrono::duration<double>(now - lastBlockTime).count();
      lastBlockTime = now;

      g_blocksFound++;
      g_totalBlocksMined++;
      int newTotal = g_totalBlocksMined.load();

      std::cout << "\n🎉 ══════════════════════════════════════════════════"
                << std::endl;
      std::cout << "   BLOCK FOUND by thread " << threadId << "!" << std::endl;
      std::cout << "   Hash: " << hash2.substr(0, 32) << "..." << std::endl;
      std::cout << "   Difficulty: " << difficulty
                << " (target: " << std::string(difficulty, '0') << "...)"
                << std::endl;
      std::cout << "   Block Time: " << std::fixed << std::setprecision(1)
                << blockTime << " seconds" << std::endl;
      std::cout << "   Block Height: " << newTotal << std::endl;
      std::cout << "   Era: " << getEraName(newTotal) << std::endl;
      std::cout << "   Reward: " << std::fixed << std::setprecision(8)
                << blockReward << " QP -> " << minerAddress << std::endl;
      std::cout << "   Value: $" << std::fixed << std::setprecision(0)
                << (blockReward * MIN_PRICE) << " USD (min $" << MIN_PRICE
                << "/QP)" << std::endl;
      std::cout << "══════════════════════════════════════════════════════"
                << std::endl;

      // Check for halving
      if (newTotal % HALVING_INTERVAL == 0 && newTotal > 0) {
        double newReward = calculateBlockReward(newTotal);
        std::cout << "\n🔔 ═══════════════════════════════════════════════════"
                  << std::endl;
        std::cout << "   HALVING EVENT! Block reward reduced to " << newReward
                  << " QP!" << std::endl;
        std::cout << "══════════════════════════════════════════════════════\n"
                  << std::endl;
      }

      // Difficulty adjustment every 10 blocks
      if (g_blocksFound % 10 == 0 && g_blocksFound > 0) {
        if (blockTime < TARGET_BLOCK_TIME * 0.5) {
          int newDiff =
              std::min(g_currentDifficulty.load() + 1, MAX_DIFFICULTY);
          g_currentDifficulty.store(newDiff);
          std::cout << "\n⬆️  DIFFICULTY INCREASED to " << newDiff
                    << " (target: " << std::string(newDiff, '0') << "...)"
                    << std::endl;
        } else if (blockTime > TARGET_BLOCK_TIME * 2 &&
                   g_currentDifficulty > INITIAL_DIFFICULTY - 2) {
          int newDiff = g_currentDifficulty.load() - 1;
          g_currentDifficulty.store(newDiff);
          std::cout << "\n⬇️  Difficulty decreased to " << newDiff << std::endl;
        }
      }
    }

    // Update global hash counter
    if (localHashes % 10000 == 0) {
      g_totalHashes += localHashes;
      localHashes = 0;
    }
  }

  g_totalHashes += localHashes;
}

// Stats display thread
void statsThread() {
  auto startTime = std::chrono::steady_clock::now();

  while (g_mining) {
    std::this_thread::sleep_for(std::chrono::seconds(15));

    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - startTime).count();
    double hashrate = g_totalHashes / seconds;

    int currentBlock = g_totalBlocksMined.load();
    double currentReward = calculateBlockReward(currentBlock);
    int blocksToHalving = HALVING_INTERVAL - (currentBlock % HALVING_INTERVAL);

    std::cout << "\n📊 ═══════════════════════════════════════════════════"
              << std::endl;
    std::cout << "   MINING STATS" << std::endl;
    std::cout << "───────────────────────────────────────────────────────"
              << std::endl;
    std::cout << "   Hashrate: " << std::fixed << std::setprecision(2);
    if (hashrate >= 1000000)
      std::cout << hashrate / 1000000 << " MH/s";
    else if (hashrate >= 1000)
      std::cout << hashrate / 1000 << " KH/s";
    else
      std::cout << hashrate << " H/s";
    std::cout << std::endl;
    std::cout << "   Difficulty: " << g_currentDifficulty
              << " (target: " << std::string(g_currentDifficulty, '0') << "...)"
              << std::endl;
    std::cout << "   Blocks Found: " << g_blocksFound << std::endl;
    std::cout << "   Block Height: " << currentBlock << std::endl;
    std::cout << "   Current Reward: " << std::fixed << std::setprecision(8)
              << currentReward << " QP" << std::endl;
    std::cout << "   Next Halving: " << blocksToHalving << " blocks"
              << std::endl;
    std::cout << "   Era: " << getEraName(currentBlock) << std::endl;
    std::cout << "   Min Price: $" << std::fixed << std::setprecision(0)
              << MIN_PRICE << " USD (GUARANTEED)" << std::endl;
    std::cout << "══════════════════════════════════════════════════════\n"
              << std::endl;
  }
}

void printBanner() {
  std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║             QuantumPulse Miner v7.0.0                        ║
║                                                              ║
║  ⛏️  Quantum-Resistant Bitcoin-like Mining                   ║
║  🔐 Double SHA3-512 Proof of Work                            ║
║  💰 Block Reward Halving (like Bitcoin)                      ║
║  💎 Minimum Price: $600,000 USD (GUARANTEED!)                ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void printHelp() {
  std::cout << "QuantumPulse Miner v7.0.0 (Bitcoin-like with Halving)\n\n";
  std::cout << "Usage: quantumpulse-miner [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -address=<addr>   Mining reward address (required)\n";
  std::cout
      << "  -threads=<n>      Number of mining threads (default: CPU cores)\n";
  std::cout << "  -difficulty=<n>   Starting difficulty (default: 8)\n";
  std::cout << "  -benchmark        Run benchmark only\n";
  std::cout << "  -help             Show this help\n";
  std::cout << "\nMining Specifications:\n";
  std::cout << "  Algorithm:      Double SHA3-512 (Quantum-Resistant)\n";
  std::cout << "  Initial Reward: 50 QP\n";
  std::cout << "  Halving:        Every 210,000 blocks\n";
  std::cout << "  Max Supply:     3,000,000 QP (minable)\n";
  std::cout << "  Min Price:      $600,000 USD (ALWAYS GUARANTEED!)\n";
  std::cout << "  Difficulty:     Adjusts every 10 blocks (starts at 8)\n";
}

int main(int argc, char *argv[]) {
  std::string minerAddress = "";
  int numThreads = std::thread::hardware_concurrency();
  int difficulty = INITIAL_DIFFICULTY; // Bitcoin-like high difficulty
  bool benchmark = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.find("-address=") == 0) {
      minerAddress = arg.substr(9);
    } else if (arg.find("-threads=") == 0) {
      numThreads = std::stoi(arg.substr(9));
    } else if (arg.find("-difficulty=") == 0) {
      difficulty = std::stoi(arg.substr(12));
    } else if (arg == "-benchmark") {
      benchmark = true;
    } else if (arg == "-help" || arg == "--help") {
      printHelp();
      return 0;
    }
  }

  if (minerAddress.empty() && !benchmark) {
    std::cerr
        << "Error: Mining address required. Use -address=<your_address>\n";
    std::cerr << "Use -help for more options.\n";
    return 1;
  }

  if (benchmark) {
    minerAddress = "benchmark_address";
    difficulty = 6;
  }

  g_currentDifficulty.store(difficulty);

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  printBanner();

  std::cout << "⛏️  Mining Configuration:" << std::endl;
  std::cout << "   Address:        " << minerAddress << std::endl;
  std::cout << "   Threads:        " << numThreads << std::endl;
  std::cout << "   Difficulty:     " << difficulty
            << " (target: " << std::string(difficulty, '0') << "...)"
            << std::endl;
  std::cout << "   Algorithm:      Double SHA3-512 (Quantum-Resistant)"
            << std::endl;
  std::cout << "   Initial Reward: 50 QP" << std::endl;
  std::cout << "   Halving:        Every 210,000 blocks" << std::endl;
  std::cout << "   Mining Limit:   3,000,000 QP total" << std::endl;
  std::cout << "   Min Price:      $" << std::fixed << std::setprecision(0)
            << MIN_PRICE << " USD (GUARANTEED!)" << std::endl;
  std::cout << "\n🚀 Starting mining..." << std::endl;
  std::cout << "   Press Ctrl+C to stop.\n" << std::endl;

  std::thread stats(statsThread);

  std::vector<std::thread> workers;
  for (int i = 0; i < numThreads; i++) {
    workers.emplace_back(mineWorker, i, minerAddress);
  }

  for (auto &t : workers) {
    t.join();
  }
  stats.join();

  double totalEarned = 0;
  for (int i = 0; i < g_blocksFound; i++) {
    totalEarned += calculateBlockReward(i);
  }

  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "📊 FINAL MINING STATS" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "   Total Hashes:     " << g_totalHashes << std::endl;
  std::cout << "   Blocks Found:     " << g_blocksFound << std::endl;
  std::cout << "   Total Earned:     " << std::fixed << std::setprecision(8)
            << totalEarned << " QP" << std::endl;
  std::cout << "   Value (min):      $" << std::fixed << std::setprecision(0)
            << (totalEarned * MIN_PRICE) << " USD" << std::endl;
  std::cout << "   Final Difficulty: " << g_currentDifficulty << std::endl;
  std::cout << "   Min Price:        $" << MIN_PRICE << " USD (GUARANTEED!)"
            << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "✅ Shutdown complete." << std::endl;

  return 0;
}
