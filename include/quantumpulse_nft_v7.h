#ifndef QUANTUMPULSE_NFT_V7_H
#define QUANTUMPULSE_NFT_V7_H

#include "quantumpulse_logging_v7.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace QuantumPulse::NFT {

// NFT metadata
struct NFTMetadata {
  std::string tokenId;
  std::string name;
  std::string description;
  std::string imageUrl;
  std::string creator;
  std::string owner;
  std::string collection;
  std::map<std::string, std::string> attributes;
  int64_t createdAt;
};

// NFT listing
struct Listing {
  std::string listingId;
  std::string tokenId;
  std::string seller;
  double priceQP;
  bool isAuction;
  double highestBid;
  std::string highestBidder;
  int64_t endTime;
};

// Collection
struct Collection {
  std::string collectionId;
  std::string name;
  std::string creator;
  std::string description;
  double floorPrice;
  double volume;
  int itemCount;
};

// NFT Marketplace
class NFTMarketplace final {
public:
  NFTMarketplace() noexcept {
    Logging::Logger::getInstance().info("NFT Marketplace initialized", "NFT",
                                        0);
  }

  // Mint NFT
  [[nodiscard]] std::string mint(const std::string &creator,
                                 const std::string &name,
                                 const std::string &description,
                                 const std::string &imageUrl,
                                 const std::string &collection) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    NFTMetadata nft;
    nft.tokenId = "nft_" + std::to_string(nextTokenId_++);
    nft.name = name;
    nft.description = description;
    nft.imageUrl = imageUrl;
    nft.creator = creator;
    nft.owner = creator;
    nft.collection = collection;
    nft.createdAt = std::time(nullptr);

    nfts_[nft.tokenId] = nft;

    Logging::Logger::getInstance().info("NFT minted: " + nft.tokenId, "NFT", 0);
    return nft.tokenId;
  }

  // List for sale
  [[nodiscard]] std::string listForSale(const std::string &tokenId,
                                        double priceQP) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto nftIt = nfts_.find(tokenId);
    if (nftIt == nfts_.end())
      return "";

    Listing listing;
    listing.listingId = "list_" + std::to_string(nextListingId_++);
    listing.tokenId = tokenId;
    listing.seller = nftIt->second.owner;
    listing.priceQP = priceQP;
    listing.isAuction = false;
    listing.highestBid = 0;
    listing.endTime = 0;

    listings_[listing.listingId] = listing;
    return listing.listingId;
  }

  // Create auction
  [[nodiscard]] std::string createAuction(const std::string &tokenId,
                                          double startingPrice,
                                          int durationHours) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto nftIt = nfts_.find(tokenId);
    if (nftIt == nfts_.end())
      return "";

    Listing listing;
    listing.listingId = "auction_" + std::to_string(nextListingId_++);
    listing.tokenId = tokenId;
    listing.seller = nftIt->second.owner;
    listing.priceQP = startingPrice;
    listing.isAuction = true;
    listing.highestBid = 0;
    listing.endTime = std::time(nullptr) + (durationHours * 3600);

    listings_[listing.listingId] = listing;
    return listing.listingId;
  }

  // Place bid
  bool placeBid(const std::string &listingId, const std::string &bidder,
                double bidQP) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = listings_.find(listingId);
    if (it == listings_.end() || !it->second.isAuction)
      return false;
    if (bidQP <= it->second.highestBid)
      return false;

    it->second.highestBid = bidQP;
    it->second.highestBidder = bidder;
    return true;
  }

  // Buy NFT
  bool buyNFT(const std::string &listingId, const std::string &buyer) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    auto listIt = listings_.find(listingId);
    if (listIt == listings_.end() || listIt->second.isAuction)
      return false;

    auto nftIt = nfts_.find(listIt->second.tokenId);
    if (nftIt == nfts_.end())
      return false;

    // Transfer ownership
    nftIt->second.owner = buyer;
    listings_.erase(listIt);

    Logging::Logger::getInstance().info(
        "NFT sold: " + nftIt->second.tokenId + " to " + buyer, "NFT", 0);
    return true;
  }

  // Get listings
  [[nodiscard]] std::vector<Listing> getActiveListings() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Listing> result;
    for (const auto &[id, listing] : listings_) {
      result.push_back(listing);
    }
    return result;
  }

  // Get NFT
  [[nodiscard]] std::optional<NFTMetadata>
  getNFT(const std::string &tokenId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nfts_.find(tokenId);
    return it != nfts_.end() ? std::optional(it->second) : std::nullopt;
  }

  // Get user NFTs
  [[nodiscard]] std::vector<NFTMetadata>
  getUserNFTs(const std::string &owner) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NFTMetadata> result;
    for (const auto &[id, nft] : nfts_) {
      if (nft.owner == owner)
        result.push_back(nft);
    }
    return result;
  }

  // Create collection
  [[nodiscard]] std::string
  createCollection(const std::string &creator, const std::string &name,
                   const std::string &description) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    Collection col;
    col.collectionId = "col_" + std::to_string(nextCollectionId_++);
    col.name = name;
    col.creator = creator;
    col.description = description;
    col.floorPrice = 0;
    col.volume = 0;
    col.itemCount = 0;

    collections_[col.collectionId] = col;
    return col.collectionId;
  }

private:
  std::map<std::string, NFTMetadata> nfts_;
  std::map<std::string, Listing> listings_;
  std::map<std::string, Collection> collections_;
  mutable std::mutex mutex_;
  int nextTokenId_{1};
  int nextListingId_{1};
  int nextCollectionId_{1};
};

} // namespace QuantumPulse::NFT

#endif // QUANTUMPULSE_NFT_V7_H
