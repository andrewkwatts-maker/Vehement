#include "Trading.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// TradeOffer Implementation
// ============================================================================

float TradeOffer::GetValueRatio() const {
    const auto& values = GetResourceValues();

    float sellValue = sellAmount * values.GetBaseValue(selling);
    float buyValue = buyAmount * values.GetBaseValue(buying);

    return sellValue > 0 ? buyValue / sellValue : 0.0f;
}

// ============================================================================
// Caravan Implementation
// ============================================================================

int Caravan::AddCargo(ResourceType type, int amount) {
    int space = GetRemainingCapacity();
    int toAdd = std::min(amount, space);

    if (toAdd <= 0) return 0;

    // Check if already carrying this type
    for (auto& [t, amt] : cargo) {
        if (t == type) {
            amt += toAdd;
            return toAdd;
        }
    }

    // Add new cargo type
    cargo.emplace_back(type, toAdd);
    return toAdd;
}

int Caravan::GetCargoValue() const {
    const auto& values = GetResourceValues();
    int total = 0;

    for (const auto& [type, amount] : cargo) {
        total += static_cast<int>(amount * values.GetBaseValue(type));
    }

    return total;
}

// ============================================================================
// MarketPrice Implementation
// ============================================================================

void MarketPrice::UpdatePrice() {
    // Price increases with demand, decreases with supply
    float supplyDemandRatio = demandLevel / std::max(0.1f, supplyLevel);

    // Smoothly adjust multiplier
    float targetMultiplier = supplyDemandRatio;
    targetMultiplier = std::clamp(targetMultiplier, MIN_MULTIPLIER, MAX_MULTIPLIER);

    // Gradual change
    priceMultiplier += (targetMultiplier - priceMultiplier) * 0.1f;
    priceMultiplier = std::clamp(priceMultiplier, MIN_MULTIPLIER, MAX_MULTIPLIER);
}

void MarketPrice::RecordPurchase(int amount) {
    // Buying increases demand
    demandLevel += amount * 0.01f;
    demandLevel = std::min(demandLevel, 5.0f);
    UpdatePrice();
}

void MarketPrice::RecordSale(int amount) {
    // Selling increases supply
    supplyLevel += amount * 0.01f;
    supplyLevel = std::min(supplyLevel, 5.0f);
    UpdatePrice();
}

void MarketPrice::DecayTowardsBase(float deltaTime) {
    // Supply and demand slowly return to 1.0
    const float decayRate = 0.05f;

    if (supplyLevel > 1.0f) {
        supplyLevel -= decayRate * deltaTime;
        supplyLevel = std::max(1.0f, supplyLevel);
    } else if (supplyLevel < 1.0f) {
        supplyLevel += decayRate * deltaTime;
        supplyLevel = std::min(1.0f, supplyLevel);
    }

    if (demandLevel > 1.0f) {
        demandLevel -= decayRate * deltaTime;
        demandLevel = std::max(1.0f, demandLevel);
    } else if (demandLevel < 1.0f) {
        demandLevel += decayRate * deltaTime;
        demandLevel = std::min(1.0f, demandLevel);
    }

    UpdatePrice();
}

// ============================================================================
// TradingPost Implementation
// ============================================================================

TradingPost::TradingPost() = default;

TradingPost::~TradingPost() = default;

void TradingPost::Initialize(const glm::vec2& position, const TradingPostConfig& config) {
    m_position = position;
    m_config = config;

    InitializeMarketPrices();

    m_initialized = true;
}

void TradingPost::Update(float deltaTime) {
    if (!m_initialized) return;

    m_currentTime += deltaTime;

    UpdateOffers(deltaTime);
    UpdateCaravans(deltaTime);
    UpdatePrices(deltaTime);
}

// -------------------------------------------------------------------------
// Direct Trading (NPC)
// -------------------------------------------------------------------------

bool TradingPost::BuyFromMerchant(ResourceType type, int amount, ResourceStock& stock) {
    if (!m_config.allowNpcTrades || amount <= 0) return false;

    int totalCost = GetBuyPrice(type) * amount;

    // Check if player can afford
    if (!stock.CanAfford(ResourceType::Coins, totalCost)) return false;

    // Execute trade
    if (!stock.Remove(ResourceType::Coins, totalCost)) return false;

    int added = stock.Add(type, amount);

    // If we couldn't add all resources (storage full), refund excess cost
    if (added < amount) {
        int refund = (amount - added) * GetBuyPrice(type);
        stock.Add(ResourceType::Coins, refund);
    }

    // Update market price
    m_marketPrices[type].RecordPurchase(added);

    return true;
}

