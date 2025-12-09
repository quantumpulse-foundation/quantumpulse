#ifndef QUANTUMPULSE_LOGGING_V7_H
#define QUANTUMPULSE_LOGGING_V7_H

#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace QuantumPulse::Logging {

// Log levels with severity ordering
enum class LogLevel : uint8_t {
  DEBUG = 0,
  INFO = 1,
  WARNING = 2,
  ERROR = 3,
  CRITICAL = 4,
  AUDIT = 5
};

// Convert log level to string
[[nodiscard]] constexpr std::string_view
logLevelToString(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARNING:
    return "WARNING";
  case LogLevel::ERROR:
    return "ERROR";
  case LogLevel::CRITICAL:
    return "CRITICAL";
  case LogLevel::AUDIT:
    return "AUDIT";
  default:
    return "UNKNOWN";
  }
}

// Thread-safe, high-performance logger with async flushing
class Logger final {
public:
  // Singleton access
  [[nodiscard]] static Logger &getInstance() noexcept {
    static Logger instance;
    return instance;
  }

  // Delete copy/move
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  // Main log function with modern interface
  void log(std::string_view message, LogLevel level, std::string_view module,
           int shardId) noexcept {
    if (!isLoggingEnabled_.load(std::memory_order_relaxed)) {
      return;
    }

    // Skip logs below minimum level
    if (static_cast<uint8_t>(level) <
        static_cast<uint8_t>(minLogLevel_.load())) {
      return;
    }

    try {
      std::string logEntry = formatLogEntry(message, level, module, shardId);

      {
        std::lock_guard<std::mutex> lock(queueMutex_);
        logQueue_.push(std::move(logEntry));
      }

      // Auto-flush for critical logs or when queue is large
      if (level >= LogLevel::CRITICAL || logQueue_.size() > 100) {
        flush();
      }
    } catch (...) {
      // Logging should never throw - silently fail
    }
  }

  // Convenience methods
  void debug(std::string_view msg, std::string_view module,
             int shard = 0) noexcept {
    log(msg, LogLevel::DEBUG, module, shard);
  }

  void info(std::string_view msg, std::string_view module,
            int shard = 0) noexcept {
    log(msg, LogLevel::INFO, module, shard);
  }

  void warning(std::string_view msg, std::string_view module,
               int shard = 0) noexcept {
    log(msg, LogLevel::WARNING, module, shard);
  }

  void error(std::string_view msg, std::string_view module,
             int shard = 0) noexcept {
    log(msg, LogLevel::ERROR, module, shard);
  }

  void critical(std::string_view msg, std::string_view module,
                int shard = 0) noexcept {
    log(msg, LogLevel::CRITICAL, module, shard);
  }

  void audit(std::string_view msg, std::string_view module,
             int shard = 0) noexcept {
    log(msg, LogLevel::AUDIT, module, shard);
  }

  // Manual flush
  void flush() noexcept {
    try {
      std::lock_guard<std::mutex> lock(queueMutex_);
      while (!logQueue_.empty()) {
        if (logFile_.is_open()) {
          logFile_ << logQueue_.front();
        }
        logQueue_.pop();
      }
      if (logFile_.is_open()) {
        logFile_.flush();
      }
    } catch (...) {
      // Silent fail
    }
  }

  // Configuration
  void setMinLogLevel(LogLevel level) noexcept { minLogLevel_.store(level); }

  void enable() noexcept {
    isLoggingEnabled_.store(true, std::memory_order_relaxed);
  }

  void disable() noexcept {
    isLoggingEnabled_.store(false, std::memory_order_relaxed);
  }

  // Statistics
  [[nodiscard]] size_t getQueueSize() const noexcept {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return logQueue_.size();
  }

private:
  Logger() noexcept {
    try {
      std::filesystem::create_directories("logs/audit");
      std::filesystem::create_directories("logs/debug");

      auto now = std::chrono::system_clock::now();
      auto time = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");

      std::string logPath = "logs/audit/quantumpulse_" + ss.str() + ".log";
      logFile_.open(logPath, std::ios::app);

      if (logFile_.is_open()) {
        logFile_ << "=== QuantumPulse v7.0 Log Started ===\n";
        logFile_ << "Timestamp: " << ss.str() << "\n";
        logFile_ << "=====================================\n\n";
      }
    } catch (...) {
      // Continue without logging
    }
  }

  ~Logger() noexcept {
    try {
      flush();
      if (logFile_.is_open()) {
        logFile_ << "\n=== Log Session Ended ===\n";
        logFile_.close();
      }
    } catch (...) {
      // Silent cleanup
    }
  }

  [[nodiscard]] std::string formatLogEntry(std::string_view message,
                                           LogLevel level,
                                           std::string_view module,
                                           int shardId) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
    ss << "[" << logLevelToString(level) << "]";
    ss << "[" << module << "]";
    ss << "[Shard:" << shardId << "] ";
    ss << message << "\n";

    return ss.str();
  }

  std::ofstream logFile_;
  mutable std::mutex queueMutex_;
  std::queue<std::string> logQueue_;
  std::atomic<bool> isLoggingEnabled_{true};
  std::atomic<LogLevel> minLogLevel_{LogLevel::DEBUG};
};

// Legacy compatibility - use enum values directly
constexpr auto DEBUG = LogLevel::DEBUG;
constexpr auto INFO = LogLevel::INFO;
constexpr auto WARNING = LogLevel::WARNING;
constexpr auto ERROR = LogLevel::ERROR;
constexpr auto CRITICAL = LogLevel::CRITICAL;
constexpr auto AUDIT = LogLevel::AUDIT;

} // namespace QuantumPulse::Logging

#endif // QUANTUMPULSE_LOGGING_V7_H
