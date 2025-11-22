#include "Gathering.hpp"
#include <algorithm>
#include <random>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// Node Type Helpers
// ============================================================================

ResourceType GetNodeResourceType(NodeType type) {
    switch (type) {
        case NodeType::Tree:           return ResourceType::Wood;
        case NodeType::RockDeposit:    return ResourceType::Stone;
        case NodeType::ScrapPile:      return ResourceType::Metal;
        case NodeType::AbandonedCache: return ResourceType::Food; // Can yield various
        case NodeType::CropField:      return ResourceType::Food;
        case NodeType::FuelTank:       return ResourceType::Fuel;
        case NodeType::MedicalSupply:  return ResourceType::Medicine;
        case NodeType::AmmoCache:      return ResourceType::Ammunition;
        default:                       return ResourceType::Wood;
    }
}

const char* GetNodeTypeName(NodeType type) {
    switch (type) {
        case NodeType::Tree:           return "Tree";
        case NodeType::RockDeposit:    return "Rock Deposit";
        case NodeType::ScrapPile:      return "Scrap Pile";
        case NodeType::AbandonedCache: return "Abandoned Cache";
        case NodeType::CropField:      return "Crop Field";
        case NodeType::FuelTank:       return "Fuel Tank";
        case NodeType::MedicalSupply:  return "Medical Supply";
        case NodeType::AmmoCache:      return "Ammo Cache";
        default:                       return "Unknown";
    }
}

// ============================================================================
// ResourceNode Implementation
// ============================================================================

int ResourceNode::Extract(int amount) {
    if (!active || remaining <= 0) return 0;

    int extracted = std::min(amount, remaining);
    remaining -= extracted;

    if (remaining <= 0) {
        active = false;
        remaining = 0;
        respawnTimer = respawnTime;
    }

    return extracted;
}

void ResourceNode::UpdateRespawn(float deltaTime) {
    if (active) return;

    respawnTimer -= deltaTime;
    if (respawnTimer <= 0.0f) {
        Respawn();
    }
}

void ResourceNode::Respawn() {
    remaining = maxAmount;
    active = true;
    respawnTimer = 0.0f;
    assignedGatherers = 0;
}

ResourceNode ResourceNode::CreateDefault(NodeType type, const glm::vec2& pos, uint32_t nodeId) {
    ResourceNode node;
    node.id = nodeId;
    node.position = pos;
    node.nodeType = type;
    node.resourceType = GetNodeResourceType(type);

    // Set type-specific defaults
    switch (type) {
        case NodeType::Tree:
            node.maxAmount = 80;
            node.remaining = 80;
            node.gatherRate = 2.5f;
            node.radius = 1.5f;
            node.respawnTime = 180.0f; // 3 minutes
            node.maxGatherers = 2;
            break;

        case NodeType::RockDeposit:
            node.maxAmount = 150;
            node.remaining = 150;
            node.gatherRate = 1.5f;
            node.radius = 2.0f;
            node.respawnTime = 300.0f; // 5 minutes
            node.maxGatherers = 3;
            break;

        case NodeType::ScrapPile:
            node.maxAmount = 60;
            node.remaining = 60;
            node.gatherRate = 1.0f;
            node.radius = 2.5f;
            node.respawnTime = 240.0f; // 4 minutes
            node.maxGatherers = 2;
            break;

        case NodeType::AbandonedCache:
            node.maxAmount = 40;
            node.remaining = 40;
            node.gatherRate = 3.0f;
            node.radius = 1.0f;
            node.respawnTime = 600.0f; // 10 minutes (rare)
            node.maxGatherers = 1;
            break;

        case NodeType::CropField:
            node.maxAmount = 100;
            node.remaining = 100;
            node.gatherRate = 2.0f;
            node.radius = 3.0f;
            node.respawnTime = 120.0f; // 2 minutes
            node.maxGatherers = 4;
            break;

        case NodeType::FuelTank:
            node.maxAmount = 50;
            node.remaining = 50;
            node.gatherRate = 1.5f;
            node.radius = 1.5f;
            node.respawnTime = 360.0f; // 6 minutes
            node.maxGatherers = 1;
            break;

        case NodeType::MedicalSupply:
            node.maxAmount = 30;
            node.remaining = 30;
            node.gatherRate = 0.5f;
            node.radius = 1.0f;
            node.respawnTime = 480.0f; // 8 minutes (valuable)
            node.maxGatherers = 1;
            break;

        case NodeType::AmmoCache:
            node.maxAmount = 100;
            node.remaining = 100;
            node.gatherRate = 2.0f;
            node.radius = 1.5f;
            node.respawnTime = 300.0f; // 5 minutes
            node.maxGatherers = 2;
            break;

        default:
            break;
    }

    return node;
}

