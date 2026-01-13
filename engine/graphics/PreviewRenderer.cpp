#include "PreviewRenderer.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace Nova {

// =============================================================================
// Embedded Shader Sources
// =============================================================================

namespace {

// Simple PBR shader for material/mesh preview
const char* PBR_VERTEX_SHADER = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    mat3 TBN;
} vs_out;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = uNormalMatrix * aNormal;
    vs_out.TexCoords = aTexCoords;

    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 B = normalize(uNormalMatrix * aBitangent);
    vec3 N = normalize(vs_out.Normal);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = uProjection * uView * worldPos;
}
)";

const char* PBR_FRAGMENT_SHADER = R"(
#version 450 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    mat3 TBN;
} fs_in;

out vec4 FragColor;

// Material properties
uniform vec3 uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAO;
uniform vec3 uEmissive;

// Texture maps
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAOMap;

uniform bool uUseAlbedoMap;
uniform bool uUseNormalMap;
uniform bool uUseMetallicMap;
uniform bool uUseRoughnessMap;
uniform bool uUseAOMap;

// Lighting
uniform vec3 uLightDir1;
uniform vec3 uLightColor1;
uniform float uLightIntensity1;
uniform vec3 uLightDir2;
uniform vec3 uLightColor2;
uniform float uLightIntensity2;
uniform vec3 uAmbientColor;

uniform vec3 uCameraPos;

const float PI = 3.14159265359;

// PBR functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculateLight(vec3 N, vec3 V, vec3 L, vec3 lightColor, float intensity,
                    vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * lightColor * intensity * NdotL;
}

void main() {
    // Sample textures or use uniform values
    vec3 albedo = uUseAlbedoMap ? texture(uAlbedoMap, fs_in.TexCoords).rgb : uAlbedo;
    float metallic = uUseMetallicMap ? texture(uMetallicMap, fs_in.TexCoords).r : uMetallic;
    float roughness = uUseRoughnessMap ? texture(uRoughnessMap, fs_in.TexCoords).r : uRoughness;
    float ao = uUseAOMap ? texture(uAOMap, fs_in.TexCoords).r : uAO;

    vec3 N = normalize(fs_in.Normal);
    if (uUseNormalMap) {
        vec3 normalMap = texture(uNormalMap, fs_in.TexCoords).xyz * 2.0 - 1.0;
        N = normalize(fs_in.TBN * normalMap);
    }

    vec3 V = normalize(uCameraPos - fs_in.FragPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Main light
    Lo += CalculateLight(N, V, normalize(-uLightDir1), uLightColor1, uLightIntensity1,
                         albedo, metallic, roughness, F0);

    // Fill light
    Lo += CalculateLight(N, V, normalize(-uLightDir2), uLightColor2, uLightIntensity2,
                         albedo, metallic, roughness, F0);

    // Ambient
    vec3 ambient = uAmbientColor * albedo * ao;

    vec3 color = ambient + Lo + uEmissive;

    // Tone mapping (simple Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
)";

// Grid shader
const char* GRID_VERTEX_SHADER = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;

out vec3 vWorldPos;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vWorldPos = aPosition;
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
}
)";

const char* GRID_FRAGMENT_SHADER = R"(
#version 450 core

in vec3 vWorldPos;
out vec4 FragColor;

uniform vec4 uGridColor;
uniform float uGridSize;
uniform vec3 uCameraPos;

void main() {
    // Compute grid pattern
    vec2 coord = vWorldPos.xz / uGridSize;
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);

    float alpha = 1.0 - min(line, 1.0);

    // Fade based on distance
    float dist = length(vWorldPos.xz - uCameraPos.xz);
    float fade = 1.0 - smoothstep(5.0, 20.0, dist);

    FragColor = vec4(uGridColor.rgb, uGridColor.a * alpha * fade);
}
)";

// Texture preview shader
const char* TEXTURE_VERTEX_SHADER = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aTexCoords;

out vec2 vTexCoords;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vTexCoords = aTexCoords;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)";

const char* TEXTURE_FRAGMENT_SHADER = R"(
#version 450 core

in vec2 vTexCoords;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool uShowAlpha;
uniform vec4 uBackgroundColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoords);

    if (uShowAlpha) {
        // Checkerboard background for alpha visualization
        vec2 checker = floor(vTexCoords * 16.0);
        float c = mod(checker.x + checker.y, 2.0);
        vec3 bg = mix(vec3(0.3), vec3(0.5), c);
        FragColor = vec4(mix(bg, texColor.rgb, texColor.a), 1.0);
    } else {
        FragColor = texColor;
    }
}
)";

// SDF preview shader
const char* SDF_VERTEX_SHADER = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aTexCoords;

out vec2 vTexCoords;
out vec3 vRayOrigin;
out vec3 vRayDir;

uniform mat4 uInvView;
uniform mat4 uInvProjection;

