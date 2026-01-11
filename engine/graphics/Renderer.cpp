/**
 * @file Renderer.cpp
 * @brief Complete rendering system implementation following SOLID principles
 *
 * Architecture Overview:
 * ======================
 * This file implements a modern rendering system designed around SOLID principles:
 *
 * - Single Responsibility: Each class has one focused purpose
 *   - RenderStateManager: GPU state management (depth, blend, cull)
 *   - MaterialBindingService: Material and shader binding
 *   - FullscreenQuadRenderer: Fullscreen quad rendering
 *   - RenderGraph: Frame scheduling and pass management
 *   - Renderer: High-level orchestration
 *
 * - Open/Closed: Extensible through interfaces without modification
 *   - IRenderBackend: Graphics API abstraction
 *   - IRenderPassExecutor: Custom render pass execution
 *
 * - Liskov Substitution: All implementations are substitutable
 *   - Any IRenderBackend can replace another
 *
 * - Interface Segregation: Small, focused interfaces
 *   - Separate interfaces for state, materials, geometry
 *
 * - Dependency Inversion: Depend on abstractions
 *   - Core Renderer depends on interfaces, not concrete implementations
 *
 * SDF Pipeline Integration:
 * ========================
 * The system integrates seamlessly with the SDF raymarching pipeline:
 * - SDFRenderer can be used as an IRenderBackend
 * - RenderGraph supports SDF passes with proper depth integration
 * - Hybrid rendering (SDF + polygon) is supported via HybridRasterizer
 *
 * @author Nova Engine Team
 * @copyright 2025 Nova Engine
 */

#include "graphics/Renderer.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"
#include "graphics/ShaderManager.hpp"
#include "graphics/TextureManager.hpp"
#include "graphics/debug/DebugDraw.hpp"
#include "graphics/OptimizedRenderer.hpp"
#include "scene/Camera.hpp"
#include "config/Config.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

namespace Nova {

// ============================================================================
// Forward Declarations for Internal Classes
// ============================================================================

class RenderStateManager;
class MaterialBindingService;
class FullscreenQuadRenderer;
class RenderGraphImpl;
class DebugOutputManager;

// ============================================================================
// RenderStateManager - Single Responsibility: GPU State Management
// ============================================================================

/**
 * @brief Manages OpenGL state with caching to minimize redundant state changes
 *
 * This class implements the Single Responsibility Principle by focusing solely
 * on GPU state management. It tracks current state and only issues GL calls
 * when the state actually needs to change.
 *
 * Thread Safety: Not thread-safe. Should only be called from render thread.
 */
class RenderStateManager {
public:
    /**
     * @brief Depth function enumeration for type safety
     */
    enum class DepthFunc : uint8_t {
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Equal,
        NotEqual,
        Always,
        Never
    };

    /**
     * @brief Blend mode presets for common operations
     */
    enum class BlendPreset : uint8_t {
        Opaque,         ///< No blending
        AlphaBlend,     ///< Standard alpha blending (src*a + dst*(1-a))
        Additive,       ///< Additive blending (src + dst)
        Multiply,       ///< Multiplicative blending (src * dst)
        PreMultiplied   ///< Pre-multiplied alpha (src + dst*(1-a))
    };

    /**
     * @brief Cull mode options
     */
    enum class CullMode : uint8_t {
        None,           ///< No culling (two-sided)
        Back,           ///< Cull back faces
        Front           ///< Cull front faces
    };

    RenderStateManager() { Reset(); }

    /**
     * @brief Reset all state to default values
     */
    void Reset() {
        m_depthTestEnabled = true;
        m_depthWriteEnabled = true;
        m_depthFunc = DepthFunc::Less;
        m_cullMode = CullMode::Back;
        m_blendPreset = BlendPreset::Opaque;
        m_wireframeEnabled = false;
        m_scissorEnabled = false;
        m_stencilEnabled = false;
        m_boundShader = 0;
        m_boundVAO = 0;

        // Apply default GL state
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
    }

