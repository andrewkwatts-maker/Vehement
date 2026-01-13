#pragma once

/**
 * @file RenderPassRegistry.hpp
 * @brief Central registry for managing render passes in the Vehement SDF Engine
 *
 * The RenderPassRegistry provides a centralized location for registering,
 * querying, and managing render passes. It handles:
 * - Pass registration and unregistration
 * - Pass lookup by name
 * - Dependency-aware pass sorting
 * - Pass enable/disable at runtime
 * - Pass group management
 *
 * Design Pattern: Registry/Service Locator
 *
 * Thread Safety: NOT thread-safe. All operations should be performed
 * on the main thread that owns the OpenGL context.
 *
 * @see IRenderPass For pass interface definition
 * @see RenderPipeline For pass execution
 *
 * @author Nova Engine Team
 * @copyright 2025 Nova Engine
 */

#include "IRenderPass.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <stdexcept>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class RenderContext;

// ============================================================================
// Pass Group
// ============================================================================

/**
 * @brief Named group of render passes for collective management
 *
 * Pass groups allow enabling/disabling sets of related passes together,
 * useful for graphics quality presets or debug visualization modes.
 */
class RenderPassGroup {
public:
    explicit RenderPassGroup(std::string_view name) : m_name(name) {}

    /**
     * @brief Add a pass to this group
     */
    void AddPass(const std::string& passName) {
        m_passNames.insert(passName);
    }

    /**
     * @brief Remove a pass from this group
     */
    void RemovePass(const std::string& passName) {
        m_passNames.erase(passName);
    }

    /**
     * @brief Check if a pass is in this group
     */
    [[nodiscard]] bool Contains(const std::string& passName) const {
        return m_passNames.find(passName) != m_passNames.end();
    }

    /**
     * @brief Get group name
     */
    [[nodiscard]] std::string_view GetName() const { return m_name; }

    /**
     * @brief Get all pass names in this group
     */
    [[nodiscard]] const std::unordered_set<std::string>& GetPassNames() const {
        return m_passNames;
    }

    /**
     * @brief Get number of passes in this group
     */
    [[nodiscard]] size_t Size() const { return m_passNames.size(); }

    /**
     * @brief Check if group is enabled
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Set group enabled state
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }

private:
    std::string m_name;
    std::unordered_set<std::string> m_passNames;
    bool m_enabled = true;
};

// ============================================================================
// Pass Factory
// ============================================================================

/**
 * @brief Factory function type for creating render passes
 */
using RenderPassFactory = std::function<std::unique_ptr<IRenderPass>()>;

/**
 * @brief Pass registration entry
 */
struct RenderPassRegistration {
    std::string name;
    RenderPassFactory factory;
    std::string description;
    std::vector<std::string> tags;
};

// ============================================================================
// Pass Query
// ============================================================================

/**
 * @brief Query builder for finding passes by criteria
 */
class RenderPassQuery {
public:
    explicit RenderPassQuery(class RenderPassRegistry& registry)
        : m_registry(registry) {}

    /**
     * @brief Filter by pass priority
     */
    RenderPassQuery& WithPriority(RenderPassPriority priority) {
        m_filterPriority = priority;
        return *this;
    }

    /**
     * @brief Filter by priority range
     */
    RenderPassQuery& WithPriorityRange(RenderPassPriority min, RenderPassPriority max) {
        m_filterPriorityMin = min;
        m_filterPriorityMax = max;
        return *this;
    }

    /**
     * @brief Filter by enabled state
     */
    RenderPassQuery& Enabled(bool enabled = true) {
        m_filterEnabled = enabled;
        return *this;
    }

    /**
     * @brief Filter by name pattern (substring match)
     */
    RenderPassQuery& WithNameContaining(const std::string& pattern) {
        m_filterNamePattern = pattern;
        return *this;
    }

    /**
     * @brief Filter by output resource
     */
    RenderPassQuery& ProducingResource(const std::string& resourceName) {
        m_filterOutputResource = resourceName;
        return *this;
    }

    /**
     * @brief Filter by dependency
     */
    RenderPassQuery& DependingOn(const std::string& passName) {
        m_filterDependency = passName;
        return *this;
    }

    /**
     * @brief Execute the query
     * @return Vector of matching passes
     */
    std::vector<IRenderPass*> Execute();

    /**
     * @brief Execute and return first match
     * @return First matching pass or nullptr
     */
    IRenderPass* First();

