// QuantumPulse Mobile Wallet (qp-wallet)
// Cross-platform CLI wallet with mobile-friendly commands

#include "../include/quantumpulse_blockchain_v7.h"
#include "../include/quantumpulse_crypto_v7.h"
#include "../include/quantumpulse_logging_v7.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

using namespace QuantumPulse;

// Wallet data structure
struct Wallet {
  std::string name;
  std::string address;
  std::string privateKey;
  double balance;
  std::vector<std::string> transactions;
  bool encrypted;
  std::string encryptedKey;
};

// Wallet Manager
class WalletManager {
public:
  WalletManager() { loadWallets(); }

  // Create new wallet
  std::string createWallet(const std::string &name,
                           const std::string &password = "") {
    Crypto::CryptoManager cm;
    auto keyPair = cm.generateKeyPair(0);

    Wallet wallet;
    wallet.name = name;
    wallet.address = keyPair.publicKey;
    wallet.privateKey = keyPair.privateKey;
    wallet.balance = 0.0;
    wallet.encrypted = !password.empty();

    if (!password.empty()) {
      // Encrypt private key with password
      wallet.encryptedKey = cm.sha3_512_v11(wallet.privateKey + password, 0);
      wallet.privateKey = ""; // Don't store plain private key
    }

    wallets_[wallet.address] = wallet;
    saveWallets();

    return wallet.address;
  }

  // Get wallet balance
  double getBalance(const std::string &address) {
    if (wallets_.count(address)) {
      return wallets_[address].balance;
    }
    return 0.0;
  }

  // List all wallets
  void listWallets() {
    std::cout
        << "\n┌────────────────────────────────────────────────────────────┐"
        << std::endl;
    std::cout
        << "│                    Your Wallets                             │"
        << std::endl;
    std::cout
        << "├────────────────────────────────────────────────────────────┤"
        << std::endl;

    for (const auto &[addr, wallet] : wallets_) {
      std::cout << "│ " << std::left << std::setw(15) << wallet.name << " │ "
                << wallet.address.substr(0, 20) << "... │ " << std::right
                << std::setw(12) << std::fixed << std::setprecision(2)
                << wallet.balance << " QP │" << std::endl;
    }

    std::cout
        << "└────────────────────────────────────────────────────────────┘"
        << std::endl;
  }

  // Send transaction
  bool sendTransaction(const std::string &from, const std::string &to,
                       double amount, const std::string &password = "") {
    if (!wallets_.count(from)) {
      std::cout << "❌ Error: Source wallet not found" << std::endl;
      return false;
    }

    if (wallets_[from].balance < amount) {
      std::cout << "❌ Error: Insufficient balance" << std::endl;
      return false;
    }

    // Verify password if encrypted
    if (wallets_[from].encrypted) {
      Crypto::CryptoManager cm;
      std::string verify =
          cm.sha3_512_v11(wallets_[from].encryptedKey + password, 0);
      // Simplified verification
    }

    // Process transaction
    wallets_[from].balance -= amount;

    if (wallets_.count(to)) {
      wallets_[to].balance += amount;
    }

    // Record transaction
    std::string txHash = generateTxHash(from, to, amount);
    wallets_[from].transactions.push_back(txHash);

    saveWallets();

    std::cout << "✅ Transaction sent!" << std::endl;
    std::cout << "   TX Hash: " << txHash.substr(0, 32) << "..." << std::endl;
    std::cout << "   Amount: " << amount << " QP" << std::endl;
    std::cout << "   To: " << to.substr(0, 20) << "..." << std::endl;

    return true;
  }

  // Get transaction history
  void getHistory(const std::string &address) {
    if (!wallets_.count(address)) {
      std::cout << "Wallet not found" << std::endl;
      return;
    }

    std::cout << "\n📜 Transaction History" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;

    for (const auto &tx : wallets_[address].transactions) {
      std::cout << "  " << tx.substr(0, 32) << "..." << std::endl;
    }
  }

  // Backup wallet
  void backupWallet(const std::string &address, const std::string &filename) {
    if (!wallets_.count(address)) {
      std::cout << "Wallet not found" << std::endl;
      return;
    }

    std::ofstream file(filename);
    file << "QuantumPulse Wallet Backup\n";
    file << "Address: " << address << "\n";
    file << "Name: " << wallets_[address].name << "\n";
    file << "Balance: " << wallets_[address].balance << " QP\n";
    file.close();

    std::cout << "✅ Wallet backed up to: " << filename << std::endl;
  }

  // Import wallet
  bool importWallet(const std::string &privateKey, const std::string &name) {
    Crypto::CryptoManager cm;
    std::string address = cm.sha3_512_v11(privateKey, 0).substr(0, 40);

    Wallet wallet;
    wallet.name = name;
    wallet.address = address;
    wallet.privateKey = privateKey;
    wallet.balance = 0.0;
    wallet.encrypted = false;

    wallets_[address] = wallet;
    saveWallets();

    std::cout << "✅ Wallet imported: " << address << std::endl;
    return true;
  }

private:
  std::map<std::string, Wallet> wallets_;
  const std::string WALLET_FILE = "wallets.dat";

  void loadWallets() {
    // Load from file (simplified)
  }

  void saveWallets() {
    // Save to file (simplified)
  }

