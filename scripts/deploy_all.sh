#!/bin/bash
# QuantumPulse Deployment Script
# Deploy all components

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║          QuantumPulse Deployment Script v7.0                 ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

PROJECT_DIR="${QP_PROJECT_DIR:-$(pwd)}"
BUILD_DIR="$PROJECT_DIR/build"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}[1/5] Building project...${NC}"
cd $PROJECT_DIR
mkdir -p build
cd build
cmake .. > /dev/null 2>&1
make -j$(nproc) > /dev/null 2>&1
echo -e "${GREEN}✅ Build complete${NC}"

echo -e "${YELLOW}[2/5] Running tests...${NC}"
./test_quantumpulse > /dev/null 2>&1
echo -e "${GREEN}✅ All tests passed${NC}"

echo -e "${YELLOW}[3/5] Starting daemon...${NC}"
pkill -f quantumpulsed 2>/dev/null || true
sleep 1
nohup ./quantumpulsed > /tmp/quantumpulsed.log 2>&1 &
DAEMON_PID=$!
sleep 2
echo -e "${GREEN}✅ Daemon started (PID: $DAEMON_PID)${NC}"

echo -e "${YELLOW}[4/5] Starting block explorer...${NC}"
pkill -f "python3 -m http.server" 2>/dev/null || true
sleep 1
cd $PROJECT_DIR/explorer
nohup python3 -m http.server 9000 > /tmp/explorer.log 2>&1 &
EXPLORER_PID=$!
echo -e "${GREEN}✅ Explorer started at http://localhost:9000${NC}"

echo -e "${YELLOW}[5/5] Starting website...${NC}"
cd $PROJECT_DIR/website
nohup python3 -m http.server 8080 > /tmp/website.log 2>&1 &
WEBSITE_PID=$!
echo -e "${GREEN}✅ Website started at http://localhost:8080${NC}"

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              QuantumPulse is now running!                    ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Component        │  URL / Command                           ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Daemon           │  RPC: localhost:8332, P2P: 8333          ║"
echo "║  CLI              │  ./quantumpulse-cli getblockchaininfo    ║"
echo "║  Block Explorer   │  http://localhost:9000                   ║"
echo "║  Website          │  http://localhost:8080                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "To start mining:"
echo "  cd $BUILD_DIR"
echo "  ./quantumpulse-miner -address=YOUR_WALLET_ADDRESS -difficulty=4"
echo ""
echo "To stop all services:"
echo "  pkill -f quantumpulsed"
echo "  pkill -f 'python3 -m http.server'"
echo ""
