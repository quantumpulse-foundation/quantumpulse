# QuantumPulse v7.0

<p align="center">
  <img src="docs/logo.png" alt="QuantumPulse Logo" width="200">
</p>

<p align="center">
  <strong>Quantum-Resistant Cryptocurrency with Military-Grade Security</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#usage">Usage</a> •
  <a href="#mining">Mining</a> •
  <a href="#api">API</a> •
  <a href="#security">Security</a>
</p>

---

## 🌟 Features

| Feature | Description |
|---------|-------------|
| **Quantum-Resistant** | SHA3-512 + Post-quantum cryptography |
| **Price Floor** | Minimum $600,000 USD per QP |
| **Mining Limit** | 3,000,000 QP maximum supply |
| **Pre-mined** | 2,000,000 QP in founder account |
| **Difficulty** | Dynamic (starts at 6, increases) |
| **Block Reward** | 50 QP per block |
| **Military Security** | AES-256-GCM, PBKDF2 600K iterations |

---

## 📦 Installation

### Prerequisites
- GCC/Clang with C++20 support
- OpenSSL 3.x
- CMake 3.16+
- Docker (optional)

### Build from Source

```bash
# Clone repository
git clone https://github.com/yourusername/quantumpulse.git
cd quantumpulse

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
./test_quantumpulse
```

### Docker Installation

```bash
# Build image
docker build -t quantumpulse:7.0 .

# Run daemon
docker run -d --name qp-node \
  -p 8332:8332 -p 8333:8333 \
  quantumpulse:7.0

# Run CLI
docker exec qp-node quantumpulse-cli getblockchaininfo
```

---

## 🚀 Usage

### Start Daemon

```bash
./quantumpulsed

# Options:
#   -daemon          Run in background
#   -rpcport=<port>  RPC port (default: 8332)
#   -port=<port>     P2P port (default: 8333)
#   -datadir=<dir>   Data directory
#   -testnet         Use testnet
```

### CLI Commands

```bash
# Blockchain info
./quantumpulse-cli getblockchaininfo

# Check balance
./quantumpulse-cli getbalance

# Get price
./quantumpulse-cli getprice

# Generate new address
./quantumpulse-cli getnewaddress

# Stop daemon
./quantumpulse-cli stop
```

---

## ⛏️ Mining

### Start Miner

```bash
./quantumpulse-miner -address=YOUR_ADDRESS -threads=4

# Options:
#   -address=<addr>   Mining reward address (required)
#   -threads=<n>      Number of threads (default: CPU cores)
#   -difficulty=<n>   Starting difficulty (default: 6)
#   -benchmark        Run benchmark only
```

### Mining Specifications

| Spec | Value |
|------|-------|
| Algorithm | SHA3-512 Proof of Work |
| Difficulty | 6 (adjusts dynamically) |
| Block Time | ~10 minutes target |
| Block Reward | 50 QP |
| Halving | Every 210,000 blocks |
| Max Supply | 3,000,000 QP |

---

## 📡 JSON-RPC API

### Endpoint
```
http://127.0.0.1:8332/
```

### Methods

| Method | Description |
|--------|-------------|
| `getblockchaininfo` | Get blockchain status |
| `getblockcount` | Get block height |
| `getbalance` | Get wallet balance |
| `getprice` | Get QP price (min $600k) |
| `getnewaddress` | Generate new address |
| `getmininginfo` | Get mining stats |
| `getnetworkinfo` | Get network info |
| `getpeerinfo` | Get connected peers |
| `stop` | Stop the daemon |

### Example Request

```bash
curl -X POST http://127.0.0.1:8332/ \
  -H "Content-Type: application/json" \
  -d '{"method": "getblockchaininfo", "params": [], "id": 1}'
```

---

## 🔒 Security

### Cryptography
- **Hashing**: SHA3-512 (quantum-resistant)
- **Encryption**: AES-256-GCM authenticated
- **Key Derivation**: PBKDF2 with 600,000 iterations
- **Keys**: RSA-4096 / ED25519

### Protection
- Intrusion Detection System (IDS)
- DDoS Protection
- Rate Limiting
- Brute Force Protection
- SQL Injection Prevention
- XSS Prevention
- Path Traversal Prevention

### Auditing
```bash
# Run security audit
./scripts/security_audit.sh
```

---

## 🐳 Docker Deployment

### docker-compose.yml

```yaml
version: '3.8'
services:
  quantumpulsed:
    image: quantumpulse:7.0
    ports:
      - "8332:8332"
      - "8333:8333"
    volumes:
      - qp-data:/data
    restart: always

volumes:
  qp-data:
```

### Commands

```bash
# Start
docker-compose up -d

# Logs
docker-compose logs -f

# Stop
docker-compose down
```

---

## 📊 Block Explorer

Access the block explorer at: `http://localhost:9000`

Features:
- Block browser
- Transaction search
- Address lookup
- Network stats
- Mining stats

---

## 📁 Project Structure

```
quantumpulse_project_v7/
├── build/                 # Compiled binaries
│   ├── quantumpulsed      # Full node daemon
│   ├── quantumpulse-cli   # CLI wallet
│   └── quantumpulse-miner # Mining client
├── include/               # Header files (32 modules)
├── src/                   # Source files
├── tests/                 # Unit tests
├── scripts/               # Build/deploy scripts
├── explorer/              # Block explorer
├── docs/                  # Documentation
└── docker/                # Docker files
```

---

## 📜 License

MIT License - See [LICENSE](LICENSE) for details.

---

## 🙏 Credits

Created by **Shankar Lal Khati**

QuantumPulse v7.0 - Quantum-Resistant Cryptocurrency

---

<p align="center">
  <strong>⚡ QuantumPulse - The Future of Cryptocurrency ⚡</strong>
</p>
