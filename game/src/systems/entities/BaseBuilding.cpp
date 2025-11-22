#include "BaseBuilding.hpp"
#include "../lifecycle/ObjectFactory.hpp"
#include <algorithm>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// GarrisonComponent Implementation
// ============================================================================

void GarrisonComponent::OnInitialize() {
    ComponentBase<GarrisonComponent>::OnInitialize();
}

void GarrisonComponent::OnTick(float deltaTime) {
    (void)deltaTime;
    // Remove invalid handles
    garrisonedUnits.erase(
        std::remove_if(garrisonedUnits.begin(), garrisonedUnits.end(),
            [](const LifecycleHandle& h) { return !h.IsValid(); }),
        garrisonedUnits.end());
}

bool GarrisonComponent::CanGarrison(LifecycleHandle unit) const {
    if (!unit.IsValid() || IsFull()) return false;

    // Check if already garrisoned
    return std::find(garrisonedUnits.begin(), garrisonedUnits.end(), unit) ==
           garrisonedUnits.end();
}

bool GarrisonComponent::Garrison(LifecycleHandle unit) {
    if (!CanGarrison(unit)) return false;

    garrisonedUnits.push_back(unit);

    GameEvent event(EventType::GarrisonEntered, unit, GetOwner());
    QueueEvent(std::move(event));

    return true;
}

bool GarrisonComponent::Ungarrison(LifecycleHandle unit) {
    auto it = std::find(garrisonedUnits.begin(), garrisonedUnits.end(), unit);
    if (it == garrisonedUnits.end()) return false;

    garrisonedUnits.erase(it);

    GameEvent event(EventType::GarrisonExited, unit, GetOwner());
    QueueEvent(std::move(event));

    return true;
}

void GarrisonComponent::UngarrisonAll() {
    for (auto& unit : garrisonedUnits) {
        GameEvent event(EventType::GarrisonExited, unit, GetOwner());
        QueueEvent(std::move(event));
    }
    garrisonedUnits.clear();
}

// ============================================================================
// ProductionComponent Implementation
// ============================================================================

void ProductionComponent::OnInitialize() {
    ComponentBase<ProductionComponent>::OnInitialize();
}

void ProductionComponent::OnTick(float deltaTime) {
    if (queue.empty()) return;

    auto& current = queue.front();
    current.timeRemaining -= deltaTime * productionSpeed;

    if (current.timeRemaining <= 0.0f) {
        // Production complete
        std::string completedType = current.unitType;
        queue.erase(queue.begin());

        GameEvent event(EventType::ProductionComplete, GetOwner());
        QueueEvent(std::move(event));

        if (onProductionComplete) {
            onProductionComplete(completedType);
        }
    }
}

bool ProductionComponent::QueueProduction(const std::string& unitType, float time) {
    if (static_cast<int>(queue.size()) >= maxQueueSize) return false;

    ProductionQueueItem item;
    item.unitType = unitType;
    item.timeRequired = time;
    item.timeRemaining = time;

    queue.push_back(item);

    GameEvent event(EventType::ProductionQueued, GetOwner());
    QueueEvent(std::move(event));

    return true;
}

bool ProductionComponent::CancelProduction(size_t index) {
    if (index >= queue.size()) return false;

    queue.erase(queue.begin() + index);

    GameEvent event(EventType::ProductionCancelled, GetOwner());
    QueueEvent(std::move(event));

    return true;
}

void ProductionComponent::CancelAll() {
    queue.clear();
}

const ProductionQueueItem* ProductionComponent::GetCurrentProduction() const {
    return queue.empty() ? nullptr : &queue.front();
}

float ProductionComponent::GetProgress() const {
    if (queue.empty()) return 0.0f;

    const auto& current = queue.front();
    if (current.timeRequired <= 0.0f) return 1.0f;

    return 1.0f - (current.timeRemaining / current.timeRequired);
}

// ============================================================================
// BaseBuilding Implementation
// ============================================================================

BaseBuilding::BaseBuilding() {
    m_components.Add<TransformComponent>();
    m_components.Add<HealthComponent>();
    m_components.Add<GarrisonComponent>();
    m_components.Add<ProductionComponent>();
}

BaseBuilding::~BaseBuilding() = default;

void BaseBuilding::OnCreate(const nlohmann::json& config) {
    ScriptedLifecycle::OnCreate(config);

    m_components.SetOwner(GetHandle());
    ParseBuildingConfig(config);
    m_components.InitializeAll();

    // Configure garrison
    if (auto* garrison = m_components.Get<GarrisonComponent>()) {
        garrison->capacity = m_stats.garrisonCapacity;
    }

    // Configure production
    if (auto* production = m_components.Get<ProductionComponent>()) {
        production->maxQueueSize = m_stats.maxQueueSize;
        production->productionSpeed = m_stats.productionSpeed;
        production->onProductionComplete = [this](const std::string& type) {
            OnProductionComplete(type);
        };
    }

    // Configure health
    if (auto* health = m_components.Get<HealthComponent>()) {
        health->maxHealth = 500.0f; // Default building health
        health->health = health->maxHealth;
    }
}

