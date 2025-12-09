// QuantumPulse CLI (quantumpulse-cli)
// Bitcoin-like command line interface

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Default RPC settings
std::string g_rpcHost = "127.0.0.1";
int g_rpcPort = 8332;
std::string g_rpcUser = "";
std::string g_rpcPassword = "";

// Send RPC request
std::string sendRPC(const std::string &method,
                    const std::string &params = "[]") {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return "error: could not create socket";
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(g_rpcPort);
  inet_pton(AF_INET, g_rpcHost.c_str(), &server.sin_addr);

  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    close(sock);
    return "error: could not connect to server at " + g_rpcHost + ":" +
           std::to_string(g_rpcPort);
  }

  // Build JSON-RPC request
  std::string jsonBody = R"({"jsonrpc": "1.0", "id": "cli", "method": ")" +
                         method + R"(", "params": )" + params + "}";

  std::string request = "POST / HTTP/1.1\r\n"
                        "Host: " +
                        g_rpcHost +
                        "\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: " +
                        std::to_string(jsonBody.length()) +
                        "\r\n"
                        "\r\n" +
                        jsonBody;

  send(sock, request.c_str(), request.length(), 0);

  char buffer[65536] = {0};
  int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
  close(sock);

  if (bytes <= 0) {
    return "error: no response from server";
  }

  // Extract JSON body from HTTP response
  std::string response(buffer);
  auto bodyStart = response.find("\r\n\r\n");
  if (bodyStart != std::string::npos) {
    return response.substr(bodyStart + 4);
  }
  return response;
}

// Format output
void printResult(const std::string &result) {
  // Pretty print JSON
  int indent = 0;
  bool inString = false;

  for (size_t i = 0; i < result.length(); i++) {
    char c = result[i];

    if (c == '"' && (i == 0 || result[i - 1] != '\\')) {
      inString = !inString;
      std::cout << c;
    } else if (!inString) {
      if (c == '{' || c == '[') {
        std::cout << c << "\n";
        indent++;
        std::cout << std::string(indent * 2, ' ');
      } else if (c == '}' || c == ']') {
        std::cout << "\n";
        indent--;
        std::cout << std::string(indent * 2, ' ') << c;
      } else if (c == ',') {
        std::cout << c << "\n" << std::string(indent * 2, ' ');
      } else if (c == ':') {
        std::cout << ": ";
      } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
        std::cout << c;
      }
    } else {
      std::cout << c;
    }
  }
  std::cout << std::endl;
}

// Print help
void printHelp() {
  std::cout << "QuantumPulse CLI v7.0.0\n\n";
  std::cout << "Usage: quantumpulse-cli [options] <command> [params]\n\n";
  std::cout << "Options:\n";
  std::cout
      << "  -rpcconnect=<ip>    Connect to RPC server (default: 127.0.0.1)\n";
  std::cout << "  -rpcport=<port>     RPC port (default: 8332)\n";
  std::cout << "  -rpcuser=<user>     RPC username\n";
  std::cout << "  -rpcpassword=<pw>   RPC password\n\n";
  std::cout << "Commands:\n";
  std::cout << "  == Blockchain ==\n";
  std::cout << "  getblockchaininfo   Get blockchain information\n";
  std::cout << "  getblockcount       Get current block count\n";
  std::cout << "  getdifficulty       Get current difficulty\n";
  std::cout << "  getbestblockhash    Get best block hash\n\n";
  std::cout << "  == Mining ==\n";
  std::cout << "  getmininginfo       Get mining information\n";
  std::cout << "  generate <n>        Mine n blocks\n\n";
  std::cout << "  == Network ==\n";
  std::cout << "  getnetworkinfo      Get network information\n";
  std::cout << "  getpeerinfo         Get peer information\n";
  std::cout << "  getconnectioncount  Get connection count\n\n";
  std::cout << "  == Wallet ==\n";
  std::cout << "  getbalance          Get wallet balance\n";
  std::cout << "  getnewaddress       Generate new address\n";
  std::cout << "  sendtoaddress       Send QP to address\n";
  std::cout << "  listtransactions    List transactions\n\n";
  std::cout << "  == QuantumPulse Specific ==\n";
  std::cout << "  getprice            Get current QP price\n";
  std::cout << "  getpreminedinfo     Get pre-mined account info\n\n";
  std::cout << "  == Control ==\n";
  std::cout << "  stop                Stop the daemon\n";
  std::cout << "  help                Show this help\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printHelp();
    return 1;
  }

  // Parse options
  int cmdStart = 1;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg[0] == '-') {
      if (arg.find("-rpcconnect=") == 0) {
        g_rpcHost = arg.substr(12);
      } else if (arg.find("-rpcport=") == 0) {
        g_rpcPort = std::stoi(arg.substr(9));
      } else if (arg.find("-rpcuser=") == 0) {
        g_rpcUser = arg.substr(9);
      } else if (arg.find("-rpcpassword=") == 0) {
        g_rpcPassword = arg.substr(13);
      }
      cmdStart = i + 1;
    } else {
      cmdStart = i;
      break;
    }
  }

  if (cmdStart >= argc) {
    printHelp();
    return 1;
  }

  std::string command = argv[cmdStart];

  // Handle help
  if (command == "help" || command == "-help" || command == "--help") {
    printHelp();
    return 0;
  }

  // Build params
  std::string params = "[";
  for (int i = cmdStart + 1; i < argc; i++) {
    if (i > cmdStart + 1)
      params += ", ";

    std::string p = argv[i];
    // Check if numeric
    bool isNum =
        !p.empty() && (std::isdigit(p[0]) || (p[0] == '-' && p.length() > 1));
    if (isNum) {
      params += p;
    } else {
      params += "\"" + p + "\"";
    }
  }
  params += "]";

  // Send RPC
  std::string result = sendRPC(command, params);

  if (result.find("error:") == 0) {
    std::cerr << result << std::endl;
    return 1;
  }

  printResult(result);
  return 0;
}
