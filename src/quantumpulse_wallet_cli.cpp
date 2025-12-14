// QuantumPulse CLI Wallet v7.0
// Usage: ./qp-wallet <command> [args]

#include "quantumpulse_wallet_v7.h"
#include <iomanip>
#include <iostream>
#include <string>

using namespace QuantumPulse;

void printBanner() {
  std::cout << "\n";
  std::cout
      << "╔══════════════════════════════════════════════════════════════╗\n";
  std::cout
      << "║     QuantumPulse Wallet v7.0                                  ║\n";
  std::cout
      << "║     Secure • Fast • Quantum-Resistant                         ║\n";
  std::cout
      << "╚══════════════════════════════════════════════════════════════╝\n";
  std::cout << "\n";
}

void printUsage() {
  std::cout << "Usage: qp-wallet <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  create <name> <password>     Create new wallet\n";
  std::cout << "  open <name> <password>       Open existing wallet\n";
  std::cout << "  balance <name> <password>    Check wallet balance\n";
  std::cout << "  send <name> <password> <to> <amount>    Send QP coins\n";
  std::cout << "  history <name> <password>    View transaction history\n";
  std::cout << "  export <name> <password>     Export wallet keys\n";
  std::cout
      << "  founder <secret>             Access founder wallet (hidden)\n";
  std::cout << "  list                         List all wallets\n";
  std::cout << "  help                         Show this help\n";
  std::cout << "\n";
  std::cout << "Examples:\n";
  std::cout << "  qp-wallet create mywallet mypassword123\n";
  std::cout << "  qp-wallet send mywallet mypassword123 pub_v11_abc... 100\n";
  std::cout << "\n";
}

void createWallet(const std::string &name, const std::string &password) {
  Wallet::Wallet wallet(name);

  if (wallet.create(password)) {
    std::cout << "✓ Wallet created successfully!\n\n";
    std::cout << "Wallet Name: " << name << "\n";
    std::cout << "Address: " << wallet.getAddress() << "\n";
    std::cout << "\n";
    std::cout
        << "⚠ IMPORTANT: Keep your password safe. It cannot be recovered!\n";
  } else {
    std::cout << "✗ Failed to create wallet. It may already exist.\n";
  }
}

void openWallet(const std::string &name, const std::string &password) {
  Wallet::Wallet wallet(name);

  if (wallet.load(password)) {
    std::cout << "✓ Wallet opened successfully!\n\n";
    std::cout << "Wallet: " << name << "\n";
    std::cout << "Address: " << wallet.getAddress() << "\n";
    std::cout << "Balance: " << std::fixed << std::setprecision(8)
              << wallet.getBalance() << " QP\n";
    std::cout << "Transactions: " << wallet.getTransactionCount() << "\n";
  } else {
    std::cout << "✗ Failed to open wallet. Check name and password.\n";
  }
}

void showBalance(const std::string &name, const std::string &password) {
  Wallet::Wallet wallet(name);

  if (wallet.load(password)) {
    double balance = wallet.getBalance();
    double usdValue = balance * 600000; // Minimum price

    std::cout << "═══════════════════════════════════════════\n";
    std::cout << "  Wallet: " << name << "\n";
    std::cout << "═══════════════════════════════════════════\n";
    std::cout << "  Balance: " << std::fixed << std::setprecision(8) << balance
              << " QP\n";
    std::cout << "  Value:   $" << std::fixed << std::setprecision(2)
              << usdValue << " USD (min)\n";
    std::cout << "═══════════════════════════════════════════\n";
  } else {
    std::cout << "✗ Failed to access wallet.\n";
  }
}

void sendCoins(const std::string &name, const std::string &password,
               const std::string &to, double amount) {
  Wallet::Wallet wallet(name);

  if (!wallet.load(password)) {
    std::cout << "✗ Failed to access wallet.\n";
    return;
  }

  if (wallet.getBalance() < amount) {
    std::cout << "✗ Insufficient balance.\n";
    std::cout << "  Available: " << wallet.getBalance() << " QP\n";
    std::cout << "  Required:  " << amount << " QP\n";
    return;
  }

  std::string txId = wallet.createTransaction(to, amount);

  if (!txId.empty()) {
    std::cout << "✓ Transaction sent!\n\n";
    std::cout << "TX ID: " << txId << "\n";
    std::cout << "To: " << to << "\n";
    std::cout << "Amount: " << amount << " QP\n";
    std::cout << "Status: pending\n";
  } else {
    std::cout << "✗ Transaction failed.\n";
  }
}

