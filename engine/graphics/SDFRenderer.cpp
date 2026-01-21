#include "SDFRenderer.hpp"
#include "../sdf/SDFModel.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include "../scene/Camera.hpp"
#include "Texture.hpp"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <unordered_map>
#include "RadianceCascade.hpp"
#include "SpectralRenderer.hpp"
#include <cmath>
#include <fstream>
#include <sstream>

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

    // Try to create compute shader (optional - fallback to fragment shader if unavailable)
    {
        std::ifstream file("assets/shaders/sdf_raymarch_compute.comp");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string computeSource = buffer.str();

            m_computeShader = std::make_unique<Shader>();
            if (!m_computeShader->LoadComputeShader(computeSource)) {
                m_computeShader.reset();  // Not fatal, will use fragment shader
            }
        }
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

    // Create SSBOs for BVH (nodes and primitive indices)
    glGenBuffers(1, &m_bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bvhSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 m_maxPrimitives * 2 * sizeof(SDFBVHNodeGPU),  // Worst case: 2N-1 nodes
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &m_bvhPrimitiveIndicesSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bvhPrimitiveIndicesSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 m_maxPrimitives * sizeof(int),
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

    if (m_bvhSSBO) {
        glDeleteBuffers(1, &m_bvhSSBO);
        m_bvhSSBO = 0;
    }

    if (m_bvhPrimitiveIndicesSSBO) {
        glDeleteBuffers(1, &m_bvhPrimitiveIndicesSSBO);
        m_bvhPrimitiveIndicesSSBO = 0;
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
    m_bvh.Clear();

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

    // Build a map from primitive pointer to its index in the output array
    std::unordered_map<const SDFPrimitive*, int> primitiveToIndex;
    int visibleIndex = 0;
    for (const auto* prim : allPrimitives) {
        if (prim->IsVisible()) {
            primitiveToIndex[prim] = visibleIndex++;
        }
    }

    for (const auto* prim : allPrimitives) {
        if (!prim->IsVisible()) continue;

        SDFPrimitiveData data{};

        // World transform - include modelTransform in both forward and inverse
        SDFTransform worldTransform = prim->GetWorldTransform();
        glm::mat4 localMatrix = worldTransform.ToMatrix();
        data.transform = modelTransform * localMatrix;
        data.inverseTransform = glm::inverse(data.transform);

        // Parameters
        const auto& params = prim->GetParameters();
        data.parameters = glm::vec4(params.radius, params.dimensions.x, params.dimensions.y, params.dimensions.z);
        data.parameters2 = glm::vec4(params.height, params.topRadius, params.bottomRadius, params.cornerRadius);
        data.parameters3 = glm::vec4(params.majorRadius, params.minorRadius, params.smoothness, static_cast<float>(params.sides));

        // Onion shell parameters (for clothing layers)
        // parameters4: x=onionThickness, y=shellMinY, z=shellMaxY, w=flags (as float bits)
        data.parameters4 = glm::vec4(
            params.onionThickness,
            params.shellMinY,
            params.shellMaxY,
            *reinterpret_cast<const float*>(&params.flags)  // Reinterpret uint32 as float bits
        );

        // Material
        const auto& mat = prim->GetMaterial();
        data.material = glm::vec4(mat.metallic, mat.roughness, mat.emissive, 0.0f);
        data.baseColor = mat.baseColor;
        data.emissiveColor = glm::vec4(mat.emissiveColor, 0.0f);

        // Type and operation
        data.type = static_cast<int>(prim->GetType());
        data.csgOperation = static_cast<int>(prim->GetCSGOperation());
        data.visible = prim->IsVisible() ? 1 : 0;

        // Parent index for CSG hierarchy (-1 for root primitives)
        const SDFPrimitive* parent = prim->GetParent();
        if (parent && primitiveToIndex.contains(parent)) {
            data.parentIndex = primitiveToIndex[parent];
        } else {
            data.parentIndex = -1;  // Root primitive or parent not visible
        }

        // Compute bounding sphere for early-out optimization
        // Get local bounds and transform to world space
        auto [localMin, localMax] = prim->GetLocalBounds();
        glm::vec3 localCenter = (localMin + localMax) * 0.5f;
        glm::vec3 localExtent = (localMax - localMin) * 0.5f;
        float localRadius = glm::length(localExtent);

        // Transform center to world space (apply full transform chain)
        glm::vec4 worldCenter = data.transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // Scale radius by maximum scale component
        glm::vec3 scaleVec = worldTransform.scale;
        float maxScale = std::max({scaleVec.x, scaleVec.y, scaleVec.z});
        float worldRadius = localRadius * maxScale * 1.1f;  // 10% margin for safety

        data.boundingSphere = glm::vec4(worldCenter.x, worldCenter.y, worldCenter.z, worldRadius);

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

        // Build and upload BVH for scenes with many primitives
        // BVH provides significant speedup for 50+ primitives
        if (m_useBVH && primitivesData.size() >= 16) {
            BuildBVH(primitivesData);
            UploadBVHToGPU();
        } else {
            m_bvhNodeCount = 0;  // Disable BVH for small scenes
        }
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

    // BVH acceleration uniforms
    m_raymarchShader->SetInt("u_bvhNodeCount", m_bvhNodeCount);
    m_raymarchShader->SetBool("u_useBVH", m_useBVH && m_bvhNodeCount > 0);

    // SDF Cache (Brick-Map) uniforms
    m_raymarchShader->SetBool("u_useCachedSDF", m_useCachedSDF && m_cacheTexture3D != 0);
    m_raymarchShader->SetInt("u_cacheResolution", m_cacheResolution);
    m_raymarchShader->SetVec3("u_cacheBoundsMin", m_cacheBoundsMin);
    m_raymarchShader->SetVec3("u_cacheBoundsMax", m_cacheBoundsMax);

    // Bind cache texture to texture unit 1
    if (m_useCachedSDF && m_cacheTexture3D != 0) {
        m_raymarchShader->SetInt("u_sdfCache", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, m_cacheTexture3D);
    }
}

void SDFRenderer::Render(const SDFModel& model, const Camera& camera, const glm::mat4& modelTransform) {
    if (!m_initialized) return;

    // Upload model data
    UploadModelData(model, modelTransform);

    // Setup uniforms
    SetupUniforms(camera);

    // Bind SSBOs
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_primitivesSSBO);

    // Bind BVH SSBOs if acceleration is enabled
    if (m_useBVH && m_bvhNodeCount > 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_bvhSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_bvhPrimitiveIndicesSSBO);
    }

    // Render fullscreen quad
    glBindVertexArray(m_fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Cleanup
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    if (m_useBVH && m_bvhNodeCount > 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    }
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

// =========================================================================
// Compute Shader Rendering
// =========================================================================

void SDFRenderer::RenderCompute(const SDFModel& model, const Camera& camera,
                                 unsigned int outputTexture, int width, int height,
                                 const glm::mat4& modelTransform) {
    if (!m_initialized || !m_computeShader) return;

    // Upload model data
    UploadModelData(model, modelTransform);

    // Bind compute shader
    m_computeShader->Bind();

    // Set camera uniforms
    m_computeShader->SetMat4("u_view", camera.GetView());
    m_computeShader->SetMat4("u_projection", camera.GetProjection());
    m_computeShader->SetMat4("u_invView", glm::inverse(camera.GetView()));
    m_computeShader->SetMat4("u_invProjection", glm::inverse(camera.GetProjection()));
    m_computeShader->SetVec3("u_cameraPos", camera.GetPosition());
    m_computeShader->SetVec3("u_cameraDir", camera.GetForward());
    m_computeShader->SetIVec2("u_resolution", glm::ivec2(width, height));

    // Raymarching settings
    m_computeShader->SetInt("u_maxSteps", m_settings.maxSteps);
    m_computeShader->SetFloat("u_maxDistance", m_settings.maxDistance);
    m_computeShader->SetFloat("u_hitThreshold", m_settings.hitThreshold);

    // Quality settings
    m_computeShader->SetInt("u_enableShadows", m_settings.enableShadows ? 1 : 0);
    m_computeShader->SetInt("u_enableAO", m_settings.enableAO ? 1 : 0);
    m_computeShader->SetInt("u_enableReflections", m_settings.enableReflections ? 1 : 0);

    // Shadow settings
    m_computeShader->SetFloat("u_shadowSoftness", m_settings.shadowSoftness);
    m_computeShader->SetInt("u_shadowSteps", m_settings.shadowSteps);

    // AO settings
    m_computeShader->SetInt("u_aoSteps", m_settings.aoSteps);
    m_computeShader->SetFloat("u_aoDistance", m_settings.aoDistance);
    m_computeShader->SetFloat("u_aoIntensity", m_settings.aoIntensity);

    // Lighting
    glm::vec3 normLightDir = glm::normalize(m_settings.lightDirection);
    m_computeShader->SetVec3("u_lightDirection", normLightDir);
    m_computeShader->SetVec3("u_lightColor", m_settings.lightColor);
    m_computeShader->SetFloat("u_lightIntensity", m_settings.lightIntensity);

    // Background
    m_computeShader->SetVec3("u_backgroundColor", m_settings.backgroundColor);

    // Primitive count
    m_computeShader->SetInt("u_primitiveCount", m_lastPrimitiveCount);

    // Bind SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_primitivesSSBO);

    // Bind output texture as image
    glBindImageTexture(0, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Dispatch compute shader (16x16 work groups)
    int workGroupsX = (width + 15) / 16;
    int workGroupsY = (height + 15) / 16;
    glDispatchCompute(workGroupsX, workGroupsY, 1);

    // Memory barrier to ensure image writes are visible
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Cleanup
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
}

// =========================================================================
// BVH Acceleration
// =========================================================================

void SDFRenderer::BuildBVH(const std::vector<SDFPrimitiveData>& primitives) {
    if (primitives.empty()) {
        m_bvh.Clear();
        m_bvhNodeCount = 0;
        return;
    }

    // Convert primitives to BVH format
    std::vector<SDFBVHPrimitive> bvhPrimitives;
    bvhPrimitives.reserve(primitives.size());

    for (size_t i = 0; i < primitives.size(); ++i) {
        const auto& prim = primitives[i];

        SDFBVHPrimitive bvhPrim;
        bvhPrim.id = static_cast<uint32_t>(i);

        // Create AABB from bounding sphere
        glm::vec3 center(prim.boundingSphere.x, prim.boundingSphere.y, prim.boundingSphere.z);
        float radius = prim.boundingSphere.w;
        bvhPrim.bounds = AABB(center - glm::vec3(radius), center + glm::vec3(radius));
        bvhPrim.centroid = center;
        bvhPrim.primitive = nullptr;  // We use index, not pointer
        bvhPrim.userData = 0;

        bvhPrimitives.push_back(bvhPrim);
    }

    // Build the BVH
    m_bvh.Build(std::move(bvhPrimitives));
    m_bvhNodeCount = static_cast<int>(m_bvh.GetNodeCount());
}

void SDFRenderer::UploadBVHToGPU() {
    if (!m_bvh.IsBuilt() || m_bvhNodeCount == 0) {
        return;
    }

    const auto& nodes = m_bvh.GetNodes();
    const auto& primitives = m_bvh.GetPrimitives();

    // Convert BVH nodes to GPU format
    std::vector<SDFBVHNodeGPU> gpuNodes;
    gpuNodes.reserve(nodes.size());

    for (const auto& node : nodes) {
        SDFBVHNodeGPU gpuNode;
        gpuNode.boundsMin = glm::vec4(node.bounds.min, 0.0f);
        gpuNode.boundsMax = glm::vec4(node.bounds.max, 0.0f);
        gpuNode.leftChild = static_cast<int>(node.GetLeftChild());
        gpuNode.rightChild = static_cast<int>(node.GetRightChild());
        gpuNode.primitiveCount = static_cast<int>(node.GetPrimitiveCount());
        gpuNode.padding = 0;
        gpuNodes.push_back(gpuNode);
    }

    // Upload BVH nodes to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bvhSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    gpuNodes.size() * sizeof(SDFBVHNodeGPU),
                    gpuNodes.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Upload primitive indices (BVH reorders primitives)
    std::vector<int> primitiveIndices;
    primitiveIndices.reserve(primitives.size());
    for (const auto& prim : primitives) {
        primitiveIndices.push_back(static_cast<int>(prim.id));
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bvhPrimitiveIndicesSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    primitiveIndices.size() * sizeof(int),
                    primitiveIndices.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// =========================================================================
// SDF Cache (Brick-Map)
// =========================================================================

void SDFRenderer::SetCacheTexture(unsigned int texture3D, const glm::vec3& boundsMin,
                                   const glm::vec3& boundsMax, int resolution) {
    // Clean up old texture if we own it
    if (m_ownsCacheTexture && m_cacheTexture3D != 0) {
        glDeleteTextures(1, &m_cacheTexture3D);
    }

    m_cacheTexture3D = texture3D;
    m_cacheBoundsMin = boundsMin;
    m_cacheBoundsMax = boundsMax;
    m_cacheResolution = resolution;
    m_ownsCacheTexture = false;  // External texture
    m_useCachedSDF = (texture3D != 0 && resolution > 0);
}

void SDFRenderer::ClearCacheTexture() {
    if (m_ownsCacheTexture && m_cacheTexture3D != 0) {
        glDeleteTextures(1, &m_cacheTexture3D);
    }
    m_cacheTexture3D = 0;
    m_cacheBoundsMin = glm::vec3(0.0f);
    m_cacheBoundsMax = glm::vec3(0.0f);
    m_cacheResolution = 0;
    m_useCachedSDF = false;
    m_ownsCacheTexture = false;
}

void SDFRenderer::BuildCacheFromModel(const SDFModel& model, int resolution) {
    // Get model bounds with padding
    auto [boundsMin, boundsMax] = model.GetBounds();
    glm::vec3 size = boundsMax - boundsMin;
    glm::vec3 padding = size * 0.1f;  // 10% padding
    boundsMin -= padding;
    boundsMax += padding;

    // Allocate 3D texture
    if (m_ownsCacheTexture && m_cacheTexture3D != 0) {
        glDeleteTextures(1, &m_cacheTexture3D);
    }

    glGenTextures(1, &m_cacheTexture3D);
    glBindTexture(GL_TEXTURE_3D, m_cacheTexture3D);

    // Compute SDF values at each voxel
    size_t voxelCount = static_cast<size_t>(resolution) * resolution * resolution;
    std::vector<float> sdfData(voxelCount);

    glm::vec3 voxelSize = (boundsMax - boundsMin) / static_cast<float>(resolution - 1);

    for (int z = 0; z < resolution; ++z) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                glm::vec3 worldPos = boundsMin + glm::vec3(x, y, z) * voxelSize;
                float distance = model.EvaluateSDF(worldPos);

                size_t index = static_cast<size_t>(x) +
                               static_cast<size_t>(y) * resolution +
                               static_cast<size_t>(z) * resolution * resolution;

                sdfData[index] = distance;
            }
        }
    }

    // Upload to GPU
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
                 resolution, resolution, resolution,
                 0, GL_RED, GL_FLOAT, sdfData.data());

    // Set filtering for trilinear interpolation
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);

    // Store cache parameters
    m_cacheBoundsMin = boundsMin;
    m_cacheBoundsMax = boundsMax;
    m_cacheResolution = resolution;
    m_useCachedSDF = true;
    m_ownsCacheTexture = true;
}

} // namespace Nova
