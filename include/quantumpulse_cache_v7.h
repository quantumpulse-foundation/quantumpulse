#ifndef QUANTUMPULSE_CACHE_V7_H
#define QUANTUMPULSE_CACHE_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace QuantumPulse::Cache {

// Cache configuration
struct CacheConfig {
  std::string host{"localhost"};
  int port{6379};
  int defaultTTL{300}; // 5 minutes
  int maxEntries{10000};
};

// Cache entry
struct CacheEntry {
  std::string value;
  std::chrono::steady_clock::time_point expiry;
};

// Redis-compatible cache (simulated for standalone use)
class RedisCache final {
public:
  explicit RedisCache(const CacheConfig &config = CacheConfig{}) noexcept
      : config_(config) {
    Logging::Logger::getInstance().info(
        "Redis cache initialized: " + config.host + ":" +
            std::to_string(config.port),
        "Cache", 0);
  }

  // Set value with TTL
  bool set(const std::string &key, const std::string &value,
           int ttlSeconds = -1) noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    if (cache_.size() >= static_cast<size_t>(config_.maxEntries)) {
      evictExpired();
    }

    int ttl = (ttlSeconds > 0) ? ttlSeconds : config_.defaultTTL;
    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl);

    cache_[key] = {value, expiry};
    setCount_++;
    return true;
  }

  // Get value
  [[nodiscard]] std::optional<std::string>
  get(const std::string &key) noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
      missCount_++;
      return std::nullopt;
    }

    if (std::chrono::steady_clock::now() > it->second.expiry) {
      cache_.erase(it);
      missCount_++;
      return std::nullopt;
    }

    hitCount_++;
    return it->second.value;
  }

  // Delete key
  bool del(const std::string &key) noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.erase(key) > 0;
  }

  // Check if key exists
  [[nodiscard]] bool exists(const std::string &key) noexcept {
    return get(key).has_value();
  }

  // Set expiry
  bool expire(const std::string &key, int seconds) noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(key);
    if (it == cache_.end())
      return false;

    it->second.expiry =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    return true;
  }

  // Increment value
  [[nodiscard]] int64_t incr(const std::string &key) noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(key);
    int64_t val = 0;

    if (it != cache_.end()) {
      try {
        val = std::stoll(it->second.value);
      } catch (...) {
      }
    }

    val++;
    auto expiry = std::chrono::steady_clock::now() +
                  std::chrono::seconds(config_.defaultTTL);
    cache_[key] = {std::to_string(val), expiry};
    return val;
  }

  // Flush all
  void flushAll() noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_.clear();
  }

  // Get stats
  [[nodiscard]] size_t size() const noexcept {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.size();
  }

  [[nodiscard]] size_t getHitCount() const noexcept { return hitCount_; }
  [[nodiscard]] size_t getMissCount() const noexcept { return missCount_; }
  [[nodiscard]] double getHitRate() const noexcept {
    size_t total = hitCount_ + missCount_;
    return total > 0 ? static_cast<double>(hitCount_) / total : 0.0;
  }

  // Cache API responses
  void cacheAPIResponse(const std::string &endpoint,
                        const std::string &response) noexcept {
    set("api:" + endpoint, response, 60); // 1 minute cache
  }

  [[nodiscard]] std::optional<std::string>
  getCachedAPIResponse(const std::string &endpoint) noexcept {
    return get("api:" + endpoint);
  }

private:
  CacheConfig config_;
  std::map<std::string, CacheEntry> cache_;
  mutable std::mutex cacheMutex_;
  size_t hitCount_{0};
  size_t missCount_{0};
  size_t setCount_{0};

  void evictExpired() noexcept {
    auto now = std::chrono::steady_clock::now();
    for (auto it = cache_.begin(); it != cache_.end();) {
      if (now > it->second.expiry) {
        it = cache_.erase(it);
      } else {
        ++it;
      }
    }
  }
};

} // namespace QuantumPulse::Cache

#endif // QUANTUMPULSE_CACHE_V7_H