// ============================================================================
// Gatherer Implementation
// ============================================================================

int Gatherer::AddToCarry(ResourceType type, int amount) {
    if (carryingAmount > 0 && carryingType != type) {
        // Already carrying a different type
        return 0;
    }

    int space = GetFreeSpace();
    int toAdd = std::min(amount, space);

    carryingType = type;
    carryingAmount += toAdd;

    return toAdd;
}

int Gatherer::DepositAll() {
    int deposited = carryingAmount;
    carryingAmount = 0;
    return deposited;
}

void Gatherer::Reset() {
    state = GathererState::Idle;
    targetNodeId = -1;
    carryingAmount = 0;
    stateTimer = 0.0f;
}

// ============================================================================
// GatheringSystem Implementation
// ============================================================================

GatheringSystem::GatheringSystem() = default;

GatheringSystem::~GatheringSystem() {
    Shutdown();
}

void GatheringSystem::Initialize(const GatheringConfig& config) {
    m_config = config;
    m_scarcitySettings = ScarcitySettings::Normal();

    // Initialize statistics
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        m_totalGathered[type] = 0;
        m_recentGatherRates[type] = 0.0f;
    }

    m_initialized = true;
}

void GatheringSystem::Shutdown() {
    m_nodes.clear();
    m_gatherers.clear();
    m_initialized = false;
}

void GatheringSystem::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateNodes(deltaTime);
    UpdateGatherers(deltaTime);
}

// -------------------------------------------------------------------------
// Node Management
// -------------------------------------------------------------------------

ResourceNode* GatheringSystem::SpawnNode(NodeType type, const glm::vec2& position, int amount) {
    uint32_t id = GenerateNodeId();
    ResourceNode node = ResourceNode::CreateDefault(type, position, id);

    // Apply scarcity settings
    node.gatherRate *= m_scarcitySettings.gatherRateMultiplier;
    node.respawnTime *= m_scarcitySettings.respawnTimeMultiplier;

    if (amount >= 0) {
        node.remaining = amount;
        node.maxAmount = std::max(node.maxAmount, amount);
    } else {
        node.remaining = static_cast<int>(node.maxAmount * m_scarcitySettings.startingResourceMultiplier);
    }

    m_nodes.push_back(std::move(node));
    return &m_nodes.back();
}

void GatheringSystem::RemoveNode(uint32_t nodeId) {
    // Unassign any gatherers from this node
    for (auto& gatherer : m_gatherers) {
        if (gatherer.targetNodeId == static_cast<int32_t>(nodeId)) {
            gatherer.targetNodeId = -1;
            if (gatherer.state == GathererState::Gathering ||
                gatherer.state == GathererState::MovingToNode) {
                gatherer.state = GathererState::Idle;
            }
        }
    }

    m_nodes.erase(
        std::remove_if(m_nodes.begin(), m_nodes.end(),
            [nodeId](const ResourceNode& n) { return n.id == nodeId; }),
        m_nodes.end()
    );
}

ResourceNode* GatheringSystem::GetNode(uint32_t nodeId) {
    for (auto& node : m_nodes) {
        if (node.id == nodeId) return &node;
    }
    return nullptr;
}

const ResourceNode* GatheringSystem::GetNode(uint32_t nodeId) const {
    for (const auto& node : m_nodes) {
        if (node.id == nodeId) return &node;
    }
    return nullptr;
}

std::vector<ResourceNode*> GatheringSystem::FindNodesNear(
    const glm::vec2& position,
    float radius,
    int type
) {
    std::vector<ResourceNode*> result;
    float radiusSq = radius * radius;

    for (auto& node : m_nodes) {
        if (!node.active) continue;
        if (type >= 0 && static_cast<int>(node.nodeType) != type) continue;

        float distSq = glm::dot(node.position - position, node.position - position);
        if (distSq <= radiusSq) {
            result.push_back(&node);
        }
    }

    // Sort by distance
    std::sort(result.begin(), result.end(),
        [&position](const ResourceNode* a, const ResourceNode* b) {
            float distA = glm::dot(a->position - position, a->position - position);
            float distB = glm::dot(b->position - position, b->position - position);
            return distA < distB;
        });

    return result;
}

