#pragma once

/**
 * @file RenderPipeline.hpp
 * @brief Render pipeline orchestration for the Vehement SDF Engine
 *
 * The RenderPipeline class manages the execution of render passes in the
 * correct order, handling dependencies, resource allocation, and frame timing.
 *
 * Key Features:
 * - Automatic dependency graph construction and topological sorting
 * - Resource lifetime management across passes
 * - Frame-based execution with timing statistics
 * - Support for async pass execution (compute passes)
 * - Dynamic pipeline reconfiguration
 *
 * Thread Safety: The pipeline itself is NOT thread-safe. However, individual
 * passes may utilize compute shaders that run asynchronously on the GPU.
 *
 * @see IRenderPass For pass interface
 * @see RenderPassRegistry For pass management
 *
 * @author Nova Engine Team
 * @copyright 2025 Nova Engine
 */

#include "IRenderPass.hpp"
#include "RenderPassRegistry.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <optional>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class Camera;
class Scene;

// ============================================================================
// Pipeline Configuration
// ============================================================================

/**
 * @brief Configuration options for the render pipeline
 */
struct RenderPipelineConfig {
    // Resolution
    int width = 1920;
    int height = 1080;

    // Timing
    bool enableProfiling = true;
    bool gpuProfiling = true;

    // Resource management
    bool autoResizeResources = true;
    bool cacheResources = true;

    // Debug
    bool validateDependencies = true;
    bool logPassExecution = false;

    // Performance
    bool enableParallelSetup = false;      // Future: parallel pass setup
    int maxConcurrentComputePasses = 2;    // Future: concurrent compute

    static RenderPipelineConfig Default() {
        return RenderPipelineConfig{};
    }

    static RenderPipelineConfig Debug() {
        RenderPipelineConfig config;
        config.enableProfiling = true;
        config.validateDependencies = true;
        config.logPassExecution = true;
        return config;
    }

    static RenderPipelineConfig Release() {
        RenderPipelineConfig config;
        config.enableProfiling = false;
        config.validateDependencies = false;
        config.logPassExecution = false;
        return config;
    }
};

// ============================================================================
// Pipeline Statistics
// ============================================================================

/**
 * @brief Per-frame statistics for the render pipeline
 */
struct RenderPipelineStats {
    // Timing
    float totalFrameTimeMs = 0.0f;
    float setupTimeMs = 0.0f;
    float executeTimeMs = 0.0f;
    float cleanupTimeMs = 0.0f;

    // Pass statistics
    struct PassTiming {
        std::string name;
        float setupMs = 0.0f;
        float executeMs = 0.0f;
        float cleanupMs = 0.0f;
        float totalMs = 0.0f;
        bool enabled = true;
        bool executed = true;
    };
    std::vector<PassTiming> passTimings;

    // Resource usage
    size_t totalTextureCount = 0;
    size_t totalBufferCount = 0;
    size_t peakTextureMemory = 0;
    size_t peakBufferMemory = 0;

    // Execution
    uint32_t passesExecuted = 0;
    uint32_t passesSkipped = 0;
    uint64_t frameNumber = 0;

    void Reset() {
        totalFrameTimeMs = setupTimeMs = executeTimeMs = cleanupTimeMs = 0.0f;
        passTimings.clear();
        passesExecuted = passesSkipped = 0;
    }
};

// ============================================================================
// Dependency Graph Node
// ============================================================================

/**
 * @brief Internal node in the dependency graph
 */
struct DependencyGraphNode {
    std::string passName;
    IRenderPass* pass = nullptr;
    RenderPassPriority priority = RenderPassPriority::Lighting;

    std::vector<std::string> dependencies;      // Passes this node depends on
    std::vector<std::string> dependents;        // Passes that depend on this node
    std::vector<std::string> inputResources;    // Resources consumed by this pass
    std::vector<std::string> outputResources;   // Resources produced by this pass

    int inDegree = 0;                           // For topological sort
    bool visited = false;                       // For cycle detection
    bool onStack = false;                       // For cycle detection
};

// ============================================================================
// Resource Lifetime
// ============================================================================

/**
 * @brief Tracks resource lifetime across the pipeline
 */
struct ResourceLifetime {
    std::string name;
    std::string producerPass;
    std::vector<std::string> consumerPasses;
    int firstUseIndex = -1;
    int lastUseIndex = -1;
    bool isPersistent = false;  // Survives frame boundaries
};

