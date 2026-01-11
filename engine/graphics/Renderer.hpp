#pragma once

/**
 * @file Renderer.hpp
 * @brief Main rendering class following SOLID principles
 *
 * The Renderer class provides a high-level interface for all rendering operations
 * in the Nova engine. It is designed following SOLID principles:
 *
 * - Single Responsibility: Orchestrates rendering, delegates specific tasks to services
 * - Open/Closed: Extensible via IRenderBackend and IRenderPassExecutor interfaces
 * - Liskov Substitution: All render backends are interchangeable
 * - Interface Segregation: Small, focused interfaces for different subsystems
 * - Dependency Inversion: Depends on abstractions (interfaces), not concrete implementations
 *
 * Key Features:
 * - Cached GPU state management to minimize redundant OpenGL calls
 * - RenderGraph for pass scheduling with dependency resolution
 * - Integration with SDF raymarching pipeline
 * - Support for polygon, SDF, and hybrid rendering modes
 * - Comprehensive performance statistics
 *
 * @see RenderStateManager For GPU state caching
 * @see RenderGraphImpl For pass scheduling
 * @see IRenderBackend For graphics API abstraction
 *
 * @author Nova Engine Team
 * @copyright 2025 Nova Engine
 */

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

// Core graphics classes
class Shader;
class Mesh;
class Material;
class Framebuffer;
class Camera;

// Subsystems
class DebugDraw;
class ShaderManager;
class TextureManager;
class OptimizedRenderer;

// Optimization components
class Batching;
class Culler;
class LODManager;
class TextureAtlas;
class RenderQueue;

// Internal implementation classes (defined in Renderer.cpp)
class RenderStateManager;
class MaterialBindingService;
class FullscreenQuadRenderer;
class RenderGraphImpl;
class MeshDrawer;

/**
 * @brief Main rendering class - orchestrates all rendering operations
 *
 * The Renderer class is the primary interface for all rendering functionality
 * in the Nova engine. It manages GPU state, coordinates render passes, and
 * provides both immediate-mode and deferred rendering capabilities.
 *
 * Architecture:
 * - Uses PIMPL idiom to hide implementation details
 * - Composes focused service classes for different responsibilities
 * - Maintains backward compatibility with existing code
 * - Supports both polygon rasterization and SDF raymarching
 *
 * Usage Example:
 * @code
 *     Renderer renderer;
 *     renderer.Initialize();
 *
 *     // Main loop
 *     renderer.BeginFrame();
 *     renderer.SetCamera(&camera);
 *     renderer.DrawMesh(mesh, material, transform);
 *     renderer.RenderDebug();
 *     renderer.EndFrame();
 * @endcode
 *
 * Thread Safety:
 * This class is NOT thread-safe. All rendering operations should be performed
 * on the main thread that owns the OpenGL context.
 */
class Renderer {
public:
    // ========================================================================
    // Construction and Lifecycle
    // ========================================================================

    /**
     * @brief Construct a new Renderer object
     *
     * The renderer is not initialized upon construction.
     * Call Initialize() before using any rendering functions.
     */
    Renderer();

    /**
     * @brief Destroy the Renderer object
     *
     * Automatically calls Shutdown() if the renderer is still initialized.
     */
    ~Renderer();

    // Non-copyable, non-movable (owns GPU resources)
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /**
     * @brief Initialize the renderer
     *
     * Sets up OpenGL state, creates internal resources, and initializes
     * all subsystems. Must be called after an OpenGL context is available.
     *
     * @return true on success, false on failure
     *
     * @pre An OpenGL 4.3+ context must be current on this thread
     * @post The renderer is ready for use
     *
     * @see Shutdown()
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup all resources
     *
     * Releases all GPU resources and resets the renderer state.
     * Safe to call multiple times or on an uninitialized renderer.
     *
     * @post The renderer is in an uninitialized state
     *
     * @see Initialize()
     */
    void Shutdown();

    /**
     * @brief Check if the renderer is initialized
     * @return true if initialized and ready for rendering
     */
    bool IsInitialized() const { return m_initialized; }

    // ========================================================================
    // Frame Management
    // ========================================================================

    /**
     * @brief Begin a new rendering frame
     *
     * Resets per-frame statistics, clears the framebuffer with the configured
     * clear color, and prepares internal state for rendering.
     *
     * @pre Initialize() has been called successfully
     * @post Ready to accept draw calls for this frame
     *
     * @see EndFrame()
     */
    void BeginFrame();

    /**
     * @brief End the current rendering frame
     *
     * Finalizes frame statistics and prepares for the next frame.
     * Note: This does NOT swap buffers - that is handled by the window system.
     *
     * @see BeginFrame()
     * @see GetFrameTimeMs()
     */
    void EndFrame();

    /**
     * @brief Get the frame time in milliseconds
     *
     * Returns the CPU time taken to process the last complete frame.
     *
     * @return Frame time in milliseconds
     */
    float GetFrameTimeMs() const;

    // ========================================================================
    // Camera and Viewport
    // ========================================================================

