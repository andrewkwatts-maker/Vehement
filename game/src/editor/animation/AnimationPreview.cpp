#include "AnimationPreview.hpp"
#include "engine/graphics/Framebuffer.hpp"
#include "engine/graphics/Shader.hpp"
#include "engine/graphics/Mesh.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Vehement {

AnimationPreview::AnimationPreview() = default;
AnimationPreview::~AnimationPreview() {
    Shutdown();
}

bool AnimationPreview::Initialize(const Config& config) {
    m_config = config;

    // Create framebuffer
    // m_framebuffer = std::make_unique<Nova::Framebuffer>();
    // m_framebuffer->Create(config.viewportWidth, config.viewportHeight, config.msaaSamples);

    // Load shaders
    // m_skeletonShader = std::make_unique<Nova::Shader>();
    // m_meshShader = std::make_unique<Nova::Shader>();
    // m_gridShader = std::make_unique<Nova::Shader>();

    ResetSettings();
    ResetCamera();
    UpdateCameraMatrices();

    m_initialized = true;
    return true;
}

void AnimationPreview::Shutdown() {
    UnloadMesh();
    m_framebuffer.reset();
    m_skeletonShader.reset();
    m_meshShader.reset();
    m_gridShader.reset();
    m_initialized = false;
}

// ============================================================================
// Mesh Loading
// ============================================================================

bool AnimationPreview::LoadMesh(const std::string& /*meshPath*/) {
    // Would load mesh from file
    // m_mesh = Nova::ModelLoader::LoadMesh(meshPath);
    return m_mesh != nullptr;
}

void AnimationPreview::UnloadMesh() {
    // Would delete mesh
    m_mesh = nullptr;
}

// ============================================================================
// Rendering
// ============================================================================

void AnimationPreview::Update(float deltaTime) {
    if (!m_initialized) return;

    // Auto-update from timeline
    if (m_autoUpdateFromTimeline && m_timeline && m_keyframeEditor) {
        m_keyframeEditor->SampleAnimation(m_timeline->GetCurrentTime());
    }

    // Update bone editor
    if (m_boneEditor) {
        m_boneEditor->Update(deltaTime);
    }
}

void AnimationPreview::Render() {
    if (!m_initialized) return;

    // Bind framebuffer
    // m_framebuffer->Bind();

    // Clear
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render background
    RenderBackground();

    // Render ground plane
    if (m_settings.showGroundPlane) {
        RenderGroundPlane();
    }

    // Render mesh
    if (m_settings.showMesh && m_mesh) {
        RenderMesh();
    }

    // Render skeleton
    if (m_settings.showSkeleton && m_boneEditor) {
        RenderSkeleton();
    }

    // Render gizmo
    if (m_showGizmo && m_boneEditor && !m_boneEditor->GetPrimarySelection().empty()) {
        RenderGizmo();
    }

    // Unbind framebuffer
    // m_framebuffer->Unbind();
}

unsigned int AnimationPreview::GetRenderedTextureId() const {
    // return m_framebuffer ? m_framebuffer->GetColorTexture() : 0;
    return 0;
}

void AnimationPreview::Resize(int width, int height) {
    m_config.viewportWidth = width;
    m_config.viewportHeight = height;

    // Recreate framebuffer
    // if (m_framebuffer) {
    //     m_framebuffer->Resize(width, height);
    // }

    UpdateCameraMatrices();
}

// ============================================================================
// Render Settings
// ============================================================================

void AnimationPreview::ApplySettings(const PreviewRenderSettings& settings) {
    m_settings = settings;
}

void AnimationPreview::ResetSettings() {
    m_settings = PreviewRenderSettings();
}