    /**
     * @brief Execute and return count
     */
    size_t Count();

private:
    class RenderPassRegistry& m_registry;

    // Filter criteria
    std::optional<RenderPassPriority> m_filterPriority;
    std::optional<RenderPassPriority> m_filterPriorityMin;
    std::optional<RenderPassPriority> m_filterPriorityMax;
    std::optional<bool> m_filterEnabled;
    std::string m_filterNamePattern;
    std::string m_filterOutputResource;
    std::string m_filterDependency;
};

// ============================================================================
// Render Pass Registry
// ============================================================================

/**
 * @brief Central registry for render pass management
 *
 * Usage Example:
 * @code
 *     RenderPassRegistry registry;
 *
 *     // Register passes
 *     registry.Register(std::make_unique<GBufferPass>());
 *     registry.Register(std::make_unique<LightingPass>());
 *     registry.Register(std::make_unique<SDFPass>());
 *
 *     // Configure passes
 *     registry.EnablePass("SDFPass", false);
 *
 *     // Get sorted passes for execution
 *     auto passes = registry.GetSortedPasses();
 *     for (auto* pass : passes) {
 *         pass->Execute(ctx, data);
 *     }
 * @endcode
 */
class RenderPassRegistry {
public:
    RenderPassRegistry();
    ~RenderPassRegistry();

    // Non-copyable
    RenderPassRegistry(const RenderPassRegistry&) = delete;
    RenderPassRegistry& operator=(const RenderPassRegistry&) = delete;

    // Movable
    RenderPassRegistry(RenderPassRegistry&&) noexcept;
    RenderPassRegistry& operator=(RenderPassRegistry&&) noexcept;

    // ========================================================================
    // Pass Registration
    // ========================================================================

    /**
     * @brief Register a render pass
     *
     * The pass must have a unique name. If a pass with the same name already
     * exists, registration fails and returns false.
     *
     * @param pass Unique pointer to the render pass
     * @return true on success, false if name conflict
     *
     * @note The pass is not initialized until InitializeAll() is called or
     *       the pass is retrieved via Get() with lazy initialization enabled.
     */
    bool Register(std::unique_ptr<IRenderPass> pass);

    /**
     * @brief Register a pass with a factory function
     *
     * Allows deferred pass creation. The factory is called when the pass
     * is first needed.
     *
     * @param name Unique pass name
     * @param factory Factory function to create the pass
     * @param description Optional description for UI
     * @return true on success
     */
    bool RegisterFactory(const std::string& name, RenderPassFactory factory,
                         const std::string& description = "");

    /**
     * @brief Unregister a render pass by name
     *
     * Calls Shutdown() on the pass before removal.
     *
     * @param name Name of the pass to unregister
     * @return true if pass was found and removed
     */
    bool Unregister(const std::string& name);

    /**
     * @brief Unregister all passes
     */
    void UnregisterAll();

    /**
     * @brief Check if a pass is registered
     */
    [[nodiscard]] bool IsRegistered(const std::string& name) const;

    // ========================================================================
    // Pass Access
    // ========================================================================

    /**
     * @brief Get a render pass by name
     * @param name Pass name
     * @return Pointer to pass, or nullptr if not found
     */
    [[nodiscard]] IRenderPass* Get(const std::string& name);

    /**
     * @brief Get a render pass by name (const)
     */
    [[nodiscard]] const IRenderPass* Get(const std::string& name) const;

    /**
     * @brief Get a render pass with type casting
     * @tparam T Pass type to cast to
     * @param name Pass name
     * @return Pointer to pass cast to type T, or nullptr
     */
    template<typename T>
    [[nodiscard]] T* Get(const std::string& name) {
        return dynamic_cast<T*>(Get(name));
    }

    /**
     * @brief Get a render pass with type casting (const)
     */
    template<typename T>
    [[nodiscard]] const T* Get(const std::string& name) const {
        return dynamic_cast<const T*>(Get(name));
    }

    /**
     * @brief Get all registered passes
     * @return Vector of all passes (unordered)
     */
    [[nodiscard]] std::vector<IRenderPass*> GetAll();

    /**
     * @brief Get all registered passes (const)
     */
    [[nodiscard]] std::vector<const IRenderPass*> GetAll() const;

    /**
     * @brief Get all pass names
     */
    [[nodiscard]] std::vector<std::string> GetPassNames() const;

    /**
     * @brief Get number of registered passes
     */
    [[nodiscard]] size_t GetPassCount() const { return m_passes.size(); }