    /**
     * @brief Set the active camera for rendering
     *
     * The active camera provides view and projection matrices for all
     * subsequent draw calls until changed.
     *
     * @param camera Pointer to the camera (may be nullptr to disable)
     *
     * @see GetCamera()
     */
    void SetCamera(Camera* camera) { m_activeCamera = camera; }

    /**
     * @brief Get the currently active camera
     * @return Pointer to the active camera, or nullptr if none set
     */
    Camera* GetCamera() const { return m_activeCamera; }

    /**
     * @brief Clear the screen with the specified color
     *
     * Clears color, depth, and stencil buffers.
     *
     * @param color RGBA clear color (default: dark blue-gray)
     */
    void Clear(const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));

    /**
     * @brief Set the viewport for rendering
     *
     * @param x Left edge of viewport in pixels
     * @param y Bottom edge of viewport in pixels
     * @param width Width of viewport in pixels
     * @param height Height of viewport in pixels
     */
    void SetViewport(int x, int y, int width, int height);

    // ========================================================================
    // GPU State Control
    // ========================================================================

    /**
     * @brief Enable or disable depth testing
     *
     * When enabled, fragments are discarded if they fail the depth test.
     *
     * @param enabled true to enable depth testing
     */
    void SetDepthTest(bool enabled);

    /**
     * @brief Enable or disable depth buffer writing
     *
     * When disabled, the depth buffer is read-only.
     *
     * @param enabled true to enable depth writes
     */
    void SetDepthWrite(bool enabled);

    /**
     * @brief Enable or disable face culling
     *
     * @param enabled true to enable culling
     * @param cullBack true to cull back faces, false to cull front faces
     */
    void SetCulling(bool enabled, bool cullBack = true);

    /**
     * @brief Enable or disable alpha blending
     *
     * When enabled, uses standard alpha blending (src*a + dst*(1-a)).
     *
     * @param enabled true to enable blending
     */
    void SetBlending(bool enabled);

    /**
     * @brief Enable or disable wireframe rendering mode
     *
     * @param enabled true for wireframe, false for filled polygons
     */
    void SetWireframe(bool enabled);

    // ========================================================================
    // Immediate-Mode Drawing
    // ========================================================================

    /**
     * @brief Draw a mesh with a material
     *
     * Binds the material's shader and textures, uploads transformation
     * matrices, and draws the mesh.
     *
     * @param mesh The mesh geometry to draw
     * @param material The material to use for rendering
     * @param transform Model transformation matrix
     */
    void DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform);

    /**
     * @brief Draw a mesh with a shader (no material)
     *
     * Useful for custom rendering passes where full material setup is not needed.
     *
     * @param mesh The mesh geometry to draw
     * @param shader The shader program to use
     * @param transform Model transformation matrix
     */
    void DrawMesh(const Mesh& mesh, Shader& shader, const glm::mat4& transform);

    /**
     * @brief Draw a fullscreen quad
     *
     * Useful for post-processing effects and SDF raymarching.
     *
     * @param shader The shader to use for rendering
     */
    void DrawFullscreenQuad(Shader& shader);

    /**
     * @brief Render debug visualizations
     *
     * Renders all queued debug primitives (lines, spheres, boxes, etc.)
     * using the currently active camera.
     */
    void RenderDebug();

    // ========================================================================
    // Subsystem Access
    // ========================================================================

    /**
     * @brief Get the debug draw instance
     * @return Reference to the debug drawing system
     */
    DebugDraw& GetDebugDraw() { return *m_debugDraw; }

    /**
     * @brief Get the shader manager
     * @return Reference to the shader manager
     */
    ShaderManager& GetShaderManager() { return *m_shaderManager; }

    /**
     * @brief Get the texture manager
     * @return Reference to the texture manager
     */
    TextureManager& GetTextureManager() { return *m_textureManager; }

    /**
     * @brief Get the render graph for advanced pass management
     *
     * The render graph allows scheduling custom render passes with
     * dependency management.
     *
     * @return Pointer to the render graph implementation
     */
    RenderGraphImpl* GetRenderGraph();

    /**
     * @brief Get the state manager for direct GPU state control
     *
     * Provides access to the internal state caching system.
     *
     * @return Pointer to the state manager
     */
    RenderStateManager* GetStateManager();

    /**
     * @brief Get the fullscreen quad renderer
     *
     * Provides access to the fullscreen quad for custom post-processing.
     *
     * @return Pointer to the fullscreen quad renderer
     */
    FullscreenQuadRenderer* GetFullscreenQuadRenderer();

    // ========================================================================
    // Statistics
    // ========================================================================

    /**
     * @brief Basic per-frame rendering statistics
     */
    struct Stats {
        uint32_t drawCalls = 0;     ///< Number of draw calls this frame
        uint32_t triangles = 0;     ///< Number of triangles rendered
        uint32_t vertices = 0;      ///< Number of vertices processed

        /**
         * @brief Reset all statistics to zero
         */
        void Reset() { drawCalls = triangles = vertices = 0; }
    };

    /**
     * @brief Get basic rendering statistics
     * @return Reference to current frame statistics
     */
    const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Extended statistics including optimization metrics
     */
    struct ExtendedStats {
        Stats baseStats;                    ///< Basic draw statistics
        uint32_t batchedDrawCalls = 0;      ///< Draw calls that were batched
        uint32_t instancedDrawCalls = 0;    ///< Instanced draw calls
        uint32_t drawCallsSaved = 0;        ///< Draw calls saved by batching
        uint32_t objectsCulled = 0;         ///< Objects culled from rendering
        float cullingEfficiency = 0.0f;     ///< Culling efficiency percentage
        float batchingEfficiency = 0.0f;    ///< Batching efficiency percentage
        float lodSavings = 0.0f;            ///< Triangle savings from LOD
        uint32_t stateChanges = 0;          ///< GPU state changes
    };

    /**
     * @brief Get extended statistics including optimization metrics
     * @return Extended statistics structure
     */
    ExtendedStats GetExtendedStats() const;

    // ========================================================================
    // Debug Utilities
    // ========================================================================

    /**
     * @brief Check for OpenGL errors
     *
     * Queries OpenGL for errors and logs any found.
     *
     * @param location Optional identifier for error location
     * @return true if no errors were found
     */
    static bool CheckGLError(const char* location = nullptr);

    /**
     * @brief Enable or disable OpenGL debug output
     *
     * When enabled, OpenGL will report errors, warnings, and performance
     * hints via the debug callback. Requires OpenGL 4.3+.
     *
     * @param enabled true to enable debug output
     */
    static void EnableDebugOutput(bool enabled);

    // ========================================================================
    // Performance Optimization Systems
    // ========================================================================

    /**
     * @brief Initialize optimized rendering subsystems
     *
     * Enables advanced optimization features including batching, culling,
     * LOD management, and texture atlasing.
     *
     * @param configPath Path to graphics configuration JSON (optional)
     * @return true on success
     *
     * @see ApplyQualityPreset()
     */
    bool InitializeOptimizations(const std::string& configPath = "");

    /**
     * @brief Get the optimized renderer
     * @return Pointer to the optimized renderer, or nullptr if not initialized
     */
    OptimizedRenderer* GetOptimizedRenderer() { return m_optimizedRenderer.get(); }

    /**
     * @brief Get the optimized renderer (const version)
     * @return Const pointer to the optimized renderer
     */
    const OptimizedRenderer* GetOptimizedRenderer() const { return m_optimizedRenderer.get(); }

    /**
     * @brief Check if optimization systems are enabled
     * @return true if optimizations are currently enabled
     */
    bool IsOptimizationEnabled() const { return m_optimizationsEnabled; }

    /**
     * @brief Enable or disable optimization systems
     *
     * When disabled, optimized submissions fall back to direct rendering.
     *
     * @param enabled true to enable optimizations
     */
    void SetOptimizationsEnabled(bool enabled) { m_optimizationsEnabled = enabled; }

    /**
     * @brief Submit mesh for optimized rendering
     *
     * Meshes submitted via this method may be batched, instanced, or
     * culled for optimal performance.
     *
     * @param mesh The mesh to render
     * @param material The material to use
     * @param transform Model transformation matrix
     * @param objectID Optional object identifier for culling
     */
    void SubmitOptimized(const std::shared_ptr<Mesh>& mesh,
                         const std::shared_ptr<Material>& material,
                         const glm::mat4& transform,
                         uint32_t objectID = 0);

    /**
     * @brief Flush the optimized render queue
     *
     * Executes all pending optimized draw calls.
     */
    void FlushOptimized();

    /**
     * @brief Apply a quality preset for optimizations
     *
     * Available presets: "low", "medium", "high", "ultra"
     *
     * @param preset Name of the quality preset
     */
    void ApplyQualityPreset(const std::string& preset);

