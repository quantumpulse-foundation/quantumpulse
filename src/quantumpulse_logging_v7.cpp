// QuantumPulse Logging Source Implementation v7.0
// Thread-safe async logging with queue-based batching

#include "quantumpulse_logging_v7.h"
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace QuantumPulse::Logging {

// Singleton instance
Logger &Logger::getInstance() {
  static Logger instance;
  return instance;
}

// Constructor - create log directories and open file
Logger::Logger() {
  std::filesystem::create_directories("logs/audit");
  std::filesystem::create_directories("logs/debug");
  std::filesystem::create_directories("logs/critical");

  auto now = std::time(nullptr);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");

  std::string logPath = "logs/audit/quantumpulse_" + ss.str() + ".log";
  logFile.open(logPath, std::ios::app);

  if (logFile.is_open()) {
    logFile << "=== QuantumPulse v7.0 Log Started ===" << std::endl;
    logFile << "Timestamp: " << getTimestamp() << std::endl;
    logFile << "======================================" << std::endl;
  }

  // Start async flush thread
  flushThread = std::thread([this]() {
    while (!shouldStop.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      flushQueue();
    }
  });
}

// Destructor - flush and close
Logger::~Logger() {
  shouldStop.store(true);
  if (flushThread.joinable()) {
    flushThread.join();
  }
  flushQueue();
  if (logFile.is_open()) {
    logFile << "=== Log Ended ===" << std::endl;
    logFile.close();
  }
}

// Get formatted timestamp
std::string Logger::getTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  ss << "." << std::setfill('0') << std::setw(3) << ms.count();
  return ss.str();
}

// Log message with level, module, and shard
void Logger::log(const std::string &message, LogLevel level,
                 const std::string &module, int shardId) {
  if (!isLoggingEnabled.load())
    return;

  std::string levelStr;
  switch (level) {
  case DEBUG:
    levelStr = "DEBUG";
    break;
  case INFO:
    levelStr = "INFO";
    break;
  case ERROR:
    levelStr = "ERROR";
    break;
  case CRITICAL:
    levelStr = "CRITICAL";
    break;
  case AUDIT:
    levelStr = "AUDIT";
    break;
  }

  std::string logEntry = "[" + getTimestamp() +
                         "] "
                         "[" +
                         levelStr +
                         "] "
                         "[" +
                         module +
                         "] "
                         "[Shard:" +
                         std::to_string(shardId) + "] " + message + "\n";

  {
    std::lock_guard<std::mutex> lock(logMutex);
    logQueue.push(logEntry);
  }

  // Immediate flush for critical logs
  if (level == CRITICAL || level == AUDIT) {
    flushQueue();
  }

  // Batch flush when queue is large
  if (logQueue.size() > 100) {
    flushQueue();
  }
}

// Flush queue to file
void Logger::flushQueue() {
  std::lock_guard<std::mutex> lock(logMutex);

  while (!logQueue.empty()) {
    if (logFile.is_open()) {
      logFile << logQueue.front();
    }
    logQueue.pop();
  }

  if (logFile.is_open()) {
    logFile.flush();
  }
}

} // namespace QuantumPulse::Logging
