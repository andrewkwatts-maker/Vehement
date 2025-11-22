#include "ComponentLifecycle.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <queue>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// ComponentContainer Implementation
// ============================================================================

ComponentContainer::ComponentContainer() = default;

ComponentContainer::ComponentContainer(LifecycleHandle owner)
    : m_owner(owner) {}

ComponentContainer::~ComponentContainer() {
    Clear();
}

bool ComponentContainer::Add(ComponentTypeId typeId, std::unique_ptr<IComponent> component) {
    if (!component) return false;

    component->OnAttach(m_owner);
    m_components[typeId] = std::move(component);
    m_dependenciesResolved = false;

    return true;
}

bool ComponentContainer::Remove(ComponentTypeId typeId) {
    auto it = m_components.find(typeId);
    if (it == m_components.end()) return false;

    it->second->OnDetach();
    m_components.erase(it);
    m_dependenciesResolved = false;

    return true;
}

IComponent* ComponentContainer::Get(ComponentTypeId typeId) {
    auto it = m_components.find(typeId);
    return it != m_components.end() ? it->second.get() : nullptr;
}

const IComponent* ComponentContainer::Get(ComponentTypeId typeId) const {
    auto it = m_components.find(typeId);
    return it != m_components.end() ? it->second.get() : nullptr;
}

bool ComponentContainer::Has(ComponentTypeId typeId) const {
    return m_components.find(typeId) != m_components.end();
}

std::vector<IComponent*> ComponentContainer::GetAll() {
    std::vector<IComponent*> result;
    result.reserve(m_components.size());
    for (auto& pair : m_components) {
        result.push_back(pair.second.get());
    }
    return result;
}

std::vector<const IComponent*> ComponentContainer::GetAll() const {
    std::vector<const IComponent*> result;
    result.reserve(m_components.size());
    for (const auto& pair : m_components) {
        result.push_back(pair.second.get());
    }
    return result;
}

void ComponentContainer::Clear() {
    // Detach in reverse init order
    for (auto it = m_initOrder.rbegin(); it != m_initOrder.rend(); ++it) {
        auto compIt = m_components.find(*it);
        if (compIt != m_components.end()) {
            compIt->second->OnDetach();
        }
    }

    m_components.clear();
    m_initOrder.clear();
    m_dependenciesResolved = false;
}

void ComponentContainer::InitializeAll() {
    if (!m_dependenciesResolved) {
        ResolveDependencies();
    }

    // Initialize in dependency order
    for (ComponentTypeId typeId : m_initOrder) {
        auto it = m_components.find(typeId);
        if (it != m_components.end() && !it->second->IsInitialized()) {
            it->second->OnInitialize();
        }
    }
}

void ComponentContainer::TickAll(float deltaTime) {
    for (auto& pair : m_components) {
        if (pair.second->IsEnabled() && pair.second->IsInitialized()) {
            pair.second->OnTick(deltaTime);
        }
    }
}

bool ComponentContainer::SendEvent(const GameEvent& event) {
    bool handled = false;
    for (auto& pair : m_components) {
        if (pair.second->IsEnabled()) {
            if (pair.second->OnEvent(event)) {
                handled = true;
            }
        }
    }
    return handled;
}

void ComponentContainer::SetOwner(LifecycleHandle owner) {
    m_owner = owner;
    for (auto& pair : m_components) {
        pair.second->OnAttach(owner);
    }
}

bool ComponentContainer::ResolveDependencies() {
    m_initOrder.clear();
    TopologicalSort();
    m_dependenciesResolved = true;

    // Check for missing required dependencies
    auto missing = GetMissingDependencies();
    return missing.empty();
}

std::vector<ComponentTypeId> ComponentContainer::GetMissingDependencies() const {
    std::vector<ComponentTypeId> missing;

    for (const auto& pair : m_components) {
        auto* depProvider = dynamic_cast<IComponentDependencies*>(pair.second.get());
        if (!depProvider) continue;

        for (const auto& dep : depProvider->GetDependencies()) {
            if (dep.required && !Has(dep.typeId)) {
                missing.push_back(dep.typeId);
            }
        }
    }

    return missing;
}

