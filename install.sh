#!/bin/bash
# QuantumPulse One-Click Installer
# Run: curl -sSL https://raw.githubusercontent.com/quantumpulse-foundation/quantumpulse/main/install.sh | bash

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     QuantumPulse Installer v7.0                              ║"
echo "║     Quantum-Resistant Cryptocurrency                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
else
    echo "❌ Unsupported OS. Please use Linux or macOS."
    exit 1
fi

echo "🔍 Detected OS: $OS"
echo ""

# Install dependencies
echo "📦 Installing dependencies..."
if command -v apt-get &> /dev/null; then
    sudo apt-get update -qq
    sudo apt-get install -y -qq git cmake g++ libssl-dev
elif command -v pacman &> /dev/null; then
    sudo pacman -Sy --noconfirm git cmake gcc openssl
elif command -v brew &> /dev/null; then
    brew install cmake openssl
fi

# Clone repository
echo "📥 Downloading QuantumPulse..."
cd ~
if [ -d "quantumpulse" ]; then
    cd quantumpulse && git pull
else
    git clone https://github.com/quantumpulse-foundation/quantumpulse.git
    cd quantumpulse
fi

# Build
echo "🔨 Building QuantumPulse..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo ""
echo "✅ Installation Complete!"
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Quick Start Commands:                                    ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Start Node:    ./quantumpulsed                              ║"
echo "║  Create Wallet: ./qp-wallet create mywallet mypassword       ║"
echo "║  Start Mining:  ./quantumpulse-miner -address=<addr> -d=4    ║"
echo "║  Check Balance: ./qp-wallet balance mywallet mypassword      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "📍 Installation Path: ~/quantumpulse/build/"
echo ""
echo "🚀 Happy Mining! Join: https://t.me/quantumpulse"
