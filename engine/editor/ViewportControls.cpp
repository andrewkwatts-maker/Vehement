/**
 * @file ViewportControls.cpp
 * @brief Implementation of the viewport navigation and control system
 */

#include "ViewportControls.hpp"
#include "../scene/Camera.hpp"
#include "../scene/SceneNode.hpp"
#include "../input/InputManager.hpp"
#include "../graphics/Shader.hpp"
#include "../graphics/Mesh.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Nova {

// ============================================================================
// Construction / Destruction
// ============================================================================

ViewportControls::ViewportControls() {
    // Initialize all bookmarks as invalid
    for (auto& bookmark : m_bookmarks) {
        bookmark.isValid = false;
    }
}

ViewportControls::~ViewportControls() {
    Shutdown();
}

ViewportControls::ViewportControls(ViewportControls&& other) noexcept
    : m_initialized(other.m_initialized)
    , m_camera(other.m_camera)
    , m_settings(std::move(other.m_settings))
    , m_focusPoint(other.m_focusPoint)
    , m_orbitDistance(other.m_orbitDistance)
    , m_orbitPitch(other.m_orbitPitch)
    , m_orbitYaw(other.m_orbitYaw)
    , m_targetFocusPoint(other.m_targetFocusPoint)
    , m_targetOrbitDistance(other.m_targetOrbitDistance)
    , m_targetOrbitPitch(other.m_targetOrbitPitch)
    , m_targetOrbitYaw(other.m_targetOrbitYaw)
    , m_isNavigating(other.m_isNavigating)
    , m_isOrbiting(other.m_isOrbiting)
    , m_isPanning(other.m_isPanning)
    , m_isZooming(other.m_isZooming)
    , m_isOrthographic(other.m_isOrthographic)
    , m_currentOrthoView(other.m_currentOrthoView)
    , m_lastOrthoView(other.m_lastOrthoView)
    , m_bookmarks(std::move(other.m_bookmarks))
    , m_gridVAO(other.m_gridVAO)
    , m_gridVBO(other.m_gridVBO)
    , m_gridVertexCount(other.m_gridVertexCount)
    , m_gridShader(std::move(other.m_gridShader))
    , m_gizmoVAO(other.m_gizmoVAO)
    , m_gizmoVBO(other.m_gizmoVBO)
    , m_gizmoEBO(other.m_gizmoEBO)
    , m_gizmoShader(std::move(other.m_gizmoShader))
{
    other.m_initialized = false;
    other.m_camera = nullptr;
    other.m_gridVAO = 0;
    other.m_gridVBO = 0;
    other.m_gizmoVAO = 0;
    other.m_gizmoVBO = 0;
    other.m_gizmoEBO = 0;
}

ViewportControls& ViewportControls::operator=(ViewportControls&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_initialized = other.m_initialized;
        m_camera = other.m_camera;
        m_settings = std::move(other.m_settings);
        m_focusPoint = other.m_focusPoint;
        m_orbitDistance = other.m_orbitDistance;
        m_orbitPitch = other.m_orbitPitch;
        m_orbitYaw = other.m_orbitYaw;
        m_targetFocusPoint = other.m_targetFocusPoint;
        m_targetOrbitDistance = other.m_targetOrbitDistance;
        m_targetOrbitPitch = other.m_targetOrbitPitch;
        m_targetOrbitYaw = other.m_targetOrbitYaw;
        m_isNavigating = other.m_isNavigating;
        m_isOrbiting = other.m_isOrbiting;
        m_isPanning = other.m_isPanning;
        m_isZooming = other.m_isZooming;
        m_isOrthographic = other.m_isOrthographic;
        m_currentOrthoView = other.m_currentOrthoView;
        m_lastOrthoView = other.m_lastOrthoView;
        m_bookmarks = std::move(other.m_bookmarks);
        m_gridVAO = other.m_gridVAO;
        m_gridVBO = other.m_gridVBO;
        m_gridVertexCount = other.m_gridVertexCount;
        m_gridShader = std::move(other.m_gridShader);
        m_gizmoVAO = other.m_gizmoVAO;
        m_gizmoVBO = other.m_gizmoVBO;
        m_gizmoEBO = other.m_gizmoEBO;
        m_gizmoShader = std::move(other.m_gizmoShader);

        other.m_initialized = false;
        other.m_camera = nullptr;
        other.m_gridVAO = 0;
        other.m_gridVBO = 0;
        other.m_gizmoVAO = 0;
        other.m_gizmoVBO = 0;
        other.m_gizmoEBO = 0;
    }
    return *this;
}

// ============================================================================
// Initialization
// ============================================================================

bool ViewportControls::Initialize() {
    if (m_initialized) {
        return true;
    }

    if (!InitializeGridResources()) {
        return false;
    }

    if (!InitializeOrientationGizmoResources()) {
        DestroyGridResources();
        return false;
    }

    m_initialized = true;
    return true;
}

void ViewportControls::Shutdown() {
    DestroyOrientationGizmoResources();
    DestroyGridResources();
    m_initialized = false;
}