void main() {
    vTexCoords = aTexCoords;

    // Compute ray for this pixel
    vec4 clipPos = vec4(aPosition.xy, -1.0, 1.0);
    vec4 viewPos = uInvProjection * clipPos;
    viewPos = vec4(viewPos.xy, -1.0, 0.0);

    vRayOrigin = (uInvView * vec4(0, 0, 0, 1)).xyz;
    vRayDir = normalize((uInvView * viewPos).xyz);

    gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
)";

const char* SDF_FRAGMENT_SHADER = R"(
#version 450 core

in vec2 vTexCoords;
in vec3 vRayOrigin;
in vec3 vRayDir;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec4 uBackgroundColor;

// SDF evaluation parameters (simplified - in real implementation would use SSBO)
uniform vec3 uSDFCenter;
uniform float uSDFRadius;
uniform vec3 uSDFColor;

const int MAX_STEPS = 128;
const float MIN_DIST = 0.001;
const float MAX_DIST = 100.0;

// Simple sphere SDF for demonstration
float SceneSDF(vec3 p) {
    return length(p - uSDFCenter) - uSDFRadius;
}

vec3 CalculateNormal(vec3 p) {
    const float eps = 0.001;
    return normalize(vec3(
        SceneSDF(vec3(p.x + eps, p.y, p.z)) - SceneSDF(vec3(p.x - eps, p.y, p.z)),
        SceneSDF(vec3(p.x, p.y + eps, p.z)) - SceneSDF(vec3(p.x, p.y - eps, p.z)),
        SceneSDF(vec3(p.x, p.y, p.z + eps)) - SceneSDF(vec3(p.x, p.y, p.z - eps))
    ));
}

void main() {
    vec3 ro = vRayOrigin;
    vec3 rd = normalize(vRayDir);

    float t = 0.0;
    bool hit = false;

    for (int i = 0; i < MAX_STEPS && t < MAX_DIST; i++) {
        vec3 p = ro + rd * t;
        float d = SceneSDF(p);

        if (d < MIN_DIST) {
            hit = true;
            break;
        }

        t += d;
    }

    if (hit) {
        vec3 p = ro + rd * t;
        vec3 N = CalculateNormal(p);
        vec3 L = normalize(-uLightDir);

        float diffuse = max(dot(N, L), 0.0);
        vec3 color = uSDFColor * (uAmbientColor + uLightColor * diffuse);

        // Gamma correction
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, 1.0);
    } else {
        FragColor = uBackgroundColor;
    }
}
)";

} // anonymous namespace

// =============================================================================
// PreviewRenderer Implementation
// =============================================================================

PreviewRenderer::PreviewRenderer() = default;

PreviewRenderer::~PreviewRenderer() {
    if (m_initialized) {
        Shutdown();
    }
}

PreviewRenderer::PreviewRenderer(PreviewRenderer&& other) noexcept
    : m_initialized(other.m_initialized)
    , m_settings(std::move(other.m_settings))
    , m_mesh(std::move(other.m_mesh))
    , m_material(std::move(other.m_material))
    , m_sdfModel(std::move(other.m_sdfModel))
    , m_texture(std::move(other.m_texture))
    , m_customRenderer(std::move(other.m_customRenderer))
    , m_sphereMesh(std::move(other.m_sphereMesh))
    , m_cubeMesh(std::move(other.m_cubeMesh))
    , m_planeMesh(std::move(other.m_planeMesh))
    , m_cylinderMesh(std::move(other.m_cylinderMesh))
    , m_torusMesh(std::move(other.m_torusMesh))
    , m_gridMesh(std::move(other.m_gridMesh))
    , m_quadMesh(std::move(other.m_quadMesh))
    , m_pbrShader(std::move(other.m_pbrShader))
    , m_gridShader(std::move(other.m_gridShader))
    , m_textureShader(std::move(other.m_textureShader))
    , m_sdfShader(std::move(other.m_sdfShader))
    , m_framebuffer(std::move(other.m_framebuffer))
    , m_framebufferWidth(other.m_framebufferWidth)
    , m_framebufferHeight(other.m_framebufferHeight)
    , m_defaultMaterial(std::move(other.m_defaultMaterial))
    , m_orbitYaw(other.m_orbitYaw)
    , m_orbitPitch(other.m_orbitPitch)
    , m_orbitDistance(other.m_orbitDistance)
    , m_isDragging(other.m_isDragging)
    , m_dragButton(other.m_dragButton)
    , m_lastMousePos(other.m_lastMousePos)
    , m_animationTime(other.m_animationTime)
    , m_animationPlaying(other.m_animationPlaying)
    , m_autoRotateAngle(other.m_autoRotateAngle) {
    other.m_initialized = false;
}

