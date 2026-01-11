#include "BehaviorSystem.hpp"
#include <algorithm>

namespace Nova {

// ============================================================================
// BehaviorInstance Implementation
// ============================================================================

BehaviorInstance::BehaviorInstance(const std::string& behaviorId, uint64_t entityId)
    : m_behaviorId(behaviorId)
    , m_entityId(entityId) {

    // Initialize with default parameters
    const auto* def = BehaviorSystem::Instance().GetBehavior(behaviorId);
    if (def) {
        for (const auto& param : def->parameters) {
            m_parameters[param.id] = param.defaultValue;
        }
    }
}

void BehaviorInstance::SetParameter(const std::string& id, const std::any& value) {
    m_parameters[id] = value;
}

std::any BehaviorInstance::GetParameter(const std::string& id) const {
    auto it = m_parameters.find(id);
    return it != m_parameters.end() ? it->second : std::any{};
}

BehaviorResult BehaviorInstance::Execute(BehaviorContext& context) {
    if (!m_enabled) {
        return BehaviorResult::Success();
    }

    const auto* def = BehaviorSystem::Instance().GetBehavior(m_behaviorId);
    if (!def || !def->function) {
        return BehaviorResult::Failure("Behavior not found or has no function");
    }

    // Add instance parameters to context
    for (const auto& [key, value] : m_parameters) {
        context.params[key] = value;
    }

    // Add persistent state to context
    for (const auto& [key, value] : m_state) {
        context.params["state_" + key] = value;
    }

    return def->function(context);
}

void BehaviorInstance::SetState(const std::string& key, const std::any& value) {
    m_state[key] = value;
}

std::any BehaviorInstance::GetState(const std::string& key) const {
    auto it = m_state.find(key);
    return it != m_state.end() ? it->second : std::any{};
}

// ============================================================================
// BehaviorSystem Implementation
// ============================================================================

BehaviorSystem& BehaviorSystem::Instance() {
    static BehaviorSystem instance;
    return instance;
}

void BehaviorSystem::RegisterBehavior(const BehaviorDef& behavior) {
    m_behaviors[behavior.id] = behavior;
}

void BehaviorSystem::RegisterBehavior(const std::string& id, const std::string& name,
                                       const std::vector<BehaviorTrigger>& triggers,
                                       BehaviorFunction function,
                                       float pointCost) {
    BehaviorDef def;
    def.id = id;
    def.name = name;
    def.triggers = triggers;
    def.function = function;
    def.pointCost = pointCost;
    m_behaviors[id] = def;
}

void BehaviorSystem::UnregisterBehavior(const std::string& id) {
    m_behaviors.erase(id);
}

const BehaviorDef* BehaviorSystem::GetBehavior(const std::string& id) const {
    auto it = m_behaviors.find(id);
    return it != m_behaviors.end() ? &it->second : nullptr;
}

std::vector<const BehaviorDef*> BehaviorSystem::GetAllBehaviors() const {
    std::vector<const BehaviorDef*> result;
    for (const auto& [id, def] : m_behaviors) {
        result.push_back(&def);
    }
    return result;
}

std::vector<const BehaviorDef*> BehaviorSystem::GetBehaviorsByTrigger(BehaviorTrigger trigger) const {
    std::vector<const BehaviorDef*> result;
    for (const auto& [id, def] : m_behaviors) {
        if (std::find(def.triggers.begin(), def.triggers.end(), trigger) != def.triggers.end()) {
            result.push_back(&def);
        }
    }
    return result;
}

std::vector<const BehaviorDef*> BehaviorSystem::GetBehaviorsByCategory(const std::string& category) const {
    std::vector<const BehaviorDef*> result;
    for (const auto& [id, def] : m_behaviors) {
        if (def.category == category) {
            result.push_back(&def);
        }
    }
    return result;
}

std::shared_ptr<BehaviorInstance> BehaviorSystem::AttachBehavior(uint64_t entityId, const std::string& behaviorId) {
    const auto* def = GetBehavior(behaviorId);
    if (!def) {
        return nullptr;
    }

    auto instance = std::make_shared<BehaviorInstance>(behaviorId, entityId);
    m_entityBehaviors[entityId].push_back(instance);

    if (OnBehaviorAttached) {
        OnBehaviorAttached(entityId, behaviorId);
    }

    return instance;
}

void BehaviorSystem::DetachBehavior(uint64_t entityId, const std::string& behaviorId) {
    auto it = m_entityBehaviors.find(entityId);
    if (it == m_entityBehaviors.end()) return;

    auto& behaviors = it->second;
    auto removeIt = std::remove_if(behaviors.begin(), behaviors.end(),
        [&behaviorId](const auto& instance) {
            return instance->GetBehaviorId() == behaviorId;
        });

    if (removeIt != behaviors.end()) {
        behaviors.erase(removeIt, behaviors.end());
        if (OnBehaviorDetached) {
            OnBehaviorDetached(entityId, behaviorId);
        }
    }
}

std::vector<std::shared_ptr<BehaviorInstance>> BehaviorSystem::GetEntityBehaviors(uint64_t entityId) const {
    auto it = m_entityBehaviors.find(entityId);
    if (it != m_entityBehaviors.end()) {
        return it->second;
    }
    return {};
}

void BehaviorSystem::ClearEntityBehaviors(uint64_t entityId) {
    m_entityBehaviors.erase(entityId);
}

void BehaviorSystem::TriggerBehaviors(BehaviorTrigger trigger, BehaviorContext& context) {
    for (auto& [entityId, behaviors] : m_entityBehaviors) {
        for (auto& instance : behaviors) {
            const auto* def = GetBehavior(instance->GetBehaviorId());
            if (def && std::find(def->triggers.begin(), def->triggers.end(), trigger) != def->triggers.end()) {
                context.entityId = entityId;
                BehaviorResult result = instance->Execute(context);
                if (OnBehaviorExecuted) {
                    OnBehaviorExecuted(result);
                }
            }
        }
    }
}

void BehaviorSystem::TriggerEntityBehaviors(uint64_t entityId, BehaviorTrigger trigger, BehaviorContext& context) {
    auto it = m_entityBehaviors.find(entityId);
    if (it == m_entityBehaviors.end()) return;

    context.entityId = entityId;

    for (auto& instance : it->second) {
        const auto* def = GetBehavior(instance->GetBehaviorId());
        if (def && std::find(def->triggers.begin(), def->triggers.end(), trigger) != def->triggers.end()) {
            BehaviorResult result = instance->Execute(context);
            if (OnBehaviorExecuted) {
                OnBehaviorExecuted(result);
            }
        }
    }
}

void BehaviorSystem::Update(float deltaTime) {
    BehaviorContext context;
    context.deltaTime = deltaTime;
    TriggerBehaviors(BehaviorTrigger::OnUpdate, context);
}

void BehaviorSystem::FixedUpdate(float fixedDeltaTime) {
    BehaviorContext context;
    context.deltaTime = fixedDeltaTime;
    TriggerBehaviors(BehaviorTrigger::OnFixedUpdate, context);
}

// ============================================================================
// BehaviorBuilder Implementation
// ============================================================================

BehaviorBuilder::BehaviorBuilder(const std::string& id) {
    m_behavior.id = id;
}

BehaviorBuilder& BehaviorBuilder::Name(const std::string& name) {
    m_behavior.name = name;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Description(const std::string& desc) {
    m_behavior.description = desc;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Category(const std::string& category) {
    m_behavior.category = category;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Trigger(BehaviorTrigger trigger) {
    m_behavior.triggers.push_back(trigger);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::CustomTrigger(const std::string& trigger) {
    m_behavior.triggers.push_back(BehaviorTrigger::Custom);
    m_behavior.customTrigger = trigger;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Function(BehaviorFunction func) {
    m_behavior.function = func;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Script(const std::string& scriptPath) {
    m_behavior.scriptPath = scriptPath;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::PointCost(float cost) {
    m_behavior.pointCost = cost;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::RequiresTag(const std::string& tag) {
    m_behavior.requiredTags.push_back(tag);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::IncompatibleWith(const std::string& behaviorId) {
    m_behavior.incompatibleBehaviors.push_back(behaviorId);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Author(const std::string& author) {
    m_behavior.author = author;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Version(const std::string& version) {
    m_behavior.version = version;
    return *this;
}

BehaviorBuilder& BehaviorBuilder::Tag(const std::string& tag) {
    m_behavior.tags.push_back(tag);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::IntParam(const std::string& id, const std::string& name, int defaultVal) {
    BehaviorDef::Parameter param;
    param.id = id;
    param.name = name;
    param.type = "int";
    param.defaultValue = defaultVal;
    m_behavior.parameters.push_back(param);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::FloatParam(const std::string& id, const std::string& name, float defaultVal) {
    BehaviorDef::Parameter param;
    param.id = id;
    param.name = name;
    param.type = "float";
    param.defaultValue = defaultVal;
    m_behavior.parameters.push_back(param);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::BoolParam(const std::string& id, const std::string& name, bool defaultVal) {
    BehaviorDef::Parameter param;
    param.id = id;
    param.name = name;
    param.type = "bool";
    param.defaultValue = defaultVal;
    m_behavior.parameters.push_back(param);
    return *this;
}

BehaviorBuilder& BehaviorBuilder::StringParam(const std::string& id, const std::string& name, const std::string& defaultVal) {
    BehaviorDef::Parameter param;
    param.id = id;
    param.name = name;
    param.type = "string";
    param.defaultValue = defaultVal;
    m_behavior.parameters.push_back(param);
    return *this;
}

BehaviorDef BehaviorBuilder::Build() const {
    return m_behavior;
}

void BehaviorBuilder::Register() {
    BehaviorSystem::Instance().RegisterBehavior(m_behavior);
}

// ============================================================================
// Built-in Behaviors
// ============================================================================

namespace Behaviors {

BehaviorResult DamageOverTime(BehaviorContext& ctx) {
    float damage = ctx.GetParam<float>("damage", 5.0f);
    float interval = ctx.GetParam<float>("interval", 1.0f);

    float timer = ctx.GetParam<float>("state_timer", 0.0f);
    timer += ctx.deltaTime;

    if (timer >= interval) {
        timer = 0.0f;
        // Apply damage logic would go here
        return BehaviorResult::Success().WithOutput("damage_dealt", damage);
    }

    ctx.SetParam("state_timer", timer);
    return BehaviorResult::Success();
}

BehaviorResult HealOverTime(BehaviorContext& ctx) {
    float healing = ctx.GetParam<float>("healing", 5.0f);
    float interval = ctx.GetParam<float>("interval", 1.0f);

    float timer = ctx.GetParam<float>("state_timer", 0.0f);
    timer += ctx.deltaTime;

    if (timer >= interval) {
        timer = 0.0f;
        return BehaviorResult::Success().WithOutput("healing_done", healing);
    }

    ctx.SetParam("state_timer", timer);
    return BehaviorResult::Success();
}

BehaviorResult MoveToTarget(BehaviorContext& ctx) {
    float speed = ctx.GetParam<float>("speed", 5.0f);
    float targetX = ctx.GetParam<float>("target_x", 0.0f);
    float targetY = ctx.GetParam<float>("target_y", 0.0f);
    float targetZ = ctx.GetParam<float>("target_z", 0.0f);

    // Calculate direction
    float dx = targetX - ctx.posX;
    float dy = targetY - ctx.posY;
    float dz = targetZ - ctx.posZ;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    if (dist < 0.1f) {
        return BehaviorResult::Success().WithOutput("reached", true);
    }

    // Normalize and scale by speed
    float factor = speed * ctx.deltaTime / dist;
    float newX = ctx.posX + dx * factor;
    float newY = ctx.posY + dy * factor;
    float newZ = ctx.posZ + dz * factor;

    return BehaviorResult::Success()
        .WithOutput("new_x", newX)
        .WithOutput("new_y", newY)
        .WithOutput("new_z", newZ);
}

BehaviorResult FleeFromTarget(BehaviorContext& ctx) {
    float speed = ctx.GetParam<float>("speed", 5.0f);
    float targetX = ctx.GetParam<float>("target_x", 0.0f);
    float targetY = ctx.GetParam<float>("target_y", 0.0f);
    float targetZ = ctx.GetParam<float>("target_z", 0.0f);
    float safeDistance = ctx.GetParam<float>("safe_distance", 10.0f);

    // Calculate direction away from target
    float dx = ctx.posX - targetX;
    float dy = ctx.posY - targetY;
    float dz = ctx.posZ - targetZ;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    if (dist >= safeDistance) {
        return BehaviorResult::Success().WithOutput("safe", true);
    }

    // Normalize and scale by speed
    float factor = speed * ctx.deltaTime / dist;
    float newX = ctx.posX + dx * factor;
    float newY = ctx.posY + dy * factor;
    float newZ = ctx.posZ + dz * factor;

    return BehaviorResult::Success()
        .WithOutput("new_x", newX)
        .WithOutput("new_y", newY)
        .WithOutput("new_z", newZ);
}

BehaviorResult AttackNearest(BehaviorContext& ctx) {
    float range = ctx.GetParam<float>("range", 5.0f);
    float damage = ctx.GetParam<float>("damage", 10.0f);
    float cooldown = ctx.GetParam<float>("cooldown", 1.0f);

    float timer = ctx.GetParam<float>("state_cooldown", 0.0f);
    if (timer > 0) {
        timer -= ctx.deltaTime;
        ctx.SetParam("state_cooldown", timer);
        return BehaviorResult::Success();
    }

    // Attack logic - would search for nearby enemies
    ctx.SetParam("state_cooldown", cooldown);
    return BehaviorResult::Success()
        .WithOutput("attacked", true)
        .WithOutput("damage", damage);
}

BehaviorResult SpawnOnDeath(BehaviorContext& ctx) {
    std::string spawnType = ctx.GetParam<std::string>("spawn_type", "");
    int spawnCount = ctx.GetParam<int>("spawn_count", 1);

    if (spawnType.empty()) {
        return BehaviorResult::Failure("No spawn type specified");
    }

    return BehaviorResult::Success()
        .WithOutput("spawn_type", spawnType)
        .WithOutput("spawn_count", spawnCount)
        .WithOutput("spawn_x", ctx.posX)
        .WithOutput("spawn_y", ctx.posY)
        .WithOutput("spawn_z", ctx.posZ);
}

BehaviorResult ReflectDamage(BehaviorContext& ctx) {
    float reflectPercent = ctx.GetParam<float>("reflect_percent", 0.2f);
    float incomingDamage = ctx.GetParam<float>("incoming_damage", 0.0f);

    float reflectedDamage = incomingDamage * reflectPercent;

    return BehaviorResult::Success()
        .WithOutput("reflected_damage", reflectedDamage)
        .WithOutput("source_entity", ctx.targetEntityId);
}

BehaviorResult AuraEffect(BehaviorContext& ctx) {
    float radius = ctx.GetParam<float>("radius", 5.0f);
    std::string effectType = ctx.GetParam<std::string>("effect_type", "buff");
    float effectValue = ctx.GetParam<float>("effect_value", 10.0f);

    // Aura logic - would affect nearby entities
    return BehaviorResult::Success()
        .WithOutput("aura_radius", radius)
        .WithOutput("aura_type", effectType)
        .WithOutput("aura_value", effectValue);
}

BehaviorResult GenerateResource(BehaviorContext& ctx) {
    std::string resourceType = ctx.GetParam<std::string>("resource_type", "gold");
    float amount = ctx.GetParam<float>("amount", 1.0f);
    float interval = ctx.GetParam<float>("interval", 1.0f);

    float timer = ctx.GetParam<float>("state_timer", 0.0f);
    timer += ctx.deltaTime;

    if (timer >= interval) {
        timer = 0.0f;
        return BehaviorResult::Success()
            .WithOutput("resource_type", resourceType)
            .WithOutput("resource_amount", amount);
    }

    ctx.SetParam("state_timer", timer);
    return BehaviorResult::Success();
}

void RegisterBuiltinBehaviors() {
    BehaviorBuilder("damage_over_time")
        .Name("Damage Over Time")
        .Description("Deals periodic damage to the entity")
        .Category("combat")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(DamageOverTime)
        .FloatParam("damage", "Damage per tick", 5.0f)
        .FloatParam("interval", "Tick interval (seconds)", 1.0f)
        .PointCost(5.0f)
        .Register();

    BehaviorBuilder("heal_over_time")
        .Name("Heal Over Time")
        .Description("Heals the entity periodically")
        .Category("support")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(HealOverTime)
        .FloatParam("healing", "Healing per tick", 5.0f)
        .FloatParam("interval", "Tick interval (seconds)", 1.0f)
        .PointCost(5.0f)
        .Register();

    BehaviorBuilder("move_to_target")
        .Name("Move To Target")
        .Description("Moves towards a specified target position")
        .Category("movement")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(MoveToTarget)
        .FloatParam("speed", "Movement speed", 5.0f)
        .PointCost(0.0f)
        .Register();

    BehaviorBuilder("flee_from_target")
        .Name("Flee From Target")
        .Description("Moves away from a specified target")
        .Category("movement")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(FleeFromTarget)
        .FloatParam("speed", "Movement speed", 5.0f)
        .FloatParam("safe_distance", "Distance to flee to", 10.0f)
        .PointCost(2.0f)
        .Register();

    BehaviorBuilder("attack_nearest")
        .Name("Attack Nearest Enemy")
        .Description("Automatically attacks the nearest enemy in range")
        .Category("combat")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(AttackNearest)
        .FloatParam("range", "Attack range", 5.0f)
        .FloatParam("damage", "Damage per attack", 10.0f)
        .FloatParam("cooldown", "Attack cooldown", 1.0f)
        .PointCost(10.0f)
        .Register();

    BehaviorBuilder("spawn_on_death")
        .Name("Spawn On Death")
        .Description("Spawns entities when this entity dies")
        .Category("special")
        .Trigger(BehaviorTrigger::OnDeath)
        .Function(SpawnOnDeath)
        .StringParam("spawn_type", "Entity type to spawn", "")
        .IntParam("spawn_count", "Number to spawn", 1)
        .PointCost(15.0f)
        .Register();

    BehaviorBuilder("reflect_damage")
        .Name("Reflect Damage")
        .Description("Reflects a portion of damage back to attackers")
        .Category("defense")
        .Trigger(BehaviorTrigger::OnDamaged)
        .Function(ReflectDamage)
        .FloatParam("reflect_percent", "Percentage of damage to reflect", 0.2f)
        .PointCost(10.0f)
        .Register();

    BehaviorBuilder("aura_effect")
        .Name("Aura Effect")
        .Description("Applies effects to nearby entities")
        .Category("support")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(AuraEffect)
        .FloatParam("radius", "Aura radius", 5.0f)
        .StringParam("effect_type", "Type of effect", "buff")
        .FloatParam("effect_value", "Effect strength", 10.0f)
        .PointCost(15.0f)
        .Register();

    BehaviorBuilder("generate_resource")
        .Name("Generate Resource")
        .Description("Generates resources over time")
        .Category("economy")
        .Trigger(BehaviorTrigger::OnUpdate)
        .Function(GenerateResource)
        .StringParam("resource_type", "Resource type", "gold")
        .FloatParam("amount", "Amount per tick", 1.0f)
        .FloatParam("interval", "Generation interval", 1.0f)
        .PointCost(20.0f)
        .Register();
}

} // namespace Behaviors

} // namespace Nova