bool ViewportControls::InitializeGridResources() {
    // Create grid shader
    const char* gridVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec4 aColor;

        uniform mat4 uViewProjection;

        out vec4 vColor;
        out vec3 vWorldPos;

        void main() {
            vColor = aColor;
            vWorldPos = aPos;
            gl_Position = uViewProjection * vec4(aPos, 1.0);
        }
    )";

    const char* gridFragmentShader = R"(
        #version 330 core
        in vec4 vColor;
        in vec3 vWorldPos;

        uniform vec3 uCameraPos;
        uniform float uFadeStart;
        uniform float uFadeEnd;

        out vec4 FragColor;

        void main() {
            // Distance-based fade
            float dist = length(vWorldPos.xz - uCameraPos.xz);
            float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, dist);

            // Height-based fade (grid fades when camera is very high)
            float heightFade = 1.0 - smoothstep(50.0, 200.0, abs(uCameraPos.y));

            FragColor = vColor;
            FragColor.a *= fade * heightFade;

            if (FragColor.a < 0.01) discard;
        }
    )";

    m_gridShader = std::make_unique<Shader>();
    if (!m_gridShader->LoadFromSource(gridVertexShader, gridFragmentShader)) {
        return false;
    }

    // Create grid VAO/VBO
    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);

    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);

    // Pre-allocate buffer
    glBufferData(GL_ARRAY_BUFFER, MAX_GRID_VERTICES * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position attribute (3 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (4 floats)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    return true;
}

void ViewportControls::DestroyGridResources() {
    if (m_gridVAO) {
        glDeleteVertexArrays(1, &m_gridVAO);
        m_gridVAO = 0;
    }
    if (m_gridVBO) {
        glDeleteBuffers(1, &m_gridVBO);
        m_gridVBO = 0;
    }
    m_gridShader.reset();
}

bool ViewportControls::InitializeOrientationGizmoResources() {
    // Create gizmo shader
    const char* gizmoVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;

        uniform mat4 uMVP;
        uniform mat4 uModel;

        out vec3 vNormal;
        out vec3 vWorldPos;

        void main() {
            vNormal = mat3(transpose(inverse(uModel))) * aNormal;
            vWorldPos = vec3(uModel * vec4(aPos, 1.0));
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* gizmoFragmentShader = R"(
        #version 330 core
        in vec3 vNormal;
        in vec3 vWorldPos;

        uniform vec4 uColor;
        uniform bool uHighlighted;
        uniform vec3 uLightDir;

        out vec4 FragColor;

        void main() {
            vec3 normal = normalize(vNormal);
            float diffuse = max(dot(normal, uLightDir), 0.0);
            float ambient = 0.3;

            vec3 color = uColor.rgb * (ambient + diffuse * 0.7);

            if (uHighlighted) {
                color = mix(color, vec3(1.0), 0.3);
            }

            FragColor = vec4(color, uColor.a);
        }
    )";

    m_gizmoShader = std::make_unique<Shader>();
    if (!m_gizmoShader->LoadFromSource(gizmoVertexShader, gizmoFragmentShader)) {
        return false;
    }

    // Create simple cube mesh for orientation gizmo faces
    // Each face is a separate quad for individual highlighting
    float cubeSize = 0.5f;

    // Vertex data: position (3) + normal (3) = 6 floats per vertex
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    // Helper to add a face
    auto addFace = [&](const glm::vec3& normal, const glm::vec3& right, const glm::vec3& up) {
        glm::vec3 center = normal * cubeSize;
        glm::vec3 v0 = center - right * cubeSize - up * cubeSize;
        glm::vec3 v1 = center + right * cubeSize - up * cubeSize;
        glm::vec3 v2 = center + right * cubeSize + up * cubeSize;
        glm::vec3 v3 = center - right * cubeSize + up * cubeSize;

        uint32_t baseIndex = static_cast<uint32_t>(vertices.size() / 6);

        // Add vertices
        for (const auto& v : {v0, v1, v2, v3}) {
            vertices.push_back(v.x);
            vertices.push_back(v.y);
            vertices.push_back(v.z);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }

        // Add indices (two triangles per face)
        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 3);
    };

    // Add all 6 faces
    addFace(glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));   // +X (Right)
    addFace(glm::vec3(-1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)); // -X (Left)
    addFace(glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1));   // +Y (Top)
    addFace(glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1)); // -Y (Bottom)
    addFace(glm::vec3(0, 0, 1), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0));  // +Z (Back)
    addFace(glm::vec3(0, 0, -1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0));  // -Z (Front)

    // Create VAO/VBO/EBO
    glGenVertexArrays(1, &m_gizmoVAO);
    glGenBuffers(1, &m_gizmoVBO);
    glGenBuffers(1, &m_gizmoEBO);

    glBindVertexArray(m_gizmoVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_gizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gizmoEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    return true;
}

void ViewportControls::DestroyOrientationGizmoResources() {
    if (m_gizmoVAO) {
        glDeleteVertexArrays(1, &m_gizmoVAO);
        m_gizmoVAO = 0;
    }
    if (m_gizmoVBO) {
        glDeleteBuffers(1, &m_gizmoVBO);
        m_gizmoVBO = 0;
    }
    if (m_gizmoEBO) {
        glDeleteBuffers(1, &m_gizmoEBO);
        m_gizmoEBO = 0;
    }
    m_gizmoShader.reset();
    m_gizmoCubeMesh.reset();
    m_gizmoConeMesh.reset();
}

// ============================================================================
// Camera Attachment
// ============================================================================

void ViewportControls::Attach(Camera* camera) {
    m_camera = camera;

    if (m_camera) {
        // Initialize orbit state from camera
        m_orbitPitch = m_camera->GetPitch();
        m_orbitYaw = m_camera->GetYaw();
        m_targetOrbitPitch = m_orbitPitch;
        m_targetOrbitYaw = m_orbitYaw;

        // Calculate initial focus point (camera position + forward * distance)
        glm::vec3 cameraPos = m_camera->GetPosition();
        glm::vec3 forward = m_camera->GetForward();
        m_focusPoint = cameraPos + forward * m_orbitDistance;
        m_targetFocusPoint = m_focusPoint;
    }
}

void ViewportControls::Detach() {
    m_camera = nullptr;
}

// ============================================================================
// Update
// ============================================================================

void ViewportControls::Update(float deltaTime, const InputManager& input, const glm::vec2& screenSize) {
    // Get input state
    glm::vec2 mousePos = input.GetMousePosition();
    glm::vec2 mouseDelta = input.GetMouseDelta();
    float scrollDelta = input.GetScrollDelta();

    bool lmbDown = input.IsMouseButtonDown(MouseButton::Left);
    bool mmbDown = input.IsMouseButtonDown(MouseButton::Middle);
    bool rmbDown = input.IsMouseButtonDown(MouseButton::Right);

    bool altDown = input.IsAltDown();
    bool shiftDown = input.IsShiftDown();
    bool ctrlDown = input.IsControlDown();

    // Check for keyboard shortcuts
    if (input.IsKeyPressed(Key::F)) {
        // Frame selection - user should call FrameSelection with bounds
        if (m_onViewChanged) {
            m_onViewChanged();
        }
    }

    if (input.IsKeyPressed(Key::Home)) {
        ResetView();
    }

    // Orthographic views (using number row keys 1, 3, 5, 7 - same as Blender/Maya numpad style)
    // Note: Uses number row keys (not numpad) for cross-platform compatibility
    if (input.IsKeyPressed(Key::Num1)) {
        SetOrthoView(ctrlDown ? OrthoView::Back : OrthoView::Front);
    }
    if (input.IsKeyPressed(Key::Num3)) {
        SetOrthoView(ctrlDown ? OrthoView::Left : OrthoView::Right);
    }
    if (input.IsKeyPressed(Key::Num7)) {
        SetOrthoView(ctrlDown ? OrthoView::Bottom : OrthoView::Top);
    }
    if (input.IsKeyPressed(Key::Num5)) {
        TogglePerspective();
    }

    // Movement keys for fly mode
    bool wDown = input.IsKeyDown(Key::W);
    bool aDown = input.IsKeyDown(Key::A);
    bool sDown = input.IsKeyDown(Key::S);
    bool dDown = input.IsKeyDown(Key::D);
    bool qDown = input.IsKeyDown(Key::Q);
    bool eDown = input.IsKeyDown(Key::E);

    // Update with extracted input state
    Update(deltaTime, mousePos, mouseDelta, scrollDelta,
           lmbDown, mmbDown, rmbDown, altDown, shiftDown, ctrlDown, screenSize);

    // Process fly mode keys separately
    if (m_settings.mode == CameraMode::Fly && rmbDown) {
        ProcessFlyMode(deltaTime, mouseDelta, scrollDelta, lmbDown, rmbDown, shiftDown,
                       wDown, aDown, sDown, dDown, qDown, eDown);
    }

    // Process walkthrough mode
    if (m_settings.mode == CameraMode::Walkthrough) {
        ProcessWalkthroughMode(deltaTime, mouseDelta, wDown, aDown, sDown, dDown, shiftDown);
    }
}