bool TradingPost::SellToMerchant(ResourceType type, int amount, ResourceStock& stock) {
    if (!m_config.allowNpcTrades || amount <= 0) return false;
    if (type == ResourceType::Coins) return false; // Can't sell coins

    // Check if player has resources
    if (!stock.CanAfford(type, amount)) return false;

    int payment = GetSellPrice(type) * amount;

    // Execute trade
    if (!stock.Remove(type, amount)) return false;
    stock.Add(ResourceType::Coins, payment);

    // Update market price
    m_marketPrices[type].RecordSale(amount);

    return true;
}

int TradingPost::GetBuyPrice(ResourceType type) const {
    auto it = m_marketPrices.find(type);
    if (it == m_marketPrices.end()) {
        return static_cast<int>(GetResourceValues().GetBaseValue(type));
    }
    return it->second.GetBuyPrice();
}

int TradingPost::GetSellPrice(ResourceType type) const {
    auto it = m_marketPrices.find(type);
    if (it == m_marketPrices.end()) {
        return static_cast<int>(GetResourceValues().GetBaseValue(type));
    }
    return it->second.GetSellPrice();
}

const MarketPrice& TradingPost::GetMarketPrice(ResourceType type) const {
    static MarketPrice defaultPrice;
    auto it = m_marketPrices.find(type);
    if (it == m_marketPrices.end()) {
        return defaultPrice;
    }
    return it->second;
}

// -------------------------------------------------------------------------
// Player Trading
// -------------------------------------------------------------------------

uint32_t TradingPost::CreateOffer(
    const std::string& sellerId,
    const std::string& sellerName,
    ResourceType selling,
    int sellAmount,
    ResourceType buying,
    int buyAmount,
    ResourceStock& stock
) {
    if (!m_config.allowPlayerTrades) return 0;
    if (sellAmount <= 0 || buyAmount <= 0) return 0;

    // Check player offer limit
    int playerOffers = 0;
    for (const auto& offer : m_offers) {
        if (offer.sellerId == sellerId && offer.state == TradeOfferState::Active) {
            playerOffers++;
        }
    }
    if (playerOffers >= m_config.maxActiveOffers) return 0;

    // Reserve resources from seller
    if (!stock.CanAfford(selling, sellAmount)) return 0;
    if (!stock.Remove(selling, sellAmount)) return 0;

    TradeOffer offer;
    offer.id = GenerateOfferId();
    offer.sellerId = sellerId;
    offer.sellerName = sellerName;
    offer.selling = selling;
    offer.sellAmount = sellAmount;
    offer.buying = buying;
    offer.buyAmount = buyAmount;
    offer.state = TradeOfferState::Active;
    offer.createdTime = m_currentTime;
    offer.isNpcOffer = false;

    m_offers.push_back(std::move(offer));

    return m_offers.back().id;
}

bool TradingPost::AcceptOffer(uint32_t offerId, const std::string& buyerId, ResourceStock& buyerStock) {
    TradeOffer* offer = nullptr;
    for (auto& o : m_offers) {
        if (o.id == offerId && o.state == TradeOfferState::Active) {
            offer = &o;
            break;
        }
    }

    if (!offer) return false;

    // Can't accept own offer
    if (offer->sellerId == buyerId) return false;

    // Check if buyer can afford
    if (!buyerStock.CanAfford(offer->buying, offer->buyAmount)) return false;

    // Execute trade
    // Remove payment from buyer
    if (!buyerStock.Remove(offer->buying, offer->buyAmount)) return false;

    // Give goods to buyer
    buyerStock.Add(offer->selling, offer->sellAmount);

    // Mark offer as completed
    offer->state = TradeOfferState::Completed;

    // Note: Seller receives goods via callback or Firebase sync
    if (m_onTradeComplete) {
        m_onTradeComplete(*offer, buyerId);
    }

    return true;
}

bool TradingPost::CancelOffer(uint32_t offerId, const std::string& playerId, ResourceStock& stock) {
    for (auto& offer : m_offers) {
        if (offer.id == offerId && offer.sellerId == playerId) {
            if (offer.state == TradeOfferState::Active) {
                // Return reserved resources
                stock.Add(offer.selling, offer.sellAmount);
                offer.state = TradeOfferState::Cancelled;
                return true;
            }
        }
    }
    return false;
}

