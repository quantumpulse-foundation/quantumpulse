FROM ubuntu:22.04

LABEL maintainer="Shankar Lal Khati"
LABEL version="7.0"
LABEL description="QuantumPulse - Quantum-Resistant Cryptocurrency"

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Create user
RUN useradd -m -s /bin/bash quantumpulse

# Copy source
WORKDIR /app
COPY . .

# Build
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Setup data directory
RUN mkdir -p /data && chown quantumpulse:quantumpulse /data

# Switch to user
USER quantumpulse

# Expose ports
EXPOSE 8332 8333

# Data volume
VOLUME ["/data"]

# Set entrypoint
ENTRYPOINT ["/app/build/quantumpulsed"]
CMD ["-datadir=/data", "-printtoconsole"]
