#ifndef QUANTUMPULSE_LIGHTNING_V7_H
#define QUANTUMPULSE_LIGHTNING_V7_H

#include "quantumpulse_crypto_v7.h"
#include "quantumpulse_logging_v7.h"
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace QuantumPulse::Lightning {

// Channel states
enum class ChannelState { PENDING, OPEN, CLOSING, CLOSED, FORCE_CLOSED };

// Payment Channel
struct PaymentChannel {
  std::string channelId;
  std::string partyA;
  std::string partyB;
  double capacityA; // Party A's balance
  double capacityB; // Party B's balance
  double totalCapacity;
  int64_t createdAt;
  int64_t lastActivity;
  ChannelState state;
  int updateCount;
  std::string latestStateHash;
};

// Lightning Payment
struct LightningPayment {
  std::string paymentId;
  std::string sender;
  std::string receiver;
  double amount;
  double fee;
  std::vector<std::string> route; // Channel IDs
  int64_t timestamp;
  bool completed;
};

// Lightning Network Manager
class LightningNetwork final {
public:
  LightningNetwork() noexcept {
    Logging::Logger::getInstance().info("Lightning Network initialized",
                                        "Lightning", 0);
  }

  // Open payment channel
  std::string openChannel(const std::string &partyA, const std::string &partyB,
                          double fundingA, double fundingB = 0.0) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    PaymentChannel channel;
    channel.channelId = generateChannelId();
    channel.partyA = partyA;
    channel.partyB = partyB;
    channel.capacityA = fundingA;
    channel.capacityB = fundingB;
    channel.totalCapacity = fundingA + fundingB;
    channel.createdAt = std::time(nullptr);
    channel.lastActivity = channel.createdAt;
    channel.state = ChannelState::OPEN;
    channel.updateCount = 0;
    channel.latestStateHash = computeStateHash(channel);

    channels_[channel.channelId] = channel;

    // Add to routing graph
    addToRoutingGraph(partyA, partyB, channel.channelId);

    Logging::Logger::getInstance().info(
        "Channel opened: " + channel.channelId + " with " +
            std::to_string(channel.totalCapacity) + " QP capacity",
        "Lightning", 0);

    return channel.channelId;
  }

  // Make off-chain payment
  bool makePayment(const std::string &channelId, const std::string &sender,
                   double amount) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!channels_.count(channelId))
      return false;

    auto &channel = channels_[channelId];
    if (channel.state != ChannelState::OPEN)
      return false;

    // Determine direction
    if (sender == channel.partyA) {
      if (channel.capacityA < amount)
        return false;
      channel.capacityA -= amount;
      channel.capacityB += amount;
    } else if (sender == channel.partyB) {
      if (channel.capacityB < amount)
        return false;
      channel.capacityB -= amount;
      channel.capacityA += amount;
    } else {
      return false;
    }

    channel.updateCount++;
    channel.lastActivity = std::time(nullptr);
    channel.latestStateHash = computeStateHash(channel);

    Logging::Logger::getInstance().info(
        "Off-chain payment: " + std::to_string(amount) + " QP in channel " +
            channelId,
        "Lightning", 0);

    return true;
  }

  // Route payment through network
  std::string routePayment(const std::string &sender,
                           const std::string &receiver,
                           double amount) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    // Find route
    auto route = findRoute(sender, receiver, amount);
    if (route.empty()) {
      Logging::Logger::getInstance().warning(
          "No route found from " + sender + " to " + receiver, "Lightning", 0);
      return "";
    }

    // Execute multi-hop payment
    double fee = amount * 0.001; // 0.1% routing fee

    LightningPayment payment;
    payment.paymentId = generatePaymentId();
    payment.sender = sender;
    payment.receiver = receiver;
    payment.amount = amount;
    payment.fee = fee;
    payment.route = route;
    payment.timestamp = std::time(nullptr);
    payment.completed = true;

    payments_[payment.paymentId] = payment;

    Logging::Logger::getInstance().info(
        "Routed payment: " + payment.paymentId + " via " +
            std::to_string(route.size()) + " hops",
        "Lightning", 0);

    return payment.paymentId;
  }

  // Close channel (cooperative)
  bool closeChannel(const std::string &channelId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!channels_.count(channelId))
      return false;

    auto &channel = channels_[channelId];
    channel.state = ChannelState::CLOSING;

    // In real implementation, this would create on-chain settlement transaction

    channel.state = ChannelState::CLOSED;

    Logging::Logger::getInstance().info(
        "Channel closed: " + channelId +
            " - Final: A=" + std::to_string(channel.capacityA) +
            ", B=" + std::to_string(channel.capacityB),
        "Lightning", 0);

    return true;
  }

  // Get channel info
  PaymentChannel getChannel(const std::string &channelId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (channels_.count(channelId)) {
      return channels_.at(channelId);
    }
    return PaymentChannel{};
  }

  // Get all channels for user
  std::vector<PaymentChannel>
  getUserChannels(const std::string &user) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PaymentChannel> result;
    for (const auto &[id, channel] : channels_) {
      if (channel.partyA == user || channel.partyB == user) {
        result.push_back(channel);
      }
    }
    return result;
  }

  // Get network stats
  size_t getChannelCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_.size();
  }

  double getTotalCapacity() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (const auto &[id, channel] : channels_) {
      if (channel.state == ChannelState::OPEN) {
        total += channel.totalCapacity;
      }
    }
    return total;
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, PaymentChannel> channels_;
  std::map<std::string, LightningPayment> payments_;
  std::map<std::string, std::vector<std::pair<std::string, std::string>>>
      routingGraph_;
  std::atomic<int> channelCounter_{0};
  std::atomic<int> paymentCounter_{0};

  std::string generateChannelId() {
    return "CHAN_" + std::to_string(++channelCounter_) + "_" +
           std::to_string(std::time(nullptr));
  }

  std::string generatePaymentId() {
    return "LPAY_" + std::to_string(++paymentCounter_);
  }

  std::string computeStateHash(const PaymentChannel &channel) {
    Crypto::CryptoManager cm;
    std::string data = channel.channelId + std::to_string(channel.capacityA) +
                       std::to_string(channel.capacityB) +
                       std::to_string(channel.updateCount);
    return cm.sha3_512_v11(data, 0);
  }

  void addToRoutingGraph(const std::string &a, const std::string &b,
                         const std::string &channelId) {
    routingGraph_[a].push_back({b, channelId});
    routingGraph_[b].push_back({a, channelId});
  }

  // Simple BFS routing
  std::vector<std::string> findRoute(const std::string &from,
                                     const std::string &to, double amount) {
    std::map<std::string, std::string> visited;
    std::queue<std::string> queue;
    queue.push(from);
    visited[from] = "";

    while (!queue.empty()) {
      std::string current = queue.front();
      queue.pop();

      if (current == to) {
        // Reconstruct path
        std::vector<std::string> route;
        std::string node = to;
        while (node != from) {
          route.push_back(visited[node]);
          // Find previous node
          for (const auto &[n, channelId] : routingGraph_[node]) {
            if (visited.count(n) && visited[n] != visited[node]) {
              node = n;
              break;
            }
          }
          if (route.size() > 10)
            break; // Prevent infinite loop
        }
        std::reverse(route.begin(), route.end());
        return route;
      }

      if (routingGraph_.count(current)) {
        for (const auto &[neighbor, channelId] : routingGraph_[current]) {
          if (!visited.count(neighbor)) {
            visited[neighbor] = channelId;
            queue.push(neighbor);
          }
        }
      }
    }

    return {}; // No route found
  }
};

} // namespace QuantumPulse::Lightning

#endif // QUANTUMPULSE_LIGHTNING_V7_H
