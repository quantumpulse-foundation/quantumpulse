#!/bin/bash
# QuantumPulse Test Script v7.0

set -e

PROJECT_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "╔══════════════════════════════════════╗"
echo "║   QuantumPulse Test Script v7.0      ║"
echo "╚══════════════════════════════════════╝"
echo ""

cd "${BUILD_DIR}"

# Run unit tests
echo "Running unit tests..."
./test_quantumpulse

echo ""
echo "Running CTest..."
ctest --verbose

echo ""
echo "All tests completed!"
