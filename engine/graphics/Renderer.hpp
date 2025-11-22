#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Shader;
class Mesh;
class Material;
class Framebuffer;
class Camera;
class DebugDraw;
class ShaderManager;
class TextureManager;
class OptimizedRenderer;
class Batching;
class Culler;
class LODManager;
class TextureAtlas;
class RenderQueue;

/**
 * @brief Main rendering class
 *
 * Handles all OpenGL rendering operations, replacing the old GL_Manager
 * with proper separation of concerns.
 */
class Renderer {
public:
    Renderer();
    ~Renderer();

    /**
     * @brief Initialize the renderer
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Begin a new frame
     */
    void BeginFrame();

    /**
     * @brief End the current frame
     */
    void EndFrame();

    /**
     * @brief Set the active camera for rendering
     */
    void SetCamera(Camera* camera) { m_activeCamera = camera; }
    Camera* GetCamera() const { return m_activeCamera; }

    /**
     * @brief Clear the screen
     */
    void Clear(const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));

    /**
     * @brief Set viewport
     */
    void SetViewport(int x, int y, int width, int height);

    /**
     * @brief Enable/disable depth testing
     */
    void SetDepthTest(bool enabled);

    /**
     * @brief Enable/disable depth writing
     */
    void SetDepthWrite(bool enabled);

    /**
     * @brief Enable/disable face culling
     */
    void SetCulling(bool enabled, bool cullBack = true);

    /**
     * @brief Enable/disable blending
     */
    void SetBlending(bool enabled);

    /**
     * @brief Enable/disable wireframe mode
     */
    void SetWireframe(bool enabled);

    /**
     * @brief Draw a mesh with a material
     */
    void DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform);

    /**
     * @brief Draw a mesh with a shader (no material)
     */
    void DrawMesh(const Mesh& mesh, Shader& shader, const glm::mat4& transform);

    /**
     * @brief Draw a fullscreen quad
     */
    void DrawFullscreenQuad(Shader& shader);

    /**
     * @brief Render debug visualizations
     */
    void RenderDebug();

    /**
     * @brief Get the debug draw instance
     */
    DebugDraw& GetDebugDraw() { return *m_debugDraw; }

    /**
     * @brief Get the shader manager
     */
    ShaderManager& GetShaderManager() { return *m_shaderManager; }

    /**
     * @brief Get the texture manager
     */
    TextureManager& GetTextureManager() { return *m_textureManager; }

    /**
     * @brief Get renderer statistics
     */
    struct Stats {
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t vertices = 0;
        void Reset() { drawCalls = triangles = vertices = 0; }
    };

    const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Check for OpenGL errors (debug utility)
     * @param location Optional string to identify where the check was called
     * @return true if no errors were found
     */
    static bool CheckGLError(const char* location = nullptr);

    /**
     * @brief Enable or disable OpenGL debug output (requires OpenGL 4.3+)
     */
    static void EnableDebugOutput(bool enabled);

    // ========================================================================
    // Performance Optimization Systems
    // ========================================================================

    /**
     * @brief Initialize optimized rendering subsystems
     * @param configPath Path to graphics configuration JSON (optional)
     */
    bool InitializeOptimizations(const std::string& configPath = "");

    /**
     * @brief Get the optimized renderer
     */
    OptimizedRenderer* GetOptimizedRenderer() { return m_optimizedRenderer.get(); }
    const OptimizedRenderer* GetOptimizedRenderer() const { return m_optimizedRenderer.get(); }

    /**
     * @brief Check if optimizations are enabled
     */
    bool IsOptimizationEnabled() const { return m_optimizationsEnabled; }

    /**
     * @brief Enable/disable optimization systems
     */
    void SetOptimizationsEnabled(bool enabled) { m_optimizationsEnabled = enabled; }

    /**
     * @brief Submit mesh for optimized rendering (batching, culling, etc.)
     */
    void SubmitOptimized(const std::shared_ptr<Mesh>& mesh,
                         const std::shared_ptr<Material>& material,
                         const glm::mat4& transform,
                         uint32_t objectID = 0);

    /**
     * @brief Flush optimized render queue
     */
    void FlushOptimized();

    /**
     * @brief Apply quality preset for optimizations
     */
    void ApplyQualityPreset(const std::string& preset);

    /**
     * @brief Get extended statistics including optimization metrics
     */
    struct ExtendedStats {
        Stats baseStats;
        uint32_t batchedDrawCalls = 0;
        uint32_t instancedDrawCalls = 0;
        uint32_t drawCallsSaved = 0;
        uint32_t objectsCulled = 0;
        float cullingEfficiency = 0.0f;
        float batchingEfficiency = 0.0f;
        float lodSavings = 0.0f;
        uint32_t stateChanges = 0;
    };

    ExtendedStats GetExtendedStats() const;

private:
    void CreateFullscreenQuad();

    Camera* m_activeCamera = nullptr;
    std::unique_ptr<DebugDraw> m_debugDraw;
    std::unique_ptr<ShaderManager> m_shaderManager;
    std::unique_ptr<TextureManager> m_textureManager;

    // Fullscreen quad
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    Stats m_stats;
    bool m_initialized = false;

    // Cached OpenGL state to avoid redundant state changes
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
};

} // namespace Nova