void ViewportControls::Update(float deltaTime,
                               const glm::vec2& mousePos,
                               const glm::vec2& mouseDelta,
                               float scrollDelta,
                               bool lmbDown, bool mmbDown, bool rmbDown,
                               bool altDown, bool shiftDown, bool ctrlDown,
                               const glm::vec2& screenSize) {
    if (!m_camera) {
        return;
    }

    // Update animation if active
    if (m_isAnimating) {
        UpdateCameraAnimation(deltaTime);
        return;
    }

    // Update navigation state
    bool wasNavigating = m_isNavigating;
    m_isOrbiting = false;
    m_isPanning = false;
    m_isZooming = false;

    // Process based on camera mode
    switch (m_settings.mode) {
        case CameraMode::Orbit:
            ProcessOrbitMode(deltaTime, mouseDelta, scrollDelta,
                             lmbDown, mmbDown, rmbDown, altDown, shiftDown);
            break;

        case CameraMode::Fly:
            // Fly mode is handled in the InputManager version of Update
            // Here we just handle mouse look when RMB is down
            if (rmbDown && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
                m_orbitYaw += mouseDelta.x * m_settings.orbitSpeed;
                m_orbitPitch -= mouseDelta.y * m_settings.orbitSpeed;
                m_orbitPitch = glm::clamp(m_orbitPitch, -89.0f, 89.0f);
                m_targetOrbitYaw = m_orbitYaw;
                m_targetOrbitPitch = m_orbitPitch;
            }
            break;

        case CameraMode::Pan:
            ProcessPanMode(deltaTime, mouseDelta, scrollDelta, lmbDown, mmbDown);
            break;

        case CameraMode::Turntable:
            ProcessTurntableMode(deltaTime, mouseDelta, scrollDelta,
                                  lmbDown, mmbDown, rmbDown, altDown);
            break;

        case CameraMode::Walkthrough:
            // Walkthrough is handled in the InputManager version of Update
            break;
    }

    m_isNavigating = m_isOrbiting || m_isPanning || m_isZooming;

    // Apply smooth motion
    if (m_settings.enableSmoothOrbit || m_settings.enableSmoothZoom) {
        ApplySmoothMotion(deltaTime);
    } else {
        m_focusPoint = m_targetFocusPoint;
        m_orbitDistance = m_targetOrbitDistance;
        m_orbitPitch = m_targetOrbitPitch;
        m_orbitYaw = m_targetOrbitYaw;
    }

    // Apply camera transform
    ApplyCameraTransform();

    // Notify view changed
    if (wasNavigating || m_isNavigating) {
        if (m_onViewChanged) {
            m_onViewChanged();
        }
    }
}

void ViewportControls::ProcessOrbitMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                                         bool lmbDown, bool mmbDown, bool rmbDown, bool altDown, bool shiftDown) {
    // Maya-style navigation: Alt + mouse buttons
    if (altDown) {
        if (lmbDown && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
            // Alt + LMB: Orbit
            m_isOrbiting = true;
            m_targetOrbitYaw += mouseDelta.x * m_settings.orbitSpeed;
            m_targetOrbitPitch -= mouseDelta.y * m_settings.orbitSpeed;
            m_targetOrbitPitch = glm::clamp(m_targetOrbitPitch, -89.0f, 89.0f);
        }

        if (mmbDown && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
            // Alt + MMB: Pan
            m_isPanning = true;

            // Calculate pan in camera space
            float panScale = m_orbitDistance * m_settings.panSpeed;
            glm::vec3 right = m_camera->GetRight();
            glm::vec3 up = m_camera->GetUp();

            m_targetFocusPoint -= right * mouseDelta.x * panScale;
            m_targetFocusPoint += up * mouseDelta.y * panScale;
        }

        if (rmbDown && mouseDelta.y != 0.0f) {
            // Alt + RMB: Dolly/Zoom
            m_isZooming = true;
            float zoomFactor = 1.0f + mouseDelta.y * m_settings.zoomSpeed * 0.1f;
            m_targetOrbitDistance *= zoomFactor;
            m_targetOrbitDistance = glm::clamp(m_targetOrbitDistance,
                                                m_settings.minZoomDistance,
                                                m_settings.maxZoomDistance);
        }
    }

    // Mouse wheel zoom (always active)
    if (scrollDelta != 0.0f) {
        m_isZooming = true;
        float zoomFactor = 1.0f - scrollDelta * m_settings.zoomSpeed;
        m_targetOrbitDistance *= zoomFactor;
        m_targetOrbitDistance = glm::clamp(m_targetOrbitDistance,
                                            m_settings.minZoomDistance,
                                            m_settings.maxZoomDistance);
    }
}

void ViewportControls::ProcessFlyMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                                       bool lmbDown, bool rmbDown, bool shiftDown,
                                       bool wDown, bool aDown, bool sDown, bool dDown,
                                       bool qDown, bool eDown) {
    if (!m_camera) return;

    // Mouse look when RMB is held
    if (rmbDown) {
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
            m_orbitYaw += mouseDelta.x * m_settings.orbitSpeed;
            m_orbitPitch -= mouseDelta.y * m_settings.orbitSpeed;
            m_orbitPitch = glm::clamp(m_orbitPitch, -89.0f, 89.0f);

            m_camera->SetRotation(m_orbitPitch, m_orbitYaw);
        }

        // WASD movement
        glm::vec3 velocity{0.0f};
        glm::vec3 forward = m_camera->GetForward();
        glm::vec3 right = m_camera->GetRight();
        glm::vec3 up{0.0f, 1.0f, 0.0f};

        float speed = m_settings.flySpeed;
        if (shiftDown) {
            speed *= m_settings.flySprintMultiplier;
        }

        if (wDown) velocity += forward;
        if (sDown) velocity -= forward;
        if (dDown) velocity += right;
        if (aDown) velocity -= right;
        if (eDown) velocity += up;
        if (qDown) velocity -= up;

        if (glm::length(velocity) > 0.0f) {
            velocity = glm::normalize(velocity) * speed * deltaTime;
            glm::vec3 newPos = m_camera->GetPosition() + velocity;
            m_camera->SetPosition(newPos);
            m_focusPoint = newPos + m_camera->GetForward() * m_orbitDistance;
        }
    }

    // Scroll wheel adjusts speed
    if (scrollDelta != 0.0f) {
        m_settings.flySpeed *= (1.0f + scrollDelta * 0.1f);
        m_settings.flySpeed = glm::clamp(m_settings.flySpeed, 0.1f, 1000.0f);
    }
}

