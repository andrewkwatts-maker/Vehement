#pragma once

#include "Resource.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <chrono>

namespace Vehement {
namespace RTS {

// ============================================================================
// Trade Offer
// ============================================================================

/**
 * @brief State of a trade offer
 */
enum class TradeOfferState : uint8_t {
    Active,         ///< Available for trading
    Pending,        ///< Waiting for acceptance
    Completed,      ///< Successfully traded
    Expired,        ///< Timed out
    Cancelled       ///< Cancelled by seller
};

/**
 * @brief A trade offer between players or with NPCs
 */
struct TradeOffer {
    /// Unique identifier
    uint32_t id = 0;

    /// Player who created the offer
    std::string sellerId;

    /// Player name for display
    std::string sellerName;

    /// Resource being sold
    ResourceType selling = ResourceType::Wood;

    /// Amount being sold
    int sellAmount = 0;

    /// Resource requested in exchange
    ResourceType buying = ResourceType::Coins;

    /// Amount requested
    int buyAmount = 0;

    /// Current state
    TradeOfferState state = TradeOfferState::Active;

    /// Time when offer was created
    float createdTime = 0.0f;

    /// Duration before expiration (seconds)
    float duration = 300.0f; // 5 minutes default

    /// Whether this is an NPC offer (always available)
    bool isNpcOffer = false;

    /**
     * @brief Check if offer has expired
     */
    [[nodiscard]] bool IsExpired(float currentTime) const {
        return !isNpcOffer && (currentTime - createdTime) >= duration;
    }

    /**
     * @brief Get time remaining before expiration
     */
    [[nodiscard]] float GetTimeRemaining(float currentTime) const {
        if (isNpcOffer) return 999999.0f;
        return std::max(0.0f, duration - (currentTime - createdTime));
    }

    /**
     * @brief Calculate the exchange rate
     * @return Units of buying resource per unit of selling resource
     */
    [[nodiscard]] float GetExchangeRate() const {
        return sellAmount > 0 ? static_cast<float>(buyAmount) / sellAmount : 0.0f;
    }

    /**
     * @brief Get the value ratio compared to base values
     */
    [[nodiscard]] float GetValueRatio() const;
};

// ============================================================================
// Caravan
// ============================================================================

/**
 * @brief State of a caravan
 */
enum class CaravanState : uint8_t {
    Loading,        ///< Loading goods at origin
    Traveling,      ///< Moving to destination
    Unloading,      ///< Unloading at destination
    Returning,      ///< Returning empty
    Destroyed       ///< Destroyed by enemies
};

/**
 * @brief A caravan transporting goods between locations
 */
struct Caravan {
    /// Unique identifier
    uint32_t id = 0;

    /// Current position in world
    glm::vec2 position{0.0f};

    /// Origin position
    glm::vec2 origin{0.0f};

    /// Destination position
    glm::vec2 destination{0.0f};

    /// Current state
    CaravanState state = CaravanState::Loading;

    /// Movement speed
    float speed = 3.0f;

    /// Health
    float health = 50.0f;
    float maxHealth = 50.0f;

    /// Cargo being transported
    std::vector<std::pair<ResourceType, int>> cargo;

    /// Maximum cargo capacity (total units)
    int maxCapacity = 50;

    /// Trade offer ID (if part of a trade)
    uint32_t tradeOfferId = 0;

    /// Player ID receiving the goods
    std::string destinationPlayerId;

    /// Progress through current journey (0-1)
    float journeyProgress = 0.0f;

    /**
     * @brief Get total cargo amount
     */
    [[nodiscard]] int GetTotalCargo() const {
        int total = 0;
        for (const auto& [type, amount] : cargo) {
            total += amount;
        }
        return total;
    }

    /**
     * @brief Get remaining capacity
     */
    [[nodiscard]] int GetRemainingCapacity() const {
        return maxCapacity - GetTotalCargo();
    }

    /**
     * @brief Check if caravan is alive
     */
    [[nodiscard]] bool IsAlive() const { return health > 0 && state != CaravanState::Destroyed; }

    /**
     * @brief Add cargo
     * @return Amount actually added
     */
    int AddCargo(ResourceType type, int amount);

    /**
     * @brief Get cargo value
     */
    [[nodiscard]] int GetCargoValue() const;
};

// ============================================================================
// Market Prices
// ============================================================================

/**
 * @brief Dynamic market price for a resource type
 */
struct MarketPrice {
    ResourceType type = ResourceType::Wood;

    /// Base price in coins
    float basePrice = 1.0f;

    /// Current price multiplier
    float priceMultiplier = 1.0f;

    /// Supply level (affects price)
    float supplyLevel = 1.0f;

    /// Demand level (affects price)
    float demandLevel = 1.0f;

    /// Minimum price multiplier
    static constexpr float MIN_MULTIPLIER = 0.5f;

    /// Maximum price multiplier
    static constexpr float MAX_MULTIPLIER = 3.0f;

