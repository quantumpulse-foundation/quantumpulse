#ifndef QUANTUMPULSE_AUDIT_V7_H
#define QUANTUMPULSE_AUDIT_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace QuantumPulse::Audit {

// Audit event types
enum class AuditEventType {
  LOGIN,
  LOGOUT,
  LOGIN_FAILED,
  WALLET_CREATED,
  WALLET_DELETED,
  WALLET_EXPORTED,
  TRANSACTION_SENT,
  TRANSACTION_RECEIVED,
  TRANSACTION_FAILED,
  ORDER_PLACED,
  ORDER_CANCELLED,
  ORDER_FILLED,
  SETTINGS_CHANGED,
  PASSWORD_CHANGED,
  TWO_FACTOR_ENABLED,
  TWO_FACTOR_DISABLED,
  API_ACCESS,
  ADMIN_ACTION,
  SECURITY_ALERT
};

// Audit entry
struct AuditEntry {
  int64_t id;
  int64_t timestamp;
  std::string userId;
  AuditEventType eventType;
  std::string action;
  std::string details;
  std::string ipAddress;
  std::string userAgent;
  bool success;
};

// Audit Logger
class AuditLogger final {
public:
  explicit AuditLogger(const std::string &logPath = "logs/audit") noexcept
      : logPath_(logPath) {
    std::filesystem::create_directories(logPath_);
    Logging::Logger::getInstance().info("Audit Logger initialized", "Audit", 0);
  }

  // Log audit event
  void log(const std::string &userId, AuditEventType type,
           const std::string &action, const std::string &details = "",
           bool success = true, const std::string &ip = "",
           const std::string &userAgent = "") noexcept {

    std::lock_guard<std::mutex> lock(auditMutex_);

    AuditEntry entry;
    entry.id = nextId_++;
    entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    entry.userId = userId;
    entry.eventType = type;
    entry.action = action;
    entry.details = details;
    entry.ipAddress = ip;
    entry.userAgent = userAgent;
    entry.success = success;

    entries_.push_back(entry);
    writeToFile(entry);

    // Keep only last 10000 entries in memory
    if (entries_.size() > 10000) {
      entries_.erase(entries_.begin(), entries_.begin() + 1000);
    }
  }

  // Get recent entries
  [[nodiscard]] std::vector<AuditEntry>
  getRecentEntries(int count = 100) const noexcept {
    std::lock_guard<std::mutex> lock(auditMutex_);

    std::vector<AuditEntry> result;
    int start = std::max(0, static_cast<int>(entries_.size()) - count);
    for (size_t i = start; i < entries_.size(); ++i) {
      result.push_back(entries_[i]);
    }
    return result;
  }

  // Get entries by user
  [[nodiscard]] std::vector<AuditEntry>
  getEntriesByUser(const std::string &userId, int count = 100) const noexcept {
    std::lock_guard<std::mutex> lock(auditMutex_);

    std::vector<AuditEntry> result;
    for (auto it = entries_.rbegin();
         it != entries_.rend() && result.size() < static_cast<size_t>(count);
         ++it) {
      if (it->userId == userId) {
        result.push_back(*it);
      }
    }
    return result;
  }

  // Export to CSV
  [[nodiscard]] std::string exportToCSV(int count = 1000) const noexcept {
    std::lock_guard<std::mutex> lock(auditMutex_);

    std::stringstream ss;
    ss << "ID,Timestamp,UserID,EventType,Action,Details,IP,Success\n";

    int start = std::max(0, static_cast<int>(entries_.size()) - count);
    for (size_t i = start; i < entries_.size(); ++i) {
      const auto &e = entries_[i];
      ss << e.id << "," << e.timestamp << "," << e.userId << ","
         << static_cast<int>(e.eventType) << "," << e.action << ","
         << "\"" << e.details << "\"," << e.ipAddress << ","
         << (e.success ? "true" : "false") << "\n";
    }

    return ss.str();
  }

  // Export to JSON
  [[nodiscard]] std::string exportToJSON(int count = 100) const noexcept {
    std::lock_guard<std::mutex> lock(auditMutex_);

    std::stringstream ss;
    ss << "[\n";

    int start = std::max(0, static_cast<int>(entries_.size()) - count);
    bool first = true;
    for (size_t i = start; i < entries_.size(); ++i) {
      if (!first)
        ss << ",\n";
      first = false;

      const auto &e = entries_[i];
      ss << "  {\"id\":" << e.id << ",\"timestamp\":" << e.timestamp
         << ",\"userId\":\"" << e.userId << "\",\"action\":\"" << e.action
         << "\",\"success\":" << (e.success ? "true" : "false") << "}";
    }

    ss << "\n]";
    return ss.str();
  }

  // Convenience methods
  void logLogin(const std::string &userId, bool success,
                const std::string &ip) noexcept {
    log(userId, success ? AuditEventType::LOGIN : AuditEventType::LOGIN_FAILED,
        success ? "User logged in" : "Login failed", "", success, ip);
  }

  void logTransaction(const std::string &userId, const std::string &txId,
                      double amount, bool sent) noexcept {
    log(userId,
        sent ? AuditEventType::TRANSACTION_SENT
             : AuditEventType::TRANSACTION_RECEIVED,
        sent ? "Transaction sent" : "Transaction received",
        "TX: " + txId + " Amount: " + std::to_string(amount) + " QP");
  }

  void logOrderPlaced(const std::string &userId, const std::string &orderId,
                      const std::string &side, double price,
                      double qty) noexcept {
    log(userId, AuditEventType::ORDER_PLACED, "Order placed",
        "Order: " + orderId + " " + side + " " + std::to_string(qty) + " @ $" +
            std::to_string(price));
  }

  void logSecurityAlert(const std::string &userId,
                        const std::string &alert) noexcept {
    log(userId, AuditEventType::SECURITY_ALERT, "Security alert", alert, false);
  }

  [[nodiscard]] size_t getEntryCount() const noexcept {
    return entries_.size();
  }

private:
  std::string logPath_;
  std::vector<AuditEntry> entries_;
  mutable std::mutex auditMutex_;
  int64_t nextId_{1};

  void writeToFile(const AuditEntry &entry) noexcept {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::stringstream filename;
    filename << logPath_ << "/audit_" << std::put_time(&tm, "%Y%m%d") << ".log";

    std::ofstream file(filename.str(), std::ios::app);
    if (file.is_open()) {
      file << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
           << entry.userId << " | " << entry.action << " | " << entry.details
           << " | " << (entry.success ? "OK" : "FAIL") << "\n";
      file.close();
    }
  }
};

} // namespace QuantumPulse::Audit

#endif // QUANTUMPULSE_AUDIT_V7_H