void ViewportControls::ProcessPanMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                                       bool lmbDown, bool mmbDown) {
    if (!m_camera) return;

    // LMB or MMB: Pan
    if ((lmbDown || mmbDown) && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
        m_isPanning = true;

        float panScale = m_orbitDistance * m_settings.panSpeed;
        glm::vec3 right = m_camera->GetRight();
        glm::vec3 up = m_camera->GetUp();

        m_targetFocusPoint -= right * mouseDelta.x * panScale;
        m_targetFocusPoint += up * mouseDelta.y * panScale;
    }

    // Scroll: Zoom
    if (scrollDelta != 0.0f) {
        m_isZooming = true;
        if (m_isOrthographic) {
            m_settings.orthoSize *= (1.0f - scrollDelta * m_settings.zoomSpeed);
            m_settings.orthoSize = glm::clamp(m_settings.orthoSize, 0.1f, 10000.0f);
            UpdateOrthoProjection();
        } else {
            float zoomFactor = 1.0f - scrollDelta * m_settings.zoomSpeed;
            m_targetOrbitDistance *= zoomFactor;
            m_targetOrbitDistance = glm::clamp(m_targetOrbitDistance,
                                                m_settings.minZoomDistance,
                                                m_settings.maxZoomDistance);
        }
    }
}

void ViewportControls::ProcessTurntableMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                                             bool lmbDown, bool mmbDown, bool rmbDown, bool altDown) {
    // Turntable: Only rotate around Y axis
    if (altDown || lmbDown) {
        if (lmbDown && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
            m_isOrbiting = true;
            m_targetOrbitYaw += mouseDelta.x * m_settings.orbitSpeed;
            // Turntable mode: pitch is constrained or disabled
            // Optional: allow limited pitch
            m_targetOrbitPitch -= mouseDelta.y * m_settings.orbitSpeed * 0.5f;
            m_targetOrbitPitch = glm::clamp(m_targetOrbitPitch, -60.0f, 60.0f);
        }
    }

    // Pan and zoom same as orbit mode
    if (mmbDown && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
        m_isPanning = true;
        float panScale = m_orbitDistance * m_settings.panSpeed;
        glm::vec3 right = m_camera->GetRight();
        glm::vec3 up = m_camera->GetUp();
        m_targetFocusPoint -= right * mouseDelta.x * panScale;
        m_targetFocusPoint += up * mouseDelta.y * panScale;
    }

    if (scrollDelta != 0.0f) {
        m_isZooming = true;
        float zoomFactor = 1.0f - scrollDelta * m_settings.zoomSpeed;
        m_targetOrbitDistance *= zoomFactor;
        m_targetOrbitDistance = glm::clamp(m_targetOrbitDistance,
                                            m_settings.minZoomDistance,
                                            m_settings.maxZoomDistance);
    }
}

void ViewportControls::ProcessWalkthroughMode(float deltaTime, const glm::vec2& mouseDelta,
                                               bool wDown, bool aDown, bool sDown, bool dDown, bool shiftDown) {
    if (!m_camera) return;

    // Always apply mouse look in walkthrough
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
        m_orbitYaw += mouseDelta.x * m_settings.orbitSpeed;
        m_orbitPitch -= mouseDelta.y * m_settings.orbitSpeed;
        m_orbitPitch = glm::clamp(m_orbitPitch, -89.0f, 89.0f);
        m_camera->SetRotation(m_orbitPitch, m_orbitYaw);
    }

    // Movement constrained to ground plane
    glm::vec3 forward = m_camera->GetForward();
    forward.y = 0.0f;
    if (glm::length(forward) > 0.001f) {
        forward = glm::normalize(forward);
    }

    glm::vec3 right = m_camera->GetRight();
    right.y = 0.0f;
    if (glm::length(right) > 0.001f) {
        right = glm::normalize(right);
    }

    glm::vec3 velocity{0.0f};
    float speed = m_settings.flySpeed;
    if (shiftDown) {
        speed *= m_settings.flySprintMultiplier;
    }

    if (wDown) velocity += forward;
    if (sDown) velocity -= forward;
    if (dDown) velocity += right;
    if (aDown) velocity -= right;

    if (glm::length(velocity) > 0.0f) {
        velocity = glm::normalize(velocity) * speed * deltaTime;
        glm::vec3 newPos = m_camera->GetPosition() + velocity;
        // Constrain to ground + eye height
        newPos.y = m_settings.groundHeight + m_settings.eyeHeight;
        m_camera->SetPosition(newPos);
        m_focusPoint = newPos + m_camera->GetForward() * m_orbitDistance;
    }
}

void ViewportControls::ApplySmoothMotion(float deltaTime) {
    float t = 1.0f - std::exp(-m_settings.smoothingFactor * deltaTime);

    if (m_settings.enableSmoothOrbit) {
        m_focusPoint = glm::mix(m_focusPoint, m_targetFocusPoint, t);
        m_orbitPitch = glm::mix(m_orbitPitch, m_targetOrbitPitch, t);
        m_orbitYaw = glm::mix(m_orbitYaw, m_targetOrbitYaw, t);
    }

    if (m_settings.enableSmoothZoom) {
        m_orbitDistance = glm::mix(m_orbitDistance, m_targetOrbitDistance, t);
    }
}

void ViewportControls::ApplyCameraTransform() {
    if (!m_camera) return;

    if (m_settings.mode == CameraMode::Fly || m_settings.mode == CameraMode::Walkthrough) {
        // Fly/Walkthrough: camera position is directly controlled
        // Just ensure rotation is applied
        m_camera->SetRotation(m_orbitPitch, m_orbitYaw);
    } else {
        // Orbit/Pan/Turntable: camera orbits around focus point
        float pitchRad = glm::radians(m_orbitPitch);
        float yawRad = glm::radians(m_orbitYaw);

        glm::vec3 offset;
        offset.x = std::cos(pitchRad) * std::cos(yawRad);
        offset.y = std::sin(pitchRad);
        offset.z = std::cos(pitchRad) * std::sin(yawRad);

        glm::vec3 cameraPos = m_focusPoint + offset * m_orbitDistance;

        m_camera->LookAt(cameraPos, m_focusPoint);
    }
}