void BaseBuilding::OnTick(float deltaTime) {
    ScriptedLifecycle::OnTick(deltaTime);

    if (m_buildingState == BaseBuildingState::Destroyed) return;

    // Check for destruction
    if (auto* health = m_components.Get<HealthComponent>()) {
        if (!health->IsAlive() && m_buildingState != BaseBuildingState::Destroyed) {
            OnBuildingDestroyed();
            return;
        }
    }

    m_components.TickAll(deltaTime);

    switch (m_buildingState) {
        case BaseBuildingState::UnderConstruction:
            UpdateConstruction(deltaTime);
            break;

        case BaseBuildingState::Operational:
            UpdateProduction(deltaTime);
            UpdateResourceGeneration(deltaTime);
            break;

        case BaseBuildingState::Upgrading:
            // Handle upgrade progress
            break;

        default:
            break;
    }
}

bool BaseBuilding::OnEvent(const GameEvent& event) {
    if (ScriptedLifecycle::OnEvent(event)) return true;
    if (m_components.SendEvent(event)) return true;

    switch (event.type) {
        case EventType::Damaged:
            if (m_buildingState == BaseBuildingState::Operational) {
                if (auto* health = m_components.Get<HealthComponent>()) {
                    if (health->GetHealthPercent() < 0.5f) {
                        SetBuildingState(BaseBuildingState::Damaged);
                    }
                }
            }
            return true;

        case EventType::Killed:
            if (event.target == GetHandle()) {
                OnBuildingDestroyed();
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void BaseBuilding::OnDestroy() {
    if (auto* garrison = m_components.Get<GarrisonComponent>()) {
        garrison->UngarrisonAll();
    }

    m_components.Clear();
    ScriptedLifecycle::OnDestroy();
}

void BaseBuilding::SetBuildingState(BaseBuildingState state) {
    if (m_buildingState == state) return;

    BaseBuildingState oldState = m_buildingState;
    m_buildingState = state;

    GameEvent event(EventType::StateChanged, GetHandle());
    QueueEvent(std::move(event));

    (void)oldState;
}

void BaseBuilding::StartConstruction() {
    if (m_buildingState != BaseBuildingState::Blueprint) return;

    m_stats.constructionProgress = 0.0f;
    SetBuildingState(BaseBuildingState::UnderConstruction);

    GameEvent event(EventType::ConstructionStarted, GetHandle());
    QueueEvent(std::move(event));
}

void BaseBuilding::AddConstructionProgress(float amount) {
    if (m_buildingState != BaseBuildingState::UnderConstruction) return;

    m_stats.constructionProgress += amount;

    GameEvent event(EventType::ConstructionProgress, GetHandle());
    QueueEvent(std::move(event));

    if (m_stats.constructionProgress >= 100.0f) {
        CompleteConstruction();
    }
}

void BaseBuilding::CompleteConstruction() {
    m_stats.constructionProgress = 100.0f;
    SetBuildingState(BaseBuildingState::Operational);

    OnConstructionComplete();

    GameEvent event(EventType::Built, GetHandle());
    QueueEvent(std::move(event));
}

void BaseBuilding::CancelConstruction() {
    if (m_buildingState != BaseBuildingState::UnderConstruction) return;

    SetBuildingState(BaseBuildingState::Blueprint);
    m_stats.constructionProgress = 0.0f;

    GameEvent event(EventType::Demolished, GetHandle());
    QueueEvent(std::move(event));
}

bool BaseBuilding::QueueUnit(const std::string& unitType) {
    if (!IsOperational()) return false;

    auto it = std::find(m_producibleUnits.begin(), m_producibleUnits.end(), unitType);
    if (it == m_producibleUnits.end()) return false;

    auto* production = m_components.Get<ProductionComponent>();
    if (!production) return false;

    return production->QueueProduction(unitType, 10.0f); // Default 10s production
}

bool BaseBuilding::CancelProduction(size_t index) {
    auto* production = m_components.Get<ProductionComponent>();
    return production && production->CancelProduction(index);
}

bool BaseBuilding::IsProducing() const {
    auto* production = m_components.Get<ProductionComponent>();
    return production && production->IsProducing();
}

bool BaseBuilding::GarrisonUnit(LifecycleHandle unit) {
    auto* garrison = m_components.Get<GarrisonComponent>();
    return garrison && garrison->Garrison(unit);
}

bool BaseBuilding::UngarrisonUnit(LifecycleHandle unit) {
    auto* garrison = m_components.Get<GarrisonComponent>();
    return garrison && garrison->Ungarrison(unit);
}

void BaseBuilding::UngarrisonAll() {
    if (auto* garrison = m_components.Get<GarrisonComponent>()) {
        garrison->UngarrisonAll();
    }
}

int BaseBuilding::GetGarrisonCount() const {
    auto* garrison = m_components.Get<GarrisonComponent>();
    return garrison ? garrison->GetGarrisonCount() : 0;
}

bool BaseBuilding::CanGarrison() const {
    auto* garrison = m_components.Get<GarrisonComponent>();
    return garrison && !garrison->IsFull();
}

void BaseBuilding::SetGridPosition(const glm::ivec2& pos) {
    m_gridPosition = pos;

    // Update transform
    if (auto* transform = m_components.Get<TransformComponent>()) {
        transform->position.x = static_cast<float>(pos.x);
        transform->position.z = static_cast<float>(pos.y);
    }
}

std::vector<glm::ivec2> BaseBuilding::GetOccupiedTiles() const {
    std::vector<glm::ivec2> tiles;
    for (int x = 0; x < m_size.x; ++x) {
        for (int y = 0; y < m_size.y; ++y) {
            tiles.push_back(m_gridPosition + glm::ivec2(x, y));
        }
    }
    return tiles;
}

bool BaseBuilding::CanUpgrade() const {
    return IsOperational() && m_stats.level < m_stats.maxLevel;
}

void BaseBuilding::StartUpgrade() {
    if (!CanUpgrade()) return;

    SetBuildingState(BaseBuildingState::Upgrading);

    GameEvent event(EventType::UpgradeStarted, GetHandle());
    QueueEvent(std::move(event));
}

void BaseBuilding::CompleteUpgrade() {
    m_stats.level++;
    SetBuildingState(BaseBuildingState::Operational);

    OnUpgradeComplete();

    GameEvent event(EventType::Upgraded, GetHandle());
    QueueEvent(std::move(event));
}

ScriptContext BaseBuilding::BuildContext() const {
    ScriptContext ctx = ScriptedLifecycle::BuildContext();
    ctx.entityType = "building";

    if (auto* transform = m_components.Get<TransformComponent>()) {
        ctx.transform.x = transform->position.x;
        ctx.transform.y = transform->position.y;
        ctx.transform.z = transform->position.z;
    }

    if (auto* health = m_components.Get<HealthComponent>()) {
        ctx.health.current = health->health;
        ctx.health.max = health->maxHealth;
    }

    return ctx;
}

void BaseBuilding::ParseBuildingConfig(const nlohmann::json& config) {
    (void)config;
}

void BaseBuilding::OnConstructionComplete() {
    // Override in derived classes
}

void BaseBuilding::OnProductionComplete(const std::string& unitType) {
    // Spawn the unit at spawn point
    (void)unitType;
}

void BaseBuilding::OnUpgradeComplete() {
    // Apply upgrade bonuses
    if (auto* health = m_components.Get<HealthComponent>()) {
        float bonus = 1.0f + m_stats.level * 0.2f;
        health->maxHealth *= bonus;
        health->health = health->maxHealth;
    }
}

void BaseBuilding::OnBuildingDestroyed() {
    SetBuildingState(BaseBuildingState::Destroyed);

    // Ungarrison all units
    UngarrisonAll();

    GameEvent event(EventType::Destroyed, GetHandle());
    QueueEvent(std::move(event));

    auto& manager = GetGlobalLifecycleManager();
    manager.Destroy(GetHandle(), false);
}

void BaseBuilding::UpdateConstruction(float deltaTime) {
    // Auto-progress construction (can be driven by workers instead)
    float progressRate = 100.0f / m_stats.constructionTime;
    AddConstructionProgress(progressRate * deltaTime);
}

void BaseBuilding::UpdateProduction(float deltaTime) {
    (void)deltaTime;
    // Production handled by ProductionComponent
}

void BaseBuilding::UpdateResourceGeneration(float deltaTime) {
    if (m_stats.producesResource.empty() || m_stats.productionRate <= 0.0f) return;

    // Generate resources
    float amount = m_stats.productionRate * deltaTime;

    ResourceEventData data;
    data.resourceType = m_stats.producesResource;
    data.amount = amount;

    GameEvent event(EventType::ResourceGained, GetHandle());
    event.SetData(data);
    QueueEvent(std::move(event));
}

// ============================================================================
// Factory Registration
// ============================================================================

namespace {
    struct BaseBuildingRegistrar {
        BaseBuildingRegistrar() {
            GetGlobalObjectFactory().RegisterType<BaseBuilding>("building");
        }
    };
    static BaseBuildingRegistrar g_baseBuildingRegistrar;
}

REGISTER_COMPONENT("garrison", GarrisonComponent)
REGISTER_COMPONENT("production", ProductionComponent)

} // namespace Lifecycle
} // namespace Vehement