void ComponentContainer::TopologicalSort() {
    // Build dependency graph
    std::unordered_map<ComponentTypeId, std::vector<ComponentTypeId>> graph;
    std::unordered_map<ComponentTypeId, int> inDegree;

    for (const auto& pair : m_components) {
        ComponentTypeId typeId = pair.first;
        graph[typeId] = {};
        inDegree[typeId] = 0;
    }

    // Add edges for dependencies
    for (const auto& pair : m_components) {
        auto* depProvider = dynamic_cast<IComponentDependencies*>(pair.second.get());
        if (!depProvider) continue;

        for (const auto& dep : depProvider->GetDependencies()) {
            if (dep.initBefore && Has(dep.typeId)) {
                // dep.typeId must init before pair.first
                graph[dep.typeId].push_back(pair.first);
                inDegree[pair.first]++;
            }
        }
    }

    // Kahn's algorithm
    std::queue<ComponentTypeId> queue;
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            queue.push(pair.first);
        }
    }

    while (!queue.empty()) {
        ComponentTypeId current = queue.front();
        queue.pop();
        m_initOrder.push_back(current);

        for (ComponentTypeId neighbor : graph[current]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    // If not all components were sorted, there's a cycle
    // Add remaining in arbitrary order
    if (m_initOrder.size() < m_components.size()) {
        for (const auto& pair : m_components) {
            if (std::find(m_initOrder.begin(), m_initOrder.end(), pair.first) ==
                m_initOrder.end()) {
                m_initOrder.push_back(pair.first);
            }
        }
    }
}

// ============================================================================
// TransformComponent Implementation
// ============================================================================

void TransformComponent::OnInitialize() {
    ComponentBase<TransformComponent>::OnInitialize();
}

void TransformComponent::OnTick(float deltaTime) {
    (void)deltaTime;
    // Transform is typically updated by other systems
}

glm::vec3 TransformComponent::GetForward() const {
    float yaw = glm::radians(rotation.y);
    float pitch = glm::radians(rotation.x);

    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::cos(yaw)
    ));
}

glm::vec3 TransformComponent::GetRight() const {
    float yaw = glm::radians(rotation.y);
    return glm::normalize(glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)));
}

glm::vec3 TransformComponent::GetUp() const {
    return glm::cross(GetRight(), GetForward());
}

