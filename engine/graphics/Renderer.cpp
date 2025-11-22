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

namespace Nova {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing renderer");

    // Enable OpenGL features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable MSAA if available
    glEnable(GL_MULTISAMPLE);

    // Enable seamless cubemaps
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Initialize subsystems
    m_shaderManager = std::make_unique<ShaderManager>();
    m_textureManager = std::make_unique<TextureManager>();

    m_debugDraw = std::make_unique<DebugDraw>();
    if (!m_debugDraw->Initialize()) {
        spdlog::error("Failed to initialize debug draw");
        return false;
    }

    CreateFullscreenQuad();

    m_initialized = true;
    spdlog::info("Renderer initialized");
    return true;
}

void Renderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_debugDraw.reset();
    m_shaderManager.reset();
    m_textureManager.reset();

    if (m_quadVAO) {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }

    m_initialized = false;
}

void Renderer::BeginFrame() {
    m_stats.Reset();

    auto& config = Config::Instance();
    glm::vec4 clearColor = config.Get("render.clear_color", glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
    Clear(clearColor);

    // Clear debug draw for new frame
    m_debugDraw->Clear();
}

void Renderer::EndFrame() {
    // Nothing special needed here currently
}

void Renderer::Clear(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void Renderer::SetDepthTest(bool enabled) {
    if (m_glState.depthTest == enabled) return;
    m_glState.depthTest = enabled;

    if (enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::SetDepthWrite(bool enabled) {
    if (m_glState.depthWrite == enabled) return;
    m_glState.depthWrite = enabled;

    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void Renderer::SetCulling(bool enabled, bool cullBack) {
    if (m_glState.culling == enabled && m_glState.cullBack == cullBack) return;

    if (m_glState.culling != enabled) {
        m_glState.culling = enabled;
        if (enabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    if (enabled && m_glState.cullBack != cullBack) {
        m_glState.cullBack = cullBack;
        glCullFace(cullBack ? GL_BACK : GL_FRONT);
    }
}

void Renderer::SetBlending(bool enabled) {
    if (m_glState.blending == enabled) return;
    m_glState.blending = enabled;

    if (enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void Renderer::SetWireframe(bool enabled) {
    if (m_glState.wireframe == enabled) return;
    m_glState.wireframe = enabled;

    glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
}

void Renderer::DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform) {
    // Bind material and set uniforms
    material.Bind();

    if (m_activeCamera) {
        material.GetShader().SetMat4("u_ProjectionView", m_activeCamera->GetProjectionView());
        material.GetShader().SetMat4("u_Model", transform);
        material.GetShader().SetMat3("u_NormalMatrix",
            glm::transpose(glm::inverse(glm::mat3(transform))));
    }

    // Draw the mesh
    mesh.Draw();

    // Update stats
    m_stats.drawCalls++;
    m_stats.vertices += mesh.GetVertexCount();
    m_stats.triangles += mesh.GetIndexCount() / 3;
}

void Renderer::DrawMesh(const Mesh& mesh, Shader& shader, const glm::mat4& transform) {
    shader.Bind();

    if (m_activeCamera) {
        shader.SetMat4("u_ProjectionView", m_activeCamera->GetProjectionView());
        shader.SetMat4("u_Model", transform);
    }

    mesh.Draw();

    m_stats.drawCalls++;
    m_stats.vertices += mesh.GetVertexCount();
    m_stats.triangles += mesh.GetIndexCount() / 3;
}

void Renderer::DrawFullscreenQuad(Shader& shader) {
    shader.Bind();

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_stats.drawCalls++;
}

void Renderer::RenderDebug() {
    if (m_activeCamera) {
        m_debugDraw->Render(m_activeCamera->GetProjectionView());
    }
}

bool Renderer::CheckGLError(const char* location) {
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return true;
    }

    const char* errorStr = "Unknown error";
    switch (error) {
        case GL_INVALID_ENUM:      errorStr = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE:     errorStr = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY:     errorStr = "GL_OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
    }

    if (location) {
        spdlog::error("OpenGL error at {}: {}", location, errorStr);
    } else {
        spdlog::error("OpenGL error: {}", errorStr);
    }

    return false;
}

// Debug output callback for OpenGL 4.3+
static void GLAPIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id,
                                        GLenum severity, GLsizei /*length*/,
                                        const GLchar* message, const void* /*userParam*/) {
    // Ignore non-significant notifications
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    const char* sourceStr = "Unknown";
    switch (source) {
        case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           sourceStr = "Other"; break;
    }

    const char* typeStr = "Unknown";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Undefined Behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              typeStr = "Marker"; break;
        case GL_DEBUG_TYPE_OTHER:               typeStr = "Other"; break;
    }

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

void Renderer::EnableDebugOutput(bool enabled) {
    if (enabled) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, nullptr);
        // Enable all debug messages
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        spdlog::info("OpenGL debug output enabled");
    } else {
        glDisable(GL_DEBUG_OUTPUT);
        spdlog::info("OpenGL debug output disabled");
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

// ============================================================================
// Performance Optimization Systems
// ============================================================================

bool Renderer::InitializeOptimizations(const std::string& configPath) {
    if (m_optimizedRenderer) {
        return true;  // Already initialized
    }

    m_optimizedRenderer = std::make_unique<OptimizedRenderer>();

    std::string config = configPath;
    if (config.empty()) {
        // Try default config path
        config = "config/graphics_config.json";
    }

    if (!m_optimizedRenderer->Initialize(this, config)) {
        spdlog::error("Failed to initialize optimization systems");
        m_optimizedRenderer.reset();
        return false;
    }

    m_optimizationsEnabled = true;
    spdlog::info("Renderer optimization systems initialized");

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
        stats.objectsCulled = perfStats.frustumCulled + perfStats.occlusionCulled + perfStats.distanceCulled;
        stats.cullingEfficiency = perfStats.cullingEfficiency;
        stats.stateChanges = perfStats.stateChanges;

        // Calculate batching efficiency
        if (perfStats.totalDrawCalls > 0) {
            stats.batchingEfficiency = static_cast<float>(stats.drawCallsSaved) /
                                       (perfStats.totalDrawCalls + stats.drawCallsSaved) * 100.0f;
        }

        // Calculate LOD savings
        if (perfStats.totalTriangles > 0) {
            stats.lodSavings = static_cast<float>(perfStats.totalTriangles - perfStats.trianglesAfterLOD) /
                               perfStats.totalTriangles * 100.0f;
        }
    }

    return stats;
}

} // namespace Nova
