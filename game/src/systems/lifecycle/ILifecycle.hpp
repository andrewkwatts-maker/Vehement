#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <memory>

// Forward declare nlohmann json
namespace nlohmann {
    template<typename, typename, typename...> class basic_json;
    using json = basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, void>;
}

namespace Vehement {
namespace Lifecycle {

// Forward declarations
struct GameEvent;
class LifecycleManager;

// ============================================================================
// Lifecycle Handle - Unique identifier for lifecycle objects
// ============================================================================

/**
 * @brief Handle for identifying lifecycle objects
 *
 * Combines generation counter with index for safe handle reuse.
 * Generation prevents use-after-free when handles are recycled.
 */
struct LifecycleHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    [[nodiscard]] bool IsValid() const noexcept { return generation != 0; }
    [[nodiscard]] uint64_t ToU64() const noexcept {
        return (static_cast<uint64_t>(generation) << 32) | index;
    }

    static LifecycleHandle FromU64(uint64_t value) {
        return { static_cast<uint32_t>(value & 0xFFFFFFFF),
                 static_cast<uint32_t>(value >> 32) };
    }

    bool operator==(const LifecycleHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const LifecycleHandle& other) const noexcept {
        return !(*this == other);
    }

    static const LifecycleHandle Invalid;
};

inline const LifecycleHandle LifecycleHandle::Invalid = { 0, 0 };

// ============================================================================
// Lifecycle State
// ============================================================================

/**
 * @brief Current lifecycle state of an object
 */
enum class LifecycleState : uint8_t {
    Uninitialized = 0,  // Not yet created
    Creating,           // OnCreate in progress
    Active,             // Fully active, receiving ticks
    Paused,             // Temporarily paused (no ticks)
    Destroying,         // OnDestroy in progress
    Destroyed,          // Fully destroyed, ready for recycle
    Pooled              // In object pool, waiting for reuse
};

/**
 * @brief Get string name for lifecycle state
 */
inline const char* LifecycleStateToString(LifecycleState state) {
    switch (state) {
        case LifecycleState::Uninitialized: return "Uninitialized";
        case LifecycleState::Creating:      return "Creating";
        case LifecycleState::Active:        return "Active";
        case LifecycleState::Paused:        return "Paused";
        case LifecycleState::Destroying:    return "Destroying";
        case LifecycleState::Destroyed:     return "Destroyed";
        case LifecycleState::Pooled:        return "Pooled";
        default:                            return "Unknown";
    }
}

// ============================================================================
// Lifecycle Flags - Bit flags for lifecycle behavior
// ============================================================================

/**
 * @brief Flags controlling lifecycle behavior
 */
enum class LifecycleFlags : uint32_t {
    None                = 0,

    // Tick control
    TickEnabled         = 1 << 0,   // Receives tick updates
    TickWhilePaused     = 1 << 1,   // Ticks even when game is paused

    // Event control
    EventsEnabled       = 1 << 2,   // Receives events
    EventBubbleUp       = 1 << 3,   // Events bubble to parent
    EventCaptureDown    = 1 << 4,   // Events capture to children

    // Destruction control
    DeferredDestroy     = 1 << 5,   // Destroy at end of frame
    AutoDestroy         = 1 << 6,   // Auto-destroy when health <= 0

    // Pooling
    Poolable            = 1 << 7,   // Can be returned to object pool

    // Script integration
    HasScript           = 1 << 8,   // Has associated Python script

    // Debug
    DebugDraw           = 1 << 9,   // Draw debug info

