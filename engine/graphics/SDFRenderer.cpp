#include "SDFRenderer.hpp"
#include "../sdf/SDFModel.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include "../scene/Camera.hpp"
#include "Texture.hpp"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include "RadianceCascade.hpp"
#include "SpectralRenderer.hpp"
#include <cmath>

namespace Nova {

SDFRenderer::SDFRenderer() = default;

SDFRenderer::~SDFRenderer() {
    Shutdown();
}

bool SDFRenderer::Initialize() {
    if (m_initialized) return true;

    // Create raymarching shader
    m_raymarchShader = std::make_unique<Shader>();
    if (!m_raymarchShader->Load("assets/shaders/sdf_raymarching.vert",
                                        "assets/shaders/sdf_raymarching.frag")) {
        return false;
    }

    // Create fullscreen quad
    CreateFullscreenQuad();

    // Create SSBO for primitives
    glGenBuffers(1, &m_primitivesSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_primitivesSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 m_maxPrimitives * sizeof(SDFPrimitiveData),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_initialized = true;
    return true;
}

void SDFRenderer::Shutdown() {
    if (!m_initialized) return;

    if (m_primitivesSSBO) {
        glDeleteBuffers(1, &m_primitivesSSBO);
        m_primitivesSSBO = 0;
    }

    if (m_fullscreenVAO) {
        glDeleteVertexArrays(1, &m_fullscreenVAO);
        m_fullscreenVAO = 0;
    }

    if (m_fullscreenVBO) {
        glDeleteBuffers(1, &m_fullscreenVBO);
        m_fullscreenVBO = 0;
    }

    m_raymarchShader.reset();
    m_environmentMap.reset();

    m_initialized = false;
}

void SDFRenderer::CreateFullscreenQuad() {
    // Fullscreen quad vertices (NDC)
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_fullscreenVAO);
    glGenBuffers(1, &m_fullscreenVBO);

    glBindVertexArray(m_fullscreenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // TexCoord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void SDFRenderer::UploadModelData(const SDFModel& model, const glm::mat4& modelTransform) {
    std::vector<SDFPrimitiveData> primitivesData;

    // Collect all primitives
    auto allPrimitives = model.GetAllPrimitives();

    for (const auto* prim : allPrimitives) {
        if (!prim->IsVisible()) continue;

        SDFPrimitiveData data{};

        // World transform
        SDFTransform worldTransform = prim->GetWorldTransform();
        glm::mat4 localMatrix = worldTransform.ToMatrix();
        data.transform = modelTransform * localMatrix;
        data.inverseTransform = worldTransform.ToInverseMatrix();

        // Parameters
        const auto& params = prim->GetParameters();
        data.parameters = glm::vec4(params.radius, params.dimensions.x, params.dimensions.y, params.dimensions.z);
        data.parameters2 = glm::vec4(params.height, params.topRadius, params.bottomRadius, params.cornerRadius);
        data.parameters3 = glm::vec4(params.majorRadius, params.minorRadius, params.smoothness, static_cast<float>(params.sides));

        // Material
        const auto& mat = prim->GetMaterial();
        data.material = glm::vec4(mat.metallic, mat.roughness, mat.emissive, 0.0f);
        data.baseColor = mat.baseColor;
        data.emissiveColor = glm::vec4(mat.emissiveColor, 0.0f);

        // Type and operation
        data.type = static_cast<int>(prim->GetType());
        data.csgOperation = static_cast<int>(prim->GetCSGOperation());
        data.visible = prim->IsVisible() ? 1 : 0;
        data.padding = 0;

        primitivesData.push_back(data);
    }

    m_lastPrimitiveCount = static_cast<int>(primitivesData.size());

    // Upload to GPU
    if (!primitivesData.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_primitivesSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        primitivesData.size() * sizeof(SDFPrimitiveData),
                        primitivesData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void SDFRenderer::SetupUniforms(const Camera& camera) {
    m_raymarchShader->Bind();

    // Camera matrices
    m_raymarchShader->SetMat4("u_view", camera.GetView());
    m_raymarchShader->SetMat4("u_projection", camera.GetProjection());
    m_raymarchShader->SetMat4("u_invView", glm::inverse(camera.GetView()));
    m_raymarchShader->SetMat4("u_invProjection", glm::inverse(camera.GetProjection()));
    m_raymarchShader->SetVec3("u_cameraPos", camera.GetPosition());
    m_raymarchShader->SetVec3("u_cameraDir", camera.GetForward());

    // Raymarching settings
    m_raymarchShader->SetInt("u_maxSteps", m_settings.maxSteps);
    m_raymarchShader->SetFloat("u_maxDistance", m_settings.maxDistance);
    m_raymarchShader->SetFloat("u_hitThreshold", m_settings.hitThreshold);

    // Quality settings
    m_raymarchShader->SetBool("u_enableShadows", m_settings.enableShadows);
    m_raymarchShader->SetBool("u_enableAO", m_settings.enableAO);
    m_raymarchShader->SetBool("u_enableReflections", m_settings.enableReflections);

    // Shadow settings
    m_raymarchShader->SetFloat("u_shadowSoftness", m_settings.shadowSoftness);
    m_raymarchShader->SetInt("u_shadowSteps", m_settings.shadowSteps);

    // AO settings
    m_raymarchShader->SetInt("u_aoSteps", m_settings.aoSteps);
    m_raymarchShader->SetFloat("u_aoDistance", m_settings.aoDistance);
    m_raymarchShader->SetFloat("u_aoIntensity", m_settings.aoIntensity);

    // Lighting
    glm::vec3 normLightDir = glm::normalize(m_settings.lightDirection);
    m_raymarchShader->SetVec3("u_lightDirection", normLightDir);
    m_raymarchShader->SetVec3("u_lightColor", m_settings.lightColor);
    m_raymarchShader->SetFloat("u_lightIntensity", m_settings.lightIntensity);

    // Background
    m_raymarchShader->SetVec3("u_backgroundColor", m_settings.backgroundColor);
    m_raymarchShader->SetBool("u_useEnvironmentMap", m_settings.useEnvironmentMap && m_environmentMap != nullptr);

    // Environment map
    if (m_environmentMap && m_settings.useEnvironmentMap) {
        m_raymarchShader->SetInt("u_environmentMap", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_environmentMap->GetID());
    }

    // Primitive count
    m_raymarchShader->SetInt("u_primitiveCount", m_lastPrimitiveCount);
}

void SDFRenderer::Render(const SDFModel& model, const Camera& camera, const glm::mat4& modelTransform) {
    if (!m_initialized) return;

    // Upload model data
    UploadModelData(model, modelTransform);

    // Setup uniforms
    SetupUniforms(camera);

    // Bind SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_primitivesSSBO);

    // Render fullscreen quad
    glBindVertexArray(m_fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Cleanup
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
}

void SDFRenderer::RenderToTexture(const SDFModel& model, const Camera& camera,
                                   std::shared_ptr<Framebuffer> framebuffer,
                                   const glm::mat4& modelTransform) {
    if (!m_initialized || !framebuffer) return;

    // Bind framebuffer
    framebuffer->Bind();

    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render
    Render(model, camera, modelTransform);

    // Unbind
    framebuffer->Unbind();
}

void SDFRenderer::RenderBatch(const std::vector<const SDFModel*>& models,
                               const std::vector<glm::mat4>& transforms,
                               const Camera& camera) {
    if (!m_initialized || models.empty()) return;

    // For batch rendering, we would need to collect all primitives from all models
    // and upload them together. For now, render sequentially.
    for (size_t i = 0; i < models.size() && i < transforms.size(); ++i) {
        if (models[i]) {
            Render(*models[i], camera, transforms[i]);
        }
    }
}

void SDFRenderer::SetEnvironmentMap(std::shared_ptr<Texture> envMap) {
    m_environmentMap = envMap;
}


// =========================================================================
// Global Illumination Implementation
// =========================================================================

void SDFRenderer::SetRadianceCascade(std::shared_ptr<Nova::RadianceCascade> cascade) {
    m_radianceCascade = cascade;
}

glm::vec3 SDFRenderer::CalculateBlackbodyColor(float temperature) {
    if (temperature < 1000.0f) temperature = 1000.0f;
    if (temperature > 40000.0f) temperature = 40000.0f;

    float t = temperature / 100.0f;
    glm::vec3 color;

    // Red channel
    if (t <= 66.0f) {
        color.r = 1.0f;
    } else {
        color.r = glm::clamp((329.698727446f * std::pow(t - 60.0f, -0.1332047592f)) / 255.0f,
                              0.0f, 1.0f);
    }

    // Green channel
    if (t <= 66.0f) {
        color.g = glm::clamp((99.4708025861f * std::log(t) - 161.1195681661f) / 255.0f,
                              0.0f, 1.0f);
    } else {
        color.g = glm::clamp((288.1221695283f * std::pow(t - 60.0f, -0.0755148492f)) / 255.0f,
                              0.0f, 1.0f);
    }

    // Blue channel
    if (t >= 66.0f) {
        color.b = 1.0f;
    } else if (t <= 19.0f) {
        color.b = 0.0f;
    } else {
        color.b = glm::clamp((138.5177312231f * std::log(t - 10.0f) - 305.0447927307f) / 255.0f,
                              0.0f, 1.0f);
    }

    return color;
}

float SDFRenderer::CalculateBlackbodyIntensity(float temperature) {
    const float sigma = 5.670374419e-8f;  // Stefan-Boltzmann constant
    const float T4 = std::pow(temperature, 4.0f);
    const float sunTemp = 5778.0f;
    const float sunIntensity = sigma * std::pow(sunTemp, 4.0f);

    return (sigma * T4) / sunIntensity;
}


} // namespace Nova