PreviewRenderer& PreviewRenderer::operator=(PreviewRenderer&& other) noexcept {
    if (this != &other) {
        if (m_initialized) {
            Shutdown();
        }

        m_initialized = other.m_initialized;
        m_settings = std::move(other.m_settings);
        m_mesh = std::move(other.m_mesh);
        m_material = std::move(other.m_material);
        m_sdfModel = std::move(other.m_sdfModel);
        m_texture = std::move(other.m_texture);
        m_customRenderer = std::move(other.m_customRenderer);
        m_sphereMesh = std::move(other.m_sphereMesh);
        m_cubeMesh = std::move(other.m_cubeMesh);
        m_planeMesh = std::move(other.m_planeMesh);
        m_cylinderMesh = std::move(other.m_cylinderMesh);
        m_torusMesh = std::move(other.m_torusMesh);
        m_gridMesh = std::move(other.m_gridMesh);
        m_quadMesh = std::move(other.m_quadMesh);
        m_pbrShader = std::move(other.m_pbrShader);
        m_gridShader = std::move(other.m_gridShader);
        m_textureShader = std::move(other.m_textureShader);
        m_sdfShader = std::move(other.m_sdfShader);
        m_framebuffer = std::move(other.m_framebuffer);
        m_framebufferWidth = other.m_framebufferWidth;
        m_framebufferHeight = other.m_framebufferHeight;
        m_defaultMaterial = std::move(other.m_defaultMaterial);
        m_orbitYaw = other.m_orbitYaw;
        m_orbitPitch = other.m_orbitPitch;
        m_orbitDistance = other.m_orbitDistance;
        m_isDragging = other.m_isDragging;
        m_dragButton = other.m_dragButton;
        m_lastMousePos = other.m_lastMousePos;
        m_animationTime = other.m_animationTime;
        m_animationPlaying = other.m_animationPlaying;
        m_autoRotateAngle = other.m_autoRotateAngle;

        other.m_initialized = false;
    }
    return *this;
}

void PreviewRenderer::Initialize() {
    if (m_initialized) {
        return;
    }

    CreateShaders();
    CreatePrimitiveMeshes();

    // Create default material
    m_defaultMaterial = std::make_shared<Material>();
    if (m_pbrShader) {
        m_defaultMaterial->SetShader(std::shared_ptr<Shader>(m_pbrShader.get(), [](Shader*) {}));
    }
    m_defaultMaterial->SetAlbedo(glm::vec3(0.8f, 0.8f, 0.8f));
    m_defaultMaterial->SetMetallic(0.0f);
    m_defaultMaterial->SetRoughness(0.5f);

    m_initialized = true;
}

void PreviewRenderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clear content
    m_mesh.reset();
    m_material.reset();
    m_sdfModel.reset();
    m_texture.reset();
    m_customRenderer = nullptr;

    // Clear primitive meshes
    m_sphereMesh.reset();
    m_cubeMesh.reset();
    m_planeMesh.reset();
    m_cylinderMesh.reset();
    m_torusMesh.reset();
    m_gridMesh.reset();
    m_quadMesh.reset();

    // Clear shaders
    m_pbrShader.reset();
    m_gridShader.reset();
    m_textureShader.reset();
    m_sdfShader.reset();

    // Clear framebuffer
    m_framebuffer.reset();

    // Clear default material
    m_defaultMaterial.reset();

    m_initialized = false;
}

void PreviewRenderer::CreateShaders() {
    // PBR shader
    m_pbrShader = std::make_unique<Shader>();
    if (!m_pbrShader->LoadFromSource(PBR_VERTEX_SHADER, PBR_FRAGMENT_SHADER)) {
        m_pbrShader.reset();
    }

    // Grid shader
    m_gridShader = std::make_unique<Shader>();
    if (!m_gridShader->LoadFromSource(GRID_VERTEX_SHADER, GRID_FRAGMENT_SHADER)) {
        m_gridShader.reset();
    }

    // Texture shader
    m_textureShader = std::make_unique<Shader>();
    if (!m_textureShader->LoadFromSource(TEXTURE_VERTEX_SHADER, TEXTURE_FRAGMENT_SHADER)) {
        m_textureShader.reset();
    }

    // SDF shader
    m_sdfShader = std::make_unique<Shader>();
    if (!m_sdfShader->LoadFromSource(SDF_VERTEX_SHADER, SDF_FRAGMENT_SHADER)) {
        m_sdfShader.reset();
    }
}

void PreviewRenderer::CreatePrimitiveMeshes() {
    // Create standard primitive meshes
    m_sphereMesh = Mesh::CreateSphere(1.0f, 32);
    m_cubeMesh = Mesh::CreateCube(1.0f);
    m_planeMesh = Mesh::CreatePlane(2.0f, 2.0f, 1, 1);
    m_cylinderMesh = Mesh::CreateCylinder(0.5f, 1.0f, 32);
    m_torusMesh = Mesh::CreateTorus(0.3f, 0.7f, 32, 32);

    // Create grid mesh (large plane)
    m_gridMesh = Mesh::CreatePlane(50.0f, 50.0f, 50, 50);

    // Create full-screen quad for texture/SDF preview
    std::vector<Vertex> quadVertices = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}
    };
    std::vector<uint32_t> quadIndices = {0, 1, 2, 2, 3, 0};

    m_quadMesh = std::make_unique<Mesh>();
    m_quadMesh->Create(quadVertices, quadIndices);
}

