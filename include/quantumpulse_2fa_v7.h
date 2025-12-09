#ifndef QUANTUMPULSE_2FA_V7_H
#define QUANTUMPULSE_2FA_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <map>
#include <mutex>
#include <openssl/hmac.h>
#include <random>
#include <sstream>
#include <string>

namespace QuantumPulse::Auth {

// 2FA Configuration
struct TwoFactorConfig {
  static constexpr int CODE_LENGTH = 6;
  static constexpr int TIME_STEP = 30;     // seconds
  static constexpr int WINDOW = 1;         // allow +/- 1 time step
  static constexpr int SECRET_LENGTH = 20; // bytes
  static constexpr int BACKUP_CODES_COUNT = 10;
};

// Base32 encoding for TOTP secrets
class Base32 {
public:
  static std::string encode(const std::string &input) noexcept {
    static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string result;

    int buffer = 0;
    int bitsLeft = 0;

    for (unsigned char c : input) {
      buffer = (buffer << 8) | c;
      bitsLeft += 8;

      while (bitsLeft >= 5) {
        result += alphabet[(buffer >> (bitsLeft - 5)) & 0x1F];
        bitsLeft -= 5;
      }
    }

    if (bitsLeft > 0) {
      result += alphabet[(buffer << (5 - bitsLeft)) & 0x1F];
    }

    return result;
  }

  static std::string decode(const std::string &input) noexcept {
    std::string result;
    int buffer = 0;
    int bitsLeft = 0;

    for (char c : input) {
      if (c == '=' || c == ' ')
        continue;

      int val;
      if (c >= 'A' && c <= 'Z')
        val = c - 'A';
      else if (c >= '2' && c <= '7')
        val = c - '2' + 26;
      else
        continue;

      buffer = (buffer << 5) | val;
      bitsLeft += 5;

      if (bitsLeft >= 8) {
        result += static_cast<char>((buffer >> (bitsLeft - 8)) & 0xFF);
        bitsLeft -= 8;
      }
    }

    return result;
  }
};

// TOTP (Time-based One-Time Password) implementation
class TOTP final {
public:
  // Generate random secret
  [[nodiscard]] static std::string generateSecret() noexcept {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string secret;
    secret.reserve(TwoFactorConfig::SECRET_LENGTH);

    for (int i = 0; i < TwoFactorConfig::SECRET_LENGTH; ++i) {
      secret += static_cast<char>(dis(gen));
    }

    return Base32::encode(secret);
  }

  // Generate TOTP code for current time
  [[nodiscard]] static std::string
  generateCode(const std::string &secret) noexcept {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    int64_t counter =
        std::chrono::duration_cast<std::chrono::seconds>(epoch).count() /
        TwoFactorConfig::TIME_STEP;

    return generateCodeForCounter(secret, counter);
  }

  // Verify TOTP code
  [[nodiscard]] static bool verifyCode(const std::string &secret,
                                       const std::string &code) noexcept {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    int64_t counter =
        std::chrono::duration_cast<std::chrono::seconds>(epoch).count() /
        TwoFactorConfig::TIME_STEP;

    // Check current and adjacent time steps
    for (int i = -TwoFactorConfig::WINDOW; i <= TwoFactorConfig::WINDOW; ++i) {
      if (generateCodeForCounter(secret, counter + i) == code) {
        return true;
      }
    }

    return false;
  }

