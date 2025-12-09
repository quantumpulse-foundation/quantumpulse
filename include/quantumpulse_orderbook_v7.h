#ifndef QUANTUMPULSE_ORDERBOOK_V7_H
#define QUANTUMPULSE_ORDERBOOK_V7_H

#include "quantumpulse_logging_v7.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace QuantumPulse::Trading {

// Order types
enum class OrderType { MARKET, LIMIT };
enum class OrderSide { BUY, SELL };
enum class OrderStatus { OPEN, FILLED, PARTIAL, CANCELLED };

// Order structure
struct Order {
  std::string orderId;
  std::string userId;
  OrderSide side;
  OrderType type;
  double price;
  double quantity;
  double filled{0};
  OrderStatus status{OrderStatus::OPEN};
  int64_t timestamp;

  [[nodiscard]] double remainingQuantity() const noexcept {
    return quantity - filled;
  }
};

// Trade structure
struct Trade {
  std::string tradeId;
  std::string buyOrderId;
  std::string sellOrderId;
  double price;
  double quantity;
  int64_t timestamp;
};

// Order Book
class OrderBook final {
public:
  OrderBook() noexcept {
    Logging::Logger::getInstance().info("Order Book initialized", "Trading", 0);
  }

  // Place order
  [[nodiscard]] std::string placeOrder(const std::string &userId,
                                       OrderSide side, OrderType type,
                                       double price, double quantity) noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);

    Order order;
    order.orderId = "ord_" + std::to_string(nextOrderId_++);
    order.userId = userId;
    order.side = side;
    order.type = type;
    order.price = price;
    order.quantity = quantity;
    order.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    orders_[order.orderId] = order;

    if (side == OrderSide::BUY) {
      buyOrders_[price].push_back(order.orderId);
    } else {
      sellOrders_[price].push_back(order.orderId);
    }

    // Try to match
    matchOrders();

    Logging::Logger::getInstance().info(
        "Order placed: " + order.orderId + " " +
            (side == OrderSide::BUY ? "BUY" : "SELL") + " " +
            std::to_string(quantity) + " @ $" + std::to_string(price),
        "Trading", 0);

    return order.orderId;
  }

  // Cancel order
  bool cancelOrder(const std::string &orderId,
                   const std::string &userId) noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);

    auto it = orders_.find(orderId);
    if (it == orders_.end())
      return false;
    if (it->second.userId != userId)
      return false;
    if (it->second.status != OrderStatus::OPEN &&
        it->second.status != OrderStatus::PARTIAL)
      return false;

    it->second.status = OrderStatus::CANCELLED;
    removeFromBook(it->second);

    Logging::Logger::getInstance().info("Order cancelled: " + orderId,
                                        "Trading", 0);
    return true;
  }

  // Get order
  [[nodiscard]] std::optional<Order>
  getOrder(const std::string &orderId) const noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);
    auto it = orders_.find(orderId);
    return it != orders_.end() ? std::optional(it->second) : std::nullopt;
  }

  // Get user orders
  [[nodiscard]] std::vector<Order>
  getUserOrders(const std::string &userId) const noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);
    std::vector<Order> result;
    for (const auto &[id, order] : orders_) {
      if (order.userId == userId) {
        result.push_back(order);
      }
    }
    return result;
  }

  // Get order book depth
  [[nodiscard]] std::pair<std::vector<std::pair<double, double>>,
                          std::vector<std::pair<double, double>>>
  getDepth(int levels = 10) const noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);

    std::vector<std::pair<double, double>> bids, asks;

    // Best bids (highest prices first)
    int count = 0;
    for (auto it = buyOrders_.rbegin();
         it != buyOrders_.rend() && count < levels; ++it, ++count) {
      double totalQty = 0;
      for (const auto &orderId : it->second) {
        auto orderIt = orders_.find(orderId);
        if (orderIt != orders_.end() &&
            orderIt->second.status == OrderStatus::OPEN) {
          totalQty += orderIt->second.remainingQuantity();
        }
      }
      if (totalQty > 0)
        bids.emplace_back(it->first, totalQty);
    }

    // Best asks (lowest prices first)
    count = 0;
    for (auto it = sellOrders_.begin();
         it != sellOrders_.end() && count < levels; ++it, ++count) {
      double totalQty = 0;
      for (const auto &orderId : it->second) {
        auto orderIt = orders_.find(orderId);
        if (orderIt != orders_.end() &&
            orderIt->second.status == OrderStatus::OPEN) {
          totalQty += orderIt->second.remainingQuantity();
        }
      }
      if (totalQty > 0)
        asks.emplace_back(it->first, totalQty);
    }

    return {bids, asks};
  }

  // Get recent trades
  [[nodiscard]] std::vector<Trade>
  getRecentTrades(int count = 50) const noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);

    std::vector<Trade> result;
    int start = std::max(0, static_cast<int>(trades_.size()) - count);
    for (size_t i = start; i < trades_.size(); ++i) {
      result.push_back(trades_[i]);
    }
    return result;
  }

  // Get best bid/ask
  [[nodiscard]] std::pair<double, double> getBestBidAsk() const noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);

    double bestBid = buyOrders_.empty() ? 0 : buyOrders_.rbegin()->first;
    double bestAsk = sellOrders_.empty() ? 0 : sellOrders_.begin()->first;

    return {bestBid, bestAsk};
  }

  // Stats
  [[nodiscard]] size_t getOpenOrderCount() const noexcept {
    std::lock_guard<std::mutex> lock(bookMutex_);
    size_t count = 0;
    for (const auto &[id, order] : orders_) {
      if (order.status == OrderStatus::OPEN ||
          order.status == OrderStatus::PARTIAL) {
        count++;
      }
    }
    return count;
  }

  [[nodiscard]] size_t getTradeCount() const noexcept { return trades_.size(); }

