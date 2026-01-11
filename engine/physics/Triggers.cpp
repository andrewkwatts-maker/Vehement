#include "Triggers.hpp"
#include "PhysicsWorld.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Nova {

// ============================================================================
// TriggerVolume
// ============================================================================

TriggerVolume::TriggerId TriggerVolume::s_nextId = 1;

TriggerVolume::TriggerVolume()
    : m_id(s_nextId++)
    , m_shape(CollisionShape::CreateBox(glm::vec3(0.5f)))
{
    m_shape.SetTrigger(true);
}

TriggerVolume::TriggerVolume(const CollisionShape& shape)
    : m_id(s_nextId++)
    , m_shape(shape)
{
    m_shape.SetTrigger(true);
}

glm::mat4 TriggerVolume::GetTransformMatrix() const {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), m_position);
    mat = mat * glm::mat4_cast(m_rotation);
    mat = glm::scale(mat, m_scale);
    return mat;
}

AABB TriggerVolume::GetWorldAABB() const {
    if (m_boundsDirty) {
        m_cachedAABB = m_shape.ComputeWorldAABB(GetTransformMatrix());
        m_boundsDirty = false;
    }
    return m_cachedAABB;
}

OBB TriggerVolume::GetWorldOBB() const {
    return m_shape.ComputeWorldOBB(GetTransformMatrix());
}

bool TriggerVolume::ShouldTriggerFor(const CollisionBody& body) const {
    return (body.GetCollisionLayer() & m_collisionMask) != 0;
}

void TriggerVolume::OnEnter(CollisionBody* body) {
    if (!m_enabled || !body) return;
    if (m_oneShot && m_hasTriggered) return;

    m_overlappingBodies.insert(body->GetId());

    TriggerEvent event;
    event.trigger = this;
    event.otherBody = body;
    event.eventName = m_eventName;
    event.contactPoint = body->GetPosition();  // Approximate
    event.userData = m_userData;

    if (m_onEnter) {
        m_onEnter(event);
    }

    if (m_oneShot) {
        m_hasTriggered = true;
    }
}

void TriggerVolume::OnStay(CollisionBody* body, float deltaTime) {
    if (!m_enabled || !body) return;

    TriggerEvent event;
    event.trigger = this;
    event.otherBody = body;
    event.eventName = m_eventName;
    event.contactPoint = body->GetPosition();
    event.userData = m_userData;

    if (m_onStay) {
        m_onStay(event, deltaTime);
    }
}

void TriggerVolume::OnExit(CollisionBody* body) {
    if (!body) return;

    m_overlappingBodies.erase(body->GetId());

    TriggerEvent event;
    event.trigger = this;
    event.otherBody = body;
    event.eventName = m_eventName;
    event.userData = m_userData;

    if (m_onExit) {
        m_onExit(event);
    }
}

nlohmann::json TriggerVolume::ToJson() const {
    nlohmann::json j;

    j["name"] = m_name;
    j["event_name"] = m_eventName;
    j["enabled"] = m_enabled;
    j["one_shot"] = m_oneShot;

    j["position"] = {m_position.x, m_position.y, m_position.z};

    if (m_rotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(m_rotation));
        j["rotation"] = {euler.x, euler.y, euler.z};
    }

    if (m_scale != glm::vec3(1.0f)) {
        j["scale"] = {m_scale.x, m_scale.y, m_scale.z};
    }

    j["shape"] = m_shape.ToJson();

    if (m_collisionMask != CollisionLayer::All) {
        j["collision_mask"] = m_collisionMask;
    }

    if (m_pythonBinding) {
        j["python_binding"] = {
            {"module", m_pythonBinding->moduleName},
            {"function", m_pythonBinding->functionName},
            {"event", m_pythonBinding->eventName}
        };
    }

    return j;
}