ResourceNode* GatheringSystem::FindNearestNode(
    const glm::vec2& position,
    ResourceType resourceType
) {
    ResourceNode* nearest = nullptr;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (auto& node : m_nodes) {
        if (!node.active || node.resourceType != resourceType) continue;
        if (!node.CanAssignGatherer()) continue;

        float distSq = glm::dot(node.position - position, node.position - position);
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = &node;
        }
    }

    return nearest;
}

void GatheringSystem::SpawnNodesInArea(
    const glm::vec2& center,
    float radius,
    int count,
    int type
) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> typeDist(0, static_cast<int>(NodeType::COUNT) - 1);

    for (int i = 0; i < count; ++i) {
        float angle = angleDist(gen);
        float r = radius * std::sqrt(radiusDist(gen)); // Square root for uniform distribution
        glm::vec2 pos = center + glm::vec2(r * std::cos(angle), r * std::sin(angle));

        NodeType nodeType = (type >= 0)
            ? static_cast<NodeType>(type)
            : static_cast<NodeType>(typeDist(gen));

        SpawnNode(nodeType, pos);
    }
}

// -------------------------------------------------------------------------
// Gatherer Management
// -------------------------------------------------------------------------

Gatherer* GatheringSystem::CreateGatherer(const glm::vec2& position) {
    Gatherer gatherer;
    gatherer.id = GenerateGathererId();
    gatherer.position = position;
    gatherer.storagePosition = m_storageLocation;
    gatherer.moveSpeed = m_config.gathererMoveSpeed;
    gatherer.carryCapacity = m_config.defaultCarryCapacity;

    m_gatherers.push_back(std::move(gatherer));
    return &m_gatherers.back();
}

void GatheringSystem::RemoveGatherer(uint32_t gathererId) {
    // First unassign from any node
    for (auto& gatherer : m_gatherers) {
        if (gatherer.id == gathererId && gatherer.targetNodeId >= 0) {
            if (auto* node = GetNode(static_cast<uint32_t>(gatherer.targetNodeId))) {
                node->assignedGatherers = std::max(0, node->assignedGatherers - 1);
            }
        }
    }

    m_gatherers.erase(
        std::remove_if(m_gatherers.begin(), m_gatherers.end(),
            [gathererId](const Gatherer& g) { return g.id == gathererId; }),
        m_gatherers.end()
    );
}

Gatherer* GatheringSystem::GetGatherer(uint32_t gathererId) {
    for (auto& g : m_gatherers) {
        if (g.id == gathererId) return &g;
    }
    return nullptr;
}

const Gatherer* GatheringSystem::GetGatherer(uint32_t gathererId) const {
    for (const auto& g : m_gatherers) {
        if (g.id == gathererId) return &g;
    }
    return nullptr;
}

bool GatheringSystem::AssignGathererToNode(uint32_t gathererId, uint32_t nodeId) {
    Gatherer* gatherer = GetGatherer(gathererId);
    ResourceNode* node = GetNode(nodeId);

    if (!gatherer || !node) return false;
    if (!node->CanAssignGatherer()) return false;

    // Unassign from previous node if any
    UnassignGatherer(gathererId);

    gatherer->targetNodeId = static_cast<int32_t>(nodeId);
    gatherer->state = GathererState::MovingToNode;
    gatherer->stateTimer = 0.0f;
    node->assignedGatherers++;

    return true;
}

void GatheringSystem::UnassignGatherer(uint32_t gathererId) {
    Gatherer* gatherer = GetGatherer(gathererId);
    if (!gatherer) return;

    if (gatherer->targetNodeId >= 0) {
        if (auto* node = GetNode(static_cast<uint32_t>(gatherer->targetNodeId))) {
            node->assignedGatherers = std::max(0, node->assignedGatherers - 1);
        }
    }

    gatherer->targetNodeId = -1;
    if (gatherer->IsCarrying()) {
        gatherer->state = GathererState::MovingToStorage;
    } else {
        gatherer->state = GathererState::Idle;
    }
}