// ============================================================================
// Pipeline Stage
// ============================================================================

/**
 * @brief Execution stage containing multiple independent passes
 *
 * Passes within a stage have no dependencies on each other and could
 * theoretically be executed in parallel (for compute passes).
 */
struct PipelineStage {
    int stageIndex = 0;
    std::vector<IRenderPass*> passes;
    bool hasComputePasses = false;
    bool hasGraphicsPasses = false;
};

// ============================================================================
// Render Pipeline
// ============================================================================

/**
 * @brief Main render pipeline class for orchestrating pass execution
 *
 * The RenderPipeline manages the full rendering frame, executing passes
 * in dependency order with proper resource management.
 *
 * Usage Example:
 * @code
 *     RenderPipeline pipeline;
 *     pipeline.Initialize(config);
 *
 *     // Add passes
 *     pipeline.AddPass(std::make_unique<GBufferPass>());
 *     pipeline.AddPass(std::make_unique<LightingPass>());
 *     pipeline.AddPass(std::make_unique<SDFPass>());
 *
 *     // Build dependency graph
 *     pipeline.RebuildDependencyGraph();
 *
 *     // Main loop
 *     while (running) {
 *         pipeline.BeginFrame(camera);
 *         pipeline.Execute(ctx, data);
 *         pipeline.EndFrame();
 *     }
 * @endcode
 */