std::expected<TriggerVolume, std::string> TriggerVolume::FromJson(const nlohmann::json& j) {
    TriggerVolume trigger;

    if (j.contains("name")) {
        trigger.SetName(j["name"].get<std::string>());
    }

    if (j.contains("event_name")) {
        trigger.SetEventName(j["event_name"].get<std::string>());
    }

    if (j.contains("enabled")) {
        trigger.SetEnabled(j["enabled"].get<bool>());
    }

    if (j.contains("one_shot")) {
        trigger.SetOneShot(j["one_shot"].get<bool>());
    }

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
        trigger.SetPosition(glm::vec3(
            j["position"][0].get<float>(),
            j["position"][1].get<float>(),
            j["position"][2].get<float>()
        ));
    }

    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
        glm::vec3 euler(
            glm::radians(j["rotation"][0].get<float>()),
            glm::radians(j["rotation"][1].get<float>()),
            glm::radians(j["rotation"][2].get<float>())
        );
        trigger.SetRotation(glm::quat(euler));
    }

    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        trigger.SetScale(glm::vec3(
            j["scale"][0].get<float>(),
            j["scale"][1].get<float>(),
            j["scale"][2].get<float>()
        ));
    }

    if (j.contains("shape")) {
        auto shapeResult = CollisionShape::FromJson(j["shape"]);
        if (!shapeResult) {
            return std::unexpected("Failed to parse trigger shape: " + shapeResult.error());
        }
        trigger.SetShape(*shapeResult);
    }

    if (j.contains("collision_mask")) {
        trigger.SetCollisionMask(j["collision_mask"].get<uint32_t>());
    }

    if (j.contains("python_binding")) {
        const auto& binding = j["python_binding"];
        PythonEventBinding pyBinding;
        if (binding.contains("module")) pyBinding.moduleName = binding["module"].get<std::string>();
        if (binding.contains("function")) pyBinding.functionName = binding["function"].get<std::string>();
        if (binding.contains("event")) pyBinding.eventName = binding["event"].get<std::string>();
        trigger.SetPythonBinding(pyBinding);
    }

    return trigger;
}

// ============================================================================
// TriggerSystem
// ============================================================================

TriggerSystem::TriggerSystem() = default;

TriggerSystem::TriggerSystem(PhysicsWorld* world) : m_physicsWorld(world) {
}

TriggerVolume* TriggerSystem::AddTrigger(std::unique_ptr<TriggerVolume> trigger) {
    if (!trigger) return nullptr;

    TriggerVolume* ptr = trigger.get();
    m_triggerMap[ptr->GetId()] = ptr;
    m_triggers.push_back(std::move(trigger));
    return ptr;
}

TriggerVolume* TriggerSystem::CreateTrigger(const CollisionShape& shape) {
    auto trigger = std::make_unique<TriggerVolume>(shape);
    return AddTrigger(std::move(trigger));
}

TriggerVolume* TriggerSystem::CreateBoxTrigger(const glm::vec3& position, const glm::vec3& halfExtents) {
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateBox(halfExtents));
    trigger->SetPosition(position);
    return AddTrigger(std::move(trigger));
}

TriggerVolume* TriggerSystem::CreateSphereTrigger(const glm::vec3& position, float radius) {
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateSphere(radius));
    trigger->SetPosition(position);
    return AddTrigger(std::move(trigger));
}

void TriggerSystem::RemoveTrigger(TriggerVolume* trigger) {
    if (!trigger) return;
    RemoveTrigger(trigger->GetId());
}

void TriggerSystem::RemoveTrigger(TriggerVolume::TriggerId id) {
    m_triggerMap.erase(id);
    m_triggers.erase(
        std::remove_if(m_triggers.begin(), m_triggers.end(),
            [id](const std::unique_ptr<TriggerVolume>& t) {
                return t && t->GetId() == id;
            }),
        m_triggers.end()
    );
}

TriggerVolume* TriggerSystem::GetTrigger(TriggerVolume::TriggerId id) {
    auto it = m_triggerMap.find(id);
    return (it != m_triggerMap.end()) ? it->second : nullptr;
}

const TriggerVolume* TriggerSystem::GetTrigger(TriggerVolume::TriggerId id) const {
    auto it = m_triggerMap.find(id);
    return (it != m_triggerMap.end()) ? it->second : nullptr;
}

TriggerVolume* TriggerSystem::GetTriggerByName(const std::string& name) {
    for (auto& trigger : m_triggers) {
        if (trigger && trigger->GetName() == name) {
            return trigger.get();
        }
    }
    return nullptr;
}

