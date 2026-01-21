#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>
#include <any>
#include <chrono>
#include <mutex>
#include <glm/glm.hpp>

namespace Vehement {
    class Entity;
    class EntityManager;
    class Building;
    class Worker;
    namespace RTS {
        struct ResourceStock;
    }
}

namespace Game {
namespace UI {
class NotificationUI;
}
}

namespace Nova {

class Renderer;
class Graph;
class AudioEngine;
class ParticleSystem;

namespace Scripting {

/**
 * @brief Variable types that can be exposed to Python
 */
using ScriptVar = std::variant<
    std::monostate,
    bool,
    int,
    float,
    double,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4
>;

/**
 * @brief Execution limits for script sandboxing
 */
struct ExecutionLimits {
    std::chrono::milliseconds maxExecutionTime{100};
    size_t maxMemoryBytes = 256 * 1024 * 1024;  // 256 MB
    size_t maxCallDepth = 100;
    size_t maxLoopIterations = 100000;
    bool allowFileAccess = false;
    bool allowNetworkAccess = false;
    bool allowSystemCalls = false;
};

/**
 * @brief Performance metrics for script context
 */
struct ContextMetrics {
    size_t apiCallCount = 0;
    double totalApiTimeMs = 0.0;
    std::unordered_map<std::string, size_t> apiCallCounts;
    std::unordered_map<std::string, double> apiCallTimes;

    void RecordApiCall(const std::string& name, double timeMs) {
        apiCallCount++;
        totalApiTimeMs += timeMs;
        apiCallCounts[name]++;
        apiCallTimes[name] += timeMs;
    }

    void Reset() {
        apiCallCount = 0;
        totalApiTimeMs = 0.0;
        apiCallCounts.clear();
        apiCallTimes.clear();
    }
};

/**
 * @brief Scope for managing variable visibility
 */
class VariableScope {
public:
    VariableScope() = default;
    explicit VariableScope(std::shared_ptr<VariableScope> parent);

    void Set(const std::string& name, const ScriptVar& value);
    [[nodiscard]] std::optional<ScriptVar> Get(const std::string& name) const;
    [[nodiscard]] bool Has(const std::string& name) const;
    void Remove(const std::string& name);
    void Clear();

    [[nodiscard]] std::vector<std::string> GetVariableNames() const;
    [[nodiscard]] std::shared_ptr<VariableScope> GetParent() const { return m_parent; }

private:
    std::unordered_map<std::string, ScriptVar> m_variables;
    std::shared_ptr<VariableScope> m_parent;
};

/**
 * @brief Script execution context that exposes game state to Python
 *
 * Provides:
 * - Access to game state (entities, buildings, resources)
 * - API functions scripts can call (spawn_entity, damage, play_sound)
 * - Sandbox restrictions (limit file/network access)
 * - Performance monitoring (execution time limits)
 * - Variable scope management
 *
 * Usage:
 * @code
 * ScriptContext ctx;
 * ctx.SetEntityManager(&entityManager);
 *
 * // Expose custom API function
 * ctx.RegisterFunction("spawn_zombie", [](float x, float z) {
 *     return entityManager.SpawnZombie(x, z);
 * });
 *
 * // Scripts can then call: spawn_zombie(10.0, 20.0)
 * @endcode
 */
class ScriptContext {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    ScriptContext();
    ~ScriptContext();

    ScriptContext(const ScriptContext&) = delete;
    ScriptContext& operator=(const ScriptContext&) = delete;

    // =========================================================================
    // Game System Registration
    // =========================================================================

    /**
     * @brief Set the entity manager for entity queries
     */
    void SetEntityManager(Vehement::EntityManager* manager) { m_entityManager = manager; }

    /**
     * @brief Set the navigation graph for pathfinding queries
     */
    void SetNavGraph(Nova::Graph* graph) { m_navGraph = graph; }

