#ifndef QUANTUMPULSE_API_V7_H
#define QUANTUMPULSE_API_V7_H

#include "quantumpulse_blockchain_v7.h"
#include <atomic>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace QuantumPulse::API {

// API Configuration
struct APIConfig {
  static constexpr int DEFAULT_PORT = 8080;
  static constexpr int MAX_CONNECTIONS = 100;
  static constexpr int BUFFER_SIZE = 65536;
  static constexpr int REQUEST_TIMEOUT_SEC = 30;
};

// HTTP Methods
enum class HttpMethod { GET, POST, PUT, DELETE, OPTIONS, UNKNOWN };

// HTTP Status codes
enum class HttpStatus {
  OK = 200,
  Created = 201,
  BadRequest = 400,
  Unauthorized = 401,
  NotFound = 404,
  MethodNotAllowed = 405,
  InternalError = 500
};

// Convert status to string
[[nodiscard]] inline std::string statusToString(HttpStatus status) noexcept {
  switch (status) {
  case HttpStatus::OK:
    return "200 OK";
  case HttpStatus::Created:
    return "201 Created";
  case HttpStatus::BadRequest:
    return "400 Bad Request";
  case HttpStatus::Unauthorized:
    return "401 Unauthorized";
  case HttpStatus::NotFound:
    return "404 Not Found";
  case HttpStatus::MethodNotAllowed:
    return "405 Method Not Allowed";
  case HttpStatus::InternalError:
    return "500 Internal Server Error";
  default:
    return "500 Internal Server Error";
  }
}

// HTTP Request
struct HttpRequest {
  HttpMethod method{HttpMethod::UNKNOWN};
  std::string path;
  std::string body;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> params;

  [[nodiscard]] std::string getParam(const std::string &key) const noexcept {
    auto it = params.find(key);
    return it != params.end() ? it->second : "";
  }
};

// HTTP Response
struct HttpResponse {
  HttpStatus status{HttpStatus::OK};
  std::string body;
  std::string contentType{"application/json"};
  std::map<std::string, std::string> headers;

  void setJSON(const std::string &json) noexcept {
    body = json;
    contentType = "application/json";
  }

  [[nodiscard]] std::string build() const noexcept {
    std::stringstream ss;
    ss << "HTTP/1.1 " << statusToString(status) << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.length() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    ss << "Connection: close\r\n";
    for (const auto &[key, value] : headers) {
      ss << key << ": " << value << "\r\n";
    }
    ss << "\r\n";
    ss << body;
    return ss.str();
  }
};

// Route handler type
using RouteHandler = std::function<HttpResponse(const HttpRequest &)>;

// REST API Server
class APIServer final {
public:
  explicit APIServer(Blockchain::Blockchain &blockchain,
                     int port = APIConfig::DEFAULT_PORT) noexcept
      : blockchain_(blockchain), port_(port), running_(false),
        serverSocket_(-1) {
    setupRoutes();
    Logging::Logger::getInstance().info(
        "API Server initialized on port " + std::to_string(port), "API", 0);
  }

  ~APIServer() noexcept { stop(); }

  // Non-copyable
  APIServer(const APIServer &) = delete;
  APIServer &operator=(const APIServer &) = delete;