void AnimationPreview::ApplyLightingPreset(LightingPreset preset) {
    switch (preset) {
        case LightingPreset::Default:
            m_settings.lightDirection = glm::vec3(-0.5f, 1.0f, 0.5f);
            m_settings.lightColor = glm::vec4(1.0f, 0.98f, 0.95f, 1.0f);
            m_settings.lightIntensity = 1.0f;
            m_settings.ambientColor = glm::vec4(0.2f, 0.2f, 0.25f, 1.0f);
            break;

        case LightingPreset::Studio:
            m_settings.lightDirection = glm::vec3(0.3f, 1.0f, 0.8f);
            m_settings.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            m_settings.lightIntensity = 1.2f;
            m_settings.ambientColor = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f);
            break;

        case LightingPreset::Outdoor:
            m_settings.lightDirection = glm::vec3(-0.2f, 1.0f, 0.3f);
            m_settings.lightColor = glm::vec4(1.0f, 0.95f, 0.85f, 1.0f);
            m_settings.lightIntensity = 1.5f;
            m_settings.ambientColor = glm::vec4(0.4f, 0.45f, 0.5f, 1.0f);
            break;

        case LightingPreset::Dramatic:
            m_settings.lightDirection = glm::vec3(-1.0f, 0.5f, 0.2f);
            m_settings.lightColor = glm::vec4(1.0f, 0.9f, 0.8f, 1.0f);
            m_settings.lightIntensity = 2.0f;
            m_settings.ambientColor = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f);
            break;

        case LightingPreset::Flat:
            m_settings.lightDirection = glm::vec3(0.0f, 1.0f, 0.0f);
            m_settings.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            m_settings.lightIntensity = 0.5f;
            m_settings.ambientColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            break;

        case LightingPreset::Custom:
            // Keep current settings
            break;
    }

    m_settings.lightingPreset = preset;
}

void AnimationPreview::ApplyBackgroundPreset(BackgroundStyle style) {
    m_settings.backgroundStyle = style;

    switch (style) {
        case BackgroundStyle::SolidColor:
            m_settings.backgroundColor = glm::vec4(0.2f, 0.2f, 0.25f, 1.0f);
            break;

        case BackgroundStyle::Gradient:
            m_settings.backgroundGradientTop = glm::vec4(0.25f, 0.25f, 0.35f, 1.0f);
            m_settings.backgroundGradientBottom = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f);
            break;

        case BackgroundStyle::Checkerboard:
            m_settings.backgroundColor = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f);
            break;

        default:
            break;
    }
}

// ============================================================================
// Camera Controls
// ============================================================================

void AnimationPreview::ApplyCameraPreset(CameraPreset preset) {
    switch (preset) {
        case CameraPreset::Front:
            m_camera.azimuth = 0.0f;
            m_camera.elevation = 0.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Back:
            m_camera.azimuth = 180.0f;
            m_camera.elevation = 0.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Left:
            m_camera.azimuth = 90.0f;
            m_camera.elevation = 0.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Right:
            m_camera.azimuth = -90.0f;
            m_camera.elevation = 0.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Top:
            m_camera.azimuth = 0.0f;
            m_camera.elevation = 89.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Bottom:
            m_camera.azimuth = 0.0f;
            m_camera.elevation = -89.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Perspective:
            m_camera.azimuth = 30.0f;
            m_camera.elevation = 20.0f;
            m_camera.distance = 5.0f;
            break;

        case CameraPreset::Custom:
            break;
    }

    UpdateCameraMatrices();
}

void AnimationPreview::OrbitCamera(float deltaAzimuth, float deltaElevation) {
    m_camera.azimuth += deltaAzimuth;
    m_camera.elevation = std::clamp(
        m_camera.elevation + deltaElevation,
        m_camera.minElevation,
        m_camera.maxElevation
    );

    // Normalize azimuth
    while (m_camera.azimuth > 180.0f) m_camera.azimuth -= 360.0f;
    while (m_camera.azimuth < -180.0f) m_camera.azimuth += 360.0f;

    UpdateCameraMatrices();

    if (OnViewChanged) {
        OnViewChanged();
    }
}

void AnimationPreview::PanCamera(float deltaX, float deltaY) {
    // Calculate right and up vectors
    glm::vec3 forward = glm::normalize(m_camera.target - m_camera.position);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_camera.up));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    float panSpeed = m_camera.distance * 0.002f;
    glm::vec3 pan = right * deltaX * panSpeed - up * deltaY * panSpeed;

    m_camera.orbitCenter += pan;
    m_camera.target += pan;

    UpdateCameraMatrices();

    if (OnViewChanged) {
        OnViewChanged();
    }
}

void AnimationPreview::ZoomCamera(float delta) {
    m_camera.distance = std::clamp(
        m_camera.distance * (1.0f - delta * 0.1f),
        m_camera.minDistance,
        m_camera.maxDistance
    );

    UpdateCameraMatrices();

    if (OnViewChanged) {
        OnViewChanged();
    }
}

