#include "graphics/Renderer.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"
#include "graphics/ShaderManager.hpp"
#include "graphics/TextureManager.hpp"
#include "graphics/debug/DebugDraw.hpp"
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
    if (enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::SetDepthWrite(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void Renderer::SetCulling(bool enabled, bool cullBack) {
    if (enabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(cullBack ? GL_BACK : GL_FRONT);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void Renderer::SetBlending(bool enabled) {
    if (enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void Renderer::SetWireframe(bool enabled) {
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
    glBindVertexArray(0);

    m_stats.drawCalls++;
}

void Renderer::RenderDebug() {
    if (m_activeCamera) {
        m_debugDraw->Render(m_activeCamera->GetProjectionView());
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

} // namespace Nova
