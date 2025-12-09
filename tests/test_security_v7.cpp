// Extended Security Tests for QuantumPulse
// Add to tests/test_blockchain_v7.cpp or run separately

#include "../include/quantumpulse_security_v7.h"
#include <cassert>
#include <iostream>

using namespace QuantumPulse::Security;

// Test Input Validation
void testInputValidation() {
  std::cout << "Testing Input Validation..." << std::endl;

  // Valid addresses
  assert(InputValidator::isValidAddress("pub_v11_abc123xyz789") == true);
  assert(InputValidator::isValidAddress("Shankar-Lal-Khati") == true);

  // Invalid addresses
  assert(InputValidator::isValidAddress("") == false);
  assert(InputValidator::isValidAddress("invalid") == false);
  assert(InputValidator::isValidAddress("'; DROP TABLE users;--") == false);

  // SQL Injection detection
  assert(InputValidator::containsSQLInjection("SELECT * FROM users") == true);
  assert(InputValidator::containsSQLInjection("normal text") == false);
  assert(InputValidator::containsSQLInjection("--comment") == true);

  // Amount validation
  assert(InputValidator::isValidAmount(100.0) == true);
  assert(InputValidator::isValidAmount(-1.0) == false);
  assert(InputValidator::isValidAmount(6000000.0) == false); // Over limit

  // XSS sanitization
  std::string sanitized = InputValidator::sanitize("<script>alert(1)</script>");
  assert(sanitized.find("<script>") == std::string::npos);

  // Password validation
  auto [weak, msg1] = InputValidator::validatePassword("weak");
  assert(weak == false);

  auto [strong, msg2] = InputValidator::validatePassword("StrongP@ss123!");
  assert(strong == true);

  std::cout << "  ✅ All input validation tests passed" << std::endl;
}

// Test Session Management
void testSessionManagement() {
  std::cout << "Testing Session Management..." << std::endl;

  SessionManager sm;

  // Create session
  std::string sessionId = sm.createSession("user123", "192.168.1.1");
  assert(!sessionId.empty());
  assert(sessionId.length() == 64); // 32 bytes hex

  // Validate session
  auto session = sm.validateSession(sessionId, "192.168.1.1");
  assert(session.has_value());
  assert(session->userId == "user123");

  // Wrong IP should fail
  auto wrongIp = sm.validateSession(sessionId, "10.0.0.1");
  assert(!wrongIp.has_value());

  // Destroy session
  sm.destroySession(sessionId);
  auto destroyed = sm.validateSession(sessionId, "192.168.1.1");
  assert(!destroyed.has_value());

  std::cout << "  ✅ All session management tests passed" << std::endl;
}

// Test Brute Force Protection
void testBruteForceProtection() {
  std::cout << "Testing Brute Force Protection..." << std::endl;

  BruteForceProtector bfp;

  // Should not be blocked initially
  assert(bfp.isBlocked("attacker") == false);

  // Record failed attempts
  for (int i = 0; i < 5; i++) {
    bfp.recordFailedAttempt("attacker");
  }

  // Should be blocked after 5 attempts
  assert(bfp.isBlocked("attacker") == true);
  assert(bfp.getRemainingLockout("attacker") > 0);

  // Reset on success
  bfp.resetAttempts("good_user");
  assert(bfp.isBlocked("good_user") == false);

  std::cout << "  ✅ All brute force protection tests passed" << std::endl;
}

// Test Encryption Utils
void testEncryptionUtils() {
  std::cout << "Testing Encryption Utils..." << std::endl;

  // Random bytes generation
  auto bytes1 = EncryptionUtils::generateRandomBytes(32);
  auto bytes2 = EncryptionUtils::generateRandomBytes(32);
  assert(bytes1.size() == 32);
  assert(bytes1 != bytes2); // Should be different

  // Password hashing
  auto salt = EncryptionUtils::generateRandomBytes(16);
  std::string hash1 = EncryptionUtils::hashPassword("password123", salt);
  std::string hash2 = EncryptionUtils::hashPassword("password123", salt);
  assert(hash1 == hash2); // Same password + salt = same hash

  std::string hash3 = EncryptionUtils::hashPassword("different", salt);
  assert(hash1 != hash3); // Different password = different hash

  std::cout << "  ✅ All encryption utils tests passed" << std::endl;
}

// Test Security Headers
void testSecurityHeaders() {
  std::cout << "Testing Security Headers..." << std::endl;

  auto headers = SecurityHeaders::getHeaders();

  assert(headers.count("Content-Security-Policy") > 0);
  assert(headers.count("X-Frame-Options") > 0);
  assert(headers.count("X-XSS-Protection") > 0);
  assert(headers.count("Strict-Transport-Security") > 0);

  // Check values
  assert(headers["X-Frame-Options"] == "DENY");
  assert(headers["X-Content-Type-Options"] == "nosniff");

  std::cout << "  ✅ All security headers tests passed" << std::endl;
}

// Run all security tests
void runSecurityTests() {
  std::cout
      << "\n╔══════════════════════════════════════════════════════════════╗"
      << std::endl;
  std::cout
      << "║     QuantumPulse Security Unit Tests                          ║"
      << std::endl;
  std::cout
      << "╚══════════════════════════════════════════════════════════════╝\n"
      << std::endl;

  testInputValidation();
  testSessionManagement();
  testBruteForceProtection();
  testEncryptionUtils();
  testSecurityHeaders();

  std::cout
      << "\n══════════════════════════════════════════════════════════════"
      << std::endl;
  std::cout << "All security tests passed! ✅" << std::endl;
  std::cout
      << "══════════════════════════════════════════════════════════════\n"
      << std::endl;
}