void GatheringSystem::AutoAssignIdleGatherers() {
    for (auto& gatherer : m_gatherers) {
        if (!gatherer.IsAvailable()) continue;

        // Find nearest available node of any type
        ResourceNode* bestNode = nullptr;
        float bestScore = -std::numeric_limits<float>::max();

        for (auto& node : m_nodes) {
            if (!node.CanAssignGatherer()) continue;

            // Score based on distance and resource value
            float distSq = glm::dot(node.position - gatherer.position, node.position - gatherer.position);
            float dist = std::sqrt(distSq);

            float value = GetResourceValues().GetBaseValue(node.resourceType);
            float efficiency = node.gatherRate * (1.0f - static_cast<float>(node.assignedGatherers) / node.maxGatherers);

            float score = (value * efficiency) / (dist + 10.0f); // +10 to avoid division by very small numbers

            if (score > bestScore) {
                bestScore = score;
                bestNode = &node;
            }
        }

        if (bestNode) {
            AssignGathererToNode(gatherer.id, bestNode->id);
        }
    }
}

int GatheringSystem::GetIdleGathererCount() const {
    int count = 0;
    for (const auto& g : m_gatherers) {
        if (g.IsAvailable()) count++;
    }
    return count;
}

// -------------------------------------------------------------------------
// Storage
// -------------------------------------------------------------------------

void GatheringSystem::SetStorageLocation(const glm::vec2& position) {
    m_storageLocation = position;
    for (auto& gatherer : m_gatherers) {
        gatherer.storagePosition = position;
    }
}

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------

void GatheringSystem::ApplyScarcitySettings(const ScarcitySettings& settings) {
    m_scarcitySettings = settings;

    // Update existing nodes
    for (auto& node : m_nodes) {
        // Note: Only affects future behavior, not current amounts
        node.gatherRate = ResourceNode::CreateDefault(node.nodeType, node.position, 0).gatherRate
                          * settings.gatherRateMultiplier;
        node.respawnTime = ResourceNode::CreateDefault(node.nodeType, node.position, 0).respawnTime
                          * settings.respawnTimeMultiplier;
    }
}

// -------------------------------------------------------------------------
// Statistics
// -------------------------------------------------------------------------

int GatheringSystem::GetTotalGathered(ResourceType type) const {
    auto it = m_totalGathered.find(type);
    return it != m_totalGathered.end() ? it->second : 0;
}

float GatheringSystem::GetCurrentGatherRate(ResourceType type) const {
    auto it = m_recentGatherRates.find(type);
    return it != m_recentGatherRates.end() ? it->second : 0.0f;
}

int GatheringSystem::GetActiveNodeCount() const {
    return static_cast<int>(std::count_if(m_nodes.begin(), m_nodes.end(),
        [](const ResourceNode& n) { return n.active; }));
}

int GatheringSystem::GetDepletedNodeCount() const {
    return static_cast<int>(std::count_if(m_nodes.begin(), m_nodes.end(),
        [](const ResourceNode& n) { return !n.active; }));
}

// -------------------------------------------------------------------------
// Private Update Methods
// -------------------------------------------------------------------------

void GatheringSystem::UpdateNodes(float deltaTime) {
    for (auto& node : m_nodes) {
        if (!node.active) {
            float oldTimer = node.respawnTimer;
            node.UpdateRespawn(deltaTime);

            // Check if node just respawned
            if (node.active && oldTimer > 0.0f && m_onNodeRespawned) {
                m_onNodeRespawned(node);
            }
        }
    }
}

void GatheringSystem::UpdateGatherers(float deltaTime) {
    // Reset recent gather rates
    for (auto& [type, rate] : m_recentGatherRates) {
        rate = 0.0f;
    }

    for (auto& gatherer : m_gatherers) {
        UpdateGatherer(gatherer, deltaTime);
    }
}

void GatheringSystem::UpdateGatherer(Gatherer& gatherer, float deltaTime) {
    gatherer.stateTimer += deltaTime;

    switch (gatherer.state) {
        case GathererState::Idle:
            // Do nothing, waiting for assignment
            break;

        case GathererState::MovingToNode:
        case GathererState::MovingToStorage:
            ProcessMovementState(gatherer, deltaTime);
            break;

        case GathererState::Gathering:
            ProcessGatheringState(gatherer, deltaTime);
            break;

        case GathererState::Depositing:
            ProcessDepositState(gatherer, deltaTime);
            break;

        case GathererState::Waiting:
            // Could add timeout logic here
            break;
    }
}

