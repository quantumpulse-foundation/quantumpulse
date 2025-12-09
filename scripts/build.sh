#!/bin/bash
# QuantumPulse Build Script v7.0

set -e

PROJECT_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "╔══════════════════════════════════════╗"
echo "║   QuantumPulse Build Script v7.0     ║"
echo "╚══════════════════════════════════════╝"
echo ""

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building project..."
make -j$(nproc)

echo ""
echo "Build completed successfully!"
echo "Executable: ${BUILD_DIR}/quantumpulse_v7"
echo ""
echo "Run with: ./quantumpulse_v7 --help"
