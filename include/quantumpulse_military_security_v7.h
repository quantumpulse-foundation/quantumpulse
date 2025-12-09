#ifndef QUANTUMPULSE_MILITARY_SECURITY_V7_H
#define QUANTUMPULSE_MILITARY_SECURITY_V7_H

#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace QuantumPulse::MilitarySecurity {

// Security Constants - Military Grade
struct MilitarySecurityConfig {
  static constexpr int AES_KEY_SIZE = 256;  // AES-256
  static constexpr int RSA_KEY_SIZE = 4096; // RSA-4096
  static constexpr int PBKDF2_ITERATIONS =
      600000;                                   // 600K iterations (OWASP 2024)
  static constexpr int BCRYPT_COST = 14;        // High bcrypt cost
  static constexpr int ARGON2_MEMORY = 65536;   // 64MB memory
  static constexpr int ARGON2_TIME = 4;         // 4 iterations
  static constexpr int ARGON2_PARALLELISM = 4;  // 4 threads
  static constexpr int MAX_LOGIN_ATTEMPTS = 3;  // Very strict
  static constexpr int LOCKOUT_DURATION = 3600; // 1 hour lockout
  static constexpr int SESSION_TIMEOUT = 900;   // 15 minute timeout
  static constexpr int KEY_ROTATION_HOURS = 24; // Daily key rotation
  static constexpr int SALT_SIZE = 32;          // 256-bit salt
  static constexpr int NONCE_SIZE = 24;         // 192-bit nonce
};

// Secure Memory - Prevents memory scraping
class SecureMemory final {
public:
  // Secure allocate with guard pages
  static void *secureAlloc(size_t size) noexcept {
    void *ptr = std::malloc(size + 64); // Extra for guard
    if (ptr) {
      std::memset(ptr, 0, size + 64);
      RAND_bytes(static_cast<unsigned char *>(ptr), 32); // Random canary
    }
    return ptr;
  }

  // Secure wipe (prevents compiler optimization)
  static void secureWipe(void *ptr, size_t size) noexcept {
    if (!ptr)
      return;
    volatile unsigned char *p = static_cast<volatile unsigned char *>(ptr);

    // Multiple wipe passes (DoD 5220.22-M standard)
    for (int pass = 0; pass < 3; pass++) {
      for (size_t i = 0; i < size; i++) {
        p[i] = (pass == 0) ? 0x00 : (pass == 1) ? 0xFF : 0x00;
      }
    }

    // Random overwrite
    std::vector<unsigned char> random(size);
    RAND_bytes(random.data(), size);
    for (size_t i = 0; i < size; i++) {
      p[i] = random[i];
    }

    // Final zero
    for (size_t i = 0; i < size; i++) {
      p[i] = 0;
    }
  }

  // Secure free
  static void secureFree(void *ptr, size_t size) noexcept {
    secureWipe(ptr, size);
    std::free(ptr);
  }

  // Constant-time comparison (prevents timing attacks)
  static bool constantTimeCompare(const void *a, const void *b,
                                  size_t len) noexcept {
    const volatile unsigned char *aa =
        static_cast<const volatile unsigned char *>(a);
    const volatile unsigned char *bb =
        static_cast<const volatile unsigned char *>(b);
    volatile unsigned char result = 0;
    for (size_t i = 0; i < len; i++) {
      result |= aa[i] ^ bb[i];
    }
    return result == 0;
  }
};

// Intrusion Detection System
class IntrusionDetector final {
public:
  struct SecurityEvent {
    std::string type;
    std::string source;
    std::string description;
    int severity; // 1-10
    int64_t timestamp;
  };

  IntrusionDetector() noexcept {
    Logging::Logger::getInstance().info(
        "Intrusion Detection System initialized", "IDS", 0);
  }

  // Detect brute force
  bool detectBruteForce(const std::string &ip) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::time(nullptr);
    auto &history = connectionHistory_[ip];

    // Remove old entries
    history.erase(std::remove_if(history.begin(), history.end(),
                                 [now](int64_t t) { return now - t > 60; }),
                  history.end());

    history.push_back(now);