  // Start server
  bool start() noexcept {
    if (running_.load()) {
      return false;
    }

    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
      Logging::Logger::getInstance().error("Failed to create socket", "API", 0);
      return false;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket_, reinterpret_cast<sockaddr *>(&serverAddr),
             sizeof(serverAddr)) < 0) {
      Logging::Logger::getInstance().error("Failed to bind socket", "API", 0);
      close(serverSocket_);
      return false;
    }

    if (listen(serverSocket_, APIConfig::MAX_CONNECTIONS) < 0) {
      Logging::Logger::getInstance().error("Failed to listen", "API", 0);
      close(serverSocket_);
      return false;
    }

    running_.store(true);
    serverThread_ = std::thread(&APIServer::acceptLoop, this);

    Logging::Logger::getInstance().info(
        "API Server started at http://localhost:" + std::to_string(port_),
        "API", 0);
    return true;
  }

  // Stop server
  void stop() noexcept {
    if (!running_.load()) {
      return;
    }

    running_.store(false);

    if (serverSocket_ >= 0) {
      shutdown(serverSocket_, SHUT_RDWR);
      close(serverSocket_);
      serverSocket_ = -1;
    }

    if (serverThread_.joinable()) {
      serverThread_.join();
    }

    Logging::Logger::getInstance().info("API Server stopped", "API", 0);
  }

  // Check if running
  [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

  // Add custom route
  void addRoute(HttpMethod method, const std::string &path,
                RouteHandler handler) noexcept {
    std::lock_guard<std::mutex> lock(routesMutex_);
    routes_[{method, path}] = std::move(handler);
  }

private:
  Blockchain::Blockchain &blockchain_;
  int port_;
  std::atomic<bool> running_;
  int serverSocket_;
  std::thread serverThread_;
  std::map<std::pair<HttpMethod, std::string>, RouteHandler> routes_;
  mutable std::mutex routesMutex_;

  void setupRoutes() noexcept {
    // GET /api/info - Blockchain info
    addRoute(HttpMethod::GET, "/api/info",
             [this](const HttpRequest &) { return handleInfo(); });

    // GET /api/balance/:address
    addRoute(HttpMethod::GET, "/api/balance",
             [this](const HttpRequest &req) { return handleBalance(req); });

    // POST /api/transaction
    addRoute(HttpMethod::POST, "/api/transaction",
             [this](const HttpRequest &req) { return handleTransaction(req); });

    // GET /api/blocks
    addRoute(HttpMethod::GET, "/api/blocks",
             [this](const HttpRequest &) { return handleBlocks(); });

    // POST /api/mine
    addRoute(HttpMethod::POST, "/api/mine",
             [this](const HttpRequest &req) { return handleMine(req); });

    // GET /api/price
    addRoute(HttpMethod::GET, "/api/price",
             [this](const HttpRequest &) { return handlePrice(); });

    // OPTIONS for CORS
    addRoute(HttpMethod::OPTIONS, "/api", [](const HttpRequest &) {
      HttpResponse res;
      res.status = HttpStatus::OK;
      res.body = "";
      return res;
    });
  }

  void acceptLoop() noexcept {
    while (running_.load()) {
      sockaddr_in clientAddr{};
      socklen_t clientLen = sizeof(clientAddr);

      int clientSocket = accept(
          serverSocket_, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);

      if (clientSocket < 0) {
        if (running_.load()) {
          Logging::Logger::getInstance().warning("Failed to accept connection",
                                                 "API", 0);
        }
        continue;
      }

      // Handle request in thread
      std::thread(&APIServer::handleClient, this, clientSocket).detach();
    }
  }

  void handleClient(int clientSocket) noexcept {
    char buffer[APIConfig::BUFFER_SIZE];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
      close(clientSocket);
      return;
    }

    buffer[bytesRead] = '\0';

    HttpRequest request = parseRequest(buffer);
    HttpResponse response = routeRequest(request);

    std::string responseStr = response.build();
    send(clientSocket, responseStr.c_str(), responseStr.length(), 0);

    close(clientSocket);
  }

  [[nodiscard]] HttpRequest
  parseRequest(const char *rawRequest) const noexcept {
    HttpRequest req;
    std::istringstream stream(rawRequest);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) {
      std::istringstream lineStream(line);
      std::string methodStr, path;
      lineStream >> methodStr >> path;

      if (methodStr == "GET")
        req.method = HttpMethod::GET;
      else if (methodStr == "POST")
        req.method = HttpMethod::POST;
      else if (methodStr == "PUT")
        req.method = HttpMethod::PUT;
      else if (methodStr == "DELETE")
        req.method = HttpMethod::DELETE;
      else if (methodStr == "OPTIONS")
        req.method = HttpMethod::OPTIONS;

      // Parse path and query params
      size_t queryPos = path.find('?');
      if (queryPos != std::string::npos) {
        std::string query = path.substr(queryPos + 1);
        req.path = path.substr(0, queryPos);

        // Parse query params
        std::istringstream queryStream(query);
        std::string param;
        while (std::getline(queryStream, param, '&')) {
          size_t eqPos = param.find('=');
          if (eqPos != std::string::npos) {
            req.params[param.substr(0, eqPos)] = param.substr(eqPos + 1);
          }
        }
      } else {
        req.path = path;
      }
    }

    // Parse headers
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
      size_t colonPos = line.find(':');
      if (colonPos != std::string::npos) {
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 2);
        if (!value.empty() && value.back() == '\r') {
          value.pop_back();
        }
        req.headers[key] = value;
      }
    }

    // Parse body
    std::string body;
    while (std::getline(stream, line)) {
      body += line;
    }
    req.body = body;

    return req;
  }

  [[nodiscard]] HttpResponse routeRequest(const HttpRequest &req) noexcept {
    std::lock_guard<std::mutex> lock(routesMutex_);

    // Handle CORS preflight
    if (req.method == HttpMethod::OPTIONS) {
      HttpResponse res;
      res.status = HttpStatus::OK;
      return res;
    }

    // Find matching route
    auto it = routes_.find({req.method, req.path});
    if (it != routes_.end()) {
      try {
        return it->second(req);
      } catch (...) {
        HttpResponse res;
        res.status = HttpStatus::InternalError;
        res.setJSON(R"({"error":"Internal server error"})");
        return res;
      }
    }

    // Try matching with /api prefix for balance
    if (req.path.find("/api/balance/") == 0) {
      HttpRequest modReq = req;
      modReq.params["address"] = req.path.substr(13);
      modReq.path = "/api/balance";
      auto balanceIt = routes_.find({HttpMethod::GET, "/api/balance"});
      if (balanceIt != routes_.end()) {
        return balanceIt->second(modReq);
      }
    }

    HttpResponse res;
    res.status = HttpStatus::NotFound;
    res.setJSON(R"({"error":"Endpoint not found"})");
    return res;
  }

  // API Handlers
  [[nodiscard]] HttpResponse handleInfo() noexcept {
    HttpResponse res;

    std::stringstream json;
    json << "{";
    json << "\"version\":\"7.0.0\",";
    json << "\"chainLength\":" << blockchain_.getChainLength() << ",";
    json << "\"totalMinedCoins\":" << blockchain_.getTotalMinedCoins() << ",";
    json << "\"miningLimit\":3000000,";
    json << "\"preminedCoins\":2000000,";
    json << "\"preminedAccount\":\"Shankar-Lal-Khati\",";
    json << "\"minimumPrice\":600000,";
    json << "\"shards\":2048,";
    json << "\"status\":\"running\"";
    json << "}";

    res.setJSON(json.str());
    return res;
  }

  [[nodiscard]] HttpResponse handleBalance(const HttpRequest &req) noexcept {
    HttpResponse res;

    std::string address = req.getParam("address");
    if (address.empty()) {
      res.status = HttpStatus::BadRequest;
      res.setJSON(R"({"error":"Address required"})");
      return res;
    }

    auto balance = blockchain_.getBalance(address, "");

    std::stringstream json;
    json << "{";
    json << "\"address\":\"" << address << "\",";
    json << "\"balance\":" << (balance ? *balance : 0.0) << ",";
    json << "\"currency\":\"QP\"";
    json << "}";

    res.setJSON(json.str());
    return res;
  }

  [[nodiscard]] HttpResponse
  handleTransaction(const HttpRequest &req) noexcept {
    HttpResponse res;

    // Simple JSON parsing (production would use proper JSON library)
    std::string from, to, amountStr;

    // Parse from body
    size_t fromPos = req.body.find("\"from\":");
    size_t toPos = req.body.find("\"to\":");
    size_t amountPos = req.body.find("\"amount\":");

    if (fromPos == std::string::npos || toPos == std::string::npos ||
        amountPos == std::string::npos) {
      res.status = HttpStatus::BadRequest;
      res.setJSON(
          R"({"error":"Invalid request body. Required: from, to, amount"})");
      return res;
    }

    // For demo, return success
    res.status = HttpStatus::Created;
    std::stringstream json;
    json << "{";
    json << "\"status\":\"pending\",";
    json << "\"message\":\"Transaction submitted\",";
    json << "\"txId\":\"tx_" << std::time(nullptr) << "\"";
    json << "}";
    res.setJSON(json.str());

    return res;
  }

  [[nodiscard]] HttpResponse handleBlocks() noexcept {
    HttpResponse res;

    std::stringstream json;
    json << "{";
    json << "\"blocks\":[";
    json << "{\"index\":0,\"hash\":\"genesis\",\"timestamp\":"
         << std::time(nullptr) << "}";
    json << "],";
    json << "\"totalBlocks\":" << blockchain_.getChainLength();
    json << "}";

    res.setJSON(json.str());
    return res;
  }

  [[nodiscard]] HttpResponse
  handleMine([[maybe_unused]] const HttpRequest &req) noexcept {
    HttpResponse res;

    if (!blockchain_.checkMiningLimit()) {
      res.status = HttpStatus::BadRequest;
      res.setJSON(R"({"error":"Mining limit reached"})");
      return res;
    }

    std::stringstream json;
    json << "{";
    json << "\"status\":\"mining_started\",";
    json << "\"difficulty\":" << blockchain_.getMiningManager().getDifficulty()
         << ",";
    json << "\"reward\":"
         << Mining::MiningManager::calculateBlockReward(
                blockchain_.getChainLength());
    json << "}";

    res.setJSON(json.str());
    return res;
  }

  [[nodiscard]] HttpResponse handlePrice() noexcept {
    HttpResponse res;

    double price =
        blockchain_.adjustCoinPrice(600000.0, blockchain_.getChainLength(), 0);

    std::stringstream json;
    json << "{";
    json << "\"price\":" << static_cast<int64_t>(price) << ",";
    json << "\"currency\":\"USD\",";
    json << "\"minimumPrice\":600000,";
    json << "\"guaranteed\":true";
    json << "}";

    res.setJSON(json.str());
    return res;
  }
};

} // namespace QuantumPulse::API

#endif // QUANTUMPULSE_API_V7_H
