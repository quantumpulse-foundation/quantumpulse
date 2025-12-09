#!/bin/bash
# Generate self-signed TLS certificates for QuantumPulse API

CERT_DIR="$(dirname "$0")/../certs"
mkdir -p "$CERT_DIR"

echo "Generating TLS certificates for QuantumPulse..."

# Generate private key
openssl genrsa -out "$CERT_DIR/server.key" 2048

# Generate self-signed certificate
openssl req -new -x509 -days 365 -key "$CERT_DIR/server.key" \
    -out "$CERT_DIR/server.crt" \
    -subj "/C=IN/ST=India/L=Delhi/O=QuantumPulse/OU=Security/CN=localhost"

# Generate DH parameters for perfect forward secrecy
openssl dhparam -out "$CERT_DIR/dhparam.pem" 2048

# Set permissions
chmod 600 "$CERT_DIR/server.key"
chmod 644 "$CERT_DIR/server.crt"
chmod 644 "$CERT_DIR/dhparam.pem"

echo ""
echo "✓ TLS certificates generated:"
echo "  - $CERT_DIR/server.key (private key)"
echo "  - $CERT_DIR/server.crt (certificate)"
echo "  - $CERT_DIR/dhparam.pem (DH parameters)"
echo ""
echo "⚠ For production, use certificates from Let's Encrypt or a trusted CA"