    // ========================================================================
    // Sorted Access (Dependency-Aware)
    // ========================================================================

    /**
     * @brief Get passes sorted by priority and dependencies
     *
     * Returns passes in execution order, respecting both priority levels
     * and declared dependencies. Disabled passes are excluded.
     *
     * @return Vector of passes in execution order
     *
     * @throws std::runtime_error if circular dependency detected
     */
    [[nodiscard]] std::vector<IRenderPass*> GetSortedPasses();

    /**
     * @brief Get passes sorted for a specific priority range
     *
     * Useful for executing only a subset of passes (e.g., only post-process).
     *
     * @param minPriority Minimum priority (inclusive)
     * @param maxPriority Maximum priority (inclusive)
     * @return Vector of passes in execution order
     */
    [[nodiscard]] std::vector<IRenderPass*> GetSortedPasses(
        RenderPassPriority minPriority,
        RenderPassPriority maxPriority);

    /**
     * @brief Invalidate the sorted pass cache
     *
     * Call this after registering/unregistering passes or changing
     * dependencies. Automatically called by Register/Unregister/EnablePass.
     */
    void InvalidateSortCache();

    // ========================================================================
    // Pass Enable/Disable
    // ========================================================================

    /**
     * @brief Enable or disable a pass by name
     * @param name Pass name
     * @param enabled true to enable, false to disable
     * @return true if pass was found
     */
    bool EnablePass(const std::string& name, bool enabled);

    /**
     * @brief Check if a pass is enabled
     * @param name Pass name
     * @return true if enabled, false if disabled or not found
     */
    [[nodiscard]] bool IsPassEnabled(const std::string& name) const;

    /**
     * @brief Enable all passes
     */
    void EnableAll();

    /**
     * @brief Disable all passes
     */
    void DisableAll();

    // ========================================================================
    // Pass Groups
    // ========================================================================

    /**
     * @brief Create a pass group
     * @param name Group name
     * @return Reference to created group
     */
    RenderPassGroup& CreateGroup(const std::string& name);

    /**
     * @brief Get a pass group by name
     * @param name Group name
     * @return Pointer to group, or nullptr if not found
     */
    [[nodiscard]] RenderPassGroup* GetGroup(const std::string& name);

    /**
     * @brief Remove a pass group
     * @param name Group name
     */
    void RemoveGroup(const std::string& name);

    /**
     * @brief Enable/disable all passes in a group
     * @param groupName Group name
     * @param enabled Enable state
     * @return true if group was found
     */
    bool SetGroupEnabled(const std::string& groupName, bool enabled);

    /**
     * @brief Get all group names
     */
    [[nodiscard]] std::vector<std::string> GetGroupNames() const;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * @brief Initialize all registered passes
     * @param ctx Render context for initialization
     * @return true if all passes initialized successfully
     */
    bool InitializeAll(RenderContext& ctx);

    /**
     * @brief Shutdown all registered passes
     */
    void ShutdownAll();

    /**
     * @brief Check if a pass has been initialized
     * @param name Pass name
     */
    [[nodiscard]] bool IsInitialized(const std::string& name) const;

    // ========================================================================
    // Query
    // ========================================================================

    /**
     * @brief Create a query builder for finding passes
     * @return Query builder instance
     */
    [[nodiscard]] RenderPassQuery Query() { return RenderPassQuery(*this); }

    // ========================================================================
    // Dependency Graph
    // ========================================================================

    /**
     * @brief Get the names of passes that depend on a given pass
     * @param passName Pass name
     * @return Vector of dependent pass names
     */
    [[nodiscard]] std::vector<std::string> GetDependents(const std::string& passName) const;

    /**
     * @brief Check if pass A depends on pass B (directly or transitively)
     * @param passA Pass that might depend
     * @param passB Pass that might be depended upon
     * @return true if A depends on B
     */
    [[nodiscard]] bool DependsOn(const std::string& passA, const std::string& passB) const;

    /**
     * @brief Validate the dependency graph
     * @return Error message if invalid, empty string if valid
     */
    [[nodiscard]] std::string ValidateDependencies() const;

    // ========================================================================
    // Events
    // ========================================================================

    /**
     * @brief Get the event dispatcher for lifecycle events
     */
    [[nodiscard]] RenderPassEventDispatcher& GetEventDispatcher() { return m_eventDispatcher; }

    // ========================================================================
    // Debug
    // ========================================================================

    /**
     * @brief Render debug UI for the registry
     *
     * Shows all registered passes with enable/disable toggles and statistics.
     */
    void RenderDebugUI();