void GatheringSystem::ProcessMovementState(Gatherer& gatherer, float deltaTime) {
    glm::vec2 target;

    if (gatherer.state == GathererState::MovingToNode) {
        if (gatherer.targetNodeId < 0) {
            gatherer.state = GathererState::Idle;
            return;
        }

        const ResourceNode* node = GetNode(static_cast<uint32_t>(gatherer.targetNodeId));
        if (!node || !node->active) {
            gatherer.targetNodeId = -1;
            gatherer.state = GathererState::Idle;
            return;
        }

        target = node->position;

        if (IsAtPosition(gatherer, target, node->radius)) {
            gatherer.state = GathererState::Gathering;
            gatherer.stateTimer = 0.0f;
            return;
        }
    } else { // MovingToStorage
        target = gatherer.storagePosition;

        if (IsAtPosition(gatherer, target, 1.0f)) {
            gatherer.state = GathererState::Depositing;
            gatherer.stateTimer = 0.0f;
            return;
        }
    }

    MoveTowards(gatherer, target, deltaTime);
}

void GatheringSystem::ProcessGatheringState(Gatherer& gatherer, float deltaTime) {
    if (gatherer.targetNodeId < 0) {
        gatherer.state = GathererState::Idle;
        return;
    }

    ResourceNode* node = GetNode(static_cast<uint32_t>(gatherer.targetNodeId));
    if (!node || !node->active) {
        // Node depleted or removed
        gatherer.targetNodeId = -1;
        if (gatherer.IsCarrying()) {
            gatherer.state = GathererState::MovingToStorage;
        } else {
            gatherer.state = GathererState::Idle;
        }
        return;
    }

    // Calculate amount to gather this frame
    float effectiveRate = node->gatherRate * gatherer.gatherEfficiency
                          * m_scarcitySettings.gatherRateMultiplier;
    float gathered = effectiveRate * deltaTime;

    int wholeUnits = static_cast<int>(gathered);
    if (wholeUnits > 0) {
        int extracted = node->Extract(wholeUnits);
        int added = gatherer.AddToCarry(node->resourceType, extracted);

        // Track statistics
        m_totalGathered[node->resourceType] += added;
        m_recentGatherRates[node->resourceType] += effectiveRate;

        if (m_onResourceGathered && added > 0) {
            m_onResourceGathered(node->resourceType, added);
        }
    }

    // Check if fully loaded or node depleted
    if (gatherer.IsFullyLoaded() || node->IsDepleted()) {
        if (node->IsDepleted() && m_onNodeDepleted) {
            m_onNodeDepleted(*node);
        }

        if (gatherer.IsCarrying()) {
            gatherer.state = GathererState::MovingToStorage;
        } else {
            gatherer.targetNodeId = -1;
            gatherer.state = GathererState::Idle;
        }
    }
}

void GatheringSystem::ProcessDepositState(Gatherer& gatherer, float deltaTime) {
    if (gatherer.stateTimer >= m_config.depositTime) {
        DepositResources(gatherer);

        // Return to assigned node or become idle
        if (gatherer.targetNodeId >= 0) {
            ResourceNode* node = GetNode(static_cast<uint32_t>(gatherer.targetNodeId));
            if (node && node->active) {
                gatherer.state = GathererState::MovingToNode;
            } else {
                gatherer.targetNodeId = -1;
                gatherer.state = GathererState::Idle;
            }
        } else {
            gatherer.state = GathererState::Idle;
        }
        gatherer.stateTimer = 0.0f;
    }
}

void GatheringSystem::MoveTowards(Gatherer& gatherer, const glm::vec2& target, float deltaTime) {
    glm::vec2 direction = target - gatherer.position;
    float distance = glm::length(direction);

    if (distance > 0.01f) {
        direction /= distance; // Normalize
        float moveDistance = gatherer.moveSpeed * deltaTime;
        if (moveDistance >= distance) {
            gatherer.position = target;
        } else {
            gatherer.position += direction * moveDistance;
        }
    }
}

bool GatheringSystem::IsAtPosition(const Gatherer& gatherer, const glm::vec2& target, float threshold) const {
    return glm::dot(gatherer.position - target, gatherer.position - target) <= threshold * threshold;
}

void GatheringSystem::DepositResources(Gatherer& gatherer) {
    if (!m_resourceStock || !gatherer.IsCarrying()) return;

    int deposited = gatherer.DepositAll();
    m_resourceStock->Add(gatherer.carryingType, deposited);
}

uint32_t GatheringSystem::GenerateNodeId() {
    return m_nextNodeId++;
}

uint32_t GatheringSystem::GenerateGathererId() {
    return m_nextGathererId++;
}

} // namespace RTS
} // namespace Vehement