std::vector<const TradeOffer*> TradingPost::GetActiveOffers() const {
    std::vector<const TradeOffer*> result;
    for (const auto& offer : m_offers) {
        if (offer.state == TradeOfferState::Active) {
            result.push_back(&offer);
        }
    }
    return result;
}

std::vector<const TradeOffer*> TradingPost::GetPlayerOffers(const std::string& playerId) const {
    std::vector<const TradeOffer*> result;
    for (const auto& offer : m_offers) {
        if (offer.sellerId == playerId) {
            result.push_back(&offer);
        }
    }
    return result;
}

const TradeOffer* TradingPost::GetOffer(uint32_t offerId) const {
    for (const auto& offer : m_offers) {
        if (offer.id == offerId) return &offer;
    }
    return nullptr;
}

// -------------------------------------------------------------------------
// Caravans
// -------------------------------------------------------------------------

uint32_t TradingPost::SendCaravan(
    const std::vector<std::pair<ResourceType, int>>& cargo,
    const glm::vec2& destination,
    const std::string& destinationPlayerId,
    ResourceStock& stock
) {
    // Verify and remove cargo from stock
    for (const auto& [type, amount] : cargo) {
        if (!stock.CanAfford(type, amount)) return 0;
    }

    for (const auto& [type, amount] : cargo) {
        stock.Remove(type, amount);
    }

    Caravan caravan;
    caravan.id = GenerateCaravanId();
    caravan.position = m_position;
    caravan.origin = m_position;
    caravan.destination = destination;
    caravan.state = CaravanState::Traveling;
    caravan.speed = m_config.caravanSpeed;
    caravan.cargo = cargo;
    caravan.destinationPlayerId = destinationPlayerId;
    caravan.journeyProgress = 0.0f;

    m_caravans.push_back(std::move(caravan));

    return m_caravans.back().id;
}

bool TradingPost::AttackCaravan(uint32_t caravanId, float damage) {
    for (auto& caravan : m_caravans) {
        if (caravan.id == caravanId && caravan.IsAlive()) {
            caravan.health -= damage;

            if (caravan.health <= 0) {
                DestroyCaravan(caravan);
                return true;
            }
            return false;
        }
    }
    return false;
}

// -------------------------------------------------------------------------
// Firebase Integration
// -------------------------------------------------------------------------

void TradingPost::SyncOffersToFirebase() {
    // TODO: Implement Firebase sync
    // This would use FirebaseManager to push active offers
}