  // Generate provisioning URI for QR code
  [[nodiscard]] static std::string
  generateProvisioningUri(const std::string &secret,
                          const std::string &accountName,
                          const std::string &issuer = "QuantumPulse") noexcept {

    std::stringstream ss;
    ss << "otpauth://totp/" << issuer << ":" << accountName;
    ss << "?secret=" << secret;
    ss << "&issuer=" << issuer;
    ss << "&algorithm=SHA1";
    ss << "&digits=" << TwoFactorConfig::CODE_LENGTH;
    ss << "&period=" << TwoFactorConfig::TIME_STEP;

    return ss.str();
  }

private:
  [[nodiscard]] static std::string
  generateCodeForCounter(const std::string &base32Secret,
                         int64_t counter) noexcept {

    std::string secret = Base32::decode(base32Secret);

    // Convert counter to big-endian bytes
    unsigned char counterBytes[8];
    for (int i = 7; i >= 0; --i) {
      counterBytes[i] = counter & 0xFF;
      counter >>= 8;
    }

    // HMAC-SHA1
    unsigned char hash[20];
    unsigned int hashLen = 20;

    HMAC(EVP_sha1(), secret.data(), static_cast<int>(secret.length()),
         counterBytes, 8, hash, &hashLen);

    // Dynamic truncation
    int offset = hash[19] & 0x0F;
    int binary = ((hash[offset] & 0x7F) << 24) |
                 ((hash[offset + 1] & 0xFF) << 16) |
                 ((hash[offset + 2] & 0xFF) << 8) | (hash[offset + 3] & 0xFF);

    int otp =
        binary % static_cast<int>(std::pow(10, TwoFactorConfig::CODE_LENGTH));

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(TwoFactorConfig::CODE_LENGTH) << otp;
    return ss.str();
  }
};

// Two-Factor Manager
class TwoFactorManager final {
public:
  TwoFactorManager() noexcept {
    Logging::Logger::getInstance().info("2FA Manager initialized", "Auth", 0);
  }

  // Enable 2FA for user
  [[nodiscard]] std::string
  enableTwoFactor(const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    std::string secret = TOTP::generateSecret();
    userSecrets_[userId] = secret;
    userEnabled_[userId] = false; // Not verified yet

    // Generate backup codes
    std::vector<std::string> backupCodes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    for (int i = 0; i < TwoFactorConfig::BACKUP_CODES_COUNT; ++i) {
      backupCodes.push_back(std::to_string(dis(gen)));
    }
    backupCodes_[userId] = backupCodes;

    Logging::Logger::getInstance().info("2FA enabled for user: " + userId,
                                        "Auth", 0);

    return secret;
  }

  // Verify and activate 2FA
  [[nodiscard]] bool verifyAndActivate(const std::string &userId,
                                       const std::string &code) noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    auto it = userSecrets_.find(userId);
    if (it == userSecrets_.end()) {
      return false;
    }

    if (TOTP::verifyCode(it->second, code)) {
      userEnabled_[userId] = true;
      Logging::Logger::getInstance().info("2FA activated for user: " + userId,
                                          "Auth", 0);
      return true;
    }

    return false;
  }

  // Verify TOTP code
  [[nodiscard]] bool verifyCode(const std::string &userId,
                                const std::string &code) noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    auto it = userSecrets_.find(userId);
    if (it == userSecrets_.end()) {
      return false;
    }

    // Check TOTP code
    if (TOTP::verifyCode(it->second, code)) {
      return true;
    }

    // Check backup codes
    auto backupIt = backupCodes_.find(userId);
    if (backupIt != backupCodes_.end()) {
      auto &codes = backupIt->second;
      auto codeIt = std::find(codes.begin(), codes.end(), code);
      if (codeIt != codes.end()) {
        codes.erase(codeIt); // One-time use
        Logging::Logger::getInstance().warning(
            "Backup code used for: " + userId, "Auth", 0);
        return true;
      }
    }

    return false;
  }

  // Check if 2FA is enabled
  [[nodiscard]] bool isEnabled(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    auto it = userEnabled_.find(userId);
    return it != userEnabled_.end() && it->second;
  }

  // Disable 2FA
  void disableTwoFactor(const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    userSecrets_.erase(userId);
    userEnabled_.erase(userId);
    backupCodes_.erase(userId);

    Logging::Logger::getInstance().info("2FA disabled for user: " + userId,
                                        "Auth", 0);
  }

  // Get provisioning URI
  [[nodiscard]] std::string
  getProvisioningUri(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    auto it = userSecrets_.find(userId);
    if (it == userSecrets_.end()) {
      return "";
    }

    return TOTP::generateProvisioningUri(it->second, userId);
  }

  // Get remaining backup codes count
  [[nodiscard]] size_t
  getBackupCodesCount(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(authMutex_);

    auto it = backupCodes_.find(userId);
    return it != backupCodes_.end() ? it->second.size() : 0;
  }

private:
  mutable std::mutex authMutex_;
  std::map<std::string, std::string> userSecrets_;
  std::map<std::string, bool> userEnabled_;
  std::map<std::string, std::vector<std::string>> backupCodes_;
};

} // namespace QuantumPulse::Auth

#endif // QUANTUMPULSE_2FA_V7_H