    /**
     * @brief Set the resource stock for economy queries
     */
    void SetResourceStock(Vehement::RTS::ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Set the renderer for visual effects
     */
    void SetRenderer(Nova::Renderer* renderer) { m_renderer = renderer; }

    /**
     * @brief Set the audio engine for sound playback
     */
    void SetAudioEngine(class Nova::AudioEngine* audio) { m_audioEngine = audio; }

    /**
     * @brief Set the particle system for spawning particles
     */
    void SetParticleSystem(Nova::ParticleSystem* particles) { m_particleSystem = particles; }

    /**
     * @brief Set the notification UI for showing notifications
     */
    void SetNotificationUI(Game::UI::NotificationUI* notificationUI) { m_notificationUI = notificationUI; }

    // =========================================================================
    // API Function Registration
    // =========================================================================

    using ApiFunction = std::function<ScriptVar(const std::vector<ScriptVar>&)>;

    /**
     * @brief Register an API function callable from Python
     * @param name Function name in Python
     * @param func Function implementation
     * @param doc Documentation string
     */
    void RegisterFunction(const std::string& name, ApiFunction func,
                         const std::string& doc = "");

    /**
     * @brief Register a void API function (no return value)
     */
    template<typename Func>
    void RegisterVoidFunction(const std::string& name, Func&& func,
                              const std::string& doc = "");

    /**
     * @brief Unregister an API function
     */
    void UnregisterFunction(const std::string& name);

    /**
     * @brief Get list of registered API functions
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredFunctions() const;

    /**
     * @brief Get documentation for an API function
     */
    [[nodiscard]] std::string GetFunctionDoc(const std::string& name) const;

    /**
     * @brief Call a registered API function
     */
    [[nodiscard]] ScriptVar CallFunction(const std::string& name,
                                         const std::vector<ScriptVar>& args);

    // =========================================================================
    // Built-in API Functions (exposed to Python)
    // =========================================================================

    // Entity API - Core
    [[nodiscard]] uint32_t SpawnEntity(const std::string& type, float x, float y, float z);
    void DespawnEntity(uint32_t entityId);
    [[nodiscard]] glm::vec3 GetEntityPosition(uint32_t entityId) const;
    void SetEntityPosition(uint32_t entityId, float x, float y, float z);

    // Entity API - Velocity
    [[nodiscard]] glm::vec3 GetEntityVelocity(uint32_t entityId) const;
    void SetEntityVelocity(uint32_t entityId, float vx, float vy, float vz);

    // Entity API - Rotation (Y-axis for top-down games)
    [[nodiscard]] float GetEntityRotation(uint32_t entityId) const;
    void SetEntityRotation(uint32_t entityId, float radians);

    // Entity API - Health
    [[nodiscard]] float GetEntityHealth(uint32_t entityId) const;
    void SetEntityHealth(uint32_t entityId, float health);
    [[nodiscard]] float GetEntityMaxHealth(uint32_t entityId) const;
    void SetEntityMaxHealth(uint32_t entityId, float maxHealth);
    void DamageEntity(uint32_t entityId, float damage, uint32_t sourceId = 0);
    void HealEntity(uint32_t entityId, float amount);
    void KillEntity(uint32_t entityId);
    [[nodiscard]] bool IsEntityAlive(uint32_t entityId) const;

    // Entity API - Movement
    [[nodiscard]] float GetEntityMoveSpeed(uint32_t entityId) const;
    void SetEntityMoveSpeed(uint32_t entityId, float speed);

    // Entity API - Collision
    [[nodiscard]] float GetEntityCollisionRadius(uint32_t entityId) const;
    void SetEntityCollisionRadius(uint32_t entityId, float radius);
    [[nodiscard]] bool IsEntityCollidable(uint32_t entityId) const;
    void SetEntityCollidable(uint32_t entityId, bool collidable);
    [[nodiscard]] bool EntitiesCollide(uint32_t entity1, uint32_t entity2) const;

    // Entity API - State
    [[nodiscard]] std::string GetEntityType(uint32_t entityId) const;
    [[nodiscard]] std::string GetEntityName(uint32_t entityId) const;
    [[nodiscard]] bool IsEntityActive(uint32_t entityId) const;
    void SetEntityActive(uint32_t entityId, bool active);

    // Spatial Query API
    [[nodiscard]] std::vector<uint32_t> FindEntitiesInRadius(float x, float y, float z, float radius);
    [[nodiscard]] std::vector<uint32_t> FindEntitiesByType(const std::string& type);
    [[nodiscard]] uint32_t GetNearestEntity(float x, float y, float z, const std::string& type = "");
    [[nodiscard]] float GetDistance(uint32_t entity1, uint32_t entity2) const;

    // Raycast API
    struct RaycastResult {
        bool hit = false;
        uint32_t entityId = 0;
        glm::vec3 hitPoint{0.0f};
        glm::vec3 hitNormal{0.0f};
        float distance = 0.0f;
    };
    [[nodiscard]] RaycastResult Raycast(float startX, float startY, float startZ,
                                        float dirX, float dirY, float dirZ,
                                        float maxDistance = 1000.0f);

    // Resource API
    [[nodiscard]] int GetResourceAmount(const std::string& resourceType) const;
    bool AddResource(const std::string& resourceType, int amount);
    bool RemoveResource(const std::string& resourceType, int amount);
    [[nodiscard]] bool CanAfford(const std::string& resourceType, int amount) const;

    // Building API
    [[nodiscard]] uint32_t GetBuildingAt(int tileX, int tileY) const;
    [[nodiscard]] std::string GetBuildingType(uint32_t buildingId) const;
    [[nodiscard]] bool IsBuildingOperational(uint32_t buildingId) const;
    [[nodiscard]] float GetBuildingProgress(uint32_t buildingId) const;

    // Sound and Effects API (integrated with AudioEngine)
    void PlaySound(const std::string& soundName, float x = 0.0f, float y = 0.0f, float z = 0.0f);
    void PlaySound3D(const std::string& soundName, float x, float y, float z, float volume = 1.0f);
    void PlaySound2D(const std::string& soundName, float volume = 1.0f, float pitch = 1.0f);
    void PlayMusic(const std::string& musicName);
    void StopMusic();
    void SetMusicVolume(float volume);
    void SetMasterVolume(float volume);
    [[nodiscard]] float GetMasterVolume() const;
    void SetSoundVolume(const std::string& category, float volume);
    void SpawnEffect(const std::string& effectName, float x, float y, float z);
    void SpawnParticles(const std::string& particleType, float x, float y, float z, int count);

    // UI Notification API
    void ShowNotification(const std::string& message, float duration = 3.0f);
    void ShowWarning(const std::string& message);
    void ShowError(const std::string& message);
    void ShowTooltip(const std::string& text, float x, float y);

    // Time API
    [[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
    [[nodiscard]] float GetGameTime() const { return m_gameTime; }
    [[nodiscard]] int GetDayNumber() const { return m_dayNumber; }
    [[nodiscard]] float GetTimeOfDay() const { return m_timeOfDay; }
    [[nodiscard]] bool IsNight() const { return m_timeOfDay < 0.25f || m_timeOfDay > 0.75f; }

    // Math Utility API
    [[nodiscard]] float Random() const;
    [[nodiscard]] float RandomRange(float min, float max) const;
    [[nodiscard]] int RandomInt(int min, int max) const;
    [[nodiscard]] glm::vec3 RandomDirection() const;

    // Logging API
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogDebug(const std::string& message);

    // =========================================================================
    // Variable Scope Management
    // =========================================================================

    /**
     * @brief Push a new variable scope
     */
    void PushScope();

    /**
     * @brief Pop the current variable scope
     */
    void PopScope();

    /**
     * @brief Get the current scope
     */
    [[nodiscard]] std::shared_ptr<VariableScope> GetCurrentScope() const { return m_currentScope; }

    /**
     * @brief Set a variable in the current scope
     */
    void SetVariable(const std::string& name, const ScriptVar& value);

    /**
     * @brief Get a variable from current or parent scopes
     */
    [[nodiscard]] std::optional<ScriptVar> GetVariable(const std::string& name) const;

    /**
     * @brief Set a global variable
     */
    void SetGlobal(const std::string& name, const ScriptVar& value);

    /**
     * @brief Get a global variable
     */
    [[nodiscard]] std::optional<ScriptVar> GetGlobal(const std::string& name) const;

    // =========================================================================
    // Execution Limits and Sandboxing
    // =========================================================================

    /**
     * @brief Set execution limits
     */
    void SetExecutionLimits(const ExecutionLimits& limits) { m_limits = limits; }

    /**
     * @brief Get current execution limits
     */
    [[nodiscard]] const ExecutionLimits& GetExecutionLimits() const { return m_limits; }

    /**
     * @brief Check if an operation is allowed by sandbox
     */
    [[nodiscard]] bool IsOperationAllowed(const std::string& operation) const;

    /**
     * @brief Begin execution monitoring
     */
    void BeginExecution();

    /**
     * @brief End execution monitoring
     */
    void EndExecution();

    /**
     * @brief Check if execution time limit exceeded
     */
    [[nodiscard]] bool IsTimeLimitExceeded() const;

    // =========================================================================
    // Performance Monitoring
    // =========================================================================

    /**
     * @brief Get performance metrics
     */
    [[nodiscard]] const ContextMetrics& GetMetrics() const { return m_metrics; }

    /**
     * @brief Reset performance metrics
     */
    void ResetMetrics() { m_metrics.Reset(); }

    // =========================================================================
    // Update (called each frame)
    // =========================================================================

    /**
     * @brief Update context state (called each frame)
     */
    void Update(float deltaTime);

    /**
     * @brief Set game time values
     */
    void SetTimeValues(float gameTime, int dayNumber, float timeOfDay) {
        m_gameTime = gameTime;
        m_dayNumber = dayNumber;
        m_timeOfDay = timeOfDay;
    }

private:
    // Registered API functions
    struct ApiFunctionInfo {
        ApiFunction function;
        std::string documentation;
    };
    std::unordered_map<std::string, ApiFunctionInfo> m_apiFunctions;

    // Game systems
    Vehement::EntityManager* m_entityManager = nullptr;
    Nova::Graph* m_navGraph = nullptr;
    Vehement::RTS::ResourceStock* m_resourceStock = nullptr;
    Nova::Renderer* m_renderer = nullptr;
    Nova::AudioEngine* m_audioEngine = nullptr;
    Nova::ParticleSystem* m_particleSystem = nullptr;
    Game::UI::NotificationUI* m_notificationUI = nullptr;

    // Variable scopes
    std::shared_ptr<VariableScope> m_globalScope;
    std::shared_ptr<VariableScope> m_currentScope;
    std::vector<std::shared_ptr<VariableScope>> m_scopeStack;

    // Execution limits
    ExecutionLimits m_limits;
    std::chrono::high_resolution_clock::time_point m_executionStartTime;
    bool m_inExecution = false;

    // Time state
    float m_deltaTime = 0.0f;
    float m_gameTime = 0.0f;
    int m_dayNumber = 1;
    float m_timeOfDay = 0.5f;  // 0.0 = midnight, 0.5 = noon

    // Metrics
    ContextMetrics m_metrics;

    // Thread safety
    mutable std::mutex m_mutex;

    // Initialize built-in functions
    void RegisterBuiltinFunctions();
};

} // namespace Scripting
} // namespace Nova