void AnimationPreview::ResetCamera() {
    m_camera = PreviewCamera();
    UpdateCameraMatrices();
}

void AnimationPreview::FocusOnSkeleton() {
    if (!m_boneEditor || !m_boneEditor->HasSkeleton()) return;

    // Calculate bounding box of skeleton
    auto joints = m_boneEditor->GetJointPositions();
    if (joints.empty()) return;

    glm::vec3 minPos(std::numeric_limits<float>::max());
    glm::vec3 maxPos(std::numeric_limits<float>::lowest());

    for (const auto& pos : joints) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
    }

    glm::vec3 center = (minPos + maxPos) * 0.5f;
    float size = glm::length(maxPos - minPos);

    m_camera.orbitCenter = center;
    m_camera.target = center;
    m_camera.distance = size * 1.5f;

    UpdateCameraMatrices();
}

void AnimationPreview::FocusOnBone(const std::string& boneName) {
    if (!m_boneEditor) return;

    glm::mat4 worldTransform = m_boneEditor->GetBoneWorldTransform(boneName);
    glm::vec3 bonePos = glm::vec3(worldTransform[3]);

    m_camera.orbitCenter = bonePos;
    m_camera.target = bonePos;

    UpdateCameraMatrices();
}

// ============================================================================
// Mouse Interaction
// ============================================================================

void AnimationPreview::OnMouseDown(const glm::vec2& position, int button) {
    m_lastMousePos = position;

    if (button == 0) {  // Left button
        // Check for gizmo interaction first
        if (m_showGizmo && m_boneEditor && !m_boneEditor->GetPrimarySelection().empty()) {
            // Would check gizmo hit
            // m_isManipulatingGizmo = true;
        }

        if (!m_isManipulatingGizmo) {
            // Try to pick bone
            std::string pickedBone = PickBoneAtScreen(position);
            if (!pickedBone.empty()) {
                if (m_boneEditor) {
                    m_boneEditor->SelectBone(pickedBone);
                }
                if (OnBoneClicked) {
                    OnBoneClicked(pickedBone);
                }
            }
        }
    } else if (button == 1) {  // Right button - orbit
        m_isOrbiting = true;
    } else if (button == 2) {  // Middle button - pan
        m_isPanning = true;
    }
}

void AnimationPreview::OnMouseUp(const glm::vec2& /*position*/, int button) {
    if (button == 0) {
        if (m_isManipulatingGizmo && m_boneEditor) {
            m_boneEditor->EndGizmoInteraction();
        }
        m_isManipulatingGizmo = false;
    } else if (button == 1) {
        m_isOrbiting = false;
    } else if (button == 2) {
        m_isPanning = false;
    }
}

void AnimationPreview::OnMouseMove(const glm::vec2& position, const glm::vec2& delta) {
    if (m_isManipulatingGizmo && m_boneEditor) {
        m_boneEditor->UpdateGizmoInteraction(position);
    } else if (m_isOrbiting) {
        OrbitCamera(delta.x * 0.5f, delta.y * 0.5f);
    } else if (m_isPanning) {
        PanCamera(delta.x, delta.y);
    }

    m_lastMousePos = position;
}

void AnimationPreview::OnMouseWheel(float delta) {
    ZoomCamera(delta);
}

std::string AnimationPreview::PickBoneAtScreen(const glm::vec2& screenPos) {
    if (!m_boneEditor || !m_boneEditor->HasSkeleton()) return "";

    // Convert screen position to normalized device coordinates
    glm::vec2 ndc;
    ndc.x = (2.0f * screenPos.x / static_cast<float>(m_config.viewportWidth)) - 1.0f;
    ndc.y = 1.0f - (2.0f * screenPos.y / static_cast<float>(m_config.viewportHeight));

    return m_boneEditor->PickBone(ndc, m_viewProjectionMatrix);
}

// ============================================================================
// Screenshot
// ============================================================================

bool AnimationPreview::CaptureScreenshot(const std::string& /*filePath*/) {
    // Would read framebuffer pixels and save to file
    return false;
}

