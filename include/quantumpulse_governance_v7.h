#ifndef QUANTUMPULSE_GOVERNANCE_V7_H
#define QUANTUMPULSE_GOVERNANCE_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace QuantumPulse::Governance {

// Proposal status
enum class ProposalStatus {
  PENDING,
  ACTIVE,
  PASSED,
  REJECTED,
  EXECUTED,
  CANCELLED
};

// Vote type
enum class VoteType { FOR, AGAINST, ABSTAIN };

// Proposal
struct Proposal {
  std::string proposalId;
  std::string title;
  std::string description;
  std::string proposer;
  ProposalStatus status;
  int64_t createdAt;
  int64_t votingEndsAt;
  double votesFor;
  double votesAgainst;
  double votesAbstain;
  double quorumRequired; // Percentage
  std::map<std::string, VoteType> votes;
};

// DAO Governance
class DAOGovernance final {
public:
  DAOGovernance() noexcept {
    Logging::Logger::getInstance().info("DAO Governance initialized",
                                        "Governance", 0);
  }

  // Create proposal
  [[nodiscard]] std::string createProposal(const std::string &proposer,
                                           const std::string &title,
                                           const std::string &description,
                                           int votingDays = 7) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if proposer has enough tokens (min 1000 QP)
    double balance = getVotingPower(proposer);
    if (balance < 1000)
      return "";

    Proposal proposal;
    proposal.proposalId = "QIP-" + std::to_string(nextProposalId_++);
    proposal.title = title;
    proposal.description = description;
    proposal.proposer = proposer;
    proposal.status = ProposalStatus::ACTIVE;
    proposal.createdAt = std::time(nullptr);
    proposal.votingEndsAt = proposal.createdAt + (votingDays * 86400);
    proposal.votesFor = 0;
    proposal.votesAgainst = 0;
    proposal.votesAbstain = 0;
    proposal.quorumRequired = 10.0; // 10% quorum

    proposals_[proposal.proposalId] = proposal;

    Logging::Logger::getInstance().info(
        "Proposal created: " + proposal.proposalId + " - " + title,
        "Governance", 0);

    return proposal.proposalId;
  }

  // Vote on proposal
  bool vote(const std::string &proposalId, const std::string &voter,
            VoteType voteType) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = proposals_.find(proposalId);
    if (it == proposals_.end())
      return false;

    auto &proposal = it->second;

    // Check if voting is still active
    if (proposal.status != ProposalStatus::ACTIVE)
      return false;
    if (std::time(nullptr) > proposal.votingEndsAt) {
      finalizeProposal(proposalId);
      return false;
    }

    // Check if already voted
    if (proposal.votes.count(voter) > 0)
      return false;

    double votingPower = getVotingPower(voter);
    if (votingPower <= 0)
      return false;

    proposal.votes[voter] = voteType;

    switch (voteType) {
    case VoteType::FOR:
      proposal.votesFor += votingPower;
      break;
    case VoteType::AGAINST:
      proposal.votesAgainst += votingPower;
      break;
    case VoteType::ABSTAIN:
      proposal.votesAbstain += votingPower;
      break;
    }

    Logging::Logger::getInstance().info(
        "Vote cast on " + proposalId + " by " + voter, "Governance", 0);

    return true;
  }

  // Finalize proposal
  void finalizeProposal(const std::string &proposalId) noexcept {
    auto it = proposals_.find(proposalId);
    if (it == proposals_.end())
      return;

    auto &proposal = it->second;
    if (proposal.status != ProposalStatus::ACTIVE)
      return;

    double totalVotes =
        proposal.votesFor + proposal.votesAgainst + proposal.votesAbstain;
    double totalSupply = 5000000; // Total QP supply
    double quorum = (totalVotes / totalSupply) * 100;

    if (quorum < proposal.quorumRequired) {
      proposal.status = ProposalStatus::REJECTED; // Quorum not met
    } else if (proposal.votesFor > proposal.votesAgainst) {
      proposal.status = ProposalStatus::PASSED;
    } else {
      proposal.status = ProposalStatus::REJECTED;
    }

    Logging::Logger::getInstance().info(
        "Proposal finalized: " + proposalId + " - " +
            (proposal.status == ProposalStatus::PASSED ? "PASSED" : "REJECTED"),
        "Governance", 0);
  }

  // Execute passed proposal
  bool executeProposal(const std::string &proposalId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = proposals_.find(proposalId);
    if (it == proposals_.end())
      return false;

    if (it->second.status != ProposalStatus::PASSED)
      return false;

    it->second.status = ProposalStatus::EXECUTED;
    return true;
  }

  // Get proposal
  [[nodiscard]] std::optional<Proposal>
  getProposal(const std::string &proposalId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = proposals_.find(proposalId);
    return it != proposals_.end() ? std::optional(it->second) : std::nullopt;
  }

  // Get all active proposals
  [[nodiscard]] std::vector<Proposal> getActiveProposals() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Proposal> result;
    for (const auto &[id, proposal] : proposals_) {
      if (proposal.status == ProposalStatus::ACTIVE) {
        result.push_back(proposal);
      }
    }
    return result;
  }

  // Delegate voting power
  void delegate(const std::string &from, const std::string &to) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    delegations_[from] = to;
  }

  // Set voting power (for testing)
  void setVotingPower(const std::string &address, double power) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    votingPower_[address] = power;
  }

private:
  std::map<std::string, Proposal> proposals_;
  std::map<std::string, std::string> delegations_;
  std::map<std::string, double> votingPower_;
  mutable std::mutex mutex_;
  int nextProposalId_{1};

  double getVotingPower(const std::string &address) noexcept {
    // Check delegation
    auto delegateIt = delegations_.find(address);
    if (delegateIt != delegations_.end()) {
      return 0; // Delegated away
    }

    // Get base power
    double power = 0;
    auto it = votingPower_.find(address);
    if (it != votingPower_.end())
      power = it->second;

    // Add delegated power
    for (const auto &[from, to] : delegations_) {
      if (to == address) {
        auto fromIt = votingPower_.find(from);
        if (fromIt != votingPower_.end())
          power += fromIt->second;
      }
    }

    // Default: check if premined account
    if (address == "Shankar-Lal-Khati" && power == 0) {
      power = 2000000; // Premined amount
    }

    return power;
  }
};

} // namespace QuantumPulse::Governance

#endif // QUANTUMPULSE_GOVERNANCE_V7_H