void PreviewRenderer::SetSettings(const PreviewSettings& settings) {
    m_settings = settings;

    // Update orbit from camera settings
    glm::vec3 offset = m_settings.camera.position - m_settings.camera.target;
    m_orbitDistance = glm::length(offset);
    if (m_orbitDistance > 0.001f) {
        offset /= m_orbitDistance;
        m_orbitPitch = std::asin(std::clamp(offset.y, -1.0f, 1.0f));
        m_orbitYaw = std::atan2(offset.x, offset.z);
    }
}

void PreviewRenderer::SetMesh(std::shared_ptr<Mesh> mesh) {
    m_mesh = std::move(mesh);
    if (OnPreviewUpdated) {
        OnPreviewUpdated();
    }
}

void PreviewRenderer::SetMaterial(std::shared_ptr<Material> material) {
    m_material = std::move(material);
    if (OnPreviewUpdated) {
        OnPreviewUpdated();
    }
}

void PreviewRenderer::SetSDF(std::shared_ptr<SDFModel> sdf) {
    m_sdfModel = std::move(sdf);
    if (OnPreviewUpdated) {
        OnPreviewUpdated();
    }
}

void PreviewRenderer::SetTexture(std::shared_ptr<Texture> texture) {
    m_texture = std::move(texture);
    if (OnPreviewUpdated) {
        OnPreviewUpdated();
    }
}

void PreviewRenderer::SetCustomRenderer(CustomRenderCallback callback) {
    m_customRenderer = std::move(callback);
    if (OnPreviewUpdated) {
        OnPreviewUpdated();
    }
}

void PreviewRenderer::ClearContent() {
    m_mesh.reset();
    m_material.reset();
    m_sdfModel.reset();
    m_texture.reset();
    m_customRenderer = nullptr;
    if (OnPreviewUpdated) {
        OnPreviewUpdated();
    }
}

void PreviewRenderer::EnsureFramebuffer(int width, int height) {
    if (m_framebuffer && m_framebufferWidth == width && m_framebufferHeight == height) {
        return;
    }

    m_framebuffer = std::make_unique<Framebuffer>();
    m_framebuffer->Create(width, height, 1, true);
    m_framebufferWidth = width;
    m_framebufferHeight = height;
}

void PreviewRenderer::Render(const glm::ivec2& size) {
    if (!m_initialized || size.x <= 0 || size.y <= 0) {
        return;
    }

    EnsureFramebuffer(size.x, size.y);

    m_framebuffer->Bind();
    m_framebuffer->Clear(m_settings.environment.backgroundColor);

    RenderInternal(size);

    Framebuffer::Unbind();
}

std::shared_ptr<Texture> PreviewRenderer::RenderToTexture(int size) {
    return RenderToTexture(size, size);
}

std::shared_ptr<Texture> PreviewRenderer::RenderToTexture(int width, int height) {
    if (!m_initialized || width <= 0 || height <= 0) {
        return nullptr;
    }

    EnsureFramebuffer(width, height);

    m_framebuffer->Bind();
    m_framebuffer->Clear(m_settings.environment.backgroundColor);

    RenderInternal({width, height});

    Framebuffer::Unbind();

    return m_framebuffer->GetColorAttachment(0);
}

uint32_t PreviewRenderer::GetPreviewTextureID() const {
    if (m_framebuffer) {
        auto texture = m_framebuffer->GetColorAttachment(0);
        if (texture) {
            return texture->GetID();
        }
    }
    return 0;
}

void PreviewRenderer::RenderInternal(const glm::ivec2& size) {
    // Set viewport
    glViewport(0, 0, size.x, size.y);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(size.x) / static_cast<float>(size.y);

    // Get view and projection matrices
    glm::mat4 view = m_settings.camera.GetViewMatrix();
    glm::mat4 projection = m_settings.camera.GetProjectionMatrix(aspectRatio);

    // Render based on mode
    switch (m_settings.mode) {
        case PreviewMode::Material:
            RenderMaterial(view, projection);
            break;

        case PreviewMode::Mesh:
            RenderMesh(view, projection);
            break;

        case PreviewMode::SDF:
            RenderSDF(view, projection);
            break;

        case PreviewMode::Texture:
            RenderTexture(view, projection);
            break;

        case PreviewMode::Animation:
            // For now, treat as mesh with animation time applied
            RenderMesh(view, projection);
            break;

        case PreviewMode::Custom:
            if (m_customRenderer) {
                m_customRenderer(view, projection, size);
            }
            break;
    }

    // Render grid if enabled
    if (m_settings.environment.showGrid && m_settings.mode != PreviewMode::Texture) {
        RenderGrid(view, projection);
    }
}

