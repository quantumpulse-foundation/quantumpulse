// QuantumPulse Upgrades Source Implementation v7.0
// Hot upgrades with rollback capability

#include "quantumpulse_upgrades_v7.h"
#include "quantumpulse_logging_v7.h"

namespace QuantumPulse::Upgrades {

// UpgradeManager constructor
UpgradeManager::UpgradeManager() : currentVersion("7.0.0") {
  versionHistory.push_back(currentVersion);

  Logging::Logger::getInstance().log("UpgradeManager initialized at version " +
                                         currentVersion,
                                     Logging::INFO, "Upgrades", 0);
}

// Apply update
void UpgradeManager::applyUpdate(const std::string &updateData) {
  std::lock_guard<std::mutex> lock(upgradeMutex);

  if (updateData.empty()) {
    Logging::Logger::getInstance().log("Empty update data - skipped",
                                       Logging::WARNING, "Upgrades", 0);
    return;
  }

  // Store previous version for rollback
  versionHistory.push_back(currentVersion);
  appliedUpdates.push_back(updateData);

  // Increment patch version
  size_t lastDot = currentVersion.rfind('.');
  if (lastDot != std::string::npos) {
    int patch = std::stoi(currentVersion.substr(lastDot + 1));
    currentVersion =
        currentVersion.substr(0, lastDot + 1) + std::to_string(patch + 1);
  }

  Logging::Logger::getInstance().log("Update applied: " + updateData +
                                         " -> Version " + currentVersion,
                                     Logging::INFO, "Upgrades", 0);
}

// Rollback to previous version
void UpgradeManager::rollback() {
  std::lock_guard<std::mutex> lock(upgradeMutex);

  if (versionHistory.size() <= 1) {
    Logging::Logger::getInstance().log("Cannot rollback - no previous version",
                                       Logging::WARNING, "Upgrades", 0);
    return;
  }

  // Remove current version
  versionHistory.pop_back();
  if (!appliedUpdates.empty()) {
    appliedUpdates.pop_back();
  }

  // Restore previous version
  currentVersion = versionHistory.back();

  Logging::Logger::getInstance().log("Rolled back to version " + currentVersion,
                                     Logging::INFO, "Upgrades", 0);
}

// Check compatibility
bool UpgradeManager::checkCompatibility() const {
  // Check if current version is compatible with v7.x
  return currentVersion.find("7.") == 0;
}

// Get current version
std::string UpgradeManager::getVersion() const { return currentVersion; }

// Get update history
std::vector<std::string> UpgradeManager::getUpdateHistory() const {
  return appliedUpdates;
}

// Schedule auto-update
void UpgradeManager::scheduleAutoUpdate(int intervalSeconds) {
  std::lock_guard<std::mutex> lock(upgradeMutex);

  autoUpdateInterval = intervalSeconds;

  Logging::Logger::getInstance().log("Auto-update scheduled every " +
                                         std::to_string(intervalSeconds) +
                                         " seconds",
                                     Logging::INFO, "Upgrades", 0);
}

} // namespace QuantumPulse::Upgrades
