#!/bin/bash
# QuantumPulse Maintenance Script v7.0

set -e

PROJECT_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "╔══════════════════════════════════════╗"
echo "║  QuantumPulse Maintenance v7.0       ║"
echo "╚══════════════════════════════════════╝"
echo ""

cd "${PROJECT_DIR}"

# Rebuild if needed
if [ ! -f "${BUILD_DIR}/quantumpulse_v7" ]; then
    echo "Building project..."
    ./scripts/build.sh
fi

# Update AI model
echo "Updating AI Model..."
cd "${BUILD_DIR}"
./quantumpulse_v7 --update-ai

# Run audit
echo ""
echo "Running Blockchain Audit..."
./quantumpulse_v7 --audit

# Check logs for errors
echo ""
echo "Checking logs for errors..."
if [ -d "${PROJECT_DIR}/logs/audit" ]; then
    grep -i "error\|critical" "${PROJECT_DIR}/logs/audit/"*.txt 2>/dev/null || echo "No errors found in logs."
else
    echo "No logs directory found."
fi

echo ""
echo "Maintenance completed!"
