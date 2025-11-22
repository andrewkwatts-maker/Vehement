#pragma once

#include "ILifecycle.hpp"
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <cstdint>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Tick Groups
// ============================================================================

/**
 * @brief Tick group determines when during the frame an object updates
 *
 * Groups are processed in order, allowing dependencies between systems.
 * For example, AI runs before Animation so AI decisions affect animations.
 */
enum class TickGroup : uint8_t {
    PrePhysics = 0,     // Before physics simulation (input, AI decisions)
    Physics,            // During physics step (collisions, movement)
    PostPhysics,        // After physics (reactions, triggers)
    Animation,          // Animation updates
    AI,                 // AI tick (pathfinding, behavior trees)
    Late,               // Late update (camera follow, UI)

    COUNT
};

/**
 * @brief Get string name for tick group
 */
inline const char* TickGroupToString(TickGroup group) {
    switch (group) {
        case TickGroup::PrePhysics:  return "PrePhysics";
        case TickGroup::Physics:     return "Physics";
        case TickGroup::PostPhysics: return "PostPhysics";
        case TickGroup::Animation:   return "Animation";
        case TickGroup::AI:          return "AI";
        case TickGroup::Late:        return "Late";
        default:                     return "Unknown";
    }
}

/**
 * @brief Parse tick group from string
 */
inline TickGroup TickGroupFromString(const char* str) {
    if (!str) return TickGroup::AI;

    if (strcmp(str, "PrePhysics") == 0)  return TickGroup::PrePhysics;
    if (strcmp(str, "Physics") == 0)     return TickGroup::Physics;
    if (strcmp(str, "PostPhysics") == 0) return TickGroup::PostPhysics;
    if (strcmp(str, "Animation") == 0)   return TickGroup::Animation;
    if (strcmp(str, "AI") == 0)          return TickGroup::AI;
    if (strcmp(str, "Late") == 0)        return TickGroup::Late;

    return TickGroup::AI; // Default
}

// ============================================================================
// Tick Configuration
// ============================================================================

/**
 * @brief Configuration for how an object should be ticked
 */
struct TickConfig {
    TickGroup group = TickGroup::AI;        // Which group to tick in
    float interval = 0.0f;                  // Min time between ticks (0 = every frame)
    int32_t priority = 0;                   // Priority within group (higher = first)
    bool enabled = true;                    // Whether ticking is enabled
    bool tickWhilePaused = false;           // Tick even when game is paused

    // For data-oriented design: function pointer instead of virtual
    // This avoids vtable lookup in hot path
    using TickFunction = void(*)(void* userData, float deltaTime);
    TickFunction tickFunc = nullptr;
    void* userData = nullptr;
};

// ============================================================================
// Tick Handle
// ============================================================================

/**
 * @brief Handle to a registered tick callback
 */
struct TickHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    [[nodiscard]] bool IsValid() const { return generation != 0; }

    bool operator==(const TickHandle& other) const {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const TickHandle& other) const {
        return !(*this == other);
    }

    static const TickHandle Invalid;
};

inline const TickHandle TickHandle::Invalid = { 0, 0 };

// ============================================================================
// Time State
// ============================================================================

/**
 * @brief Current time state
 */
struct TimeState {
    double totalTime = 0.0;         // Total elapsed time
    double deltaTime = 0.0;         // Time since last frame
    double unscaledDeltaTime = 0.0; // Delta time without time scale
    float timeScale = 1.0f;         // Time scaling factor

    uint64_t frameCount = 0;        // Total frames
    uint64_t tickCount = 0;         // Total ticks processed

    bool isPaused = false;          // Game is paused

    // Fixed timestep
    double fixedDeltaTime = 1.0 / 60.0; // Fixed timestep (60 Hz default)
    double accumulator = 0.0;            // Accumulated time for fixed step
    uint32_t fixedStepsThisFrame = 0;    // Fixed steps this frame

    // Performance
    double lastTickDuration = 0.0;  // Time spent in last tick
    double averageTickDuration = 0.0;
};

// ============================================================================
// Tick Statistics
// ============================================================================

/**
 * @brief Statistics for a tick group
 */
struct TickGroupStats {
    size_t objectCount = 0;
    size_t tickedCount = 0;
    double totalDuration = 0.0;
    double maxDuration = 0.0;
    double averageDuration = 0.0;
};

// ============================================================================
// TickScheduler
// ============================================================================

/**
 * @brief Manages tick scheduling for all lifecycle objects
 *
 * Features:
 * - Group-based tick ordering
 * - Fixed timestep support for physics
 * - Priority within groups
 * - Interval-based ticking
 * - Pause/resume
 * - Time scaling
 * - Data-oriented tick functions (no virtual calls)
 *
 * Design:
 * - Objects register with a TickConfig
 * - Scheduler calls ticks in group order
 * - Within groups, ordered by priority
 * - Supports both virtual (ILifecycle) and function pointer ticks
 */
class TickScheduler {
public:
    TickScheduler();
    ~TickScheduler();

    // Disable copy
    TickScheduler(const TickScheduler&) = delete;
    TickScheduler& operator=(const TickScheduler&) = delete;

    // =========================================================================
    // Registration
    // =========================================================================

    /**
     * @brief Register an ILifecycle object for ticking
     *
     * @param object The object to tick
     * @param config Tick configuration
     * @return Handle for managing the registration
     */
    TickHandle Register(ILifecycle* object, const TickConfig& config);