void PreviewRenderer::RenderMaterial(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_pbrShader) {
        return;
    }

    // Select shape mesh
    Mesh* shapeMesh = nullptr;
    switch (m_settings.shape) {
        case PreviewShape::Sphere:   shapeMesh = m_sphereMesh.get(); break;
        case PreviewShape::Cube:     shapeMesh = m_cubeMesh.get(); break;
        case PreviewShape::Plane:    shapeMesh = m_planeMesh.get(); break;
        case PreviewShape::Cylinder: shapeMesh = m_cylinderMesh.get(); break;
        case PreviewShape::Torus:    shapeMesh = m_torusMesh.get(); break;
        case PreviewShape::Custom:   shapeMesh = m_mesh.get(); break;
    }

    if (!shapeMesh) {
        shapeMesh = m_sphereMesh.get();
    }

    // Set up model matrix with auto-rotation
    glm::mat4 model = glm::mat4(1.0f);
    if (m_settings.interaction.autoRotate) {
        model = glm::rotate(model, m_autoRotateAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    // Bind shader and set uniforms
    m_pbrShader->Bind();
    m_pbrShader->SetMat4("uModel", model);
    m_pbrShader->SetMat4("uView", view);
    m_pbrShader->SetMat4("uProjection", projection);
    m_pbrShader->SetMat3("uNormalMatrix", normalMatrix);
    m_pbrShader->SetVec3("uCameraPos", m_settings.camera.position);

    // Lighting
    m_pbrShader->SetVec3("uLightDir1", m_settings.mainLight.direction);
    m_pbrShader->SetVec3("uLightColor1", m_settings.mainLight.color);
    m_pbrShader->SetFloat("uLightIntensity1", m_settings.mainLight.intensity);
    m_pbrShader->SetVec3("uLightDir2", m_settings.fillLight.direction);
    m_pbrShader->SetVec3("uLightColor2", m_settings.fillLight.color);
    m_pbrShader->SetFloat("uLightIntensity2", m_settings.fillLight.intensity);
    m_pbrShader->SetVec3("uAmbientColor", glm::vec3(m_settings.environment.ambientColor));

    // Material properties
    Material* mat = m_material ? m_material.get() : m_defaultMaterial.get();
    if (mat) {
        mat->Bind();
    } else {
        // Use default values
        m_pbrShader->SetVec3("uAlbedo", glm::vec3(0.8f));
        m_pbrShader->SetFloat("uMetallic", 0.0f);
        m_pbrShader->SetFloat("uRoughness", 0.5f);
        m_pbrShader->SetFloat("uAO", 1.0f);
        m_pbrShader->SetVec3("uEmissive", glm::vec3(0.0f));

        // Disable texture maps
        m_pbrShader->SetBool("uUseAlbedoMap", false);
        m_pbrShader->SetBool("uUseNormalMap", false);
        m_pbrShader->SetBool("uUseMetallicMap", false);
        m_pbrShader->SetBool("uUseRoughnessMap", false);
        m_pbrShader->SetBool("uUseAOMap", false);
    }

    shapeMesh->Draw();

    Shader::Unbind();
}

void PreviewRenderer::RenderMesh(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_mesh || !m_pbrShader) {
        // If no custom mesh, fall back to material preview
        RenderMaterial(view, projection);
        return;
    }

    // Calculate model matrix to center and scale mesh
    glm::vec3 boundsMin = m_mesh->GetBoundsMin();
    glm::vec3 boundsMax = m_mesh->GetBoundsMax();
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::vec3 size = boundsMax - boundsMin;
    float maxDim = std::max({size.x, size.y, size.z, 0.001f});
    float scale = 2.0f / maxDim;

    glm::mat4 model = glm::mat4(1.0f);
    if (m_settings.interaction.autoRotate) {
        model = glm::rotate(model, m_autoRotateAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    model = glm::scale(model, glm::vec3(scale));
    model = glm::translate(model, -center);

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    // Bind shader
    m_pbrShader->Bind();
    m_pbrShader->SetMat4("uModel", model);
    m_pbrShader->SetMat4("uView", view);
    m_pbrShader->SetMat4("uProjection", projection);
    m_pbrShader->SetMat3("uNormalMatrix", normalMatrix);
    m_pbrShader->SetVec3("uCameraPos", m_settings.camera.position);

    // Lighting
    m_pbrShader->SetVec3("uLightDir1", m_settings.mainLight.direction);
    m_pbrShader->SetVec3("uLightColor1", m_settings.mainLight.color);
    m_pbrShader->SetFloat("uLightIntensity1", m_settings.mainLight.intensity);
    m_pbrShader->SetVec3("uLightDir2", m_settings.fillLight.direction);
    m_pbrShader->SetVec3("uLightColor2", m_settings.fillLight.color);
    m_pbrShader->SetFloat("uLightIntensity2", m_settings.fillLight.intensity);
    m_pbrShader->SetVec3("uAmbientColor", glm::vec3(m_settings.environment.ambientColor));

    // Material
    Material* mat = m_material ? m_material.get() : m_defaultMaterial.get();
    if (mat) {
        mat->Bind();
    } else {
        m_pbrShader->SetVec3("uAlbedo", glm::vec3(0.8f));
        m_pbrShader->SetFloat("uMetallic", 0.0f);
        m_pbrShader->SetFloat("uRoughness", 0.5f);
        m_pbrShader->SetFloat("uAO", 1.0f);
        m_pbrShader->SetVec3("uEmissive", glm::vec3(0.0f));
        m_pbrShader->SetBool("uUseAlbedoMap", false);
        m_pbrShader->SetBool("uUseNormalMap", false);
        m_pbrShader->SetBool("uUseMetallicMap", false);
        m_pbrShader->SetBool("uUseRoughnessMap", false);
        m_pbrShader->SetBool("uUseAOMap", false);
    }

    m_mesh->Draw();

    Shader::Unbind();
}

void PreviewRenderer::RenderSDF(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_sdfShader || !m_quadMesh) {
        return;
    }

    // Disable depth testing for full-screen quad
    glDisable(GL_DEPTH_TEST);

    // Compute inverse matrices for ray generation
    glm::mat4 invView = glm::inverse(view);
    glm::mat4 invProjection = glm::inverse(projection);

    m_sdfShader->Bind();
    m_sdfShader->SetMat4("uInvView", invView);
    m_sdfShader->SetMat4("uInvProjection", invProjection);

    // Lighting
    m_sdfShader->SetVec3("uLightDir", m_settings.mainLight.direction);
    m_sdfShader->SetVec3("uLightColor", m_settings.mainLight.color);
    m_sdfShader->SetVec3("uAmbientColor", glm::vec3(m_settings.environment.ambientColor));
    m_sdfShader->SetVec4("uBackgroundColor", m_settings.environment.backgroundColor);

    // SDF parameters (simplified - in real implementation would pass full SDF data)
    if (m_sdfModel) {
        auto bounds = m_sdfModel->GetBounds();
        glm::vec3 center = (bounds.first + bounds.second) * 0.5f;
        float radius = glm::length(bounds.second - bounds.first) * 0.5f;

        m_sdfShader->SetVec3("uSDFCenter", center);
        m_sdfShader->SetFloat("uSDFRadius", radius);
        m_sdfShader->SetVec3("uSDFColor", glm::vec3(0.8f, 0.8f, 0.8f));
    } else {
        // Default sphere
        m_sdfShader->SetVec3("uSDFCenter", glm::vec3(0.0f));
        m_sdfShader->SetFloat("uSDFRadius", 1.0f);
        m_sdfShader->SetVec3("uSDFColor", glm::vec3(0.8f, 0.8f, 0.8f));
    }

    m_quadMesh->Draw();

    Shader::Unbind();

    // Re-enable depth testing
    glEnable(GL_DEPTH_TEST);
}

void PreviewRenderer::RenderTexture(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_textureShader || !m_quadMesh || !m_texture) {
        return;
    }

    glDisable(GL_DEPTH_TEST);

    m_textureShader->Bind();

    // For texture preview, use orthographic projection centered on quad
    glm::mat4 model = glm::mat4(1.0f);

    // Scale to maintain aspect ratio
    float texAspect = static_cast<float>(m_texture->GetWidth()) / static_cast<float>(m_texture->GetHeight());
    if (texAspect > 1.0f) {
        model = glm::scale(model, glm::vec3(1.0f, 1.0f / texAspect, 1.0f));
    } else {
        model = glm::scale(model, glm::vec3(texAspect, 1.0f, 1.0f));
    }

    m_textureShader->SetMat4("uModel", model);
    m_textureShader->SetMat4("uView", view);
    m_textureShader->SetMat4("uProjection", projection);
    m_textureShader->SetBool("uShowAlpha", true);
    m_textureShader->SetVec4("uBackgroundColor", m_settings.environment.backgroundColor);

    m_texture->Bind(0);
    m_textureShader->SetInt("uTexture", 0);

    m_quadMesh->Draw();

    Shader::Unbind();
    glEnable(GL_DEPTH_TEST);
}