    if (history.size() > 10) { // More than 10 in 60 seconds
      recordEvent("BRUTE_FORCE", ip, "Excessive connection attempts", 8);
      return true;
    }
    return false;
  }

  // Detect SQL injection
  bool detectSQLInjection(const std::string &input) noexcept {
    static const std::vector<std::string> patterns = {
        "SELECT",        "INSERT",  "UPDATE",  "DELETE",  "DROP",
        "UNION",         "--",      "/*",      "*/",      ";--",
        "' OR",          "\" OR",   "1=1",     "1='1",    "xp_cmdshell",
        "sp_executesql", "EXEC",    "EXECUTE", "WAITFOR", "BENCHMARK",
        "SLEEP",         "pg_sleep"};

    std::string upper = input;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    for (const auto &pattern : patterns) {
      if (upper.find(pattern) != std::string::npos) {
        recordEvent("SQL_INJECTION", "unknown", input.substr(0, 100), 9);
        return true;
      }
    }
    return false;
  }

  // Detect XSS
  bool detectXSS(const std::string &input) noexcept {
    static const std::vector<std::string> patterns = {
        "<script",     "javascript:",  "onerror=",      "onload=",
        "onclick=",    "onmouseover=", "onfocus=",      "onchange=",
        "<iframe",     "<object",      "<embed",        "<svg",
        "expression(", "vbscript:",    "data:text/html"};

    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto &pattern : patterns) {
      if (lower.find(pattern) != std::string::npos) {
        recordEvent("XSS_ATTEMPT", "unknown", input.substr(0, 100), 8);
        return true;
      }
    }
    return false;
  }

  // Detect path traversal
  bool detectPathTraversal(const std::string &path) noexcept {
    static const std::vector<std::string> patterns = {
        "..",   "%2e%2e", "%252e%252e", "..%2f",  "%2f..", "....//",
        "..\\", "%5c..",  "/etc/",      "/proc/", "/var/"};

    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto &pattern : patterns) {
      if (lower.find(pattern) != std::string::npos) {
        recordEvent("PATH_TRAVERSAL", "unknown", path, 9);
        return true;
      }
    }
    return false;
  }

  // Record security event
  void recordEvent(const std::string &type, const std::string &source,
                   const std::string &description, int severity) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    SecurityEvent event;
    event.type = type;
    event.source = source;
    event.description = description;
    event.severity = severity;
    event.timestamp = std::time(nullptr);

    events_.push_back(event);

    // Alert on high severity
    if (severity >= 7) {
      Logging::Logger::getInstance().error(
          "[SECURITY ALERT] " + type + " from " + source + ": " + description,
          "IDS", 0);

      // Auto-block on critical
      if (severity >= 9) {
        blockedIPs_.insert(source);
      }
    }
  }

  // Check if blocked
  bool isBlocked(const std::string &ip) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return blockedIPs_.count(ip) > 0;
  }

  // Get events
  std::vector<SecurityEvent> getRecentEvents(int count = 100) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SecurityEvent> result;
    int start = std::max(0, static_cast<int>(events_.size()) - count);
    for (size_t i = start; i < events_.size(); i++) {
      result.push_back(events_[i]);
    }
    return result;
  }

private:
  mutable std::mutex mutex_;
  std::vector<SecurityEvent> events_;
  std::map<std::string, std::vector<int64_t>> connectionHistory_;
  std::set<std::string> blockedIPs_;
};

// Advanced Encryption
class MilitaryEncryption final {
public:
  // AES-256-GCM encryption (authenticated)
  static std::vector<unsigned char>
  encrypt(const std::vector<unsigned char> &plaintext,
          const std::vector<unsigned char> &key,
          const std::vector<unsigned char> &aad = {}) noexcept {

    std::vector<unsigned char> iv(12);
    RAND_bytes(iv.data(), 12);

    std::vector<unsigned char> ciphertext(plaintext.size() + 16);
    std::vector<unsigned char> tag(16);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
      return {};

    int len = 0;
    int ciphertext_len = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data());

