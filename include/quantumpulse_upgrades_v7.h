#ifndef QUANTUMPULSE_UPGRADES_V7_H
#define QUANTUMPULSE_UPGRADES_V7_H

#include "quantumpulse_logging_v7.h"
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace QuantumPulse::Upgrades {

// Upgrade configuration
struct UpgradeConfig {
  static constexpr int AUTO_UPDATE_INTERVAL_SEC = 1296000; // 15 days
  static constexpr int MAX_ROLLBACK_VERSIONS = 10;
};

// Thread-safe upgrade manager
class UpgradeManager final {
public:
  UpgradeManager() noexcept
      : currentVersion_("7.0.0"),
        autoUpdateInterval_(UpgradeConfig::AUTO_UPDATE_INTERVAL_SEC) {
    versionHistory_.push_back(currentVersion_);

    Logging::Logger::getInstance().info(
        "UpgradeManager initialized at version " + currentVersion_, "Upgrades",
        0);
  }

  // Non-copyable
  UpgradeManager(const UpgradeManager &) = delete;
  UpgradeManager &operator=(const UpgradeManager &) = delete;

  // Apply update
  void applyUpdate(std::string_view updateData) noexcept {
    std::lock_guard<std::mutex> lock(upgradeMutex_);

    if (updateData.empty()) {
      Logging::Logger::getInstance().warning("Empty update data - skipped",
                                             "Upgrades", 0);
      return;
    }

    // Store for rollback
    if (versionHistory_.size() >= UpgradeConfig::MAX_ROLLBACK_VERSIONS) {
      versionHistory_.erase(versionHistory_.begin());
    }
    versionHistory_.push_back(currentVersion_);
    appliedUpdates_.emplace_back(updateData);

    // Increment version
    incrementVersion();

    Logging::Logger::getInstance().info(
        "Update applied -> Version " + currentVersion_, "Upgrades", 0);
  }

  // Rollback to previous version
  [[nodiscard]] bool rollback() noexcept {
    std::lock_guard<std::mutex> lock(upgradeMutex_);

    if (versionHistory_.size() <= 1) {
      Logging::Logger::getInstance().warning(
          "Cannot rollback - no previous version", "Upgrades", 0);
      return false;
    }

    versionHistory_.pop_back();
    if (!appliedUpdates_.empty()) {
      appliedUpdates_.pop_back();
    }

    currentVersion_ = versionHistory_.back();

    Logging::Logger::getInstance().info(
        "Rolled back to version " + currentVersion_, "Upgrades", 0);
    return true;
  }

  // Check compatibility
  [[nodiscard]] bool checkCompatibility() const noexcept {
    return currentVersion_.find("7.") == 0;
  }

  // Getters
  [[nodiscard]] std::string getVersion() const noexcept {
    std::lock_guard<std::mutex> lock(upgradeMutex_);
    return currentVersion_;
  }

  [[nodiscard]] std::vector<std::string> getUpdateHistory() const noexcept {
    std::lock_guard<std::mutex> lock(upgradeMutex_);
    return appliedUpdates_;
  }

  [[nodiscard]] size_t getVersionHistorySize() const noexcept {
    std::lock_guard<std::mutex> lock(upgradeMutex_);
    return versionHistory_.size();
  }

  // Schedule auto-update
  void scheduleAutoUpdate(int intervalSeconds) noexcept {
    std::lock_guard<std::mutex> lock(upgradeMutex_);

    autoUpdateInterval_ = intervalSeconds;

    Logging::Logger::getInstance().info("Auto-update scheduled every " +
                                            std::to_string(intervalSeconds) +
                                            " seconds",
                                        "Upgrades", 0);
  }

private:
  std::string currentVersion_;
  std::vector<std::string> versionHistory_;
  std::vector<std::string> appliedUpdates_;
  int autoUpdateInterval_;
  mutable std::mutex upgradeMutex_;

  void incrementVersion() noexcept {
    size_t lastDot = currentVersion_.rfind('.');
    if (lastDot != std::string::npos) {
      try {
        int patch = std::stoi(currentVersion_.substr(lastDot + 1));
        currentVersion_ =
            currentVersion_.substr(0, lastDot + 1) + std::to_string(patch + 1);
      } catch (...) {
        // Keep current version on error
      }
    }
  }
};

} // namespace QuantumPulse::Upgrades

#endif // QUANTUMPULSE_UPGRADES_V7_H