    // Default flags for game objects
    Default = TickEnabled | EventsEnabled | DeferredDestroy | AutoDestroy
};

inline LifecycleFlags operator|(LifecycleFlags a, LifecycleFlags b) {
    return static_cast<LifecycleFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline LifecycleFlags operator&(LifecycleFlags a, LifecycleFlags b) {
    return static_cast<LifecycleFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline LifecycleFlags operator~(LifecycleFlags a) {
    return static_cast<LifecycleFlags>(~static_cast<uint32_t>(a));
}
inline LifecycleFlags& operator|=(LifecycleFlags& a, LifecycleFlags b) {
    return a = a | b;
}
inline LifecycleFlags& operator&=(LifecycleFlags& a, LifecycleFlags b) {
    return a = a & b;
}
inline bool HasFlag(LifecycleFlags flags, LifecycleFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// ILifecycle Interface
// ============================================================================

/**
 * @brief Core interface for all lifecycle-managed objects
 *
 * All game objects (units, buildings, spells, projectiles, effects) implement
 * this interface to participate in the unified lifecycle system.
 *
 * Lifecycle Flow:
 * 1. Object allocated (from pool or fresh)
 * 2. OnCreate() called with configuration
 * 3. OnTick() called each frame (based on tick group)
 * 4. OnEvent() called for subscribed events
 * 5. OnDestroy() called when removed
 * 6. Object returned to pool or deleted
 *
 * Design Goals:
 * - No virtual calls in hot paths (data-oriented tick)
 * - Minimal memory footprint
 * - Support for object pooling
 * - Flexible event system
 */
class ILifecycle {
public:
    virtual ~ILifecycle() = default;

    // =========================================================================
    // Core Lifecycle Methods
    // =========================================================================

    /**
     * @brief Called when the object is created/spawned
     *
     * Initialize the object from JSON configuration. This is called once
     * when the object is first created, or when reused from pool.
     *
     * @param config JSON configuration for this object
     */
    virtual void OnCreate(const nlohmann::json& config) = 0;

    /**
     * @brief Called every tick/frame
     *
     * Update object state. The tick rate and group are determined by
     * the TickScheduler configuration.
     *
     * @param deltaTime Time since last tick in seconds
     */
    virtual void OnTick(float deltaTime) = 0;

    /**
     * @brief Called when an event is received
     *
     * Handle game events (damage, death, spawn, etc). Return true if
     * the event was handled and should stop propagating.
     *
     * @param event The event to handle
     * @return true if event was consumed, false to continue propagation
     */
    virtual bool OnEvent(const GameEvent& event) = 0;

    /**
     * @brief Called when the object is about to be destroyed
     *
     * Clean up resources, notify dependents, trigger death effects, etc.
     * After this returns, the object may be returned to pool or deleted.
     */
    virtual void OnDestroy() = 0;

    // =========================================================================
    // Optional Lifecycle Hooks
    // =========================================================================

    /**
     * @brief Called when the object is activated from pool
     *
     * Override to reset state when reusing pooled objects.
     */
    virtual void OnActivate() {}

    /**
     * @brief Called when the object is returned to pool
     *
     * Override to clean up state before pooling.
     */
    virtual void OnDeactivate() {}

    /**
     * @brief Called when the object is paused
     */
    virtual void OnPause() {}

    /**
     * @brief Called when the object is resumed
     */
    virtual void OnResume() {}

    // =========================================================================
    // Lifecycle State Access
    // =========================================================================

    /**
     * @brief Get current lifecycle state
     */
    [[nodiscard]] virtual LifecycleState GetLifecycleState() const = 0;

    /**
     * @brief Get lifecycle flags
     */
    [[nodiscard]] virtual LifecycleFlags GetLifecycleFlags() const = 0;

    /**
     * @brief Get unique handle for this object
     */
    [[nodiscard]] virtual LifecycleHandle GetHandle() const = 0;

    /**
     * @brief Get type identifier string
     */
    [[nodiscard]] virtual const char* GetTypeName() const = 0;
};

// ============================================================================
// LifecycleBase - Default implementation helper
// ============================================================================

/**
 * @brief Base class providing default ILifecycle implementation
 *
 * Inherit from this to get common functionality:
 * - State management
 * - Flag management
 * - Handle storage
 * - Default empty implementations
 */
class LifecycleBase : public ILifecycle {
public:
    LifecycleBase() = default;
    ~LifecycleBase() override = default;

    // Disable copy, allow move
    LifecycleBase(const LifecycleBase&) = delete;
    LifecycleBase& operator=(const LifecycleBase&) = delete;
    LifecycleBase(LifecycleBase&&) noexcept = default;
    LifecycleBase& operator=(LifecycleBase&&) noexcept = default;

    // =========================================================================
    // Default Implementations
    // =========================================================================

    void OnCreate(const nlohmann::json& config) override {
        (void)config;
        m_state = LifecycleState::Active;
    }

    void OnTick(float deltaTime) override {
        (void)deltaTime;
    }

    bool OnEvent(const GameEvent& event) override {
        (void)event;
        return false;
    }

    void OnDestroy() override {
        m_state = LifecycleState::Destroyed;
    }

    // =========================================================================
    // State Management
    // =========================================================================

    [[nodiscard]] LifecycleState GetLifecycleState() const override {
        return m_state;
    }

    void SetLifecycleState(LifecycleState state) {
        m_state = state;
    }

    [[nodiscard]] LifecycleFlags GetLifecycleFlags() const override {
        return m_flags;
    }

    void SetLifecycleFlags(LifecycleFlags flags) {
        m_flags = flags;
    }

    void AddLifecycleFlags(LifecycleFlags flags) {
        m_flags |= flags;
    }

    void RemoveLifecycleFlags(LifecycleFlags flags) {
        m_flags &= ~flags;
    }

    [[nodiscard]] bool HasLifecycleFlag(LifecycleFlags flag) const {
        return HasFlag(m_flags, flag);
    }

    [[nodiscard]] LifecycleHandle GetHandle() const override {
        return m_handle;
    }

    void SetHandle(LifecycleHandle handle) {
        m_handle = handle;
    }

    [[nodiscard]] const char* GetTypeName() const override {
        return "LifecycleBase";
    }

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    [[nodiscard]] bool IsActive() const {
        return m_state == LifecycleState::Active;
    }

    [[nodiscard]] bool IsPaused() const {
        return m_state == LifecycleState::Paused;
    }

    [[nodiscard]] bool IsDestroyed() const {
        return m_state == LifecycleState::Destroyed ||
               m_state == LifecycleState::Destroying;
    }

    [[nodiscard]] bool CanTick() const {
        return HasLifecycleFlag(LifecycleFlags::TickEnabled) &&
               (m_state == LifecycleState::Active ||
                (m_state == LifecycleState::Paused &&
                 HasLifecycleFlag(LifecycleFlags::TickWhilePaused)));
    }

    [[nodiscard]] bool CanReceiveEvents() const {
        return HasLifecycleFlag(LifecycleFlags::EventsEnabled) &&
               m_state == LifecycleState::Active;
    }

protected:
    LifecycleState m_state = LifecycleState::Uninitialized;
    LifecycleFlags m_flags = LifecycleFlags::Default;
    LifecycleHandle m_handle = LifecycleHandle::Invalid;
};

// ============================================================================
// Hash for LifecycleHandle (for use in containers)
// ============================================================================

} // namespace Lifecycle
} // namespace Vehement

// Hash specialization
namespace std {
    template<>
    struct hash<Vehement::Lifecycle::LifecycleHandle> {
        size_t operator()(const Vehement::Lifecycle::LifecycleHandle& h) const noexcept {
            return hash<uint64_t>{}(h.ToU64());
        }
    };
}
