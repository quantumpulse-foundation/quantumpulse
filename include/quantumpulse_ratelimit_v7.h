#ifndef QUANTUMPULSE_RATELIMIT_V7_H
#define QUANTUMPULSE_RATELIMIT_V7_H

#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace QuantumPulse::Security {

// Rate limiting configuration
struct RateLimitConfig {
  int requestsPerSecond{100};
  int requestsPerMinute{3000};
  int requestsPerHour{50000};
  int burstSize{50};
};

// Token bucket rate limiter
class RateLimiter final {
public:
  explicit RateLimiter(
      const RateLimitConfig &config = RateLimitConfig{}) noexcept
      : config_(config) {}

  // Check if request is allowed
  [[nodiscard]] bool allowRequest(const std::string &clientId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto &bucket = buckets_[clientId];

    // Refill tokens
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - bucket.lastRefill)
                       .count();
    double tokensToAdd = (elapsed / 1000.0) * config_.requestsPerSecond;
    bucket.tokens = std::min(static_cast<double>(config_.burstSize),
                             bucket.tokens + tokensToAdd);
    bucket.lastRefill = now;

    // Check tokens
    if (bucket.tokens >= 1.0) {
      bucket.tokens -= 1.0;
      bucket.totalRequests++;
      return true;
    }

    bucket.blockedRequests++;
    return false;
  }

  // Get remaining requests
  [[nodiscard]] int
  getRemainingRequests(const std::string &clientId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(clientId);
    return it != buckets_.end() ? static_cast<int>(it->second.tokens)
                                : config_.burstSize;
  }

  // Reset client
  void resetClient(const std::string &clientId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.erase(clientId);
  }

  // Get stats
  [[nodiscard]] std::pair<size_t, size_t>
  getStats(const std::string &clientId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(clientId);
    if (it != buckets_.end()) {
      return {it->second.totalRequests, it->second.blockedRequests};
    }
    return {0, 0};
  }

private:
  struct TokenBucket {
    double tokens{50};
    std::chrono::steady_clock::time_point lastRefill{
        std::chrono::steady_clock::now()};
    size_t totalRequests{0};
    size_t blockedRequests{0};
  };

  RateLimitConfig config_;
  std::map<std::string, TokenBucket> buckets_;
  mutable std::mutex mutex_;
};

// IP-based blocker
class IPBlocker final {
public:
  void blockIP(const std::string &ip, int durationSeconds = 3600) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto expiry = std::chrono::steady_clock::now() +
                  std::chrono::seconds(durationSeconds);
    blockedIPs_[ip] = expiry;
  }

  void unblockIP(const std::string &ip) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    blockedIPs_.erase(ip);
  }

  [[nodiscard]] bool isBlocked(const std::string &ip) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blockedIPs_.find(ip);
    if (it == blockedIPs_.end())
      return false;
    if (std::chrono::steady_clock::now() > it->second) {
      blockedIPs_.erase(it);
      return false;
    }
    return true;
  }

  [[nodiscard]] std::vector<std::string> getBlockedIPs() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto &[ip, expiry] : blockedIPs_) {
      result.push_back(ip);
    }
    return result;
  }

private:
  std::map<std::string, std::chrono::steady_clock::time_point> blockedIPs_;
  mutable std::mutex mutex_;
};

} // namespace QuantumPulse::Security

#endif // QUANTUMPULSE_RATELIMIT_V7_H