void ViewportControls::UpdateOrthoProjection() {
    if (!m_camera || !m_isOrthographic) return;

    float aspect = m_camera->GetAspectRatio();
    float halfHeight = m_settings.orthoSize * 0.5f;
    float halfWidth = halfHeight * aspect;

    m_camera->SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight,
                               m_settings.nearPlane, m_settings.farPlane);
}

// ============================================================================
// Animation
// ============================================================================

void ViewportControls::StartCameraAnimation(const glm::vec3& targetPosition, const glm::vec3& targetFocus,
                                             float targetPitch, float targetYaw, float duration) {
    if (!m_camera) return;

    m_isAnimating = true;
    m_animationTime = 0.0f;
    m_animationDuration = duration;

    // Store start state
    m_animStartPosition = m_camera->GetPosition();
    m_animStartFocus = m_focusPoint;
    m_animStartPitch = m_orbitPitch;
    m_animStartYaw = m_orbitYaw;
    m_animStartDistance = m_orbitDistance;

    // Store target state
    m_animTargetPosition = targetPosition;
    m_animTargetFocus = targetFocus;
    m_animTargetPitch = targetPitch;
    m_animTargetYaw = targetYaw;
    m_animTargetDistance = glm::length(targetPosition - targetFocus);
}

void ViewportControls::UpdateCameraAnimation(float deltaTime) {
    m_animationTime += deltaTime;
    float t = glm::clamp(m_animationTime / m_animationDuration, 0.0f, 1.0f);

    // Smooth step easing
    t = t * t * (3.0f - 2.0f * t);

    // Interpolate all values
    m_focusPoint = glm::mix(m_animStartFocus, m_animTargetFocus, t);
    m_orbitPitch = glm::mix(m_animStartPitch, m_animTargetPitch, t);
    m_orbitYaw = glm::mix(m_animStartYaw, m_animTargetYaw, t);
    m_orbitDistance = glm::mix(m_animStartDistance, m_animTargetDistance, t);

    // Update targets to match (disable smoothing during animation)
    m_targetFocusPoint = m_focusPoint;
    m_targetOrbitPitch = m_orbitPitch;
    m_targetOrbitYaw = m_orbitYaw;
    m_targetOrbitDistance = m_orbitDistance;

    // Apply to camera
    ApplyCameraTransform();

    // Handle ortho transition
    if (m_animTargetOrtho && t >= 1.0f) {
        m_isOrthographic = true;
        m_settings.orthoSize = m_animTargetOrthoSize;
        UpdateOrthoProjection();
    }

    // Check if animation complete
    if (t >= 1.0f) {
        m_isAnimating = false;
        if (m_onViewChanged) {
            m_onViewChanged();
        }
    }
}

// ============================================================================
// Camera Mode
// ============================================================================

void ViewportControls::SetMode(CameraMode mode) {
    if (m_settings.mode == mode) return;

    CameraMode oldMode = m_settings.mode;
    m_settings.mode = mode;

    // Transition logic
    if (mode == CameraMode::Fly || mode == CameraMode::Walkthrough) {
        // When entering fly mode, keep current camera position
        if (m_camera) {
            m_orbitPitch = m_camera->GetPitch();
            m_orbitYaw = m_camera->GetYaw();
            m_targetOrbitPitch = m_orbitPitch;
            m_targetOrbitYaw = m_orbitYaw;
        }
    } else if (oldMode == CameraMode::Fly || oldMode == CameraMode::Walkthrough) {
        // When exiting fly mode, set focus point in front of camera
        if (m_camera) {
            m_focusPoint = m_camera->GetPosition() + m_camera->GetForward() * m_orbitDistance;
            m_targetFocusPoint = m_focusPoint;
        }
    }

    if (m_onCameraModeChanged) {
        m_onCameraModeChanged(mode);
    }
}

void ViewportControls::CycleMode() {
    CameraMode nextMode;
    switch (m_settings.mode) {
        case CameraMode::Orbit:      nextMode = CameraMode::Fly; break;
        case CameraMode::Fly:        nextMode = CameraMode::Pan; break;
        case CameraMode::Pan:        nextMode = CameraMode::Turntable; break;
        case CameraMode::Turntable:  nextMode = CameraMode::Walkthrough; break;
        case CameraMode::Walkthrough: nextMode = CameraMode::Orbit; break;
        default:                     nextMode = CameraMode::Orbit; break;
    }
    SetMode(nextMode);
}

// ============================================================================
// Focus and Framing
// ============================================================================

void ViewportControls::SetFocusPoint(const glm::vec3& point) {
    m_targetFocusPoint = point;
    if (!m_settings.enableSmoothOrbit) {
        m_focusPoint = point;
    }
}

void ViewportControls::FrameSelection(const std::vector<glm::vec3>& bounds, float padding) {
    if (bounds.empty() || !m_camera) return;

    // Calculate bounding box
    glm::vec3 minBounds = bounds[0];
    glm::vec3 maxBounds = bounds[0];

    for (const auto& point : bounds) {
        minBounds = glm::min(minBounds, point);
        maxBounds = glm::max(maxBounds, point);
    }

    FrameBounds(minBounds, maxBounds, padding);
}

void ViewportControls::FrameBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds, float padding) {
    if (!m_camera) return;

    // Calculate center and size
    glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    glm::vec3 size = maxBounds - minBounds;
    float maxSize = glm::max(glm::max(size.x, size.y), size.z);

    // Calculate required distance to frame the object
    float distance = CalculateFrameDistance(size, m_camera->GetFOV()) * padding;

    // Animate to new position
    glm::vec3 direction = glm::normalize(m_camera->GetPosition() - m_focusPoint);
    if (glm::length(direction) < 0.001f) {
        direction = glm::vec3(0, 0, 1);
    }

    glm::vec3 targetPosition = center + direction * distance;

    // Calculate pitch and yaw for the direction
    float targetPitch = glm::degrees(std::asin(direction.y));
    float targetYaw = glm::degrees(std::atan2(direction.z, direction.x));

    StartCameraAnimation(targetPosition, center, targetPitch, targetYaw, 0.3f);
}

void ViewportControls::FocusOnObject(SceneNode* node, bool frameSelection) {
    if (!node || !m_camera) return;

    glm::vec3 center = node->GetWorldPosition();

    if (frameSelection) {
        // Get bounds from mesh if available
        // For now, use a default size
        glm::vec3 halfSize{1.0f, 1.0f, 1.0f};
        if (node->GetMesh()) {
            // TODO: Get actual mesh bounds
        }

        FrameBounds(center - halfSize, center + halfSize, 1.5f);
    } else {
        // Just set focus point
        m_targetFocusPoint = center;
    }
}

