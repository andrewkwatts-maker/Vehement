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
};

} // namespace Nova