private:
  mutable std::mutex bookMutex_;
  std::map<std::string, Order> orders_;
  std::map<double, std::vector<std::string>> buyOrders_; // price -> order IDs
  std::map<double, std::vector<std::string>> sellOrders_;
  std::vector<Trade> trades_;
  int nextOrderId_{1};
  int nextTradeId_{1};

  void matchOrders() noexcept {
    while (!buyOrders_.empty() && !sellOrders_.empty()) {
      double bestBid = buyOrders_.rbegin()->first;
      double bestAsk = sellOrders_.begin()->first;

      if (bestBid < bestAsk)
        break; // No match

      auto &buyOrderIds = buyOrders_.rbegin()->second;
      auto &sellOrderIds = sellOrders_.begin()->second;

      if (buyOrderIds.empty() || sellOrderIds.empty())
        break;

      std::string buyId = buyOrderIds.front();
      std::string sellId = sellOrderIds.front();

      auto &buyOrder = orders_[buyId];
      auto &sellOrder = orders_[sellId];

      double matchQty =
          std::min(buyOrder.remainingQuantity(), sellOrder.remainingQuantity());
      double matchPrice = sellOrder.price; // Maker price

      // Execute trade
      Trade trade;
      trade.tradeId = "trd_" + std::to_string(nextTradeId_++);
      trade.buyOrderId = buyId;
      trade.sellOrderId = sellId;
      trade.price = matchPrice;
      trade.quantity = matchQty;
      trade.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

      trades_.push_back(trade);

      // Update orders
      buyOrder.filled += matchQty;
      sellOrder.filled += matchQty;

      buyOrder.status = (buyOrder.filled >= buyOrder.quantity)
                            ? OrderStatus::FILLED
                            : OrderStatus::PARTIAL;
      sellOrder.status = (sellOrder.filled >= sellOrder.quantity)
                             ? OrderStatus::FILLED
                             : OrderStatus::PARTIAL;

      // Remove filled orders from book
      if (buyOrder.status == OrderStatus::FILLED) {
        buyOrderIds.erase(buyOrderIds.begin());
        if (buyOrderIds.empty()) {
          buyOrders_.erase(std::prev(buyOrders_.end()));
        }
      }

      if (sellOrder.status == OrderStatus::FILLED) {
        sellOrderIds.erase(sellOrderIds.begin());
        if (sellOrderIds.empty()) {
          sellOrders_.erase(sellOrders_.begin());
        }
      }

      Logging::Logger::getInstance().info(
          "Trade executed: " + trade.tradeId + " " + std::to_string(matchQty) +
              " @ $" + std::to_string(matchPrice),
          "Trading", 0);
    }
  }

  void removeFromBook(const Order &order) noexcept {
    auto &book = (order.side == OrderSide::BUY) ? buyOrders_ : sellOrders_;
    auto it = book.find(order.price);
    if (it != book.end()) {
      auto &ids = it->second;
      ids.erase(std::remove(ids.begin(), ids.end(), order.orderId), ids.end());
      if (ids.empty())
        book.erase(it);
    }
  }
};

} // namespace QuantumPulse::Trading

#endif // QUANTUMPULSE_ORDERBOOK_V7_H