private:
    // ========================================================================
    // Private Methods
    // ========================================================================

    /**
     * @brief Create the fullscreen quad geometry
     *
     * Creates a VAO/VBO for rendering fullscreen effects.
     */
    void CreateFullscreenQuad();

    // ========================================================================
    // Member Variables
    // ========================================================================

    // Active camera for rendering
    Camera* m_activeCamera = nullptr;

    // Subsystems (owned)
    std::unique_ptr<DebugDraw> m_debugDraw;
    std::unique_ptr<ShaderManager> m_shaderManager;
    std::unique_ptr<TextureManager> m_textureManager;

    // Legacy fullscreen quad (for backward compatibility)
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    // Per-frame statistics
    Stats m_stats;

    // Initialization state
    bool m_initialized = false;

    // Legacy OpenGL state cache (maintained for backward compatibility)
    // Note: New code should use the RenderStateManager via GetStateManager()
    struct GLState {
        bool depthTest = true;
        bool depthWrite = true;
        bool culling = true;
        bool cullBack = true;
        bool blending = false;
        bool wireframe = false;
        uint32_t boundVAO = 0;
        uint32_t boundShader = 0;
    };
    mutable GLState m_glState;

    // Performance optimization systems
    std::unique_ptr<OptimizedRenderer> m_optimizedRenderer;
    bool m_optimizationsEnabled = false;

    // PIMPL implementation (hides internal details)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Nova