void TriggerSystem::Clear() {
    m_triggers.clear();
    m_triggerMap.clear();
}

void TriggerSystem::Update(float deltaTime) {
    if (!m_physicsWorld) return;

    for (auto& trigger : m_triggers) {
        if (trigger && trigger->IsEnabled()) {
            UpdateTrigger(*trigger, deltaTime);
        }
    }
}

void TriggerSystem::UpdateTrigger(TriggerVolume& trigger, float deltaTime) {
    if (!m_physicsWorld) return;

    // Get bodies that could potentially overlap
    AABB triggerAABB = trigger.GetWorldAABB();
    auto potentialOverlaps = m_physicsWorld->OverlapAABB(triggerAABB, trigger.GetCollisionMask());

    // Track which bodies are currently overlapping
    std::unordered_set<CollisionBody::BodyId> currentOverlaps;

    for (const auto& overlap : potentialOverlaps) {
        if (!overlap.body) continue;
        if (!trigger.ShouldTriggerFor(*overlap.body)) continue;

        // Detailed overlap test
        if (TestOverlap(trigger, *overlap.body)) {
            currentOverlaps.insert(overlap.body->GetId());

            // Check if this is a new overlap
            if (!trigger.IsOverlapping(overlap.body->GetId())) {
                trigger.OnEnter(overlap.body);

                // Fire global callback
                if (m_globalOnEnter) {
                    TriggerEvent event;
                    event.trigger = &trigger;
                    event.otherBody = overlap.body;
                    event.eventName = trigger.GetEventName();
                    m_globalOnEnter(event);
                }

                // Fire Python event
                if (trigger.GetPythonBinding()) {
                    TriggerEvent event;
                    event.trigger = &trigger;
                    event.otherBody = overlap.body;
                    event.eventName = trigger.GetEventName();
                    FirePythonEvent(trigger, event);
                }
            } else {
                // Ongoing overlap
                trigger.OnStay(overlap.body, deltaTime);
            }
        }
    }

    // Check for exits
    auto previousOverlaps = trigger.GetOverlappingBodies();
    for (auto bodyId : previousOverlaps) {
        if (currentOverlaps.count(bodyId) == 0) {
            // Body has exited
            CollisionBody* body = m_physicsWorld->GetBody(bodyId);
            trigger.OnExit(body);

            // Fire global callback
            if (m_globalOnExit && body) {
                TriggerEvent event;
                event.trigger = &trigger;
                event.otherBody = body;
                event.eventName = trigger.GetEventName();
                m_globalOnExit(event);
            }
        }
    }
}

bool TriggerSystem::TestOverlap(const TriggerVolume& trigger, const CollisionBody& body) const {
    OBB triggerOBB = trigger.GetWorldOBB();

    for (const auto& shape : body.GetShapes()) {
        OBB bodyOBB = shape.ComputeWorldOBB(body.GetTransformMatrix());

        if (triggerOBB.Intersects(bodyOBB)) {
            return true;
        }
    }

    return false;
}

void TriggerSystem::FirePythonEvent(const TriggerVolume& trigger, const TriggerEvent& event) {
    if (!m_pythonEventHandler) return;

    auto& binding = trigger.GetPythonBinding();
    if (!binding || !binding->IsValid()) return;

    m_pythonEventHandler(binding->moduleName, binding->functionName, event);
}

std::vector<TriggerVolume*> TriggerSystem::QueryPoint(const glm::vec3& point) const {
    std::vector<TriggerVolume*> results;

    for (const auto& trigger : m_triggers) {
        if (!trigger || !trigger->IsEnabled()) continue;

        OBB obb = trigger->GetWorldOBB();
        if (obb.Contains(point)) {
            results.push_back(trigger.get());
        }
    }

    return results;
}

std::vector<TriggerVolume*> TriggerSystem::QuerySphere(const glm::vec3& center, float radius) const {
    std::vector<TriggerVolume*> results;

    AABB queryAABB = AABB::FromCenterExtents(center, glm::vec3(radius));

    for (const auto& trigger : m_triggers) {
        if (!trigger || !trigger->IsEnabled()) continue;

        AABB triggerAABB = trigger->GetWorldAABB();
        if (!queryAABB.Intersects(triggerAABB)) continue;

        // More precise sphere-OBB test
        OBB obb = trigger->GetWorldOBB();
        glm::vec3 closest = obb.ClosestPoint(center);
        if (glm::distance2(closest, center) <= radius * radius) {
            results.push_back(trigger.get());
        }
    }

    return results;
}