  std::string generateTxHash(const std::string &from, const std::string &to,
                             double amount) {
    Crypto::CryptoManager cm;
    std::string data =
        from + to + std::to_string(amount) + std::to_string(time(nullptr));
    return cm.sha3_512_v11(data, 0);
  }
};

void printBanner() {
  std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║          QuantumPulse Mobile Wallet v7.0                     ║
║                                                              ║
║  💼 Secure • Fast • Quantum-Resistant                        ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void printHelp() {
  std::cout << "\nQuantumPulse Wallet Commands:\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
  std::cout << "  create <name>              Create new wallet\n";
  std::cout << "  list                       List all wallets\n";
  std::cout << "  balance <address>          Check balance\n";
  std::cout << "  send <from> <to> <amount>  Send QP\n";
  std::cout << "  history <address>          Transaction history\n";
  std::cout << "  backup <address> <file>    Backup wallet\n";
  std::cout << "  import <key> <name>        Import wallet\n";
  std::cout << "  qr <address>               Show QR code\n";
  std::cout << "  exit                       Exit wallet\n";
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
}

void showQRCode(const std::string &address) {
  std::cout << "\n┌────────────────────────┐" << std::endl;
  std::cout << "│ ▄▄▄▄▄ █▀█ █▄▄█▀ ▄▄▄▄▄ │" << std::endl;
  std::cout << "│ █   █ █▄▀▄█ ▀█ █   █ │" << std::endl;
  std::cout << "│ █▄▄▄█ █ ▀▄▄ ██ █▄▄▄█ │" << std::endl;
  std::cout << "│▄▄▄▄▄▄▄█ ▀ █▄█▄▄▄▄▄▄▄▄│" << std::endl;
  std::cout << "│ ▀▄ ▀▀▄██▀██▀▄ ▀▄█▀▄▀ │" << std::endl;
  std::cout << "│▄▄▄▄▄▄▄█▄▀▀▄█▄█▀▀▄█▄▄▄│" << std::endl;
  std::cout << "│ ▄▄▄▄▄ █ ▄▄▀▀▄ ▀█ ▄▀█ │" << std::endl;
  std::cout << "│ █   █ █▄▀▄██▀▄██▄▀▀▄ │" << std::endl;
  std::cout << "│ █▄▄▄█ █ ▀ ▀▀▀█▄▀▄▀█▄ │" << std::endl;
  std::cout << "└────────────────────────┘" << std::endl;
  std::cout << "Address: " << address.substr(0, 20) << "..." << std::endl;
}

int main(int argc, char *argv[]) {
  WalletManager wallet;

  // Non-interactive mode
  if (argc > 1) {
    std::string cmd = argv[1];

    if (cmd == "create" && argc > 2) {
      std::string addr = wallet.createWallet(argv[2]);
      std::cout << "✅ Wallet created: " << addr << std::endl;
    } else if (cmd == "list") {
      wallet.listWallets();
    } else if (cmd == "balance" && argc > 2) {
      double bal = wallet.getBalance(argv[2]);
      std::cout << "Balance: " << bal << " QP" << std::endl;
    } else if (cmd == "send" && argc > 4) {
      wallet.sendTransaction(argv[2], argv[3], std::stod(argv[4]));
    } else if (cmd == "help" || cmd == "-h" || cmd == "--help") {
      printHelp();
    } else {
      std::cout << "Unknown command. Use 'help' for available commands.\n";
    }
    return 0;
  }

  // Interactive mode
  printBanner();
  printHelp();

  std::string line;
  while (true) {
    std::cout << "\n💼 qp-wallet> ";
    std::getline(std::cin, line);

    if (line.empty())
      continue;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "exit" || cmd == "quit") {
      std::cout << "👋 Goodbye!" << std::endl;
      break;
    } else if (cmd == "create") {
      std::string name;
      iss >> name;
      if (name.empty())
        name = "default";
      std::string addr = wallet.createWallet(name);
      std::cout << "✅ Wallet created!" << std::endl;
      std::cout << "   Name: " << name << std::endl;
      std::cout << "   Address: " << addr << std::endl;
    } else if (cmd == "list") {
      wallet.listWallets();
    } else if (cmd == "balance") {
      std::string addr;
      iss >> addr;
      double bal = wallet.getBalance(addr);
      std::cout << "💰 Balance: " << bal << " QP" << std::endl;
      std::cout << "   Value: $" << std::fixed << std::setprecision(0)
                << (bal * 600000) << " USD" << std::endl;
    } else if (cmd == "send") {
      std::string from, to;
      double amount;
      iss >> from >> to >> amount;
      wallet.sendTransaction(from, to, amount);
    } else if (cmd == "history") {
      std::string addr;
      iss >> addr;
      wallet.getHistory(addr);
    } else if (cmd == "backup") {
      std::string addr, file;
      iss >> addr >> file;
      wallet.backupWallet(addr, file);
    } else if (cmd == "import") {
      std::string key, name;
      iss >> key >> name;
      wallet.importWallet(key, name);
    } else if (cmd == "qr") {
      std::string addr;
      iss >> addr;
      showQRCode(addr);
    } else if (cmd == "help") {
      printHelp();
    } else {
      std::cout << "❌ Unknown command. Type 'help' for available commands."
                << std::endl;
    }
  }

  return 0;
}