bool AnimationPreview::CaptureThumbnail(const std::string& /*filePath*/, int /*width*/, int /*height*/) {
    // Would render at smaller resolution and save
    return false;
}

// ============================================================================
// Animation Playback
// ============================================================================

void AnimationPreview::SetAnimationTime(float time) {
    if (m_keyframeEditor && m_boneEditor) {
        m_keyframeEditor->SampleAnimation(time);
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void AnimationPreview::RenderSkeleton() {
    if (!m_boneEditor) return;

    auto boneLines = m_boneEditor->GetBoneLines();
    auto jointPositions = m_boneEditor->GetJointPositions();

    // Would render using skeleton shader
    // Draw bone lines
    // for (const auto& [start, end] : boneLines) {
    //     DrawLine(start, end, m_settings.boneColor, m_settings.boneThickness);
    // }

    // Draw joints
    if (m_settings.showJoints) {
        // for (const auto& pos : jointPositions) {
        //     DrawSphere(pos, 0.02f, m_settings.jointColor);
        // }
    }

    // Highlight selected bone
    const auto& selected = m_boneEditor->GetSelectedBones();
    for (const auto& boneName : selected) {
        glm::mat4 worldTransform = m_boneEditor->GetBoneWorldTransform(boneName);
        // Would draw highlighted bone
    }
}

void AnimationPreview::RenderMesh() {
    if (!m_mesh || !m_boneEditor) return;

    // Get bone matrices
    auto transforms = m_boneEditor->GetAllTransforms();
    // auto boneMatrices = m_skeleton->CalculateBoneMatrices(transforms);

    // Would render skinned mesh
}

void AnimationPreview::RenderGroundPlane() {
    // Would render grid
}

void AnimationPreview::RenderBackground() {
    switch (m_settings.backgroundStyle) {
        case BackgroundStyle::SolidColor:
            // glClearColor(m_settings.backgroundColor);
            break;

        case BackgroundStyle::Gradient:
            // Would render gradient quad
            break;

        case BackgroundStyle::Checkerboard:
            // Would render checkerboard pattern
            break;

        default:
            break;
    }
}

void AnimationPreview::RenderGizmo() {
    if (!m_boneEditor) return;

    glm::mat4 gizmoTransform = m_boneEditor->GetGizmoTransform();
    // Would render transform gizmo based on mode
}

void AnimationPreview::UpdateCameraMatrices() {
    // Calculate camera position from orbit parameters
    float azimuthRad = glm::radians(m_camera.azimuth);
    float elevationRad = glm::radians(m_camera.elevation);

    m_camera.position.x = m_camera.orbitCenter.x + m_camera.distance * std::cos(elevationRad) * std::sin(azimuthRad);
    m_camera.position.y = m_camera.orbitCenter.y + m_camera.distance * std::sin(elevationRad);
    m_camera.position.z = m_camera.orbitCenter.z + m_camera.distance * std::cos(elevationRad) * std::cos(azimuthRad);

    m_camera.target = m_camera.orbitCenter;

    // Calculate matrices
    m_viewMatrix = glm::lookAt(m_camera.position, m_camera.target, m_camera.up);

    float aspect = static_cast<float>(m_config.viewportWidth) / static_cast<float>(m_config.viewportHeight);
    m_projectionMatrix = glm::perspective(glm::radians(m_camera.fov), aspect, m_camera.nearPlane, m_camera.farPlane);

    m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
}

glm::vec3 AnimationPreview::ScreenToWorld(const glm::vec2& screenPos, float depth) const {
    glm::vec4 viewport(0, 0, m_config.viewportWidth, m_config.viewportHeight);
    glm::vec3 screenPosWithDepth(screenPos.x, static_cast<float>(m_config.viewportHeight) - screenPos.y, depth);
    return glm::unProject(screenPosWithDepth, m_viewMatrix, m_projectionMatrix, viewport);
}

glm::vec2 AnimationPreview::WorldToScreen(const glm::vec3& worldPos) const {
    glm::vec4 viewport(0, 0, m_config.viewportWidth, m_config.viewportHeight);
    glm::vec3 projected = glm::project(worldPos, m_viewMatrix, m_projectionMatrix, viewport);
    return glm::vec2(projected.x, static_cast<float>(m_config.viewportHeight) - projected.y);
}

} // namespace Vehement