std::vector<TriggerVolume*> TriggerSystem::QueryAABB(const AABB& aabb) const {
    std::vector<TriggerVolume*> results;

    for (const auto& trigger : m_triggers) {
        if (!trigger || !trigger->IsEnabled()) continue;

        AABB triggerAABB = trigger->GetWorldAABB();
        if (aabb.Intersects(triggerAABB)) {
            results.push_back(trigger.get());
        }
    }

    return results;
}

std::vector<TriggerVolume*> TriggerSystem::QueryBody(const CollisionBody& body) const {
    std::vector<TriggerVolume*> results;

    AABB bodyAABB = body.GetWorldAABB();

    for (const auto& trigger : m_triggers) {
        if (!trigger || !trigger->IsEnabled()) continue;
        if (!trigger->ShouldTriggerFor(body)) continue;

        AABB triggerAABB = trigger->GetWorldAABB();
        if (!bodyAABB.Intersects(triggerAABB)) continue;

        if (TestOverlap(*trigger, body)) {
            results.push_back(trigger.get());
        }
    }

    return results;
}

// ============================================================================
// TriggerHelpers
// ============================================================================

namespace TriggerHelpers {

std::unique_ptr<TriggerVolume> CreateCheckpoint(
    const glm::vec3& position,
    const glm::vec3& size,
    const std::string& checkpointId,
    TriggerEnterCallback onReach)
{
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateBox(size));
    trigger->SetPosition(position);
    trigger->SetName("checkpoint_" + checkpointId);
    trigger->SetEventName("checkpoint_reached");
    trigger->SetOneShot(true);
    trigger->SetCollisionMask(CollisionLayer::Player);

    if (onReach) {
        trigger->SetOnEnter(std::move(onReach));
    }

    return trigger;
}

std::unique_ptr<TriggerVolume> CreateDamageZone(
    const glm::vec3& position,
    float radius,
    float damagePerSecond,
    uint32_t affectedLayers)
{
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateSphere(radius));
    trigger->SetPosition(position);
    trigger->SetName("damage_zone");
    trigger->SetEventName("damage_tick");
    trigger->SetCollisionMask(affectedLayers);

    // The damage logic would be handled by the onStay callback set by the user
    // This is just a convenience setup

    return trigger;
}

std::unique_ptr<TriggerVolume> CreateAreaEffect(
    const glm::vec3& position,
    const glm::vec3& halfExtents,
    const std::string& effectId,
    TriggerStayCallback onStay)
{
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateBox(halfExtents));
    trigger->SetPosition(position);
    trigger->SetName("area_effect_" + effectId);
    trigger->SetEventName("area_effect_tick");

    if (onStay) {
        trigger->SetOnStay(std::move(onStay));
    }

    return trigger;
}

std::unique_ptr<TriggerVolume> CreateSpawnZone(
    const glm::vec3& position,
    const glm::vec3& halfExtents,
    const std::string& spawnEventName)
{
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateBox(halfExtents));
    trigger->SetPosition(position);
    trigger->SetName("spawn_zone");
    trigger->SetEventName(spawnEventName);
    trigger->SetCollisionMask(CollisionLayer::None);  // No collision, just position marker

    return trigger;
}

std::unique_ptr<TriggerVolume> CreateDetectionZone(
    const glm::vec3& position,
    float radius,
    uint32_t detectLayers,
    TriggerEnterCallback onDetect)
{
    auto trigger = std::make_unique<TriggerVolume>(CollisionShape::CreateSphere(radius));
    trigger->SetPosition(position);
    trigger->SetName("detection_zone");
    trigger->SetEventName("entity_detected");
    trigger->SetCollisionMask(detectLayers);

    if (onDetect) {
        trigger->SetOnEnter(std::move(onDetect));
    }

    return trigger;
}

} // namespace TriggerHelpers

} // namespace Nova