    /**
     * @brief Print the dependency graph to the log
     */
    void PrintDependencyGraph() const;

    /**
     * @brief Get pass execution statistics
     */
    struct PassStats {
        std::string name;
        float lastExecutionTimeMs = 0.0f;
        float averageExecutionTimeMs = 0.0f;
        uint64_t executionCount = 0;
        bool enabled = true;
        bool initialized = true;
    };

    [[nodiscard]] std::vector<PassStats> GetPassStats() const;

private:
    // Internal helper for topological sort
    void TopologicalSort(std::vector<IRenderPass*>& result,
                         const std::unordered_map<std::string, std::vector<std::string>>& adjList) const;

    bool HasCircularDependency(const std::string& startPass,
                               std::unordered_set<std::string>& visited,
                               std::unordered_set<std::string>& recursionStack) const;

    // Pass storage
    std::unordered_map<std::string, std::unique_ptr<IRenderPass>> m_passes;

    // Factory storage (for lazy creation)
    std::unordered_map<std::string, RenderPassRegistration> m_factories;

    // Pass groups
    std::unordered_map<std::string, std::unique_ptr<RenderPassGroup>> m_groups;

    // Initialization state
    std::unordered_set<std::string> m_initializedPasses;

    // Sorted pass cache
    std::vector<IRenderPass*> m_sortedPassCache;
    bool m_sortCacheValid = false;

    // Pass statistics
    mutable std::unordered_map<std::string, PassStats> m_passStats;

    // Event dispatcher
    RenderPassEventDispatcher m_eventDispatcher;
};

// ============================================================================
// Built-in Pass Names (Constants)
// ============================================================================

namespace RenderPassNames {
    inline constexpr const char* PreDepth = "PreDepth";
    inline constexpr const char* ShadowMap = "ShadowMap";
    inline constexpr const char* GBuffer = "GBuffer";
    inline constexpr const char* SSAO = "SSAO";
    inline constexpr const char* DeferredLighting = "DeferredLighting";
    inline constexpr const char* ForwardLighting = "ForwardLighting";
    inline constexpr const char* SDFRaymarching = "SDFRaymarching";
    inline constexpr const char* SDFShadows = "SDFShadows";
    inline constexpr const char* SDFGlobalIllumination = "SDFGI";
    inline constexpr const char* Transparent = "Transparent";
    inline constexpr const char* Bloom = "Bloom";
    inline constexpr const char* ToneMapping = "ToneMapping";
    inline constexpr const char* TAA = "TAA";
    inline constexpr const char* FXAA = "FXAA";
    inline constexpr const char* MotionBlur = "MotionBlur";
    inline constexpr const char* DepthOfField = "DepthOfField";
    inline constexpr const char* UI = "UI";
    inline constexpr const char* DebugOverlay = "DebugOverlay";
}

// ============================================================================
// Built-in Resource Names (Constants)
// ============================================================================

namespace RenderResourceNames {
    inline constexpr const char* SceneColor = "SceneColor";
    inline constexpr const char* SceneColorHDR = "SceneColorHDR";
    inline constexpr const char* SceneDepth = "SceneDepth";
    inline constexpr const char* GBufferPosition = "GBufferPosition";
    inline constexpr const char* GBufferNormal = "GBufferNormal";
    inline constexpr const char* GBufferAlbedo = "GBufferAlbedo";
    inline constexpr const char* GBufferMaterial = "GBufferMaterial";
    inline constexpr const char* GBufferEmission = "GBufferEmission";
    inline constexpr const char* GBufferVelocity = "GBufferVelocity";
    inline constexpr const char* SSAOTexture = "SSAOTexture";
    inline constexpr const char* ShadowAtlas = "ShadowAtlas";
    inline constexpr const char* BloomTexture = "BloomTexture";
    inline constexpr const char* SDFSceneTexture = "SDFSceneTexture";
    inline constexpr const char* SDFDepthTexture = "SDFDepthTexture";
}

// ============================================================================
// Built-in Group Names (Constants)
// ============================================================================

namespace RenderPassGroups {
    inline constexpr const char* ShadowPasses = "ShadowPasses";
    inline constexpr const char* DeferredPasses = "DeferredPasses";
    inline constexpr const char* SDFPasses = "SDFPasses";
    inline constexpr const char* PostProcessPasses = "PostProcessPasses";
    inline constexpr const char* DebugPasses = "DebugPasses";
}

} // namespace Nova
