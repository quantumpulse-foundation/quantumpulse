#ifndef QUANTUMPULSE_METRICS_V7_H
#define QUANTUMPULSE_METRICS_V7_H

#include "quantumpulse_logging_v7.h"
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

namespace QuantumPulse::Metrics {

// Metric types
enum class MetricType { COUNTER, GAUGE, HISTOGRAM };

// Single metric
struct Metric {
  std::string name;
  std::string help;
  MetricType type;
  std::atomic<double> value{0};
  std::map<std::string, std::string> labels;
};

// Prometheus-compatible metrics exporter
class PrometheusExporter final {
public:
  PrometheusExporter() noexcept {
    initializeMetrics();
    Logging::Logger::getInstance().info(
        "Prometheus metrics exporter initialized", "Metrics", 0);
  }

  // Register counter
  void registerCounter(const std::string &name,
                       const std::string &help) noexcept {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_[name] = {name, help, MetricType::COUNTER, {0}, {}};
  }

  // Register gauge
  void registerGauge(const std::string &name,
                     const std::string &help) noexcept {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_[name] = {name, help, MetricType::GAUGE, {0}, {}};
  }

  // Increment counter
  void incrementCounter(const std::string &name, double delta = 1.0) noexcept {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
      double current = it->second.value.load();
      it->second.value.store(current + delta);
    }
  }

  // Set gauge value
  void setGauge(const std::string &name, double value) noexcept {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
      it->second.value.store(value);
    }
  }

  // Get metric value
  [[nodiscard]] double getMetricValue(const std::string &name) const noexcept {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
      return it->second.value.load();
    }
    return 0;
  }

  // Export in Prometheus format
  [[nodiscard]] std::string exportMetrics() const noexcept {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    std::stringstream ss;

    for (const auto &[name, metric] : metrics_) {
      // HELP line
      ss << "# HELP " << metric.name << " " << metric.help << "\n";

      // TYPE line
      ss << "# TYPE " << metric.name << " ";
      switch (metric.type) {
      case MetricType::COUNTER:
        ss << "counter";
        break;
      case MetricType::GAUGE:
        ss << "gauge";
        break;
      case MetricType::HISTOGRAM:
        ss << "histogram";
        break;
      }
      ss << "\n";

      // Value line
      ss << metric.name;
      if (!metric.labels.empty()) {
        ss << "{";
        bool first = true;
        for (const auto &[k, v] : metric.labels) {
          if (!first)
            ss << ",";
          ss << k << "=\"" << v << "\"";
          first = false;
        }
        ss << "}";
      }
      ss << " " << metric.value.load() << "\n\n";
    }

    return ss.str();
  }

  // Update blockchain metrics
  void updateBlockchainMetrics(int chainLength, double minedCoins,
                               int activePeers, double hashrate) noexcept {
    setGauge("quantumpulse_chain_length", chainLength);
    setGauge("quantumpulse_mined_coins_total", minedCoins);
    setGauge("quantumpulse_active_peers", activePeers);
    setGauge("quantumpulse_hashrate_mhs", hashrate);
  }

  // Increment transaction counter
  void recordTransaction() noexcept {
    incrementCounter("quantumpulse_transactions_total");
  }

  // Increment block counter
  void recordBlock() noexcept {
    incrementCounter("quantumpulse_blocks_mined_total");
  }

  // Record API request
  void recordAPIRequest(const std::string &endpoint) noexcept {
    incrementCounter("quantumpulse_api_requests_total");
    incrementCounter("quantumpulse_api_" + endpoint + "_total");
  }

  // Record WebSocket connection
  void recordWSConnection() noexcept {
    incrementCounter("quantumpulse_ws_connections_total");
  }

private:
  mutable std::mutex metricsMutex_;
  std::map<std::string, Metric> metrics_;

  void initializeMetrics() noexcept {
    // Blockchain metrics
    registerGauge("quantumpulse_chain_length",
                  "Current blockchain length in blocks");
    registerGauge("quantumpulse_mined_coins_total", "Total coins mined so far");
    registerGauge("quantumpulse_active_peers", "Number of connected peers");
    registerGauge("quantumpulse_hashrate_mhs", "Network hashrate in MH/s");
    registerGauge("quantumpulse_difficulty", "Current mining difficulty");
    registerGauge("quantumpulse_mempool_size",
                  "Number of pending transactions");

    // Counters
    registerCounter("quantumpulse_transactions_total",
                    "Total transactions processed");
    registerCounter("quantumpulse_blocks_mined_total", "Total blocks mined");
    registerCounter("quantumpulse_api_requests_total", "Total API requests");
    registerCounter("quantumpulse_ws_connections_total",
                    "Total WebSocket connections");

    // Price
    registerGauge("quantumpulse_price_usd", "Current QP price in USD");
    setGauge("quantumpulse_price_usd", 600000);
  }
};

} // namespace QuantumPulse::Metrics

#endif // QUANTUMPULSE_METRICS_V7_H