    /**
     * @brief Get current price
     */
    [[nodiscard]] int GetCurrentPrice() const {
        return static_cast<int>(basePrice * priceMultiplier);
    }

    /**
     * @brief Get buy price (slightly higher than base)
     */
    [[nodiscard]] int GetBuyPrice() const {
        return static_cast<int>(GetCurrentPrice() * 1.1f);
    }

    /**
     * @brief Get sell price (slightly lower than base)
     */
    [[nodiscard]] int GetSellPrice() const {
        return static_cast<int>(GetCurrentPrice() * 0.9f);
    }

    /**
     * @brief Update price based on supply/demand
     */
    void UpdatePrice();

    /**
     * @brief Record a purchase (increases demand)
     */
    void RecordPurchase(int amount);

    /**
     * @brief Record a sale (increases supply)
     */
    void RecordSale(int amount);

    /**
     * @brief Decay price towards base over time
     */
    void DecayTowardsBase(float deltaTime);
};

// ============================================================================
// Trading Post
// ============================================================================

/**
 * @brief Configuration for a trading post
 */
struct TradingPostConfig {
    bool allowPlayerTrades = true;      ///< Allow player-to-player trades
    bool allowNpcTrades = true;         ///< Allow trades with NPC merchants
    float tradeRange = 50.0f;           ///< Maximum distance for trading
    float caravanSpeed = 3.0f;          ///< Base caravan speed
    int maxActiveOffers = 10;           ///< Maximum active offers per player
    float priceDecayRate = 0.01f;       ///< How fast prices return to normal
};

/**
 * @brief A trading post building that facilitates resource exchange
 */
class TradingPost {
public:
    TradingPost();
    ~TradingPost();

    // Delete copy, allow move
    TradingPost(const TradingPost&) = delete;
    TradingPost& operator=(const TradingPost&) = delete;
    TradingPost(TradingPost&&) noexcept = default;
    TradingPost& operator=(TradingPost&&) noexcept = default;

    /**
     * @brief Initialize the trading post
     */
    void Initialize(const glm::vec2& position, const TradingPostConfig& config = TradingPostConfig{});

    /**
     * @brief Update trading post and caravans
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Direct Trading (NPC)
    // -------------------------------------------------------------------------

    /**
     * @brief Buy resources with coins from NPC merchant
     * @param type Resource to buy
     * @param amount Amount to buy
     * @param stock Resource stock to modify
     * @return true if purchase succeeded
     */
    bool BuyFromMerchant(ResourceType type, int amount, ResourceStock& stock);

    /**
     * @brief Sell resources for coins to NPC merchant
     * @param type Resource to sell
     * @param amount Amount to sell
     * @param stock Resource stock to modify
     * @return true if sale succeeded
     */
    bool SellToMerchant(ResourceType type, int amount, ResourceStock& stock);

    /**
     * @brief Get current buy price for a resource
     */
    [[nodiscard]] int GetBuyPrice(ResourceType type) const;

    /**
     * @brief Get current sell price for a resource
     */
    [[nodiscard]] int GetSellPrice(ResourceType type) const;

    /**
     * @brief Get market price info for a resource
     */
    [[nodiscard]] const MarketPrice& GetMarketPrice(ResourceType type) const;

    // -------------------------------------------------------------------------
    // Player Trading
    // -------------------------------------------------------------------------

    /**
     * @brief Create a trade offer
     * @param sellerId Player ID creating the offer
     * @param sellerName Player name
     * @param selling Resource being sold
     * @param sellAmount Amount being sold
     * @param buying Resource wanted
     * @param buyAmount Amount wanted
     * @param stock Seller's stock (resources will be reserved)
     * @return Offer ID, or 0 if failed
     */
    uint32_t CreateOffer(
        const std::string& sellerId,
        const std::string& sellerName,
        ResourceType selling,
        int sellAmount,
        ResourceType buying,
        int buyAmount,
        ResourceStock& stock
    );

    /**
     * @brief Accept a trade offer
     * @param offerId Offer to accept
     * @param buyerId Player accepting
     * @param buyerStock Buyer's resource stock
     * @return true if trade succeeded
     */
    bool AcceptOffer(uint32_t offerId, const std::string& buyerId, ResourceStock& buyerStock);

    /**
     * @brief Cancel a trade offer
     * @param offerId Offer to cancel
     * @param playerId Player cancelling (must be seller)
     * @param stock Stock to return resources to
     * @return true if cancelled
     */
    bool CancelOffer(uint32_t offerId, const std::string& playerId, ResourceStock& stock);

    /**
     * @brief Get all active offers
     */
    [[nodiscard]] std::vector<const TradeOffer*> GetActiveOffers() const;

    /**
     * @brief Get offers by a specific player
     */
    [[nodiscard]] std::vector<const TradeOffer*> GetPlayerOffers(const std::string& playerId) const;