class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    // Non-copyable
    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    // Movable
    RenderPipeline(RenderPipeline&&) noexcept;
    RenderPipeline& operator=(RenderPipeline&&) noexcept;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * @brief Initialize the pipeline
     * @param config Pipeline configuration
     * @return true on success
     */
    bool Initialize(const RenderPipelineConfig& config = RenderPipelineConfig::Default());

    /**
     * @brief Shutdown and cleanup the pipeline
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Resize the pipeline (affects all render targets)
     * @param width New width
     * @param height New height
     */
    void Resize(int width, int height);

    // ========================================================================
    // Pass Management
    // ========================================================================

    /**
     * @brief Add a render pass to the pipeline
     *
     * The pass is added to the internal registry and the dependency graph
     * is marked for rebuild.
     *
     * @param pass Unique pointer to the pass
     * @return true on success
     */
    bool AddPass(std::unique_ptr<IRenderPass> pass);

    /**
     * @brief Remove a render pass by name
     * @param name Pass name
     * @return true if pass was found and removed
     */
    bool RemovePass(const std::string& name);

    /**
     * @brief Get a pass by name
     * @param name Pass name
     * @return Pointer to pass, or nullptr
     */
    [[nodiscard]] IRenderPass* GetPass(const std::string& name);

    /**
     * @brief Get a pass with type casting
     */
    template<typename T>
    [[nodiscard]] T* GetPass(const std::string& name) {
        return dynamic_cast<T*>(GetPass(name));
    }

    /**
     * @brief Enable or disable a pass
     * @param name Pass name
     * @param enabled Enable state
     * @return true if pass was found
     */
    bool SetPassEnabled(const std::string& name, bool enabled);

    /**
     * @brief Check if a pass is enabled
     */
    [[nodiscard]] bool IsPassEnabled(const std::string& name) const;

    /**
     * @brief Get the internal pass registry
     */
    [[nodiscard]] RenderPassRegistry& GetRegistry() { return m_registry; }
    [[nodiscard]] const RenderPassRegistry& GetRegistry() const { return m_registry; }

    // ========================================================================
    // Dependency Graph
    // ========================================================================

    /**
     * @brief Rebuild the dependency graph
     *
     * Called automatically when passes are added/removed. Can be called
     * manually if pass dependencies change at runtime.
     *
     * @throws std::runtime_error if circular dependency detected
     */
    void RebuildDependencyGraph();

    /**
     * @brief Check if the dependency graph needs rebuilding
     */
    [[nodiscard]] bool NeedsRebuild() const { return m_needsRebuild; }

    /**
     * @brief Validate the dependency graph
     * @return Error message if invalid, empty string if valid
     */
    [[nodiscard]] std::string ValidateDependencyGraph() const;

    /**
     * @brief Get execution order (pass names)
     */
    [[nodiscard]] std::vector<std::string> GetExecutionOrder() const;

    /**
     * @brief Get pipeline stages (for potential parallel execution)
     */
    [[nodiscard]] const std::vector<PipelineStage>& GetStages() const { return m_stages; }

    // ========================================================================
    // Execution
    // ========================================================================

    /**
     * @brief Begin a new frame
     * @param camera Active camera
     */
    void BeginFrame(Camera& camera);

    /**
     * @brief Execute all passes in the pipeline
     * @param ctx Render context
     * @param data Render data
     */
    void Execute(RenderContext& ctx, const RenderData& data);

    /**
     * @brief Execute a specific range of passes by priority
     * @param ctx Render context
     * @param data Render data
     * @param minPriority Minimum priority (inclusive)
     * @param maxPriority Maximum priority (inclusive)
     */
    void Execute(RenderContext& ctx, const RenderData& data,
                 RenderPassPriority minPriority, RenderPassPriority maxPriority);

    /**
     * @brief Execute a single named pass
     * @param passName Pass name
     * @param ctx Render context
     * @param data Render data
     * @return true if pass was executed
     */
    bool ExecutePass(const std::string& passName, RenderContext& ctx, const RenderData& data);

    /**
     * @brief End the current frame
     */
    void EndFrame();

    // ========================================================================
    // Resource Management
    // ========================================================================

    /**
     * @brief Get shared resources
     */
    [[nodiscard]] RenderPassResources& GetResources() { return m_resources; }
    [[nodiscard]] const RenderPassResources& GetResources() const { return m_resources; }

    /**
     * @brief Set a persistent resource (survives frame boundaries)
     * @param name Resource name
     * @param texture Texture resource
     */
    void SetPersistentTexture(const std::string& name, std::shared_ptr<Texture> texture);

    /**
     * @brief Set a persistent buffer resource
     * @param name Resource name
     * @param buffer Buffer resource
     */
    void SetPersistentBuffer(const std::string& name, std::shared_ptr<Buffer> buffer);

    /**
     * @brief Clear transient resources (called automatically at frame end)
     */
    void ClearTransientResources();

    /**
     * @brief Get resource lifetimes (for debugging/optimization)
     */
    [[nodiscard]] const std::vector<ResourceLifetime>& GetResourceLifetimes() const {
        return m_resourceLifetimes;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * @brief Get pipeline statistics
     */
    [[nodiscard]] const RenderPipelineStats& GetStats() const { return m_stats; }

    /**
     * @brief Get frame number
     */
    [[nodiscard]] uint64_t GetFrameNumber() const { return m_frameNumber; }

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const RenderPipelineConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void SetConfig(const RenderPipelineConfig& config);

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * @brief Set callback for before pass execution
     */
    void SetBeforePassCallback(std::function<void(IRenderPass*)> callback) {
        m_beforePassCallback = std::move(callback);
    }

    /**
     * @brief Set callback for after pass execution
     */
    void SetAfterPassCallback(std::function<void(IRenderPass*)> callback) {
        m_afterPassCallback = std::move(callback);
    }

    /**
     * @brief Set callback for resource allocation
     */
    void SetResourceAllocatedCallback(
        std::function<void(const std::string&, RenderPassResources&)> callback) {
        m_resourceAllocatedCallback = std::move(callback);
    }

    // ========================================================================
    // Debug
    // ========================================================================

    /**
     * @brief Render debug UI
     */
    void RenderDebugUI();

    /**
     * @brief Print dependency graph to log
     */
    void PrintDependencyGraph() const;

    /**
     * @brief Print execution order to log
     */
    void PrintExecutionOrder() const;

    /**
     * @brief Print resource lifetimes to log
     */
    void PrintResourceLifetimes() const;

    /**
     * @brief Enable/disable debug visualization
     */
    void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }
    [[nodiscard]] bool IsDebugVisualizationEnabled() const { return m_debugVisualization; }

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    // Build dependency graph from registered passes
    void BuildDependencyGraph();

    // Topological sort of passes
    void TopologicalSort();

    // Detect cycles in dependency graph
    bool DetectCycle(DependencyGraphNode& node,
                     std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& onStack);

    // Build execution stages
    void BuildExecutionStages();

    // Analyze resource lifetimes
    void AnalyzeResourceLifetimes();

    // Execute a single pass with timing
    void ExecutePassInternal(IRenderPass* pass, RenderContext& ctx, const RenderData& data);

    // Update render context matrices
    void UpdateRenderContext(RenderContext& ctx, Camera& camera);

    // ========================================================================
    // Member Variables
    // ========================================================================

    // Configuration
    RenderPipelineConfig m_config;
    bool m_initialized = false;

    // Pass management
    RenderPassRegistry m_registry;
    std::vector<std::string> m_executionOrder;
    bool m_needsRebuild = true;

    // Dependency graph
    std::unordered_map<std::string, DependencyGraphNode> m_dependencyGraph;
    std::vector<PipelineStage> m_stages;

    // Resource management
    RenderPassResources m_resources;
    std::unordered_set<std::string> m_persistentResources;
    std::vector<ResourceLifetime> m_resourceLifetimes;

    // Frame tracking
    uint64_t m_frameNumber = 0;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;

    // Statistics
    RenderPipelineStats m_stats;

    // Callbacks
    std::function<void(IRenderPass*)> m_beforePassCallback;
    std::function<void(IRenderPass*)> m_afterPassCallback;
    std::function<void(const std::string&, RenderPassResources&)> m_resourceAllocatedCallback;

    // Debug
    bool m_debugVisualization = false;

    // Render context (cached for frame)
    RenderContext m_cachedContext;
};

