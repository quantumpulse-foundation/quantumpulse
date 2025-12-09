#ifndef QUANTUMPULSE_SECURITY_V7_H
#define QUANTUMPULSE_SECURITY_V7_H

#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace QuantumPulse::Security {

// Security Constants
struct SecurityConfig {
  static constexpr int MAX_INPUT_LENGTH = 10000;
  static constexpr int MIN_PASSWORD_LENGTH = 12;
  static constexpr int MAX_LOGIN_ATTEMPTS = 5;
  static constexpr int LOCKOUT_DURATION_SECONDS = 900; // 15 minutes
  static constexpr int SESSION_TIMEOUT_SECONDS = 3600; // 1 hour
  static constexpr int ENCRYPTION_KEY_SIZE = 32;       // 256-bit
  static constexpr int SALT_SIZE = 16;
  static constexpr int PBKDF2_ITERATIONS = 100000;
};

// Input Validator - Prevents injection attacks
class InputValidator final {
public:
  // Validate address format
  [[nodiscard]] static bool
  isValidAddress(const std::string &address) noexcept {
    if (address.empty() || address.length() > 128)
      return false;

    // Must start with pub_v11_ or be a known premined account
    if (address == "Shankar-Lal-Khati")
      return true;

    static const std::regex addressPattern("^pub_v11_[a-zA-Z0-9]{10,64}$");
    return std::regex_match(address, addressPattern);
  }

  // Validate transaction ID
  [[nodiscard]] static bool isValidTxId(const std::string &txId) noexcept {
    if (txId.empty() || txId.length() > 128)
      return false;
    static const std::regex txPattern("^tx_[a-zA-Z0-9]{10,64}$");
    return std::regex_match(txId, txPattern);
  }

  // Validate amount (must be positive and not overflow)
  [[nodiscard]] static bool isValidAmount(double amount) noexcept {
    return amount > 0 && amount <= 5000000 && !std::isnan(amount) &&
           !std::isinf(amount);
  }

  // Sanitize string input (prevent XSS/injection)
  [[nodiscard]] static std::string sanitize(const std::string &input) noexcept {
    if (input.length() > SecurityConfig::MAX_INPUT_LENGTH) {
      return input.substr(0, SecurityConfig::MAX_INPUT_LENGTH);
    }

    std::string result;
    result.reserve(input.length());

    for (char c : input) {
      switch (c) {
      case '<':
        result += "&lt;";
        break;
      case '>':
        result += "&gt;";
        break;
      case '&':
        result += "&amp;";
        break;
      case '"':
        result += "&quot;";
        break;
      case '\'':
        result += "&#x27;";
        break;
      case '/':
        result += "&#x2F;";
        break;
      default:
        if (c >= 32 && c < 127)
          result += c;
      }
    }
    return result;
  }

  // Validate password strength
  [[nodiscard]] static std::pair<bool, std::string>
  validatePassword(const std::string &password) noexcept {
    if (password.length() < SecurityConfig::MIN_PASSWORD_LENGTH) {
      return {false, "Password must be at least 12 characters"};
    }

    bool hasUpper = false, hasLower = false, hasDigit = false,
         hasSpecial = false;
    for (char c : password) {
      if (std::isupper(c))
        hasUpper = true;
      else if (std::islower(c))
        hasLower = true;
      else if (std::isdigit(c))
        hasDigit = true;
      else
        hasSpecial = true;
    }

    if (!hasUpper)
      return {false, "Password must contain uppercase letter"};
    if (!hasLower)
      return {false, "Password must contain lowercase letter"};
    if (!hasDigit)
      return {false, "Password must contain digit"};
    if (!hasSpecial)
      return {false, "Password must contain special character"};

    return {true, "Password is strong"};
  }

  // Check for SQL injection patterns
  [[nodiscard]] static bool
  containsSQLInjection(const std::string &input) noexcept {
    static const std::vector<std::string> patterns = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "DROP", "UNION", "--",    "/*",
        "*/",     "xp_",    "sp_",    "0x",     "@@",   "char(", "nchar("};

    std::string upper = input;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    for (const auto &pattern : patterns) {
      if (upper.find(pattern) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  // Validate email format
  [[nodiscard]] static bool isValidEmail(const std::string &email) noexcept {
    static const std::regex emailPattern(
        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, emailPattern);
  }
};

// Session Manager - Secure session handling
class SessionManager final {
public:
  struct Session {
    std::string sessionId;
    std::string userId;
    std::string ipAddress;
    int64_t createdAt;
    int64_t lastActivity;
    bool is2FAVerified;
  };

  // Create session
  [[nodiscard]] std::string createSession(const std::string &userId,
                                          const std::string &ip) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Generate secure random session ID
    unsigned char bytes[32];
    RAND_bytes(bytes, 32);

    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
      ss << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<int>(bytes[i]);
    }
    std::string sessionId = ss.str();

    Session session;
    session.sessionId = sessionId;
    session.userId = userId;
    session.ipAddress = ip;
    session.createdAt = std::time(nullptr);
    session.lastActivity = session.createdAt;
    session.is2FAVerified = false;

    sessions_[sessionId] = session;

    Logging::Logger::getInstance().info("Session created for: " + userId,
                                        "Security", 0);
    return sessionId;
  }