void PreviewRenderer::RenderGrid(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_gridShader || !m_gridMesh) {
        return;
    }

    // Enable blending for transparent grid
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_gridShader->Bind();
    m_gridShader->SetMat4("uView", view);
    m_gridShader->SetMat4("uProjection", projection);
    m_gridShader->SetVec4("uGridColor", m_settings.environment.gridColor);
    m_gridShader->SetFloat("uGridSize", m_settings.environment.gridSize);
    m_gridShader->SetVec3("uCameraPos", m_settings.camera.position);

    m_gridMesh->Draw();

    Shader::Unbind();
    glDisable(GL_BLEND);
}

void PreviewRenderer::RenderBackground() {
    // Currently handled by framebuffer clear
    // Could be extended to render gradient or environment map
}

bool PreviewRenderer::HandleInput(const PreviewInputEvent& event) {
    switch (event.type) {
        case PreviewInputEvent::Type::MouseDown:
            m_isDragging = true;
            m_dragButton = event.button;
            m_lastMousePos = event.position;
            return true;

        case PreviewInputEvent::Type::MouseUp:
            m_isDragging = false;
            return true;

        case PreviewInputEvent::Type::MouseMove:
        case PreviewInputEvent::Type::MouseDrag:
            if (m_isDragging) {
                glm::vec2 delta = event.position - m_lastMousePos;
                m_lastMousePos = event.position;

                if (m_dragButton == 0 && m_settings.interaction.enableRotation) {
                    // Left button: rotate
                    OrbitCamera(delta.x * m_settings.interaction.rotationSensitivity,
                               delta.y * m_settings.interaction.rotationSensitivity);
                } else if (m_dragButton == 1 && m_settings.interaction.enablePan) {
                    // Right button: pan
                    PanCamera(delta * m_settings.interaction.panSensitivity);
                } else if (m_dragButton == 2 && m_settings.interaction.enableZoom) {
                    // Middle button: zoom
                    ZoomCamera(-delta.y * m_settings.interaction.zoomSensitivity);
                }
                return true;
            }
            break;

        case PreviewInputEvent::Type::Scroll:
            if (m_settings.interaction.enableZoom) {
                ZoomCamera(event.scrollDelta * m_settings.interaction.zoomSensitivity);
                return true;
            }
            break;

        case PreviewInputEvent::Type::KeyDown:
            // Reset camera on 'R' key
            if (event.key == 'R' || event.key == 'r') {
                ResetCamera();
                return true;
            }
            // Focus on content on 'F' key
            if (event.key == 'F' || event.key == 'f') {
                FocusOnContent();
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void PreviewRenderer::Update(float deltaTime) {
    // Handle auto-rotation
    if (m_settings.interaction.autoRotate) {
        m_autoRotateAngle += m_settings.interaction.autoRotateSpeed * deltaTime;
        if (m_autoRotateAngle > glm::two_pi<float>()) {
            m_autoRotateAngle -= glm::two_pi<float>();
        }
    }

    // Handle animation playback
    if (m_animationPlaying) {
        m_animationTime += deltaTime;
    }
}

void PreviewRenderer::ResetCamera() {
    m_orbitYaw = 0.0f;
    m_orbitPitch = 0.3f;
    m_orbitDistance = 3.0f;
    m_settings.camera.target = glm::vec3(0.0f);
    UpdateCameraFromOrbit();
}

void PreviewRenderer::FocusOnContent() {
    glm::vec3 center = GetContentCenter();
    float radius = GetContentRadius();

    m_settings.camera.target = center;
    m_orbitDistance = std::max(radius * 2.5f, m_settings.interaction.minDistance);
    m_orbitDistance = std::min(m_orbitDistance, m_settings.interaction.maxDistance);

    UpdateCameraFromOrbit();
}

void PreviewRenderer::OrbitCamera(float deltaYaw, float deltaPitch) {
    m_orbitYaw += deltaYaw;
    m_orbitPitch = std::clamp(m_orbitPitch - deltaPitch, -glm::half_pi<float>() + 0.1f,
                              glm::half_pi<float>() - 0.1f);
    UpdateCameraFromOrbit();
}

void PreviewRenderer::ZoomCamera(float delta) {
    m_orbitDistance -= delta;
    m_orbitDistance = std::clamp(m_orbitDistance, m_settings.interaction.minDistance,
                                 m_settings.interaction.maxDistance);
    UpdateCameraFromOrbit();
}

void PreviewRenderer::PanCamera(const glm::vec2& delta) {
    // Calculate right and up vectors in camera space
    glm::vec3 forward = glm::normalize(m_settings.camera.target - m_settings.camera.position);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_settings.camera.up));
    glm::vec3 up = glm::cross(right, forward);

    // Move target
    m_settings.camera.target -= right * delta.x + up * delta.y;
    UpdateCameraFromOrbit();
}

void PreviewRenderer::SetCameraDistance(float distance) {
    m_orbitDistance = std::clamp(distance, m_settings.interaction.minDistance,
                                 m_settings.interaction.maxDistance);
    UpdateCameraFromOrbit();
}

float PreviewRenderer::GetCameraDistance() const {
    return m_orbitDistance;
}

void PreviewRenderer::UpdateCameraFromOrbit() {
    // Calculate camera position from orbit parameters
    float x = std::sin(m_orbitYaw) * std::cos(m_orbitPitch);
    float y = std::sin(m_orbitPitch);
    float z = std::cos(m_orbitYaw) * std::cos(m_orbitPitch);

    glm::vec3 offset = glm::vec3(x, y, z) * m_orbitDistance;
    m_settings.camera.position = m_settings.camera.target + offset;
}

glm::vec3 PreviewRenderer::GetContentCenter() const {
    if (m_mesh) {
        return (m_mesh->GetBoundsMin() + m_mesh->GetBoundsMax()) * 0.5f;
    }
    if (m_sdfModel) {
        auto bounds = m_sdfModel->GetBounds();
        return (bounds.first + bounds.second) * 0.5f;
    }
    return glm::vec3(0.0f);
}

float PreviewRenderer::GetContentRadius() const {
    if (m_mesh) {
        glm::vec3 size = m_mesh->GetBoundsMax() - m_mesh->GetBoundsMin();
        return glm::length(size) * 0.5f;
    }
    if (m_sdfModel) {
        auto bounds = m_sdfModel->GetBounds();
        return glm::length(bounds.second - bounds.first) * 0.5f;
    }
    return 1.0f;
}

// =============================================================================
// Legacy Wrapper Implementations
// =============================================================================

MaterialGraphPreviewRendererWrapper::MaterialGraphPreviewRendererWrapper()
    : m_renderer(std::make_unique<PreviewRenderer>()) {
}

MaterialGraphPreviewRendererWrapper::~MaterialGraphPreviewRendererWrapper() = default;

void MaterialGraphPreviewRendererWrapper::Initialize() {
    m_renderer->Initialize();
    m_renderer->SetSettings(PreviewSettings::MaterialPreview());
}

void MaterialGraphPreviewRendererWrapper::Render(std::shared_ptr<class MaterialGraph> /*graph*/) {
    // In a full implementation, would compile graph to material
    // For now, just render with current material
    PreviewSettings settings = m_renderer->GetSettings();

    // Map legacy shape to new enum
    switch (previewShape) {
        case PreviewShape::Sphere:   settings.shape = Nova::PreviewShape::Sphere; break;
        case PreviewShape::Cube:     settings.shape = Nova::PreviewShape::Cube; break;
        case PreviewShape::Plane:    settings.shape = Nova::PreviewShape::Plane; break;
        case PreviewShape::Cylinder: settings.shape = Nova::PreviewShape::Cylinder; break;
        case PreviewShape::Torus:    settings.shape = Nova::PreviewShape::Torus; break;
    }

    settings.mainLight.intensity = lightIntensity;
    settings.mainLight.color = lightColor;
    settings.interaction.autoRotate = autoRotate;

    m_renderer->SetSettings(settings);

    if (autoRotate) {
        rotation += 0.016f; // Approximate 60fps
    }

    m_renderer->Render({m_width, m_height});
}

void MaterialGraphPreviewRendererWrapper::Resize(int width, int height) {
    m_width = width;
    m_height = height;
}

unsigned int MaterialGraphPreviewRendererWrapper::GetPreviewTexture() const {
    return m_renderer->GetPreviewTextureID();
}

BuildingPreviewRendererWrapper::BuildingPreviewRendererWrapper()
    : m_renderer(std::make_unique<PreviewRenderer>()) {
}

BuildingPreviewRendererWrapper::~BuildingPreviewRendererWrapper() = default;

void BuildingPreviewRendererWrapper::Initialize() {
    m_renderer->Initialize();
    m_renderer->SetSettings(PreviewSettings::MeshPreview());
}

void BuildingPreviewRendererWrapper::RenderBuildingPreview(std::shared_ptr<Mesh> buildingMesh,
                                                            std::shared_ptr<Material> material) {
    m_renderer->SetMesh(buildingMesh);
    m_renderer->SetMaterial(material);
    m_renderer->Render({m_width, m_height});
}

void BuildingPreviewRendererWrapper::Resize(int width, int height) {
    m_width = width;
    m_height = height;
}

unsigned int BuildingPreviewRendererWrapper::GetPreviewTexture() const {
    return m_renderer->GetPreviewTextureID();
}

TemplatePreviewRendererWrapper::TemplatePreviewRendererWrapper()
    : m_renderer(std::make_unique<PreviewRenderer>()) {
}

TemplatePreviewRendererWrapper::~TemplatePreviewRendererWrapper() = default;

void TemplatePreviewRendererWrapper::Initialize() {
    m_renderer->Initialize();
    m_renderer->SetSettings(PreviewSettings::TexturePreview());
}

void TemplatePreviewRendererWrapper::RenderPreview(std::shared_ptr<Texture> texture) {
    m_renderer->SetTexture(texture);
    m_renderer->Render({m_width, m_height});
}

void TemplatePreviewRendererWrapper::Resize(int width, int height) {
    m_width = width;
    m_height = height;
}

unsigned int TemplatePreviewRendererWrapper::GetPreviewTexture() const {
    return m_renderer->GetPreviewTextureID();
}

} // namespace Nova
