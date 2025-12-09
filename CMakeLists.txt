cmake_minimum_required(VERSION 3.16)
project(QuantumPulse VERSION 7.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -pthread")

# Find required packages
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

# ========================================
# BITCOIN-LIKE BINARIES
# ========================================

# 1. quantumpulsed - Full Node Daemon
add_executable(quantumpulsed
    src/quantumpulsed.cpp
)
target_link_libraries(quantumpulsed 
    PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

# 2. quantumpulse-cli - Command Line Interface
add_executable(quantumpulse-cli
    src/quantumpulse-cli.cpp
)
target_link_libraries(quantumpulse-cli
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# 3. quantumpulse-miner - Mining Client
add_executable(quantumpulse-miner
    src/quantumpulse-miner.cpp
)
target_link_libraries(quantumpulse-miner
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# ========================================
# LEGACY BINARIES (optional)
# ========================================

# Main executable (legacy)
add_executable(quantumpulse_v7
    src/main.cpp
)
target_link_libraries(quantumpulse_v7 
    PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)

# Wallet CLI executable (legacy)
add_executable(qp-wallet
    src/quantumpulse_wallet_cli.cpp
)
target_link_libraries(qp-wallet
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# ========================================
# TESTS
# ========================================

enable_testing()

add_executable(test_quantumpulse
    tests/test_blockchain_v7.cpp
)
target_link_libraries(test_quantumpulse 
    PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)
target_include_directories(test_quantumpulse PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME BlockchainTest COMMAND test_quantumpulse)

# ========================================
# OPTIONAL CUDA SUPPORT
# ========================================

find_package(CUDA QUIET)
if(CUDA_FOUND)
    message(STATUS "CUDA found: ${CUDA_VERSION}")
    enable_language(CUDA)
    add_definitions(-DQUANTUMPULSE_CUDA_ENABLED)
endif()

# ========================================
# INSTALLATION
# ========================================

install(TARGETS 
    quantumpulsed 
    quantumpulse-cli 
    quantumpulse-miner
    quantumpulse_v7 
    qp-wallet 
    DESTINATION bin
)
install(DIRECTORY config/ DESTINATION etc/quantumpulse)

# Print configuration
message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════════════╗")
message(STATUS "║             QuantumPulse v7.0 Build Configuration            ║")
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║ Core Binaries:                                               ║")
message(STATUS "║   • quantumpulsed      - Full Node Daemon                    ║")
message(STATUS "║   • quantumpulse-cli   - Command Line Interface              ║")
message(STATUS "║   • quantumpulse-miner - Mining Client                       ║")
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║ C++ Standard: C++20                                          ║")
message(STATUS "║ OpenSSL: ${OPENSSL_VERSION}                                          ║")
if(CUDA_FOUND)
    message(STATUS "║ CUDA: ${CUDA_VERSION}                                              ║")
endif()
message(STATUS "╚══════════════════════════════════════════════════════════════╝")
message(STATUS "")
