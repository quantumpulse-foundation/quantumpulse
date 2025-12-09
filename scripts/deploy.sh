#!/bin/bash
# QuantumPulse Deploy Script v7.0

set -e

PROJECT_DIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "╔══════════════════════════════════════╗"
echo "║   QuantumPulse Deploy Script v7.0    ║"
echo "╚══════════════════════════════════════╝"
echo ""

# Parse arguments
SHARD_ID=0
CONFIG_PATH="${PROJECT_DIR}/config/quantumpulse_config_v7.json"

while [[ $# -gt 0 ]]; do
    case $1 in
        --shard)
            SHARD_ID="$2"
            shift 2
            ;;
        --config)
            CONFIG_PATH="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

echo "Starting QuantumPulse Node..."
echo "  Shard ID: ${SHARD_ID}"
echo "  Config: ${CONFIG_PATH}"
echo ""

cd "${BUILD_DIR}"
./quantumpulse_v7 --node --shard ${SHARD_ID} --config "${CONFIG_PATH}"