void ViewportControls::ResetView() {
    if (!m_camera) return;

    glm::vec3 direction = glm::normalize(m_defaultPosition - m_defaultTarget);
    float targetPitch = glm::degrees(std::asin(direction.y));
    float targetYaw = glm::degrees(std::atan2(direction.z, direction.x));
    float distance = glm::length(m_defaultPosition - m_defaultTarget);

    // Reset to perspective
    if (m_isOrthographic) {
        m_isOrthographic = false;
        m_currentOrthoView = OrthoView::Perspective;
        m_camera->SetPerspective(m_settings.fieldOfView, m_camera->GetAspectRatio(),
                                  m_settings.nearPlane, m_settings.farPlane);
    }

    StartCameraAnimation(m_defaultPosition, m_defaultTarget, targetPitch, targetYaw, 0.3f);
}

void ViewportControls::SetDefaultView(const glm::vec3& position, const glm::vec3& target) {
    m_defaultPosition = position;
    m_defaultTarget = target;
}

float ViewportControls::CalculateFrameDistance(const glm::vec3& boundsSize, float fov) const {
    float maxSize = glm::max(glm::max(boundsSize.x, boundsSize.y), boundsSize.z);
    float halfFov = glm::radians(fov * 0.5f);
    return maxSize / (2.0f * std::tan(halfFov));
}

// ============================================================================
// Orthographic Views
// ============================================================================

void ViewportControls::SetOrthoView(OrthoView view, bool animate) {
    if (!m_camera) return;

    if (view == OrthoView::Perspective) {
        // Return to perspective
        m_isOrthographic = false;
        m_currentOrthoView = OrthoView::Perspective;
        m_camera->SetPerspective(m_settings.fieldOfView, m_camera->GetAspectRatio(),
                                  m_settings.nearPlane, m_settings.farPlane);

        if (m_onOrthoViewChanged) {
            m_onOrthoViewChanged(view);
        }
        return;
    }

    m_lastOrthoView = view;

    // Get view direction and up vector
    glm::vec3 direction = GetOrthoViewDirection(view);
    glm::vec3 up = GetOrthoViewUp(view);

    // Calculate target camera position
    glm::vec3 targetPosition = m_focusPoint - direction * m_orbitDistance;

    // Calculate pitch and yaw
    float targetPitch = glm::degrees(std::asin(-direction.y));
    float targetYaw = glm::degrees(std::atan2(-direction.z, -direction.x));

    if (animate) {
        m_animTargetOrtho = true;
        m_animTargetOrthoSize = m_orbitDistance * 2.0f;
        StartCameraAnimation(targetPosition, m_focusPoint, targetPitch, targetYaw, 0.25f);
    } else {
        m_isOrthographic = true;
        m_currentOrthoView = view;
        m_orbitPitch = m_targetOrbitPitch = targetPitch;
        m_orbitYaw = m_targetOrbitYaw = targetYaw;
        m_settings.orthoSize = m_orbitDistance * 2.0f;
        UpdateOrthoProjection();
        ApplyCameraTransform();
    }

    if (m_onOrthoViewChanged) {
        m_onOrthoViewChanged(view);
    }
}

void ViewportControls::TogglePerspective() {
    if (m_isOrthographic) {
        SetOrthoView(OrthoView::Perspective);
    } else {
        SetOrthoView(m_lastOrthoView);
    }
}

glm::vec3 ViewportControls::GetOrthoViewDirection(OrthoView view) const {
    switch (view) {
        case OrthoView::Front:   return glm::vec3(0, 0, -1);
        case OrthoView::Back:    return glm::vec3(0, 0, 1);
        case OrthoView::Left:    return glm::vec3(-1, 0, 0);
        case OrthoView::Right:   return glm::vec3(1, 0, 0);
        case OrthoView::Top:     return glm::vec3(0, -1, 0);
        case OrthoView::Bottom:  return glm::vec3(0, 1, 0);
        default:                 return glm::vec3(0, 0, -1);
    }
}

glm::vec3 ViewportControls::GetOrthoViewUp(OrthoView view) const {
    switch (view) {
        case OrthoView::Top:     return glm::vec3(0, 0, -1);
        case OrthoView::Bottom:  return glm::vec3(0, 0, 1);
        default:                 return glm::vec3(0, 1, 0);
    }
}

// ============================================================================
// Bookmarks
// ============================================================================

void ViewportControls::SaveBookmark(int slot, const std::string& name) {
    if (slot < 0 || slot >= MAX_BOOKMARKS || !m_camera) return;

    auto& bookmark = m_bookmarks[slot];
    bookmark.position = m_camera->GetPosition();
    bookmark.focusPoint = m_focusPoint;
    bookmark.pitch = m_orbitPitch;
    bookmark.yaw = m_orbitYaw;
    bookmark.distance = m_orbitDistance;
    bookmark.isOrthographic = m_isOrthographic;
    bookmark.orthoSize = m_settings.orthoSize;
    bookmark.name = name.empty() ? ("Bookmark " + std::to_string(slot + 1)) : name;
    bookmark.isValid = true;
}

bool ViewportControls::RestoreBookmark(int slot, bool animate) {
    if (slot < 0 || slot >= MAX_BOOKMARKS || !m_camera) return false;

    const auto& bookmark = m_bookmarks[slot];
    if (!bookmark.isValid) return false;

    if (animate) {
        m_animTargetOrtho = bookmark.isOrthographic;
        m_animTargetOrthoSize = bookmark.orthoSize;
        StartCameraAnimation(bookmark.position, bookmark.focusPoint,
                              bookmark.pitch, bookmark.yaw, 0.3f);
    } else {
        m_focusPoint = m_targetFocusPoint = bookmark.focusPoint;
        m_orbitPitch = m_targetOrbitPitch = bookmark.pitch;
        m_orbitYaw = m_targetOrbitYaw = bookmark.yaw;
        m_orbitDistance = m_targetOrbitDistance = bookmark.distance;
        m_isOrthographic = bookmark.isOrthographic;
        m_settings.orthoSize = bookmark.orthoSize;

        if (m_isOrthographic) {
            UpdateOrthoProjection();
        } else {
            m_camera->SetPerspective(m_settings.fieldOfView, m_camera->GetAspectRatio(),
                                      m_settings.nearPlane, m_settings.farPlane);
        }

        ApplyCameraTransform();
    }

    return true;
}

void ViewportControls::ClearBookmark(int slot) {
    if (slot >= 0 && slot < MAX_BOOKMARKS) {
        m_bookmarks[slot].isValid = false;
    }
}

const CameraBookmark& ViewportControls::GetBookmark(int slot) const {
    static CameraBookmark invalidBookmark;
    if (slot < 0 || slot >= MAX_BOOKMARKS) return invalidBookmark;
    return m_bookmarks[slot];
}