    /**
     * @brief Enable or disable depth testing
     * @param enabled True to enable depth testing
     */
    void SetDepthTest(bool enabled) {
        if (m_depthTestEnabled == enabled) return;
        m_depthTestEnabled = enabled;

        if (enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    /**
     * @brief Enable or disable depth buffer writing
     * @param enabled True to enable depth writes
     */
    void SetDepthWrite(bool enabled) {
        if (m_depthWriteEnabled == enabled) return;
        m_depthWriteEnabled = enabled;
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    /**
     * @brief Set the depth comparison function
     * @param func The depth function to use
     */
    void SetDepthFunc(DepthFunc func) {
        if (m_depthFunc == func) return;
        m_depthFunc = func;

        static const GLenum glFuncs[] = {
            GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL,
            GL_EQUAL, GL_NOTEQUAL, GL_ALWAYS, GL_NEVER
        };
        glDepthFunc(glFuncs[static_cast<uint8_t>(func)]);
    }

    /**
     * @brief Set face culling mode
     * @param mode The culling mode to use
     */
    void SetCullMode(CullMode mode) {
        if (m_cullMode == mode) return;
        m_cullMode = mode;

        if (mode == CullMode::None) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(mode == CullMode::Back ? GL_BACK : GL_FRONT);
        }
    }

    /**
     * @brief Set blend mode using a preset
     * @param preset The blend preset to apply
     */
    void SetBlendPreset(BlendPreset preset) {
        if (m_blendPreset == preset) return;
        m_blendPreset = preset;

        if (preset == BlendPreset::Opaque) {
            glDisable(GL_BLEND);
        } else {
            glEnable(GL_BLEND);
            switch (preset) {
                case BlendPreset::AlphaBlend:
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case BlendPreset::Additive:
                    glBlendFunc(GL_ONE, GL_ONE);
                    break;
                case BlendPreset::Multiply:
                    glBlendFunc(GL_DST_COLOR, GL_ZERO);
                    break;
                case BlendPreset::PreMultiplied:
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                default:
                    break;
            }
        }
    }

    /**
     * @brief Enable or disable wireframe rendering
     * @param enabled True for wireframe mode
     */
    void SetWireframe(bool enabled) {
        if (m_wireframeEnabled == enabled) return;
        m_wireframeEnabled = enabled;
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }

    /**
     * @brief Enable or disable scissor testing
     * @param enabled True to enable scissor test
     */
    void SetScissorTest(bool enabled) {
        if (m_scissorEnabled == enabled) return;
        m_scissorEnabled = enabled;
        if (enabled) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    /**
     * @brief Set scissor rectangle
     * @param x X coordinate of scissor rect
     * @param y Y coordinate of scissor rect
     * @param width Width of scissor rect
     * @param height Height of scissor rect
     */
    void SetScissorRect(int x, int y, int width, int height) {
        glScissor(x, y, width, height);
    }

    /**
     * @brief Enable or disable stencil testing
     * @param enabled True to enable stencil test
     */
    void SetStencilTest(bool enabled) {
        if (m_stencilEnabled == enabled) return;
        m_stencilEnabled = enabled;
        if (enabled) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }

    /**
     * @brief Bind a shader program with caching
     * @param programId The shader program ID
     * @return True if the shader was actually changed
     */
    bool BindShader(uint32_t programId) {
        if (m_boundShader == programId) return false;
        m_boundShader = programId;
        glUseProgram(programId);
        return true;
    }

    /**
     * @brief Bind a VAO with caching
     * @param vaoId The VAO ID
     * @return True if the VAO was actually changed
     */
    bool BindVAO(uint32_t vaoId) {
        if (m_boundVAO == vaoId) return false;
        m_boundVAO = vaoId;
        glBindVertexArray(vaoId);
        return true;
    }

    /**
     * @brief Get current bound shader
     */
    uint32_t GetBoundShader() const { return m_boundShader; }

    /**
     * @brief Get current bound VAO
     */
    uint32_t GetBoundVAO() const { return m_boundVAO; }

    // State accessors
    bool IsDepthTestEnabled() const { return m_depthTestEnabled; }
    bool IsDepthWriteEnabled() const { return m_depthWriteEnabled; }
    DepthFunc GetDepthFunc() const { return m_depthFunc; }
    CullMode GetCullMode() const { return m_cullMode; }
    BlendPreset GetBlendPreset() const { return m_blendPreset; }
    bool IsWireframeEnabled() const { return m_wireframeEnabled; }

private:
    bool m_depthTestEnabled;
    bool m_depthWriteEnabled;
    DepthFunc m_depthFunc;
    CullMode m_cullMode;
    BlendPreset m_blendPreset;
    bool m_wireframeEnabled;
    bool m_scissorEnabled;
    bool m_stencilEnabled;
    uint32_t m_boundShader;
    uint32_t m_boundVAO;
};

// ============================================================================
// MaterialBindingService - Single Responsibility: Material Binding
// ============================================================================

/**
 * @brief Service for binding materials and managing uniform uploads
 *
 * Implements Interface Segregation by providing a focused API for
 * material-related operations, separate from general state management.
 */
class MaterialBindingService {
public:
    /**
     * @brief Bind a material for rendering
     * @param material The material to bind
     * @param stateManager State manager for blend/cull configuration
     */
    void BindMaterial(const Material& material, RenderStateManager& stateManager) {
        // Bind shader
        material.Bind();

        // Configure state based on material properties
        if (material.IsTwoSided()) {
            stateManager.SetCullMode(RenderStateManager::CullMode::None);
        } else {
            stateManager.SetCullMode(RenderStateManager::CullMode::Back);
        }

        if (material.IsTransparent()) {
            stateManager.SetBlendPreset(RenderStateManager::BlendPreset::AlphaBlend);
            stateManager.SetDepthWrite(false);
        } else {
            stateManager.SetBlendPreset(RenderStateManager::BlendPreset::Opaque);
            stateManager.SetDepthWrite(true);
        }
    }

    /**
     * @brief Upload camera matrices to a shader
     * @param shader The shader to configure
     * @param camera The camera providing matrices
     * @param modelTransform The model transformation matrix
     */
    void UploadCameraUniforms(Shader& shader, const Camera& camera, const glm::mat4& modelTransform) {
        shader.SetMat4("u_ProjectionView", camera.GetProjectionView());
        shader.SetMat4("u_Model", modelTransform);
        shader.SetMat3("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(modelTransform))));
        shader.SetVec3("u_CameraPosition", camera.GetPosition());
    }

    /**
     * @brief Upload standard PBR uniforms
     * @param shader The shader to configure
     * @param albedo Albedo color
     * @param metallic Metallic factor
     * @param roughness Roughness factor
     * @param ao Ambient occlusion factor
     */
    void UploadPBRUniforms(Shader& shader, const glm::vec3& albedo, float metallic,
                          float roughness, float ao) {
        shader.SetVec3("u_Albedo", albedo);
        shader.SetFloat("u_Metallic", metallic);
        shader.SetFloat("u_Roughness", roughness);
        shader.SetFloat("u_AO", ao);
    }
};

// ============================================================================
// FullscreenQuadRenderer - Single Responsibility: Fullscreen Quad Rendering
// ============================================================================

/**
 * @brief Manages fullscreen quad geometry for post-processing effects
 *
 * This class encapsulates the creation and rendering of fullscreen quads,
 * used extensively in post-processing, deferred shading, and SDF rendering.
 */
class FullscreenQuadRenderer {
public:
    FullscreenQuadRenderer() = default;
    ~FullscreenQuadRenderer() { Cleanup(); }

    /**
     * @brief Initialize the fullscreen quad geometry
     * @return True on success
     */
    bool Initialize() {
        if (m_initialized) return true;

        // Fullscreen quad vertices: position (2D) + texcoord (2D)
        // Uses a triangle strip for efficiency
        const float vertices[] = {
            // Position     // TexCoord
            -1.0f,  1.0f,   0.0f, 1.0f,  // Top-left
            -1.0f, -1.0f,   0.0f, 0.0f,  // Bottom-left
             1.0f,  1.0f,   1.0f, 1.0f,  // Top-right
             1.0f, -1.0f,   1.0f, 0.0f   // Bottom-right
        };

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Position attribute (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

        // Texcoord attribute (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                            reinterpret_cast<void*>(2 * sizeof(float)));

        glBindVertexArray(0);

        m_initialized = true;
        return true;
    }

    /**
     * @brief Render the fullscreen quad
     * @param shader The shader to use (must be already bound)
     */
    void Render(Shader& shader) {
        if (!m_initialized) return;

        shader.Bind();
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    /**
     * @brief Render the quad without binding a shader (assumes shader is bound)
     */
    void RenderWithoutShaderBind() {
        if (!m_initialized) return;
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    /**
     * @brief Get the VAO ID for external use
     */
    uint32_t GetVAO() const { return m_vao; }

private:
    void Cleanup() {
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        if (m_vbo) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        m_initialized = false;
    }

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    bool m_initialized = false;
};

// ============================================================================
// RenderPass - Supporting Types for RenderGraph
// ============================================================================

/**
 * @brief Enumeration of render pass types
 */
enum class RenderPassType : uint8_t {
    Shadow,         ///< Shadow map generation
    DepthPrepass,   ///< Depth pre-pass for early-z
    GBuffer,        ///< Deferred G-buffer fill
    SDFRaymarch,    ///< SDF raymarching pass
    Lighting,       ///< Deferred lighting
    Forward,        ///< Forward rendering pass
    Transparent,    ///< Transparent object pass
    PostProcess,    ///< Post-processing effects
    UI,             ///< UI rendering
    Debug           ///< Debug visualization
};

/**
 * @brief Interface for render pass executors
 *
 * Implements Open/Closed Principle: new pass types can be added by
 * implementing this interface without modifying existing code.
 */
class IRenderPassExecutor {
public:
    virtual ~IRenderPassExecutor() = default;

    /**
     * @brief Get the pass type
     */
    virtual RenderPassType GetType() const = 0;

    /**
     * @brief Get pass name for debugging
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief Execute the render pass
     * @param stateManager State manager for GPU state
     */
    virtual void Execute(RenderStateManager& stateManager) = 0;

    /**
     * @brief Check if this pass should execute this frame
     */
    virtual bool ShouldExecute() const { return true; }
};

// ============================================================================
// RenderGraph - Frame Scheduling and Pass Management
// ============================================================================

/**
 * @brief Manages render pass scheduling and dependencies
 *
 * The RenderGraph provides:
 * - Automatic pass ordering based on dependencies
 * - Resource lifetime management
 * - Frame timing statistics
 * - Support for both polygon and SDF rendering passes
 */
class RenderGraphImpl {
public:
    /**
     * @brief Represents a node in the render graph
     */
    struct PassNode {
        std::unique_ptr<IRenderPassExecutor> executor;
        std::vector<size_t> dependencies;   ///< Indices of passes this depends on
        float executionTimeMs = 0.0f;
        bool executed = false;
    };

    RenderGraphImpl() = default;

    /**
     * @brief Add a render pass to the graph
     * @param executor The pass executor
     * @param dependencies Indices of passes this depends on
     * @return Index of the added pass
     */
    size_t AddPass(std::unique_ptr<IRenderPassExecutor> executor,
                   std::vector<size_t> dependencies = {}) {
        PassNode node;
        node.executor = std::move(executor);
        node.dependencies = std::move(dependencies);
        m_passes.push_back(std::move(node));
        return m_passes.size() - 1;
    }

    /**
     * @brief Execute all passes in dependency order
     * @param stateManager State manager for GPU state
     */
    void Execute(RenderStateManager& stateManager) {
        // Reset execution state
        for (auto& pass : m_passes) {
            pass.executed = false;
        }

        // Execute passes in topological order
        std::vector<size_t> executionOrder;
        BuildExecutionOrder(executionOrder);

        for (size_t idx : executionOrder) {
            auto& pass = m_passes[idx];
            if (pass.executor && pass.executor->ShouldExecute()) {
                auto startTime = std::chrono::high_resolution_clock::now();

                pass.executor->Execute(stateManager);

                auto endTime = std::chrono::high_resolution_clock::now();
                std::chrono::duration<float, std::milli> duration = endTime - startTime;
                pass.executionTimeMs = duration.count();
            }
            pass.executed = true;
        }
    }

    /**
     * @brief Clear all passes from the graph
     */
    void Clear() {
        m_passes.clear();
    }

    /**
     * @brief Get pass count
     */
    size_t GetPassCount() const { return m_passes.size(); }

    /**
     * @brief Get execution time for a pass
     */
    float GetPassTime(size_t index) const {
        if (index < m_passes.size()) {
            return m_passes[index].executionTimeMs;
        }
        return 0.0f;
    }

    /**
     * @brief Get total execution time for all passes
     */
    float GetTotalTime() const {
        float total = 0.0f;
        for (const auto& pass : m_passes) {
            total += pass.executionTimeMs;
        }
        return total;
    }

private:
    /**
     * @brief Build topological execution order respecting dependencies
     */
    void BuildExecutionOrder(std::vector<size_t>& order) {
        order.clear();
        std::vector<bool> visited(m_passes.size(), false);
        std::vector<bool> inStack(m_passes.size(), false);

        for (size_t i = 0; i < m_passes.size(); ++i) {
            if (!visited[i]) {
                TopologicalSort(i, visited, inStack, order);
            }
        }

        // The order is built in reverse (post-order), so reverse it
        std::reverse(order.begin(), order.end());
    }

    void TopologicalSort(size_t node, std::vector<bool>& visited,
                        std::vector<bool>& inStack, std::vector<size_t>& order) {
        visited[node] = true;
        inStack[node] = true;

        for (size_t dep : m_passes[node].dependencies) {
            if (dep >= m_passes.size()) continue;

            if (!visited[dep]) {
                TopologicalSort(dep, visited, inStack, order);
            } else if (inStack[dep]) {
                spdlog::warn("RenderGraph: Circular dependency detected at pass {}",
                           m_passes[node].executor ? m_passes[node].executor->GetName() : "unknown");
            }
        }

        inStack[node] = false;
        order.push_back(node);
    }

    std::vector<PassNode> m_passes;
};

// ============================================================================
// DebugOutputManager - OpenGL Debug Output Management
// ============================================================================

/**
 * @brief Manages OpenGL debug output (requires OpenGL 4.3+)
 */
class DebugOutputManager {
public:
    /**
     * @brief Enable OpenGL debug output with callback
     */
    static void Enable() {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugCallback, nullptr);
        // Enable all messages
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        spdlog::info("OpenGL debug output enabled");
    }

    /**
     * @brief Disable OpenGL debug output
     */
    static void Disable() {
        glDisable(GL_DEBUG_OUTPUT);
        spdlog::info("OpenGL debug output disabled");
    }

    /**
     * @brief Check for OpenGL errors
     * @param location Optional location identifier for error messages
     * @return True if no errors were found
     */
    static bool CheckError(const char* location = nullptr) {
        GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            return true;
        }

        const char* errorStr = GetErrorString(error);
        if (location) {
            spdlog::error("OpenGL error at {}: {}", location, errorStr);
        } else {
            spdlog::error("OpenGL error: {}", errorStr);
        }
        return false;
    }

private:
    static const char* GetErrorString(GLenum error) {
        switch (error) {
            case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
            case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
            case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
            default:                               return "Unknown error";
        }
    }

    static const char* GetSourceString(GLenum source) {
        switch (source) {
            case GL_DEBUG_SOURCE_API:             return "API";
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "Window System";
            case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
            case GL_DEBUG_SOURCE_THIRD_PARTY:     return "Third Party";
            case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
            case GL_DEBUG_SOURCE_OTHER:           return "Other";
            default:                              return "Unknown";
        }
    }

    static const char* GetTypeString(GLenum type) {
        switch (type) {
            case GL_DEBUG_TYPE_ERROR:               return "Error";
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated";
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "Undefined Behavior";
            case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
            case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
            case GL_DEBUG_TYPE_MARKER:              return "Marker";
            case GL_DEBUG_TYPE_OTHER:               return "Other";
            default:                                return "Unknown";
        }
    }

    static void GLAPIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
                                         GLenum severity, GLsizei /*length*/,
                                         const GLchar* message, const void* /*userParam*/) {
        // Skip non-significant notifications
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

        const char* sourceStr = GetSourceString(source);
        const char* typeStr = GetTypeString(type);

        switch (severity) {
            case GL_DEBUG_SEVERITY_HIGH:
                spdlog::error("GL Debug [{}][{}] ({}): {}", sourceStr, typeStr, id, message);
                break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                spdlog::warn("GL Debug [{}][{}] ({}): {}", sourceStr, typeStr, id, message);
                break;
            case GL_DEBUG_SEVERITY_LOW:
                spdlog::info("GL Debug [{}][{}] ({}): {}", sourceStr, typeStr, id, message);
                break;
            default:
                spdlog::debug("GL Debug [{}][{}] ({}): {}", sourceStr, typeStr, id, message);
                break;
        }
    }
};

// ============================================================================
// MeshDrawer - Geometry Rendering Service
// ============================================================================

/**
 * @brief Service for drawing mesh geometry
 *
 * Encapsulates mesh drawing operations and statistics tracking.
 */
class MeshDrawer {
public:
    /**
     * @brief Draw a mesh with material
     * @param mesh The mesh to draw
     * @param material The material to use
     * @param transform Model transformation matrix
     * @param camera Active camera (may be null)
     * @param stateManager State manager for GPU state
     * @param materialService Material binding service
     * @param stats Statistics to update
     */
    void Draw(const Mesh& mesh, const Material& material, const glm::mat4& transform,
              const Camera* camera, RenderStateManager& stateManager,
              MaterialBindingService& materialService, Renderer::Stats& stats) {
        // Bind material (configures shader and textures)
        materialService.BindMaterial(material, stateManager);

        // Upload camera uniforms if camera is available
        if (camera) {
            materialService.UploadCameraUniforms(material.GetShader(), *camera, transform);
        }

        // Draw the mesh
        mesh.Draw();

        // Update statistics
        stats.drawCalls++;
        stats.vertices += mesh.GetVertexCount();
        stats.triangles += mesh.GetIndexCount() / 3;
    }

    /**
     * @brief Draw a mesh with a raw shader (no material)
     * @param mesh The mesh to draw
     * @param shader The shader to use
     * @param transform Model transformation matrix
     * @param camera Active camera (may be null)
     * @param stats Statistics to update
     */
    void DrawWithShader(const Mesh& mesh, Shader& shader, const glm::mat4& transform,
                        const Camera* camera, Renderer::Stats& stats) {
        shader.Bind();

        if (camera) {
            shader.SetMat4("u_ProjectionView", camera->GetProjectionView());
            shader.SetMat4("u_Model", transform);
        }

        mesh.Draw();

        stats.drawCalls++;
        stats.vertices += mesh.GetVertexCount();
        stats.triangles += mesh.GetIndexCount() / 3;
    }
};

// ============================================================================
// Renderer Implementation
// ============================================================================

/**
 * @brief Internal implementation details for Renderer
 *
 * Using PIMPL pattern to hide implementation details and reduce compilation
 * dependencies. This allows changing the implementation without affecting
 * code that uses Renderer.
 */
struct Renderer::Impl {
    // Core services following Single Responsibility
    std::unique_ptr<RenderStateManager> stateManager;
    std::unique_ptr<MaterialBindingService> materialService;
    std::unique_ptr<FullscreenQuadRenderer> fullscreenQuad;
    std::unique_ptr<MeshDrawer> meshDrawer;
    std::unique_ptr<RenderGraphImpl> renderGraph;

    // Subsystems
    std::unique_ptr<DebugDraw> debugDraw;
    std::unique_ptr<ShaderManager> shaderManager;
    std::unique_ptr<TextureManager> textureManager;

    // Optimization systems (composed, not inherited - Dependency Inversion)
    std::unique_ptr<OptimizedRenderer> optimizedRenderer;
    bool optimizationsEnabled = false;

    // Frame state
    Camera* activeCamera = nullptr;
    Stats stats;
    bool initialized = false;

    // Frame timing
    std::chrono::high_resolution_clock::time_point frameStartTime;
    float lastFrameTimeMs = 0.0f;

    Impl() {
        stateManager = std::make_unique<RenderStateManager>();
        materialService = std::make_unique<MaterialBindingService>();
        fullscreenQuad = std::make_unique<FullscreenQuadRenderer>();
        meshDrawer = std::make_unique<MeshDrawer>();
        renderGraph = std::make_unique<RenderGraphImpl>();
    }
};

// ============================================================================
// Renderer Public API Implementation
// ============================================================================

Renderer::Renderer()
    : m_impl(std::make_unique<Impl>()) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing Nova Renderer");

    // Enable essential OpenGL features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable MSAA if available
    glEnable(GL_MULTISAMPLE);

    // Enable seamless cubemaps for environment mapping
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Initialize state manager (sets default GL state)
    m_impl->stateManager->Reset();

    // Initialize shader manager
    m_shaderManager = std::make_unique<ShaderManager>();
    m_impl->shaderManager = std::make_unique<ShaderManager>();

    // Initialize texture manager
    m_textureManager = std::make_unique<TextureManager>();
    m_impl->textureManager = std::make_unique<TextureManager>();

    // Initialize debug drawing system
    m_debugDraw = std::make_unique<DebugDraw>();
    m_impl->debugDraw = std::make_unique<DebugDraw>();
    if (!m_debugDraw->Initialize()) {
        spdlog::error("Failed to initialize debug draw system");
        return false;
    }

    // Initialize fullscreen quad (used for post-processing and SDF rendering)
    if (!m_impl->fullscreenQuad->Initialize()) {
        spdlog::error("Failed to initialize fullscreen quad");
        return false;
    }

    // Create legacy fullscreen quad for backward compatibility
    CreateFullscreenQuad();

    m_initialized = true;
    m_impl->initialized = true;

    spdlog::info("Nova Renderer initialized successfully");
    return true;
}

void Renderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down Nova Renderer");

    // Shutdown optimizations first
    if (m_optimizedRenderer) {
        m_optimizedRenderer.reset();
    }
    m_impl->optimizedRenderer.reset();

    // Shutdown debug draw
    m_debugDraw.reset();
    m_impl->debugDraw.reset();

    // Shutdown managers
    m_shaderManager.reset();
    m_textureManager.reset();
    m_impl->shaderManager.reset();
    m_impl->textureManager.reset();

    // Cleanup legacy fullscreen quad
    if (m_quadVAO) {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }

    // Clear render graph
    m_impl->renderGraph->Clear();

    m_initialized = false;
    m_impl->initialized = false;

    spdlog::info("Nova Renderer shutdown complete");
}

void Renderer::BeginFrame() {
    // Record frame start time
    m_impl->frameStartTime = std::chrono::high_resolution_clock::now();

    // Reset frame statistics
    m_stats.Reset();
    m_impl->stats.Reset();

    // Get clear color from config
    auto& config = Config::Instance();
    glm::vec4 clearColor = config.Get("render.clear_color",
                                       glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
    Clear(clearColor);

    // Reset debug draw for new frame
    if (m_debugDraw) {
        m_debugDraw->Clear();
    }
}

void Renderer::EndFrame() {
    // Calculate frame time
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration =
        endTime - m_impl->frameStartTime;
    m_impl->lastFrameTimeMs = duration.count();
}

void Renderer::Clear(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Renderer::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void Renderer::SetDepthTest(bool enabled) {
    // Use state manager for cached state changes
    m_impl->stateManager->SetDepthTest(enabled);

    // Update legacy state tracking
    if (m_glState.depthTest == enabled) return;
    m_glState.depthTest = enabled;
}

void Renderer::SetDepthWrite(bool enabled) {
    m_impl->stateManager->SetDepthWrite(enabled);

    if (m_glState.depthWrite == enabled) return;
    m_glState.depthWrite = enabled;
}

void Renderer::SetCulling(bool enabled, bool cullBack) {
    // Use state manager
    if (!enabled) {
        m_impl->stateManager->SetCullMode(RenderStateManager::CullMode::None);
    } else if (cullBack) {
        m_impl->stateManager->SetCullMode(RenderStateManager::CullMode::Back);
    } else {
        m_impl->stateManager->SetCullMode(RenderStateManager::CullMode::Front);
    }

    // Update legacy state tracking
    if (m_glState.culling == enabled && m_glState.cullBack == cullBack) return;
    m_glState.culling = enabled;
    m_glState.cullBack = cullBack;
}

void Renderer::SetBlending(bool enabled) {
    if (enabled) {
        m_impl->stateManager->SetBlendPreset(RenderStateManager::BlendPreset::AlphaBlend);
    } else {
        m_impl->stateManager->SetBlendPreset(RenderStateManager::BlendPreset::Opaque);
    }

    if (m_glState.blending == enabled) return;
    m_glState.blending = enabled;
}

void Renderer::SetWireframe(bool enabled) {
    m_impl->stateManager->SetWireframe(enabled);

    if (m_glState.wireframe == enabled) return;
    m_glState.wireframe = enabled;
}

void Renderer::DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform) {
    m_impl->meshDrawer->Draw(mesh, material, transform, m_activeCamera,
                             *m_impl->stateManager, *m_impl->materialService, m_stats);
}

void Renderer::DrawMesh(const Mesh& mesh, Shader& shader, const glm::mat4& transform) {
    m_impl->meshDrawer->DrawWithShader(mesh, shader, transform, m_activeCamera, m_stats);
}

void Renderer::DrawFullscreenQuad(Shader& shader) {
    shader.Bind();

    // Prefer new implementation
    m_impl->fullscreenQuad->RenderWithoutShaderBind();

    m_stats.drawCalls++;
}

void Renderer::RenderDebug() {
    if (m_activeCamera && m_debugDraw) {
        m_debugDraw->Render(m_activeCamera->GetProjectionView());
    }
}

bool Renderer::CheckGLError(const char* location) {
    return DebugOutputManager::CheckError(location);
}

void Renderer::EnableDebugOutput(bool enabled) {
    if (enabled) {
        DebugOutputManager::Enable();
    } else {
        DebugOutputManager::Disable();
    }
}

void Renderer::CreateFullscreenQuad() {
    // Fullscreen quad vertices (position + texcoord)
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                         reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
}

// ============================================================================
// Performance Optimization Systems
// ============================================================================

bool Renderer::InitializeOptimizations(const std::string& configPath) {
    if (m_optimizedRenderer) {
        return true;  // Already initialized
    }

    spdlog::info("Initializing renderer optimization systems");

    m_optimizedRenderer = std::make_unique<OptimizedRenderer>();
    m_impl->optimizedRenderer = std::make_unique<OptimizedRenderer>();

    std::string config = configPath;
    if (config.empty()) {
        config = "config/graphics_config.json";
    }

    if (!m_optimizedRenderer->Initialize(this, config)) {
        spdlog::error("Failed to initialize optimization systems");
        m_optimizedRenderer.reset();
        m_impl->optimizedRenderer.reset();
        return false;
    }

    m_optimizationsEnabled = true;
    m_impl->optimizationsEnabled = true;

    spdlog::info("Renderer optimization systems initialized successfully");
    return true;
}

void Renderer::SubmitOptimized(const std::shared_ptr<Mesh>& mesh,
                               const std::shared_ptr<Material>& material,
                               const glm::mat4& transform,
                               uint32_t objectID) {
    if (!m_optimizedRenderer || !m_optimizationsEnabled) {
        // Fall back to direct rendering
        DrawMesh(*mesh, *material, transform);
        return;
    }

    m_optimizedRenderer->Submit(mesh, material, transform, objectID);
}

void Renderer::FlushOptimized() {
    if (!m_optimizedRenderer || !m_optimizationsEnabled) {
        return;
    }

    m_optimizedRenderer->Render();
}

void Renderer::ApplyQualityPreset(const std::string& preset) {
    if (m_optimizedRenderer) {
        m_optimizedRenderer->ApplyQualityPreset(preset);
        spdlog::info("Applied quality preset: {}", preset);
    }
}

Renderer::ExtendedStats Renderer::GetExtendedStats() const {
    ExtendedStats stats;
    stats.baseStats = m_stats;

    if (m_optimizedRenderer) {
        const auto& perfStats = m_optimizedRenderer->GetStats();

        stats.batchedDrawCalls = perfStats.batchedDrawCalls;
        stats.instancedDrawCalls = perfStats.instancedDrawCalls;
        stats.drawCallsSaved = perfStats.drawCallsSaved;
        stats.objectsCulled = perfStats.frustumCulled + perfStats.occlusionCulled +
                              perfStats.distanceCulled;
        stats.cullingEfficiency = perfStats.cullingEfficiency;
        stats.stateChanges = perfStats.stateChanges;

        // Calculate batching efficiency
        if (perfStats.totalDrawCalls > 0) {
            stats.batchingEfficiency = static_cast<float>(stats.drawCallsSaved) /
                                       (perfStats.totalDrawCalls + stats.drawCallsSaved) * 100.0f;
        }

        // Calculate LOD savings
        if (perfStats.totalTriangles > 0) {
            stats.lodSavings = static_cast<float>(perfStats.totalTriangles -
                                                   perfStats.trianglesAfterLOD) /
                               perfStats.totalTriangles * 100.0f;
        }
    }

    return stats;
}

// ============================================================================
// Additional Utility Methods
// ============================================================================

/**
 * @brief Get the current frame time in milliseconds
 */
float Renderer::GetFrameTimeMs() const {
    return m_impl->lastFrameTimeMs;
}

/**
 * @brief Get the render graph for advanced pass management
 */
RenderGraphImpl* Renderer::GetRenderGraph() {
    return m_impl->renderGraph.get();
}

/**
 * @brief Get the state manager for direct state control
 */
RenderStateManager* Renderer::GetStateManager() {
    return m_impl->stateManager.get();
}

/**
 * @brief Get the fullscreen quad renderer
 */
FullscreenQuadRenderer* Renderer::GetFullscreenQuadRenderer() {
    return m_impl->fullscreenQuad.get();
}

} // namespace Nova