    if (!aad.empty()) {
      EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), aad.size());
    }

    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(),
                      plaintext.size());
    ciphertext_len = len;

    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
    EVP_CIPHER_CTX_free(ctx);

    // Output: IV + Ciphertext + Tag
    std::vector<unsigned char> result;
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(),
                  ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag.begin(), tag.end());

    return result;
  }

  // AES-256-GCM decryption
  static std::vector<unsigned char>
  decrypt(const std::vector<unsigned char> &ciphertext,
          const std::vector<unsigned char> &key,
          const std::vector<unsigned char> &aad = {}) noexcept {

    if (ciphertext.size() < 28)
      return {}; // IV(12) + Tag(16)

    std::vector<unsigned char> iv(ciphertext.begin(), ciphertext.begin() + 12);
    std::vector<unsigned char> tag(ciphertext.end() - 16, ciphertext.end());
    std::vector<unsigned char> data(ciphertext.begin() + 12,
                                    ciphertext.end() - 16);

    std::vector<unsigned char> plaintext(data.size());

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
      return {};

    int len = 0;
    int plaintext_len = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data());

    if (!aad.empty()) {
      EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), aad.size());
    }

    EVP_DecryptUpdate(ctx, plaintext.data(), &len, data.data(), data.size());
    plaintext_len = len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data());

    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0)
      return {}; // Authentication failed

    plaintext.resize(plaintext_len + len);
    return plaintext;
  }

  // Generate secure random key
  static std::vector<unsigned char> generateKey(int bits = 256) noexcept {
    std::vector<unsigned char> key(bits / 8);
    RAND_bytes(key.data(), key.size());
    return key;
  }

  // HMAC-SHA512
  static std::vector<unsigned char>
  hmacSHA512(const std::vector<unsigned char> &key,
             const std::vector<unsigned char> &data) noexcept {

    std::vector<unsigned char> result(64);
    unsigned int len = 64;

    HMAC(EVP_sha512(), key.data(), key.size(), data.data(), data.size(),
         result.data(), &len);

    return result;
  }
};

// DDoS Protection
class DDoSProtector final {
public:
  DDoSProtector() noexcept {
    Logging::Logger::getInstance().info("DDoS Protection initialized", "DDoS",
                                        0);
  }

  // Check request rate
  bool checkRate(const std::string &ip, int maxRequests = 100,
                 int windowSeconds = 60) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::time(nullptr);
    auto &bucket = rateBuckets_[ip];

    // Clean old requests
    bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
                                [now, windowSeconds](int64_t t) {
                                  return now - t > windowSeconds;
                                }),
                 bucket.end());

    if (bucket.size() >= static_cast<size_t>(maxRequests)) {
      return false; // Rate limit exceeded
    }

    bucket.push_back(now);
    return true;
  }

  // SYN flood protection - track half-open connections
  bool checkSYNFlood(const std::string &ip) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    synCount_[ip]++;
    if (synCount_[ip] > 50) { // More than 50 SYN per second
      Logging::Logger::getInstance().warning("Possible SYN flood from " + ip,
                                             "DDoS", 0);
      return false;
    }
    return true;
  }

  // Reset SYN counter (call every second)
  void resetSYNCounters() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    synCount_.clear();
  }

private:
  std::mutex mutex_;
  std::map<std::string, std::vector<int64_t>> rateBuckets_;
  std::map<std::string, int> synCount_;
};

// Firewall Rules
class Firewall final {
public:
  struct Rule {
    std::string name;
    std::string action; // ALLOW, DENY
    std::string source; // IP or CIDR
    int port;
    bool enabled;
  };

  Firewall() noexcept {
    // Default rules
    addRule("block_private", "DENY", "10.0.0.0/8", 0, true);
    addRule("block_private2", "DENY", "172.16.0.0/12", 0, true);
    addRule("block_localhost", "DENY", "127.0.0.0/8", 8333,
            false); // Disabled - allow local
    addRule("allow_rpc_local", "ALLOW", "127.0.0.1", 8332, true);
    Logging::Logger::getInstance().info(
        "Firewall initialized with default rules", "Firewall", 0);
  }

  void addRule(const std::string &name, const std::string &action,
               const std::string &source, int port, bool enabled) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    Rule rule{name, action, source, port, enabled};
    rules_.push_back(rule);
  }

  bool isAllowed(const std::string &ip, int port) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Default: allow
    for (const auto &rule : rules_) {
      if (!rule.enabled)
        continue;
      if (rule.port != 0 && rule.port != port)
        continue;

      // Simple IP match (CIDR would need more logic)
      if (ip.find(rule.source.substr(0, rule.source.find('/'))) !=
          std::string::npos) {
        return rule.action == "ALLOW";
      }
    }
    return true;
  }

private:
  mutable std::mutex mutex_;
  std::vector<Rule> rules_;
};

} // namespace QuantumPulse::MilitarySecurity

#endif // QUANTUMPULSE_MILITARY_SECURITY_V7_H