bool ViewportControls::IsBookmarkValid(int slot) const {
    if (slot < 0 || slot >= MAX_BOOKMARKS) return false;
    return m_bookmarks[slot].isValid;
}

// ============================================================================
// Settings and State
// ============================================================================

void ViewportControls::ToggleOverlay(ViewportOverlay overlay) {
    if (HasOverlay(m_settings.overlays, overlay)) {
        m_settings.overlays &= ~overlay;
    } else {
        m_settings.overlays |= overlay;
    }
}

bool ViewportControls::IsOverlayEnabled(ViewportOverlay overlay) const {
    return HasOverlay(m_settings.overlays, overlay);
}

void ViewportControls::CycleRenderMode() {
    int mode = static_cast<int>(m_settings.renderMode);
    mode = (mode + 1) % 9; // 9 render modes
    m_settings.renderMode = static_cast<RenderMode>(mode);
}

void ViewportControls::SetOrbitDistance(float distance) {
    m_targetOrbitDistance = glm::clamp(distance, m_settings.minZoomDistance, m_settings.maxZoomDistance);
    if (!m_settings.enableSmoothZoom) {
        m_orbitDistance = m_targetOrbitDistance;
    }
}

// ============================================================================
// Grid Rendering
// ============================================================================

void ViewportControls::RenderGrid(const Camera& camera) {
    RenderGrid(camera.GetView(), camera.GetProjection(), camera.GetPosition());
}

void ViewportControls::RenderGrid(const glm::mat4& view, const glm::mat4& projection,
                                   const glm::vec3& cameraPosition) {
    if (!m_initialized || !HasOverlay(m_settings.overlays, ViewportOverlay::Grid)) {
        return;
    }

    // Generate grid lines centered around camera
    GenerateInfiniteGrid(cameraPosition);

    if (m_gridVertexCount == 0) return;

    // Setup OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // Don't write to depth buffer

    m_gridShader->Bind();
    m_gridShader->SetMat4("uViewProjection", projection * view);
    m_gridShader->SetVec3("uCameraPos", cameraPosition);
    m_gridShader->SetFloat("uFadeStart", m_settings.gridExtent * 0.5f);
    m_gridShader->SetFloat("uFadeEnd", m_settings.gridExtent);

    glBindVertexArray(m_gridVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_gridVertexCount));
    glBindVertexArray(0);

    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ViewportControls::GenerateInfiniteGrid(const glm::vec3& cameraPos) {
    std::vector<float> vertices;
    vertices.reserve(MAX_GRID_VERTICES * 7);

    float gridSize = m_settings.gridSize;
    float extent = m_settings.gridExtent;
    int subdivisions = m_settings.gridSubdivisions;

    // Snap grid origin to grid lines
    float gridX = std::floor(cameraPos.x / gridSize) * gridSize;
    float gridZ = std::floor(cameraPos.z / gridSize) * gridSize;

    // Calculate number of grid lines
    int numLines = static_cast<int>(extent / gridSize) + 1;

    auto addLine = [&](const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) {
        if (vertices.size() + 14 > MAX_GRID_VERTICES * 7) return;

        vertices.push_back(start.x);
        vertices.push_back(start.y);
        vertices.push_back(start.z);
        vertices.push_back(color.r);
        vertices.push_back(color.g);
        vertices.push_back(color.b);
        vertices.push_back(color.a);

        vertices.push_back(end.x);
        vertices.push_back(end.y);
        vertices.push_back(end.z);
        vertices.push_back(color.r);
        vertices.push_back(color.g);
        vertices.push_back(color.b);
        vertices.push_back(color.a);
    };

    // Y level for grid (ground plane)
    float y = 0.0f;

    // Main grid lines (along X and Z)
    for (int i = -numLines; i <= numLines; ++i) {
        float offset = i * gridSize;

        // Determine line color
        glm::vec4 color = m_settings.gridColor;

        // X axis line (red)
        if (std::abs(gridZ + offset) < 0.001f) {
            addLine(glm::vec3(gridX - extent, y, 0.0f),
                    glm::vec3(gridX + extent, y, 0.0f),
                    m_settings.gridAxisXColor);
        } else {
            // Regular Z lines (parallel to X axis)
            addLine(glm::vec3(gridX - extent, y, gridZ + offset),
                    glm::vec3(gridX + extent, y, gridZ + offset),
                    color);
        }

        // Z axis line (blue)
        if (std::abs(gridX + offset) < 0.001f) {
            addLine(glm::vec3(0.0f, y, gridZ - extent),
                    glm::vec3(0.0f, y, gridZ + extent),
                    m_settings.gridAxisZColor);
        } else {
            // Regular X lines (parallel to Z axis)
            addLine(glm::vec3(gridX + offset, y, gridZ - extent),
                    glm::vec3(gridX + offset, y, gridZ + extent),
                    color);
        }
    }

    // Subdivision lines
    if (subdivisions > 1) {
        float subSize = gridSize / subdivisions;
        int numSubLines = static_cast<int>(extent / subSize) + 1;

        for (int i = -numSubLines; i <= numSubLines; ++i) {
            float offset = i * subSize;

            // Skip main grid lines
            if (i % subdivisions == 0) continue;

            // Subdivision lines
            addLine(glm::vec3(gridX - extent, y, gridZ + offset),
                    glm::vec3(gridX + extent, y, gridZ + offset),
                    m_settings.gridSubdivColor);

            addLine(glm::vec3(gridX + offset, y, gridZ - extent),
                    glm::vec3(gridX + offset, y, gridZ + extent),
                    m_settings.gridSubdivColor);
        }
    }

    // Upload to GPU
    m_gridVertexCount = vertices.size() / 7;

    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================================
// Orientation Gizmo Rendering
// ============================================================================

OrientationGizmoResult ViewportControls::RenderOrientationGizmo(const Camera& camera,
                                                                  const glm::vec2& screenSize,
                                                                  const glm::vec2& mousePos) {
    OrientationGizmoResult result;

    if (!m_initialized) {
        return result;
    }

    // Calculate gizmo screen position
    glm::vec2 gizmoCenter = m_orientationGizmoPosition * screenSize;
    float gizmoSize = m_orientationGizmoSize;

    // Create orthographic projection for gizmo
    float aspect = 1.0f;
    glm::mat4 gizmoProjection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -10.0f, 10.0f);

    // Create view matrix from camera rotation only (no position)
    glm::mat4 cameraView = camera.GetView();
    glm::mat4 gizmoView = glm::mat4(glm::mat3(cameraView)); // Remove translation

    // Hit test
    result.clickedFace = HitTestOrientationGizmo(mousePos, screenSize, gizmoView);
    result.isHovered = result.clickedFace != OrthoView::Perspective;

    // Setup viewport for gizmo rendering
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    int gizmoX = static_cast<int>(gizmoCenter.x - gizmoSize * 0.5f);
    int gizmoY = static_cast<int>(screenSize.y - gizmoCenter.y - gizmoSize * 0.5f);

    glViewport(gizmoX, gizmoY, static_cast<int>(gizmoSize), static_cast<int>(gizmoSize));

    // Clear depth in gizmo area
    glEnable(GL_SCISSOR_TEST);
    glScissor(gizmoX, gizmoY, static_cast<int>(gizmoSize), static_cast<int>(gizmoSize));
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Render gizmo cube
    m_gizmoShader->Bind();

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = gizmoProjection * gizmoView * model;

    m_gizmoShader->SetMat4("uMVP", mvp);
    m_gizmoShader->SetMat4("uModel", model);
    m_gizmoShader->SetVec3("uLightDir", glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f)));

    glBindVertexArray(m_gizmoVAO);

    // Render each face with appropriate color
    struct FaceInfo {
        OrthoView view;
        glm::vec4 color;
        int startIndex;
    };

    std::array<FaceInfo, 6> faces = {{
        {OrthoView::Right,  glm::vec4(0.8f, 0.2f, 0.2f, 1.0f), 0},   // +X Red
        {OrthoView::Left,   glm::vec4(0.4f, 0.1f, 0.1f, 1.0f), 6},   // -X Dark Red
        {OrthoView::Top,    glm::vec4(0.2f, 0.8f, 0.2f, 1.0f), 12},  // +Y Green
        {OrthoView::Bottom, glm::vec4(0.1f, 0.4f, 0.1f, 1.0f), 18},  // -Y Dark Green
        {OrthoView::Back,   glm::vec4(0.2f, 0.2f, 0.8f, 1.0f), 24},  // +Z Blue
        {OrthoView::Front,  glm::vec4(0.1f, 0.1f, 0.4f, 1.0f), 30},  // -Z Dark Blue
    }};

    for (const auto& face : faces) {
        bool highlighted = (result.clickedFace == face.view);
        m_gizmoShader->SetVec4("uColor", face.color);
        m_gizmoShader->SetBool("uHighlighted", highlighted);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(face.startIndex * sizeof(uint32_t)));
    }

    glBindVertexArray(0);

    // Restore viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    return result;
}