void TradingPost::ReceiveOffersFromFirebase(const std::vector<TradeOffer>& offers) {
    // Merge received offers, avoiding duplicates
    for (const auto& received : offers) {
        bool exists = false;
        for (auto& existing : m_offers) {
            if (existing.id == received.id) {
                exists = true;
                // Update state
                existing.state = received.state;
                break;
            }
        }

        if (!exists && received.state == TradeOfferState::Active) {
            m_offers.push_back(received);
        }
    }
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void TradingPost::UpdateOffers(float deltaTime) {
    // Check for expired offers
    for (auto& offer : m_offers) {
        if (offer.state == TradeOfferState::Active && offer.IsExpired(m_currentTime)) {
            offer.state = TradeOfferState::Expired;
            // Resources are lost (or could be returned via callback)
        }
    }

    // Remove old completed/expired/cancelled offers
    m_offers.erase(
        std::remove_if(m_offers.begin(), m_offers.end(),
            [this](const TradeOffer& offer) {
                if (offer.state != TradeOfferState::Active) {
                    return (m_currentTime - offer.createdTime) > offer.duration * 2;
                }
                return false;
            }),
        m_offers.end()
    );
}

void TradingPost::UpdateCaravans(float deltaTime) {
    for (auto& caravan : m_caravans) {
        if (!caravan.IsAlive()) continue;

        if (caravan.state == CaravanState::Traveling) {
            // Calculate journey
            glm::vec2 direction = caravan.destination - caravan.origin;
            float totalDistance = glm::length(direction);

            if (totalDistance > 0) {
                float progressPerSecond = caravan.speed / totalDistance;
                caravan.journeyProgress += progressPerSecond * deltaTime;

                // Update position
                caravan.position = caravan.origin + direction * caravan.journeyProgress;

                // Check arrival
                if (caravan.journeyProgress >= 1.0f) {
                    caravan.position = caravan.destination;
                    CompleteCaravanDelivery(caravan);
                }
            } else {
                // Already at destination
                CompleteCaravanDelivery(caravan);
            }
        } else if (caravan.state == CaravanState::Returning) {
            // Return journey
            glm::vec2 direction = caravan.origin - caravan.destination;
            float totalDistance = glm::length(direction);

            if (totalDistance > 0) {
                float progressPerSecond = caravan.speed / totalDistance;
                caravan.journeyProgress += progressPerSecond * deltaTime;

                caravan.position = caravan.destination + direction * caravan.journeyProgress;

                if (caravan.journeyProgress >= 1.0f) {
                    caravan.position = caravan.origin;
                    // Caravan has returned - can be removed or reused
                }
            }
        }
    }

    // Remove completed/destroyed caravans that have returned
    m_caravans.erase(
        std::remove_if(m_caravans.begin(), m_caravans.end(),
            [](const Caravan& c) {
                return c.state == CaravanState::Destroyed ||
                       (c.state == CaravanState::Returning && c.journeyProgress >= 1.0f);
            }),
        m_caravans.end()
    );
}

void TradingPost::UpdatePrices(float deltaTime) {
    for (auto& [type, price] : m_marketPrices) {
        int oldPrice = price.GetCurrentPrice();
        price.DecayTowardsBase(deltaTime);
        int newPrice = price.GetCurrentPrice();

        if (oldPrice != newPrice && m_onPriceChanged) {
            m_onPriceChanged(type, oldPrice, newPrice);
        }
    }
}

void TradingPost::CompleteCaravanDelivery(Caravan& caravan) {
    caravan.state = CaravanState::Unloading;

    // Cargo delivered - notify via callback
    if (m_onCaravanArrived) {
        m_onCaravanArrived(caravan);
    }

    // If we have the destination player's stock, add resources directly
    // In multiplayer, this would be handled via Firebase

    caravan.cargo.clear();
    caravan.state = CaravanState::Returning;
    caravan.journeyProgress = 0.0f;
}

void TradingPost::DestroyCaravan(Caravan& caravan) {
    caravan.state = CaravanState::Destroyed;
    caravan.health = 0;

    // Cargo is lost
    if (m_onCaravanDestroyed) {
        m_onCaravanDestroyed(caravan);
    }

    caravan.cargo.clear();
}

uint32_t TradingPost::GenerateOfferId() {
    return m_nextOfferId++;
}

uint32_t TradingPost::GenerateCaravanId() {
    return m_nextCaravanId++;
}

void TradingPost::InitializeMarketPrices() {
    const auto& values = GetResourceValues();

    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        if (type == ResourceType::Coins) continue; // Coins don't have market price

        MarketPrice price;
        price.type = type;
        price.basePrice = values.GetBaseValue(type);
        price.priceMultiplier = 1.0f;
        price.supplyLevel = 1.0f;
        price.demandLevel = 1.0f;

        m_marketPrices[type] = price;
    }
}

// ============================================================================
// TradingSystem Implementation
// ============================================================================

TradingSystem::TradingSystem() = default;

TradingSystem::~TradingSystem() {
    Shutdown();
}

void TradingSystem::Initialize() {
    m_initialized = true;
}

void TradingSystem::Shutdown() {
    m_tradingPosts.clear();
    m_initialized = false;
}

void TradingSystem::Update(float deltaTime) {
    if (!m_initialized) return;

    for (auto& post : m_tradingPosts) {
        post->Update(deltaTime);
    }
}

TradingPost* TradingSystem::CreateTradingPost(const glm::vec2& position) {
    auto post = std::make_unique<TradingPost>();
    post->Initialize(position);
    post->SetResourceStock(m_resourceStock);

    m_tradingPosts.push_back(std::move(post));
    return m_tradingPosts.back().get();
}

TradingPost* TradingSystem::GetTradingPost(int index) {
    if (index >= 0 && index < static_cast<int>(m_tradingPosts.size())) {
        return m_tradingPosts[index].get();
    }
    return nullptr;
}

void TradingSystem::SetResourceStock(ResourceStock* stock) {
    m_resourceStock = stock;
    for (auto& post : m_tradingPosts) {
        post->SetResourceStock(stock);
    }
}

int TradingSystem::GetAveragePrice(ResourceType type) const {
    if (m_tradingPosts.empty()) {
        return static_cast<int>(GetResourceValues().GetBaseValue(type));
    }

    int total = 0;
    for (const auto& post : m_tradingPosts) {
        total += post->GetBuyPrice(type);
    }
    return total / static_cast<int>(m_tradingPosts.size());
}

} // namespace RTS
} // namespace Vehement