  // Validate session
  [[nodiscard]] std::optional<Session>
  validateSession(const std::string &sessionId,
                  const std::string &ip) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end())
      return std::nullopt;

    auto &session = it->second;

    // Check IP (session fixation protection)
    if (session.ipAddress != ip) {
      Logging::Logger::getInstance().warning(
          "Session IP mismatch: " + session.userId, "Security", 0);
      sessions_.erase(it);
      return std::nullopt;
    }

    // Check expiry
    if (std::time(nullptr) - session.lastActivity >
        SecurityConfig::SESSION_TIMEOUT_SECONDS) {
      sessions_.erase(it);
      return std::nullopt;
    }

    // Update last activity
    session.lastActivity = std::time(nullptr);
    return session;
  }

  // Destroy session
  void destroySession(const std::string &sessionId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sessionId);
  }

  // Destroy all user sessions
  void destroyUserSessions(const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (it->second.userId == userId) {
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  std::map<std::string, Session> sessions_;
  std::mutex mutex_;
};

// Brute Force Protection
class BruteForceProtector final {
public:
  // Record failed attempt
  void recordFailedAttempt(const std::string &identifier) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto &record = attempts_[identifier];
    record.attempts++;
    record.lastAttempt = std::time(nullptr);

    if (record.attempts >= SecurityConfig::MAX_LOGIN_ATTEMPTS) {
      record.lockedUntil =
          record.lastAttempt + SecurityConfig::LOCKOUT_DURATION_SECONDS;
      Logging::Logger::getInstance().warning(
          "Account locked due to brute force: " + identifier, "Security", 0);
    }
  }

  // Check if blocked
  [[nodiscard]] bool isBlocked(const std::string &identifier) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(identifier);
    if (it == attempts_.end())
      return false;

    if (it->second.lockedUntil > 0) {
      if (std::time(nullptr) < it->second.lockedUntil) {
        return true;
      } else {
        // Lockout expired, reset
        attempts_.erase(it);
      }
    }
    return false;
  }

  // Reset on successful login
  void resetAttempts(const std::string &identifier) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    attempts_.erase(identifier);
  }

  // Get remaining lockout time
  [[nodiscard]] int
  getRemainingLockout(const std::string &identifier) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(identifier);
    if (it == attempts_.end())
      return 0;

    int remaining =
        static_cast<int>(it->second.lockedUntil - std::time(nullptr));
    return remaining > 0 ? remaining : 0;
  }

private:
  struct AttemptRecord {
    int attempts{0};
    int64_t lastAttempt{0};
    int64_t lockedUntil{0};
  };

  std::map<std::string, AttemptRecord> attempts_;
  std::mutex mutex_;
};

// CSP and Security Headers
class SecurityHeaders final {
public:
  [[nodiscard]] static std::map<std::string, std::string>
  getHeaders() noexcept {
    return {{"Content-Security-Policy",
             "default-src 'self'; script-src 'self'; style-src 'self' "
             "'unsafe-inline'; "
             "img-src 'self' data:; font-src 'self'; connect-src 'self' wss:; "
             "frame-ancestors 'none'; base-uri 'self'; form-action 'self'"},
            {"X-Content-Type-Options", "nosniff"},
            {"X-Frame-Options", "DENY"},
            {"X-XSS-Protection", "1; mode=block"},
            {"Strict-Transport-Security",
             "max-age=31536000; includeSubDomains; preload"},
            {"Referrer-Policy", "strict-origin-when-cross-origin"},
            {"Permissions-Policy", "geolocation=(), microphone=(), camera=()"},
            {"Cache-Control", "no-store, no-cache, must-revalidate"},
            {"Pragma", "no-cache"}};
  }
};

// Encryption Utils
class EncryptionUtils final {
public:
  // Generate secure random bytes
  [[nodiscard]] static std::vector<unsigned char>
  generateRandomBytes(int length) noexcept {
    std::vector<unsigned char> bytes(length);
    RAND_bytes(bytes.data(), length);
    return bytes;
  }

  // Hash password with PBKDF2
  [[nodiscard]] static std::string
  hashPassword(const std::string &password,
               const std::vector<unsigned char> &salt) noexcept {
    unsigned char hash[32];

    PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.length()),
                      salt.data(), static_cast<int>(salt.size()),
                      SecurityConfig::PBKDF2_ITERATIONS, EVP_sha512(), 32,
                      hash);

    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
      ss << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<int>(hash[i]);
    }
    return ss.str();
  }

  // Secure memory wipe
  static void secureWipe(void *ptr, size_t size) noexcept {
    volatile unsigned char *p = static_cast<volatile unsigned char *>(ptr);
    while (size--)
      *p++ = 0;
  }
};

} // namespace QuantumPulse::Security

#endif // QUANTUMPULSE_SECURITY_V7_H