    /**
     * @brief Register a function pointer for data-oriented ticking
     *
     * @param tickFunc Function to call
     * @param userData User data passed to function
     * @param config Tick configuration
     * @return Handle for managing the registration
     */
    TickHandle RegisterFunction(TickConfig::TickFunction tickFunc,
                                void* userData,
                                const TickConfig& config);

    /**
     * @brief Unregister a tick handler
     */
    void Unregister(TickHandle handle);

    /**
     * @brief Update tick configuration
     */
    void UpdateConfig(TickHandle handle, const TickConfig& config);

    /**
     * @brief Enable/disable a tick handler
     */
    void SetEnabled(TickHandle handle, bool enabled);

    /**
     * @brief Check if handle is registered and valid
     */
    [[nodiscard]] bool IsValid(TickHandle handle) const;

    // =========================================================================
    // Tick Execution
    // =========================================================================

    /**
     * @brief Process all ticks for the frame
     *
     * @param deltaTime Time since last frame
     */
    void Tick(float deltaTime);

    /**
     * @brief Process ticks for a specific group
     *
     * @param group The group to tick
     * @param deltaTime Time since last frame
     */
    void TickGroup(TickGroup group, float deltaTime);

    /**
     * @brief Process fixed timestep ticks (for physics)
     *
     * Call this in your game loop to handle fixed timestep updates.
     * Accumulates delta time and calls physics ticks at fixed intervals.
     *
     * @param deltaTime Time since last frame
     * @return Number of fixed steps executed
     */
    uint32_t TickFixed(float deltaTime);

    // =========================================================================
    // Time Control
    // =========================================================================

    /**
     * @brief Pause all ticking (except tickWhilePaused objects)
     */
    void Pause();

    /**
     * @brief Resume ticking
     */
    void Resume();

    /**
     * @brief Check if paused
     */
    [[nodiscard]] bool IsPaused() const { return m_timeState.isPaused; }

    /**
     * @brief Set time scale (1.0 = normal, 0.5 = half speed, 2.0 = double)
     */
    void SetTimeScale(float scale);

    /**
     * @brief Get current time scale
     */
    [[nodiscard]] float GetTimeScale() const { return m_timeState.timeScale; }

    /**
     * @brief Set fixed timestep interval
     */
    void SetFixedDeltaTime(double deltaTime);

    /**
     * @brief Get fixed timestep interval
     */
    [[nodiscard]] double GetFixedDeltaTime() const { return m_timeState.fixedDeltaTime; }

    // =========================================================================
    // Time State Access
    // =========================================================================

    /**
     * @brief Get current time state
     */
    [[nodiscard]] const TimeState& GetTimeState() const { return m_timeState; }

    /**
     * @brief Get total elapsed time
     */
    [[nodiscard]] double GetTotalTime() const { return m_timeState.totalTime; }

    /**
     * @brief Get frame count
     */
    [[nodiscard]] uint64_t GetFrameCount() const { return m_timeState.frameCount; }

    /**
     * @brief Get current delta time (scaled)
     */
    [[nodiscard]] float GetDeltaTime() const { return static_cast<float>(m_timeState.deltaTime); }

    /**
     * @brief Get unscaled delta time
     */
    [[nodiscard]] float GetUnscaledDeltaTime() const {
        return static_cast<float>(m_timeState.unscaledDeltaTime);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get statistics for a tick group
     */
    [[nodiscard]] TickGroupStats GetGroupStats(TickGroup group) const;

    /**
     * @brief Get total registered tick count
     */
    [[nodiscard]] size_t GetTotalTickCount() const;

    /**
     * @brief Get registered count for a group
     */
    [[nodiscard]] size_t GetGroupTickCount(TickGroup group) const;

    /**
     * @brief Reset statistics
     */
    void ResetStats();

    // =========================================================================
    // Debug
    // =========================================================================

    /**
     * @brief Enable/disable tick profiling
     */
    void SetProfilingEnabled(bool enabled) { m_profilingEnabled = enabled; }

    /**
     * @brief Check if profiling is enabled
     */
    [[nodiscard]] bool IsProfilingEnabled() const { return m_profilingEnabled; }

private:
    // Internal tick entry
    struct TickEntry {
        // For virtual calls
        ILifecycle* object = nullptr;

        // For function pointer calls (data-oriented)
        TickConfig::TickFunction tickFunc = nullptr;
        void* userData = nullptr;

        // Configuration
        TickConfig config;

        // State
        uint32_t generation = 0;
        double lastTickTime = 0.0;
        bool pendingRemoval = false;

        // Stats
        double lastDuration = 0.0;
    };

    // Storage per group
    struct GroupData {
        std::vector<TickEntry> entries;
        bool needsSort = false;
        TickGroupStats stats;
    };

    GroupData m_groups[static_cast<size_t>(TickGroup::COUNT)];

    // Handle management
    std::vector<uint32_t> m_freeIndices;
    std::vector<TickEntry*> m_handleToEntry; // Quick lookup
    uint32_t m_nextGeneration = 1;

    // Time state
    TimeState m_timeState;

    // Settings
    bool m_profilingEnabled = false;
    uint32_t m_maxFixedStepsPerFrame = 10; // Prevent spiral of death

    // Helpers
    TickEntry* GetEntry(TickHandle handle);
    const TickEntry* GetEntry(TickHandle handle) const;
    void SortGroup(TickGroup group);
    void CleanupPendingRemovals();
    void TickEntry(TickEntry& entry, float deltaTime);
};

// ============================================================================
// Global Scheduler Access
// ============================================================================

/**
 * @brief Get global tick scheduler instance
 */
TickScheduler& GetGlobalTickScheduler();

} // namespace Lifecycle
} // namespace Vehement