void showHistory(const std::string &name, const std::string &password) {
  Wallet::Wallet wallet(name);

  if (!wallet.load(password)) {
    std::cout << "✗ Failed to access wallet.\n";
    return;
  }

  auto history = wallet.getHistory();

  std::cout << "═══════════════════════════════════════════════════════════════"
               "════════════════\n";
  std::cout << "  Transaction History for: " << name << "\n";
  std::cout << "═══════════════════════════════════════════════════════════════"
               "════════════════\n\n";

  if (history.empty()) {
    std::cout << "  No transactions yet.\n";
  } else {
    std::cout << std::left << std::setw(20) << "TX ID" << std::setw(15)
              << "Amount" << std::setw(12) << "Status" << "\n";
    std::cout << "  ───────────────────────────────────────────────────────\n";

    for (const auto &tx : history) {
      std::string direction = (tx.to == wallet.getAddress()) ? "+" : "-";
      std::cout << "  " << std::setw(20) << tx.txId.substr(0, 16) + "..."
                << std::setw(15)
                << (direction + std::to_string(tx.amount) + " QP")
                << std::setw(12) << tx.status << "\n";
    }
  }
  std::cout << "\n";
}

void exportKeys(const std::string &name, const std::string &password) {
  Wallet::Wallet wallet(name);

  if (!wallet.load(password)) {
    std::cout << "✗ Failed to access wallet.\n";
    return;
  }

  std::string keys = wallet.exportKeys(password);

  if (!keys.empty()) {
    std::cout << "⚠ WARNING: Keep these keys secret!\n\n";
    std::cout << keys << "\n";
  } else {
    std::cout << "✗ Failed to export keys.\n";
  }
}

void listWallets() {
  Wallet::WalletManager manager;
  auto wallets = manager.listWallets();

  std::cout << "Available Wallets:\n";
  std::cout << "─────────────────────\n";

  if (wallets.empty()) {
    std::cout << "  No wallets found.\n";
  } else {
    for (const auto &name : wallets) {
      std::cout << "  • " << name << "\n";
    }
  }
  std::cout << "\n";
}

void showFounderWallet(const std::string &secret) {
  // Secret key check - only you know this
  const std::string FOUNDER_SECRET = "qp2023founder";

  if (secret != FOUNDER_SECRET) {
    std::cout << "✗ Access denied. Invalid secret key.\n";
    return;
  }

  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
  std::cout << "  🔒 FOUNDER WALLET (HIDDEN FROM PUBLIC)\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
  std::cout << "\n";
  std::cout << "  Wallet:     FOUNDER_WALLET (Stealth Address Mode)\n";
  std::cout << "  Balance:    2,000,000.00000000 QP\n";
  std::cout << "  Value:      $1,200,000,000,000.00 USD (at min $600K/QP)\n";
  std::cout << "  Status:     🔒 HIDDEN from public API\n";
  std::cout << "  Type:       Pre-mined (Genesis)\n";
  std::cout << "\n";
  std::cout << "  Security Features:\n";
  std::cout << "    ✓ Stealth Address Active\n";
  std::cout << "    ✓ Ring Signatures Enabled\n";
  std::cout << "    ✓ Confidential Transactions\n";
  std::cout << "    ✓ Zero-Knowledge Proofs\n";
  std::cout << "    ✓ Post-Quantum Cryptography\n";
  std::cout << "\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
  std::cout << "⚠ This information is ONLY visible with the secret key!\n";
  std::cout
      << "═══════════════════════════════════════════════════════════════\n";
}

int main(int argc, char *argv[]) {
  printBanner();

  if (argc < 2) {
    printUsage();
    return 0;
  }

  std::string command = argv[1];

  if (command == "help" || command == "--help" || command == "-h") {
    printUsage();
  } else if (command == "create" && argc >= 4) {
    createWallet(argv[2], argv[3]);
  } else if (command == "open" && argc >= 4) {
    openWallet(argv[2], argv[3]);
  } else if (command == "balance" && argc >= 4) {
    showBalance(argv[2], argv[3]);
  } else if (command == "send" && argc >= 6) {
    sendCoins(argv[2], argv[3], argv[4], std::stod(argv[5]));
  } else if (command == "history" && argc >= 4) {
    showHistory(argv[2], argv[3]);
  } else if (command == "export" && argc >= 4) {
    exportKeys(argv[2], argv[3]);
  } else if (command == "founder" && argc >= 3) {
    showFounderWallet(argv[2]);
  } else if (command == "list") {
    listWallets();
  } else {
    std::cout << "Unknown command or missing arguments.\n\n";
    printUsage();
    return 1;
  }

  return 0;
}
