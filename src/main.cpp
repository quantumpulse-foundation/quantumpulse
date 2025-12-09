#include "quantumpulse_blockchain_v7.h"
#include <cstdlib>
#include <iostream>
#include <string>

void printUsage() {
  std::cout << "QuantumPulse Cryptocurrency v7.0\n";
  std::cout << "================================\n";
  std::cout << "Usage: quantumpulse_v7 [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --node          Start as a network node\n";
  std::cout << "  --mine          Start mining\n";
  std::cout << "  --shard <id>    Specify shard ID (0-2047)\n";
  std::cout << "  --config <path> Path to config file\n";
  std::cout << "  --update-ai     Update AI model\n";
  std::cout << "  --audit         Run blockchain audit\n";
  std::cout << "  --info          Show blockchain info\n";
  std::cout << "  --help          Show this help\n";
}

void printBlockchainInfo(QuantumPulse::Blockchain::Blockchain &blockchain) {
  std::cout << "\n=== QuantumPulse Blockchain Info ===\n";
  std::cout << "Version: 7.0.0\n";
  std::cout << "Chain Length: " << blockchain.getChainLength() << " blocks\n";
  std::cout << "Total Mined Coins: " << blockchain.getTotalMinedCoins()
            << " QP\n";
  std::cout << "Mining Limit: 3,000,000 QP\n";
  std::cout << "Premined Coins: 2,000,000 QP (Shankar-Lal-Khati)\n";
  std::cout << "Minimum Coin Price: $600,000 USD\n";
  std::cout << "Total Coins: 5,000,000 QP\n";
  std::cout << "Shards: 2048\n";
  std::cout << "====================================\n\n";
}

int main(int argc, char *argv[]) {
  try {
    std::cout << "\n";
    std::cout
        << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     QuantumPulse Cryptocurrency v7.0                       "
                 "   ║\n";
    std::cout << "║     Secure • Fast • AI-Powered • Quantum-Resistant         "
                 "   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════"
                 "══╝\n\n";

    if (argc < 2) {
      printUsage();
      return 0;
    }

    // Parse command line arguments
    bool startNode = false;
    bool startMining = false;
    bool updateAI = false;
    bool runAudit = false;
    bool showInfo = false;
    int shardId = 0;
    std::string configPath = "config/quantumpulse_config_v7.json";

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--node")
        startNode = true;
      else if (arg == "--mine")
        startMining = true;
      else if (arg == "--update-ai")
        updateAI = true;
      else if (arg == "--audit")
        runAudit = true;
      else if (arg == "--info")
        showInfo = true;
      else if (arg == "--shard" && i + 1 < argc)
        shardId = std::atoi(argv[++i]);
      else if (arg == "--config" && i + 1 < argc)
        configPath = argv[++i];
      else if (arg == "--help") {
        printUsage();
        return 0;
      }
    }

    // Initialize blockchain
    std::cout << "Initializing QuantumPulse Blockchain...\n\n";
    QuantumPulse::Blockchain::Blockchain blockchain;

    if (showInfo) {
      printBlockchainInfo(blockchain);
    }

    if (updateAI) {
      std::cout << "Updating AI Model...\n";
      blockchain.getAIManager().selfUpdate();
      double accuracy =
          blockchain.getAIManager().evaluateModelPerformance(shardId);
      std::cout << "AI Model updated. Accuracy: " << accuracy << "%\n\n";
    }

    if (runAudit) {
      std::cout << "Running Blockchain Audit...\n";
      blockchain.audit();
      bool valid = blockchain.validateChain();
      std::cout << "Chain Validation: " << (valid ? "PASSED" : "FAILED")
                << "\n\n";
    }

    if (startMining) {
      std::cout << "Starting Mining on Shard " << shardId << "...\n";

      if (!blockchain.checkMiningLimit()) {
        std::cout
            << "Mining limit reached. All 3,000,000 coins have been mined.\n";
        return 0;
      }

      // Create a test transaction
      auto keyPair = blockchain.getCryptoManager().generateKeyPair(shardId);

      // Mine a block
      std::vector<QuantumPulse::Blockchain::Transaction> txs;
      double reward =
          blockchain.calculateBlockReward(blockchain.getChainLength());

      QuantumPulse::Blockchain::Block block(
          "prev_hash_" + std::to_string(blockchain.getChainLength()),
          std::move(txs),
          4, // difficulty
          reward, shardId, blockchain.getCryptoManager());

      std::cout << "Mining block with reward: " << reward << " QP\n";
      bool mined = block.mine(blockchain.getMiningManager(),
                              blockchain.getCryptoManager());

      if (mined) {
        blockchain.addBlock(block);
        std::cout << "Block mined successfully!\n";
        std::cout << "Hash: " << block.hash.substr(0, 32) << "...\n";
        std::cout << "Nonce: " << block.nonce << "\n\n";
      } else {
        std::cout << "Mining failed or limit reached.\n\n";
      }
    }

    if (startNode) {
      std::cout << "Starting Network Node on Shard " << shardId << "...\n";
      std::cout << "Node is running. Press Ctrl+C to stop.\n\n";

      // Simulate node operations
      while (true) {
        blockchain.getNetworkManager().syncChain(shardId);
        blockchain.getNetworkManager().discoverPeers(shardId);
        blockchain.getShardingManager().syncShards();

        // Sleep for 60 seconds
        std::this_thread::sleep_for(std::chrono::seconds(60));
      }
    }

    std::cout << "QuantumPulse operation completed successfully.\n";
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