OrthoView ViewportControls::HitTestOrientationGizmo(const glm::vec2& mousePos, const glm::vec2& screenSize,
                                                      const glm::mat4& viewRotation) {
    // Calculate gizmo bounds in screen space
    glm::vec2 gizmoCenter = m_orientationGizmoPosition * screenSize;
    float gizmoRadius = m_orientationGizmoSize * 0.5f;

    // Check if mouse is within gizmo bounds
    glm::vec2 relPos = mousePos - gizmoCenter;
    if (glm::length(relPos) > gizmoRadius) {
        return OrthoView::Perspective;
    }

    // Convert to NDC within gizmo space
    glm::vec2 ndcPos = relPos / gizmoRadius;

    // Raycast through the cube
    // Create ray in gizmo space
    glm::vec3 rayOrigin = glm::vec3(ndcPos.x, -ndcPos.y, -5.0f);
    glm::vec3 rayDir = glm::vec3(0, 0, 1);

    // Transform ray by inverse view rotation
    glm::mat4 invView = glm::inverse(viewRotation);
    rayOrigin = glm::vec3(invView * glm::vec4(rayOrigin, 1.0f));
    rayDir = glm::normalize(glm::vec3(invView * glm::vec4(rayDir, 0.0f)));

    // Intersect with cube faces
    float closestDist = std::numeric_limits<float>::max();
    OrthoView closestFace = OrthoView::Perspective;

    auto testFace = [&](const glm::vec3& normal, OrthoView face) {
        float cubeSize = 0.5f;
        glm::vec3 planePoint = normal * cubeSize;

        float denom = glm::dot(normal, rayDir);
        if (std::abs(denom) < 0.0001f) return;

        float t = glm::dot(planePoint - rayOrigin, normal) / denom;
        if (t < 0) return;

        glm::vec3 hitPoint = rayOrigin + rayDir * t;

        // Check if hit point is within face bounds
        bool inBounds = true;
        for (int i = 0; i < 3; ++i) {
            if (std::abs(normal[i]) < 0.5f) { // Not the normal axis
                if (std::abs(hitPoint[i]) > cubeSize) {
                    inBounds = false;
                    break;
                }
            }
        }

        if (inBounds && t < closestDist) {
            closestDist = t;
            closestFace = face;
        }
    };

    testFace(glm::vec3(1, 0, 0), OrthoView::Right);
    testFace(glm::vec3(-1, 0, 0), OrthoView::Left);
    testFace(glm::vec3(0, 1, 0), OrthoView::Top);
    testFace(glm::vec3(0, -1, 0), OrthoView::Bottom);
    testFace(glm::vec3(0, 0, 1), OrthoView::Back);
    testFace(glm::vec3(0, 0, -1), OrthoView::Front);

    return closestFace;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* ViewportControls::GetCameraModeName(CameraMode mode) {
    switch (mode) {
        case CameraMode::Orbit:       return "Orbit";
        case CameraMode::Fly:         return "Fly";
        case CameraMode::Pan:         return "Pan";
        case CameraMode::Turntable:   return "Turntable";
        case CameraMode::Walkthrough: return "Walkthrough";
        default:                      return "Unknown";
    }
}

const char* ViewportControls::GetRenderModeName(RenderMode mode) {
    switch (mode) {
        case RenderMode::Shaded:           return "Shaded";
        case RenderMode::Unlit:            return "Unlit";
        case RenderMode::Wireframe:        return "Wireframe";
        case RenderMode::ShadedWireframe:  return "Shaded + Wireframe";
        case RenderMode::SDFDistance:      return "SDF Distance";
        case RenderMode::Normals:          return "Normals";
        case RenderMode::UVs:              return "UVs";
        case RenderMode::Overdraw:         return "Overdraw";
        case RenderMode::LODColors:        return "LOD Colors";
        default:                           return "Unknown";
    }
}

const char* ViewportControls::GetOrthoViewName(OrthoView view) {
    switch (view) {
        case OrthoView::Front:       return "Front";
        case OrthoView::Back:        return "Back";
        case OrthoView::Left:        return "Left";
        case OrthoView::Right:       return "Right";
        case OrthoView::Top:         return "Top";
        case OrthoView::Bottom:      return "Bottom";
        case OrthoView::Perspective: return "Perspective";
        default:                     return "Unknown";
    }
}

} // namespace Nova