// ============================================================================
// Pipeline Builder
// ============================================================================

/**
 * @brief Fluent builder for constructing render pipelines
 *
 * Usage Example:
 * @code
 *     auto pipeline = RenderPipelineBuilder()
 *         .WithConfig(RenderPipelineConfig::Default())
 *         .AddPass<GBufferPass>()
 *         .AddPass<LightingPass>()
 *         .AddPass<SDFPass>()
 *         .EnableProfiling(true)
 *         .Build();
 * @endcode
 */
class RenderPipelineBuilder {
public:
    RenderPipelineBuilder() = default;

    /**
     * @brief Set pipeline configuration
     */
    RenderPipelineBuilder& WithConfig(const RenderPipelineConfig& config) {
        m_config = config;
        return *this;
    }

    /**
     * @brief Set viewport dimensions
     */
    RenderPipelineBuilder& WithDimensions(int width, int height) {
        m_config.width = width;
        m_config.height = height;
        return *this;
    }

    /**
     * @brief Add a pass by instance
     */
    RenderPipelineBuilder& AddPass(std::unique_ptr<IRenderPass> pass) {
        m_passes.push_back(std::move(pass));
        return *this;
    }

    /**
     * @brief Add a pass by type
     */
    template<typename T, typename... Args>
    RenderPipelineBuilder& AddPass(Args&&... args) {
        m_passes.push_back(std::make_unique<T>(std::forward<Args>(args)...));
        return *this;
    }

    /**
     * @brief Enable profiling
     */
    RenderPipelineBuilder& EnableProfiling(bool enabled) {
        m_config.enableProfiling = enabled;
        return *this;
    }

    /**
     * @brief Enable dependency validation
     */
    RenderPipelineBuilder& ValidateDependencies(bool enabled) {
        m_config.validateDependencies = enabled;
        return *this;
    }

    /**
     * @brief Enable pass execution logging
     */
    RenderPipelineBuilder& LogPassExecution(bool enabled) {
        m_config.logPassExecution = enabled;
        return *this;
    }

    /**
     * @brief Build the pipeline
     * @return Constructed pipeline
     */
    std::unique_ptr<RenderPipeline> Build() {
        auto pipeline = std::make_unique<RenderPipeline>();

        if (!pipeline->Initialize(m_config)) {
            return nullptr;
        }

        for (auto& pass : m_passes) {
            pipeline->AddPass(std::move(pass));
        }

        pipeline->RebuildDependencyGraph();

        return pipeline;
    }

private:
    RenderPipelineConfig m_config;
    std::vector<std::unique_ptr<IRenderPass>> m_passes;
};

// ============================================================================
// Preset Pipeline Configurations
// ============================================================================

namespace PipelinePresets {

/**
 * @brief Create a forward rendering pipeline
 */
std::unique_ptr<RenderPipeline> CreateForwardPipeline(int width, int height);

/**
 * @brief Create a deferred rendering pipeline
 */
std::unique_ptr<RenderPipeline> CreateDeferredPipeline(int width, int height);

/**
 * @brief Create an SDF-only rendering pipeline
 */
std::unique_ptr<RenderPipeline> CreateSDFPipeline(int width, int height);

/**
 * @brief Create a hybrid (SDF + polygon) rendering pipeline
 */
std::unique_ptr<RenderPipeline> CreateHybridPipeline(int width, int height);

} // namespace PipelinePresets

} // namespace Nova