    /**
     * @brief Get an offer by ID
     */
    [[nodiscard]] const TradeOffer* GetOffer(uint32_t offerId) const;

    // -------------------------------------------------------------------------
    // Caravans
    // -------------------------------------------------------------------------

    /**
     * @brief Send a caravan to deliver goods
     * @param cargo Resources to transport
     * @param destination Destination position
     * @param destinationPlayerId Player receiving goods
     * @param stock Source stock (removes cargo)
     * @return Caravan ID, or 0 if failed
     */
    uint32_t SendCaravan(
        const std::vector<std::pair<ResourceType, int>>& cargo,
        const glm::vec2& destination,
        const std::string& destinationPlayerId,
        ResourceStock& stock
    );

    /**
     * @brief Get all active caravans
     */
    [[nodiscard]] const std::vector<Caravan>& GetCaravans() const { return m_caravans; }

    /**
     * @brief Attack a caravan (from combat system)
     * @param caravanId Caravan to attack
     * @param damage Damage to deal
     * @return true if caravan was destroyed
     */
    bool AttackCaravan(uint32_t caravanId, float damage);

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Get position
     */
    [[nodiscard]] const glm::vec2& GetPosition() const { return m_position; }

    /**
     * @brief Set position
     */
    void SetPosition(const glm::vec2& pos) { m_position = pos; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const TradingPostConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set resource stock for NPC trades
     */
    void SetResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    // -------------------------------------------------------------------------
    // Firebase Integration
    // -------------------------------------------------------------------------

    /**
     * @brief Sync offers to Firebase
     */
    void SyncOffersToFirebase();

    /**
     * @brief Receive offers from Firebase
     */
    void ReceiveOffersFromFirebase(const std::vector<TradeOffer>& offers);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using TradeCompleteCallback = std::function<void(const TradeOffer&, const std::string& buyerId)>;
    using CaravanArrivedCallback = std::function<void(const Caravan&)>;
    using CaravanDestroyedCallback = std::function<void(const Caravan&)>;
    using PriceChangedCallback = std::function<void(ResourceType, int oldPrice, int newPrice)>;

    void SetOnTradeComplete(TradeCompleteCallback cb) { m_onTradeComplete = std::move(cb); }
    void SetOnCaravanArrived(CaravanArrivedCallback cb) { m_onCaravanArrived = std::move(cb); }
    void SetOnCaravanDestroyed(CaravanDestroyedCallback cb) { m_onCaravanDestroyed = std::move(cb); }
    void SetOnPriceChanged(PriceChangedCallback cb) { m_onPriceChanged = std::move(cb); }

private:
    void UpdateOffers(float deltaTime);
    void UpdateCaravans(float deltaTime);
    void UpdatePrices(float deltaTime);

    void CompleteCaravanDelivery(Caravan& caravan);
    void DestroyCaravan(Caravan& caravan);

    uint32_t GenerateOfferId();
    uint32_t GenerateCaravanId();

    void InitializeMarketPrices();

    glm::vec2 m_position{0.0f};
    TradingPostConfig m_config;

    std::vector<TradeOffer> m_offers;
    std::vector<Caravan> m_caravans;

    std::unordered_map<ResourceType, MarketPrice> m_marketPrices;

    ResourceStock* m_resourceStock = nullptr;

    uint32_t m_nextOfferId = 1;
    uint32_t m_nextCaravanId = 1;

    float m_currentTime = 0.0f;

    // Callbacks
    TradeCompleteCallback m_onTradeComplete;
    CaravanArrivedCallback m_onCaravanArrived;
    CaravanDestroyedCallback m_onCaravanDestroyed;
    PriceChangedCallback m_onPriceChanged;

    bool m_initialized = false;
};

// ============================================================================
// Trading System
// ============================================================================

/**
 * @brief Global trading system managing all trading posts
 */
class TradingSystem {
public:
    TradingSystem();
    ~TradingSystem();

    /**
     * @brief Initialize the trading system
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update all trading
     */
    void Update(float deltaTime);

    /**
     * @brief Create a trading post
     */
    TradingPost* CreateTradingPost(const glm::vec2& position);

    /**
     * @brief Get trading post by ID
     */
    [[nodiscard]] TradingPost* GetTradingPost(int index);

    /**
     * @brief Get all trading posts
     */
    [[nodiscard]] const std::vector<std::unique_ptr<TradingPost>>& GetTradingPosts() const {
        return m_tradingPosts;
    }

    /**
     * @brief Set resource stock for all trading posts
     */
    void SetResourceStock(ResourceStock* stock);

    /**
     * @brief Get global average price for a resource
     */
    [[nodiscard]] int GetAveragePrice(ResourceType type) const;

private:
    std::vector<std::unique_ptr<TradingPost>> m_tradingPosts;
    ResourceStock* m_resourceStock = nullptr;
    bool m_initialized = false;
};

} // namespace RTS
} // namespace Vehement