glm::mat4 TransformComponent::GetMatrix() const {
    glm::mat4 mat = glm::mat4(1.0f);

    mat = glm::translate(mat, position);
    mat = glm::rotate(mat, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    mat = glm::rotate(mat, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    mat = glm::rotate(mat, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    mat = glm::scale(mat, scale);

    return mat;
}

// ============================================================================
// HealthComponent Implementation
// ============================================================================

void HealthComponent::OnInitialize() {
    ComponentBase<HealthComponent>::OnInitialize();
    health = maxHealth;
}

bool HealthComponent::OnEvent(const GameEvent& event) {
    if (event.type == EventType::Damaged) {
        if (const auto* data = event.GetData<DamageEventData>()) {
            TakeDamage(data->amount, data->sourceHandle);
            return true;
        }
    }
    else if (event.type == EventType::Healed) {
        if (const auto* data = event.GetData<DamageEventData>()) {
            Heal(data->amount);
            return true;
        }
    }
    return false;
}

float HealthComponent::TakeDamage(float amount, LifecycleHandle source) {
    if (invulnerable || amount <= 0.0f || !IsAlive()) {
        return 0.0f;
    }

    // Apply armor reduction
    float damageReduction = armor / (armor + 100.0f);
    float actualDamage = amount * (1.0f - damageReduction);

    float previousHealth = health;
    health = std::max(0.0f, health - actualDamage);

    // Fire damage event
    if (m_owner.IsValid()) {
        DamageEventData data;
        data.amount = amount;
        data.actualDamage = previousHealth - health;
        data.sourceHandle = source;
        data.targetHandle = m_owner;

        GameEvent dmgEvent(EventType::Damaged, source, m_owner);
        dmgEvent.SetData(data);
        QueueEvent(std::move(dmgEvent));

        // Check for death
        if (!IsAlive()) {
            GameEvent killEvent(EventType::Killed, source, m_owner);
            killEvent.SetData(data);
            QueueEvent(std::move(killEvent));
        }
    }

    return previousHealth - health;
}

void HealthComponent::Heal(float amount) {
    if (amount <= 0.0f || !IsAlive()) return;

    float previousHealth = health;
    health = std::min(maxHealth, health + amount);

    if (m_owner.IsValid() && health > previousHealth) {
        DamageEventData data;
        data.amount = health - previousHealth;
        data.targetHandle = m_owner;

        GameEvent healEvent(EventType::Healed, LifecycleHandle::Invalid, m_owner);
        healEvent.SetData(data);
        QueueEvent(std::move(healEvent));
    }
}

void HealthComponent::SetHealth(float value) {
    health = std::clamp(value, 0.0f, maxHealth);
}

float HealthComponent::GetHealthPercent() const {
    return maxHealth > 0.0f ? health / maxHealth : 0.0f;
}

// ============================================================================
// MovementComponent Implementation
// ============================================================================

void MovementComponent::OnInitialize() {
    ComponentBase<MovementComponent>::OnInitialize();

    // Get transform dependency
    auto* container = reinterpret_cast<ComponentContainer*>(
        reinterpret_cast<char*>(this) - offsetof(ComponentContainer, m_components));
    // Note: This is a simplified approach. In production, use proper component access.
}

void MovementComponent::OnTick(float deltaTime) {
    if (!m_transform) return;

    // Apply friction
    if (glm::length(m_targetVelocity) < 0.01f) {
        float frictionAmount = friction * deltaTime;
        float currentSpeed = glm::length(velocity);
        if (currentSpeed > frictionAmount) {
            velocity -= glm::normalize(velocity) * frictionAmount;
        } else {
            velocity = glm::vec3(0.0f);
        }
    } else {
        // Accelerate towards target velocity
        glm::vec3 diff = m_targetVelocity - velocity;
        float diffLen = glm::length(diff);
        if (diffLen > 0.01f) {
            float accelAmount = acceleration * deltaTime;
            if (accelAmount > diffLen) {
                velocity = m_targetVelocity;
            } else {
                velocity += glm::normalize(diff) * accelAmount;
            }
        }
    }

    // Clamp to max speed
    float speed = glm::length(velocity);
    if (speed > maxSpeed) {
        velocity = glm::normalize(velocity) * maxSpeed;
    }

    // Update position
    m_transform->position += velocity * deltaTime;
}

void MovementComponent::SetTargetVelocity(const glm::vec3& target) {
    m_targetVelocity = target;

    // Clamp to max speed
    float len = glm::length(m_targetVelocity);
    if (len > maxSpeed) {
        m_targetVelocity = glm::normalize(m_targetVelocity) * maxSpeed;
    }
}

void MovementComponent::ApplyForce(const glm::vec3& force) {
    velocity += force;
}

void MovementComponent::Stop() {
    m_targetVelocity = glm::vec3(0.0f);
}

float MovementComponent::GetSpeed() const {
    return glm::length(velocity);
}

// ============================================================================
// ComponentRegistry Implementation
// ============================================================================

ComponentRegistry& ComponentRegistry::Instance() {
    static ComponentRegistry instance;
    return instance;
}

void ComponentRegistry::Register(const std::string& name, ComponentTypeId typeId,
                                 CreatorFunc creator) {
    TypeInfo info;
    info.typeId = typeId;
    info.creator = std::move(creator);

    m_nameToInfo[name] = std::move(info);
    m_idToName[typeId] = name;
}

std::unique_ptr<IComponent> ComponentRegistry::Create(const std::string& name) {
    auto it = m_nameToInfo.find(name);
    if (it != m_nameToInfo.end() && it->second.creator) {
        return it->second.creator();
    }
    return nullptr;
}

std::unique_ptr<IComponent> ComponentRegistry::Create(ComponentTypeId typeId) {
    auto nameIt = m_idToName.find(typeId);
    if (nameIt != m_idToName.end()) {
        return Create(nameIt->second);
    }
    return nullptr;
}

bool ComponentRegistry::IsRegistered(const std::string& name) const {
    return m_nameToInfo.find(name) != m_nameToInfo.end();
}

ComponentTypeId ComponentRegistry::GetTypeId(const std::string& name) const {
    auto it = m_nameToInfo.find(name);
    return it != m_nameToInfo.end() ? it->second.typeId : 0;
}

std::string ComponentRegistry::GetTypeName(ComponentTypeId typeId) const {
    auto it = m_idToName.find(typeId);
    return it != m_idToName.end() ? it->second : "";
}

std::vector<std::string> ComponentRegistry::GetRegisteredNames() const {
    std::vector<std::string> names;
    names.reserve(m_nameToInfo.size());
    for (const auto& pair : m_nameToInfo) {
        names.push_back(pair.first);
    }
    return names;
}

// ============================================================================
// Auto-register built-in components
// ============================================================================

namespace {
    struct BuiltinComponentRegistrar {
        BuiltinComponentRegistrar() {
            auto& registry = ComponentRegistry::Instance();
            registry.Register<TransformComponent>("transform");
            registry.Register<HealthComponent>("health");
            registry.Register<MovementComponent>("movement");
        }
    };
    static BuiltinComponentRegistrar g_builtinComponentRegistrar;
}

} // namespace Lifecycle
} // namespace Vehement
