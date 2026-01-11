/**
 * @file TransformGizmo.cpp
 * @brief Transform manipulation gizmo implementation
 */

#include "TransformGizmo.hpp"
#include "../graphics/Shader.hpp"
#include "../graphics/Mesh.hpp"
#include "../scene/Camera.hpp"
#include "../input/InputManager.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <algorithm>

namespace Nova {

// =============================================================================
// Shader Sources
// =============================================================================

static const char* s_gizmoVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

uniform mat4 u_MVP;
uniform mat4 u_Model;

out vec3 v_Normal;
out vec3 v_FragPos;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));
}
)";

static const char* s_gizmoFragmentShader = R"(
#version 330 core

in vec3 v_Normal;
in vec3 v_FragPos;

uniform vec4 u_Color;
uniform vec3 u_CameraPos;
uniform bool u_UseLighting;

out vec4 FragColor;

void main() {
    if (u_UseLighting) {
        vec3 normal = normalize(v_Normal);
        vec3 viewDir = normalize(u_CameraPos - v_FragPos);

        // Simple directional light from camera
        float diff = max(dot(normal, viewDir), 0.0) * 0.6 + 0.4;

        FragColor = vec4(u_Color.rgb * diff, u_Color.a);
    } else {
        FragColor = u_Color;
    }
}
)";

static const char* s_lineVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_MVP;

out vec4 v_Color;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    v_Color = a_Color;
}
)";

static const char* s_lineFragmentShader = R"(
#version 330 core

in vec4 v_Color;

out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)";

// =============================================================================
// Constructor / Destructor
// =============================================================================

TransformGizmo::TransformGizmo() = default;

TransformGizmo::~TransformGizmo() {
    Shutdown();
}

TransformGizmo::TransformGizmo(TransformGizmo&& other) noexcept = default;
TransformGizmo& TransformGizmo::operator=(TransformGizmo&& other) noexcept = default;

// =============================================================================
// Initialization
// =============================================================================

bool TransformGizmo::Initialize() {
    if (m_initialized) {
        return true;
    }

    if (!CreateShaders()) {
        return false;
    }

    if (!CreateMeshes()) {
        DestroyShaders();
        return false;
    }

    // Create line rendering buffers
    glGenVertexArrays(1, &m_lineVAO);
    glGenBuffers(1, &m_lineVBO);

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_LINE_VERTICES * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);

    // Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    m_initialized = true;
    return true;
}

void TransformGizmo::Shutdown() {
    if (!m_initialized) {
        return;
    }

    DestroyMeshes();
    DestroyShaders();

    if (m_lineVAO) {
        glDeleteVertexArrays(1, &m_lineVAO);
        m_lineVAO = 0;
    }
    if (m_lineVBO) {
        glDeleteBuffers(1, &m_lineVBO);
        m_lineVBO = 0;
    }

    m_initialized = false;
}

bool TransformGizmo::CreateShaders() {
    m_shader = std::make_unique<Shader>();
    if (!m_shader->LoadFromSource(s_gizmoVertexShader, s_gizmoFragmentShader)) {
        return false;
    }

    m_lineShader = std::make_unique<Shader>();
    if (!m_lineShader->LoadFromSource(s_lineVertexShader, s_lineFragmentShader)) {
        return false;
    }

    return true;
}

void TransformGizmo::DestroyShaders() {
    m_shader.reset();
    m_lineShader.reset();
}

bool TransformGizmo::CreateMeshes() {
    CreateTranslateMeshes();
    CreateRotateMeshes();
    CreateScaleMeshes();
    return true;
}

void TransformGizmo::DestroyMeshes() {
    m_arrowMesh.reset();
    m_planeMesh.reset();
    m_torusMesh.reset();
    m_circleMesh.reset();
    m_scaleCubeMesh.reset();
    m_scaleLineMesh.reset();
    m_centerCubeMesh.reset();
    m_coneMesh.reset();
}

void TransformGizmo::CreateTranslateMeshes() {
    // Create arrow (cylinder + cone)
    const int segments = 16;
    const float cylinderRadius = 0.02f;
    const float cylinderLength = 0.85f;
    const float coneRadius = 0.08f;
    const float coneLength = 0.15f;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Cylinder body (along +X axis)
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * glm::two_pi<float>();
        float y = std::cos(angle) * cylinderRadius;
        float z = std::sin(angle) * cylinderRadius;
        glm::vec3 normal(0.0f, std::cos(angle), std::sin(angle));

        // Start cap vertex
        vertices.push_back({glm::vec3(0.0f, y, z), normal, glm::vec2(0), glm::vec3(0), glm::vec3(0)});
        // End cap vertex
        vertices.push_back({glm::vec3(cylinderLength, y, z), normal, glm::vec2(0), glm::vec3(0), glm::vec3(0)});
    }

    // Cylinder indices
    for (int i = 0; i < segments; ++i) {
        int base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    m_arrowMesh = std::make_unique<Mesh>();
    m_arrowMesh->Create(vertices, indices);

    // Create cone (arrow head)
    vertices.clear();
    indices.clear();

    // Cone tip
    vertices.push_back({glm::vec3(coneLength, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0), glm::vec3(0), glm::vec3(0)});

    // Cone base vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * glm::two_pi<float>();
        float y = std::cos(angle) * coneRadius;
        float z = std::sin(angle) * coneRadius;

        // Compute normal for cone surface
        glm::vec3 toTip(coneLength, -y, -z);
        glm::vec3 tangent(-z, 0.0f, y);
        glm::vec3 normal = glm::normalize(glm::cross(tangent, toTip));

        vertices.push_back({glm::vec3(0.0f, y, z), normal, glm::vec2(0), glm::vec3(0), glm::vec3(0)});
    }

    // Cone indices
    for (int i = 1; i <= segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i);
    }

    m_coneMesh = std::make_unique<Mesh>();
    m_coneMesh->Create(vertices, indices);

    // Create plane quad for XY/XZ/YZ plane handles
    vertices.clear();
    indices.clear();

    float planeOffset = 0.3f;
    float planeSize = m_planeSize;

    vertices.push_back({glm::vec3(planeOffset, planeOffset, 0.0f), glm::vec3(0, 0, 1), glm::vec2(0, 0), glm::vec3(0), glm::vec3(0)});
    vertices.push_back({glm::vec3(planeOffset + planeSize, planeOffset, 0.0f), glm::vec3(0, 0, 1), glm::vec2(1, 0), glm::vec3(0), glm::vec3(0)});
    vertices.push_back({glm::vec3(planeOffset + planeSize, planeOffset + planeSize, 0.0f), glm::vec3(0, 0, 1), glm::vec2(1, 1), glm::vec3(0), glm::vec3(0)});
    vertices.push_back({glm::vec3(planeOffset, planeOffset + planeSize, 0.0f), glm::vec3(0, 0, 1), glm::vec2(0, 1), glm::vec3(0), glm::vec3(0)});

    indices = {0, 1, 2, 0, 2, 3};

    m_planeMesh = std::make_unique<Mesh>();
    m_planeMesh->Create(vertices, indices);
}

void TransformGizmo::CreateRotateMeshes() {
    const int segments = 64;
    const float majorRadius = m_rotateRadius;
    const float minorRadius = 0.02f;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Create torus for solid rotation ring
    const int rings = 8;
    for (int i = 0; i <= segments; ++i) {
        float u = (float)i / segments * glm::two_pi<float>();
        float cu = std::cos(u);
        float su = std::sin(u);

        for (int j = 0; j <= rings; ++j) {
            float v = (float)j / rings * glm::two_pi<float>();
            float cv = std::cos(v);
            float sv = std::sin(v);

            // Torus position (ring in XY plane, rotating around Y axis)
            float x = (majorRadius + minorRadius * cv) * cu;
            float y = minorRadius * sv;
            float z = (majorRadius + minorRadius * cv) * su;

            // Normal
            glm::vec3 normal(cv * cu, sv, cv * su);

            vertices.push_back({glm::vec3(x, y, z), normal, glm::vec2(0), glm::vec3(0), glm::vec3(0)});
        }
    }

    // Torus indices
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < rings; ++j) {
            int cur = i * (rings + 1) + j;
            int next = (i + 1) * (rings + 1) + j;

            indices.push_back(cur);
            indices.push_back(next);
            indices.push_back(cur + 1);

            indices.push_back(next);
            indices.push_back(next + 1);
            indices.push_back(cur + 1);
        }
    }

    m_torusMesh = std::make_unique<Mesh>();
    m_torusMesh->Create(vertices, indices);

    // Create circle outline mesh (for thinner visual)
    vertices.clear();
    indices.clear();

    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * glm::two_pi<float>();
        float x = std::cos(angle) * majorRadius;
        float z = std::sin(angle) * majorRadius;

        vertices.push_back({glm::vec3(x, 0.0f, z), glm::vec3(0, 1, 0), glm::vec2(0), glm::vec3(0), glm::vec3(0)});
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    m_circleMesh = std::make_unique<Mesh>();
    m_circleMesh->Create(vertices, indices);
}

void TransformGizmo::CreateScaleMeshes() {
    // Create small cube for scale handle ends
    float size = m_scaleBoxSize;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Cube vertices
    glm::vec3 positions[] = {
        // Front face
        {-size, -size,  size}, { size, -size,  size}, { size,  size,  size}, {-size,  size,  size},
        // Back face
        { size, -size, -size}, {-size, -size, -size}, {-size,  size, -size}, { size,  size, -size},
        // Top face
        {-size,  size,  size}, { size,  size,  size}, { size,  size, -size}, {-size,  size, -size},
        // Bottom face
        {-size, -size, -size}, { size, -size, -size}, { size, -size,  size}, {-size, -size,  size},
        // Right face
        { size, -size,  size}, { size, -size, -size}, { size,  size, -size}, { size,  size,  size},
        // Left face
        {-size, -size, -size}, {-size, -size,  size}, {-size,  size,  size}, {-size,  size, -size}
    };

    glm::vec3 normals[] = {
        {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1},
        {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
        {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0},
        {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0},
        {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0},
        {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}
    };

    for (int i = 0; i < 24; ++i) {
        vertices.push_back({positions[i], normals[i], glm::vec2(0), glm::vec3(0), glm::vec3(0)});
    }

    // Cube indices
    for (int face = 0; face < 6; ++face) {
        int base = face * 4;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    m_scaleCubeMesh = std::make_unique<Mesh>();
    m_scaleCubeMesh->Create(vertices, indices);

    // Center cube (slightly larger, for uniform scale)
    size = m_scaleBoxSize * 1.5f;
    vertices.clear();

    for (int i = 0; i < 24; ++i) {
        glm::vec3 pos = positions[i] * (1.5f);
        vertices.push_back({pos, normals[i], glm::vec2(0), glm::vec3(0), glm::vec3(0)});
    }

    m_centerCubeMesh = std::make_unique<Mesh>();
    m_centerCubeMesh->Create(vertices, indices);
}

// =============================================================================
// Configuration
// =============================================================================

void TransformGizmo::SetSnapValues(float translate, float rotate, float scale) {
    m_snapping.translateSnap = translate;
    m_snapping.rotateSnap = rotate;
    m_snapping.scaleSnap = scale;
}

void TransformGizmo::SetAxisColors(const glm::vec4& xColor, const glm::vec4& yColor, const glm::vec4& zColor) {
    m_xAxisColor = xColor;
    m_yAxisColor = yColor;
    m_zAxisColor = zColor;
}

// =============================================================================
// Transform Management
// =============================================================================

void TransformGizmo::SetTransform(const glm::vec3& position, const glm::quat& rotation) {
    m_position = position;
    m_rotation = rotation;
}

void TransformGizmo::SetTransform(const glm::mat4& transform) {
    // Extract position
    m_position = glm::vec3(transform[3]);

    // Extract rotation (assumes no shear)
    glm::vec3 scale;
    scale.x = glm::length(glm::vec3(transform[0]));
    scale.y = glm::length(glm::vec3(transform[1]));
    scale.z = glm::length(glm::vec3(transform[2]));

    glm::mat3 rotMat(
        glm::vec3(transform[0]) / scale.x,
        glm::vec3(transform[1]) / scale.y,
        glm::vec3(transform[2]) / scale.z
    );

    m_rotation = glm::quat_cast(rotMat);
    m_scale = scale;
}

glm::mat4 TransformGizmo::GetTransformMatrix() const {
    glm::mat4 result = glm::translate(glm::mat4(1.0f), m_position);
    result *= glm::toMat4(m_rotation);
    result = glm::scale(result, m_scale);
    return result;
}

// =============================================================================
// Interaction
// =============================================================================

GizmoResult TransformGizmo::Update(const Camera& camera, const InputManager& input,
                                   const glm::vec2& screenSize) {
    glm::vec2 mousePos = input.GetMousePosition();
    bool mouseDown = input.IsMouseButtonDown(MouseButton::Left);

    return Update(camera, mousePos, mouseDown, screenSize);
}

GizmoResult TransformGizmo::Update(const Camera& camera, const glm::vec2& mousePos,
                                   bool mouseDown, const glm::vec2& screenSize) {
    GizmoResult result;

    if (!m_enabled || !m_visible || !m_initialized) {
        return result;
    }

    // Hit test when not actively manipulating
    if (!m_isActive) {
        m_hoveredAxis = HitTest(camera, mousePos, screenSize);

        // Begin manipulation on mouse press over a handle
        if (mouseDown && m_hoveredAxis != GizmoAxis::None) {
            BeginManipulation(camera, mousePos, screenSize);
        }
    }

    // Continue manipulation
    if (m_isActive) {
        if (mouseDown) {
            result = ContinueManipulation(camera, mousePos, screenSize);
        } else {
            EndManipulation();
        }
    }

    result.isActive = m_isActive;
    result.activeAxis = m_activeAxis;

    return result;
}

void TransformGizmo::BeginManipulation(const Camera& camera, const glm::vec2& mousePos,
                                       const glm::vec2& screenSize) {
    m_isActive = true;
    m_activeAxis = m_hoveredAxis;
    m_dragStartMouse = mousePos;
    m_dragStartPosition = m_position;
    m_dragStartRotation = m_rotation;
    m_dragStartScale = m_scale;
    m_lastTranslation = glm::vec3(0.0f);
    m_lastRotation = glm::quat(1, 0, 0, 0);
    m_lastScale = glm::vec3(1.0f);
    m_accumulatedRotation = 0.0f;

    // Compute drag plane for translation/scale
    glm::mat4 orientation = GetGizmoOrientation();
    glm::vec3 viewDir = glm::normalize(m_position - camera.GetPosition());

    switch (m_activeAxis) {
        case GizmoAxis::X:
            m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            // Choose plane perpendicular to X that faces camera most
            if (std::abs(glm::dot(viewDir, glm::vec3(orientation * glm::vec4(0, 1, 0, 0)))) >
                std::abs(glm::dot(viewDir, glm::vec3(orientation * glm::vec4(0, 0, 1, 0))))) {
                m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(0, 1, 0, 0));
            } else {
                m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(0, 0, 1, 0));
            }
            break;
        case GizmoAxis::Y:
            if (std::abs(glm::dot(viewDir, glm::vec3(orientation * glm::vec4(1, 0, 0, 0)))) >
                std::abs(glm::dot(viewDir, glm::vec3(orientation * glm::vec4(0, 0, 1, 0))))) {
                m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            } else {
                m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(0, 0, 1, 0));
            }
            break;
        case GizmoAxis::Z:
            if (std::abs(glm::dot(viewDir, glm::vec3(orientation * glm::vec4(1, 0, 0, 0)))) >
                std::abs(glm::dot(viewDir, glm::vec3(orientation * glm::vec4(0, 1, 0, 0))))) {
                m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            } else {
                m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(0, 1, 0, 0));
            }
            break;
        case GizmoAxis::XY:
            m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(0, 0, 1, 0));
            break;
        case GizmoAxis::XZ:
            m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(0, 1, 0, 0));
            break;
        case GizmoAxis::YZ:
            m_dragPlaneNormal = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            break;
        case GizmoAxis::XYZ:
        case GizmoAxis::View:
            m_dragPlaneNormal = -viewDir;
            break;
        default:
            m_dragPlaneNormal = glm::vec3(0, 1, 0);
            break;
    }

    // Find initial hit point
    glm::vec3 rayOrigin = camera.GetPosition();
    glm::vec3 rayDir = ScreenToWorldRay(camera, mousePos, screenSize);
    float dist;
    RayPlaneTest(rayOrigin, rayDir, m_position, m_dragPlaneNormal, dist, m_dragStartHitPoint);

    // For rotation, calculate starting angle
    if (m_mode == GizmoMode::Rotate) {
        glm::vec3 toHit = glm::normalize(m_dragStartHitPoint - m_position);
        glm::mat4 ori = GetGizmoOrientation();
        glm::vec3 rotAxis;

        switch (m_activeAxis) {
            case GizmoAxis::X:
                rotAxis = glm::vec3(ori * glm::vec4(1, 0, 0, 0));
                break;
            case GizmoAxis::Y:
                rotAxis = glm::vec3(ori * glm::vec4(0, 1, 0, 0));
                break;
            case GizmoAxis::Z:
                rotAxis = glm::vec3(ori * glm::vec4(0, 0, 1, 0));
                break;
            default:
                rotAxis = -camera.GetForward();
                break;
        }

        // Project hit point onto rotation plane
        glm::vec3 projected = toHit - glm::dot(toHit, rotAxis) * rotAxis;
        if (glm::length(projected) > 0.001f) {
            projected = glm::normalize(projected);
        }

        // Calculate reference vector perpendicular to rotation axis
        glm::vec3 refVec;
        if (std::abs(rotAxis.y) < 0.99f) {
            refVec = glm::normalize(glm::cross(rotAxis, glm::vec3(0, 1, 0)));
        } else {
            refVec = glm::normalize(glm::cross(rotAxis, glm::vec3(1, 0, 0)));
        }

        m_dragStartAngle = std::atan2(glm::dot(glm::cross(refVec, projected), rotAxis),
                                      glm::dot(refVec, projected));
    }
}

GizmoResult TransformGizmo::ContinueManipulation(const Camera& camera, const glm::vec2& mousePos,
                                                 const glm::vec2& screenSize) {
    GizmoResult result;
    result.isActive = true;
    result.activeAxis = m_activeAxis;

    switch (m_mode) {
        case GizmoMode::Translate: {
            glm::vec3 translation = ComputeTranslation(camera, mousePos, screenSize);
            glm::vec3 delta = translation - m_lastTranslation;
            m_lastTranslation = translation;

            if (glm::length(delta) > 0.0001f) {
                result.wasModified = true;
                result.translationDelta = delta;
                m_position = m_dragStartPosition + translation;

                if (m_onTransformChanged) {
                    m_onTransformChanged(delta, glm::quat(1, 0, 0, 0), glm::vec3(1));
                }
            }
            break;
        }
        case GizmoMode::Rotate: {
            glm::quat rotation = ComputeRotation(camera, mousePos, screenSize);
            glm::quat delta = rotation * glm::inverse(m_lastRotation);
            m_lastRotation = rotation;

            if (std::abs(glm::angle(delta)) > 0.0001f) {
                result.wasModified = true;
                result.rotationDelta = delta;
                m_rotation = rotation * m_dragStartRotation;

                if (m_onTransformChanged) {
                    m_onTransformChanged(glm::vec3(0), delta, glm::vec3(1));
                }
            }
            break;
        }
        case GizmoMode::Scale: {
            glm::vec3 scale = ComputeScale(camera, mousePos, screenSize);
            glm::vec3 delta = scale / m_lastScale;
            m_lastScale = scale;

            if (glm::length(delta - glm::vec3(1.0f)) > 0.0001f) {
                result.wasModified = true;
                result.scaleDelta = delta;
                m_scale = m_dragStartScale * scale;

                if (m_onTransformChanged) {
                    m_onTransformChanged(glm::vec3(0), glm::quat(1, 0, 0, 0), delta);
                }
            }
            break;
        }
    }

    return result;
}

void TransformGizmo::EndManipulation() {
    m_isActive = false;
    m_activeAxis = GizmoAxis::None;
}

void TransformGizmo::CancelManipulation() {
    if (m_isActive) {
        m_position = m_dragStartPosition;
        m_rotation = m_dragStartRotation;
        m_scale = m_dragStartScale;
        EndManipulation();
    }
}

glm::vec3 TransformGizmo::ComputeTranslation(const Camera& camera, const glm::vec2& mousePos,
                                             const glm::vec2& screenSize) {
    glm::vec3 rayOrigin = camera.GetPosition();
    glm::vec3 rayDir = ScreenToWorldRay(camera, mousePos, screenSize);

    float dist;
    glm::vec3 hitPoint;
    if (!RayPlaneTest(rayOrigin, rayDir, m_dragStartPosition, m_dragPlaneNormal, dist, hitPoint)) {
        return m_lastTranslation;
    }

    glm::vec3 delta = hitPoint - m_dragStartHitPoint;
    glm::mat4 orientation = GetGizmoOrientation();

    // Constrain to selected axis/plane
    switch (m_activeAxis) {
        case GizmoAxis::X: {
            glm::vec3 axis = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            delta = glm::dot(delta, axis) * axis;
            break;
        }
        case GizmoAxis::Y: {
            glm::vec3 axis = glm::vec3(orientation * glm::vec4(0, 1, 0, 0));
            delta = glm::dot(delta, axis) * axis;
            break;
        }
        case GizmoAxis::Z: {
            glm::vec3 axis = glm::vec3(orientation * glm::vec4(0, 0, 1, 0));
            delta = glm::dot(delta, axis) * axis;
            break;
        }
        case GizmoAxis::XY: {
            glm::vec3 axisZ = glm::vec3(orientation * glm::vec4(0, 0, 1, 0));
            delta = delta - glm::dot(delta, axisZ) * axisZ;
            break;
        }
        case GizmoAxis::XZ: {
            glm::vec3 axisY = glm::vec3(orientation * glm::vec4(0, 1, 0, 0));
            delta = delta - glm::dot(delta, axisY) * axisY;
            break;
        }
        case GizmoAxis::YZ: {
            glm::vec3 axisX = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            delta = delta - glm::dot(delta, axisX) * axisX;
            break;
        }
        default:
            break;
    }

    if (m_snapping.enabled) {
        delta = ApplyTranslationSnap(delta);
    }

    return delta;
}

glm::quat TransformGizmo::ComputeRotation(const Camera& camera, const glm::vec2& mousePos,
                                          const glm::vec2& screenSize) {
    glm::vec3 rayOrigin = camera.GetPosition();
    glm::vec3 rayDir = ScreenToWorldRay(camera, mousePos, screenSize);

    glm::mat4 orientation = GetGizmoOrientation();
    glm::vec3 rotAxis;

    switch (m_activeAxis) {
        case GizmoAxis::X:
            rotAxis = glm::vec3(orientation * glm::vec4(1, 0, 0, 0));
            break;
        case GizmoAxis::Y:
            rotAxis = glm::vec3(orientation * glm::vec4(0, 1, 0, 0));
            break;
        case GizmoAxis::Z:
            rotAxis = glm::vec3(orientation * glm::vec4(0, 0, 1, 0));
            break;
        case GizmoAxis::View:
            rotAxis = -camera.GetForward();
            break;
        default:
            return glm::quat(1, 0, 0, 0);
    }

    // Intersect with rotation plane
    float dist;
    glm::vec3 hitPoint;
    if (!RayPlaneTest(rayOrigin, rayDir, m_position, rotAxis, dist, hitPoint)) {
        return m_lastRotation;
    }

    // Calculate angle from center
    glm::vec3 toHit = hitPoint - m_position;
    if (glm::length(toHit) < 0.001f) {
        return m_lastRotation;
    }

    toHit = glm::normalize(toHit);

    // Project onto rotation plane
    glm::vec3 projected = toHit - glm::dot(toHit, rotAxis) * rotAxis;
    if (glm::length(projected) < 0.001f) {
        return m_lastRotation;
    }
    projected = glm::normalize(projected);

    // Calculate reference vector
    glm::vec3 refVec;
    if (std::abs(rotAxis.y) < 0.99f) {
        refVec = glm::normalize(glm::cross(rotAxis, glm::vec3(0, 1, 0)));
    } else {
        refVec = glm::normalize(glm::cross(rotAxis, glm::vec3(1, 0, 0)));
    }

    float currentAngle = std::atan2(glm::dot(glm::cross(refVec, projected), rotAxis),
                                    glm::dot(refVec, projected));

    float angleDelta = currentAngle - m_dragStartAngle;

    // Convert to degrees for snapping
    float angleDegrees = glm::degrees(angleDelta);
    if (m_snapping.enabled) {
        angleDegrees = ApplyRotationSnap(angleDegrees);
    }

    return glm::angleAxis(glm::radians(angleDegrees), rotAxis);
}

glm::vec3 TransformGizmo::ComputeScale(const Camera& camera, const glm::vec2& mousePos,
                                       const glm::vec2& screenSize) {
    glm::vec3 rayOrigin = camera.GetPosition();
    glm::vec3 rayDir = ScreenToWorldRay(camera, mousePos, screenSize);

    float dist;
    glm::vec3 hitPoint;
    if (!RayPlaneTest(rayOrigin, rayDir, m_dragStartPosition, m_dragPlaneNormal, dist, hitPoint)) {
        return m_lastScale;
    }

    // Calculate scale based on distance from center
    float startDist = glm::length(m_dragStartHitPoint - m_dragStartPosition);
    float currentDist = glm::length(hitPoint - m_dragStartPosition);

    if (startDist < 0.001f) {
        return m_lastScale;
    }

    float scaleFactor = currentDist / startDist;
    scaleFactor = std::max(0.01f, scaleFactor); // Prevent negative/zero scale

    glm::vec3 scale(1.0f);
    glm::mat4 orientation = GetGizmoOrientation();

    switch (m_activeAxis) {
        case GizmoAxis::X:
            scale.x = scaleFactor;
            break;
        case GizmoAxis::Y:
            scale.y = scaleFactor;
            break;
        case GizmoAxis::Z:
            scale.z = scaleFactor;
            break;
        case GizmoAxis::XY:
            scale.x = scale.y = scaleFactor;
            break;
        case GizmoAxis::XZ:
            scale.x = scale.z = scaleFactor;
            break;
        case GizmoAxis::YZ:
            scale.y = scale.z = scaleFactor;
            break;
        case GizmoAxis::XYZ:
            scale = glm::vec3(scaleFactor);
            break;
        default:
            break;
    }

    if (m_snapping.enabled) {
        scale = ApplyScaleSnap(scale);
    }

    return scale;
}

// =============================================================================
// Snapping
// =============================================================================

float TransformGizmo::ApplySnap(float value, float snapInterval) {
    if (snapInterval <= 0.0f) return value;
    return std::round(value / snapInterval) * snapInterval;
}

glm::vec3 TransformGizmo::ApplyTranslationSnap(const glm::vec3& translation) {
    return glm::vec3(
        ApplySnap(translation.x, m_snapping.translateSnap),
        ApplySnap(translation.y, m_snapping.translateSnap),
        ApplySnap(translation.z, m_snapping.translateSnap)
    );
}

float TransformGizmo::ApplyRotationSnap(float angleDegrees) {
    return ApplySnap(angleDegrees, m_snapping.rotateSnap);
}

glm::vec3 TransformGizmo::ApplyScaleSnap(const glm::vec3& scale) {
    return glm::vec3(
        ApplySnap(scale.x, m_snapping.scaleSnap),
        ApplySnap(scale.y, m_snapping.scaleSnap),
        ApplySnap(scale.z, m_snapping.scaleSnap)
    );
}

// =============================================================================
// Hit Testing
// =============================================================================

GizmoAxis TransformGizmo::HitTest(const Camera& camera, const glm::vec2& mousePos,
                                  const glm::vec2& screenSize) {
    glm::vec3 rayOrigin = camera.GetPosition();
    glm::vec3 rayDir = ScreenToWorldRay(camera, mousePos, screenSize);

    float scale = ComputeScreenScale(camera);
    glm::mat4 orientation = GetGizmoOrientation();

    float closestDist = std::numeric_limits<float>::max();
    GizmoAxis closest = GizmoAxis::None;

    auto testAxis = [&](GizmoAxis axis, const glm::vec3& dir, float length, float radius) {
        glm::vec3 worldDir = glm::vec3(orientation * glm::vec4(dir, 0.0f));
        float dist;
        if (RayAxisTest(rayOrigin, rayDir, m_position, worldDir, length * scale, radius * scale, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closest = axis;
            }
        }
    };

    auto testPlane = [&](GizmoAxis axis, const glm::vec3& normal, const glm::vec3& offset1, const glm::vec3& offset2) {
        if (m_mode != GizmoMode::Translate) return;

        glm::vec3 worldNormal = glm::vec3(orientation * glm::vec4(normal, 0.0f));
        glm::vec3 worldOffset1 = glm::vec3(orientation * glm::vec4(offset1, 0.0f));
        glm::vec3 worldOffset2 = glm::vec3(orientation * glm::vec4(offset2, 0.0f));

        float planeOffset = 0.3f * scale;
        float planeSize = m_planeSize * scale;

        glm::vec3 planeCenter = m_position + worldOffset1 * (planeOffset + planeSize * 0.5f)
                                           + worldOffset2 * (planeOffset + planeSize * 0.5f);

        float dist;
        glm::vec3 hitPoint;
        if (RayPlaneTest(rayOrigin, rayDir, planeCenter, worldNormal, dist, hitPoint)) {
            glm::vec3 localHit = hitPoint - m_position;
            float u = glm::dot(localHit, worldOffset1) / scale;
            float v = glm::dot(localHit, worldOffset2) / scale;

            if (u >= planeOffset / scale && u <= (planeOffset + planeSize) / scale &&
                v >= planeOffset / scale && v <= (planeOffset + planeSize) / scale) {
                if (dist < closestDist) {
                    closestDist = dist;
                    closest = axis;
                }
            }
        }
    };

    auto testRotationRing = [&](GizmoAxis axis, const glm::vec3& normal) {
        if (m_mode != GizmoMode::Rotate) return;

        glm::vec3 worldNormal = glm::vec3(orientation * glm::vec4(normal, 0.0f));
        float radius = m_rotateRadius * scale;
        float thickness = 0.05f * scale;

        float dist;
        if (RayTorusTest(rayOrigin, rayDir, m_position, worldNormal, radius, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closest = axis;
            }
        }
    };

    // Test based on current mode
    switch (m_mode) {
        case GizmoMode::Translate:
            // Test plane handles first (smaller, need priority)
            testPlane(GizmoAxis::XY, glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0));
            testPlane(GizmoAxis::XZ, glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1));
            testPlane(GizmoAxis::YZ, glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));

            // Test axis handles
            testAxis(GizmoAxis::X, glm::vec3(1, 0, 0), m_handleLength, m_handleRadius);
            testAxis(GizmoAxis::Y, glm::vec3(0, 1, 0), m_handleLength, m_handleRadius);
            testAxis(GizmoAxis::Z, glm::vec3(0, 0, 1), m_handleLength, m_handleRadius);
            break;

        case GizmoMode::Rotate:
            testRotationRing(GizmoAxis::X, glm::vec3(1, 0, 0));
            testRotationRing(GizmoAxis::Y, glm::vec3(0, 1, 0));
            testRotationRing(GizmoAxis::Z, glm::vec3(0, 0, 1));

            // View-aligned ring
            {
                float dist;
                if (RayTorusTest(rayOrigin, rayDir, m_position, -camera.GetForward(),
                                m_rotateRadius * 1.1f * scale, 0.03f * scale, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        closest = GizmoAxis::View;
                    }
                }
            }
            break;

        case GizmoMode::Scale:
            // Test center cube first
            {
                float dist;
                if (RaySphereTest(rayOrigin, rayDir, m_position, m_scaleBoxSize * 2.0f * scale, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        closest = GizmoAxis::XYZ;
                    }
                }
            }

            // Test axis handles
            testAxis(GizmoAxis::X, glm::vec3(1, 0, 0), m_handleLength, m_handleRadius);
            testAxis(GizmoAxis::Y, glm::vec3(0, 1, 0), m_handleLength, m_handleRadius);
            testAxis(GizmoAxis::Z, glm::vec3(0, 0, 1), m_handleLength, m_handleRadius);
            break;
    }

    return closest;
}

bool TransformGizmo::RayAxisTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                 const glm::vec3& axisOrigin, const glm::vec3& axisDir,
                                 float length, float radius, float& outDistance) {
    // Find closest points between ray and axis line
    glm::vec3 w = rayOrigin - axisOrigin;
    float a = glm::dot(rayDir, rayDir);
    float b = glm::dot(rayDir, axisDir);
    float c = glm::dot(axisDir, axisDir);
    float d = glm::dot(rayDir, w);
    float e = glm::dot(axisDir, w);

    float denom = a * c - b * b;
    if (std::abs(denom) < 0.0001f) {
        return false; // Parallel
    }

    float s = (b * e - c * d) / denom;
    float t = (a * e - b * d) / denom;

    // Check if point is within axis length
    if (t < 0.0f || t > length) {
        return false;
    }

    // Check if point is within radius
    glm::vec3 closestOnRay = rayOrigin + rayDir * s;
    glm::vec3 closestOnAxis = axisOrigin + axisDir * t;
    float dist = glm::length(closestOnRay - closestOnAxis);

    if (dist > radius) {
        return false;
    }

    outDistance = s;
    return s > 0.0f;
}

bool TransformGizmo::RayPlaneTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                  const glm::vec3& planeOrigin, const glm::vec3& planeNormal,
                                  float& outDistance, glm::vec3& outHitPoint) {
    float denom = glm::dot(planeNormal, rayDir);
    if (std::abs(denom) < 0.0001f) {
        return false;
    }

    float t = glm::dot(planeOrigin - rayOrigin, planeNormal) / denom;
    if (t < 0.0f) {
        return false;
    }

    outDistance = t;
    outHitPoint = rayOrigin + rayDir * t;
    return true;
}

bool TransformGizmo::RayTorusTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                  const glm::vec3& center, const glm::vec3& normal,
                                  float majorRadius, float minorRadius, float& outDistance) {
    // Simplified torus intersection: treat as ring with thickness
    // First intersect with the plane of the torus
    float denom = glm::dot(normal, rayDir);
    if (std::abs(denom) < 0.0001f) {
        // Ray parallel to plane - check if close enough
        float planeDist = glm::dot(center - rayOrigin, normal);
        if (std::abs(planeDist) > minorRadius) {
            return false;
        }
    }

    float t = glm::dot(center - rayOrigin, normal) / (std::abs(denom) > 0.0001f ? denom : 0.0001f);
    if (t < 0.0f) {
        return false;
    }

    glm::vec3 hitPoint = rayOrigin + rayDir * t;
    float distFromCenter = glm::length(hitPoint - center);

    // Check if hit is on the ring (within major radius +/- minor radius)
    if (distFromCenter >= majorRadius - minorRadius && distFromCenter <= majorRadius + minorRadius) {
        // Additional check: distance from actual torus surface
        float ringDist = std::abs(distFromCenter - majorRadius);
        float planeDist = std::abs(glm::dot(hitPoint - center, normal));

        if (ringDist * ringDist + planeDist * planeDist <= minorRadius * minorRadius * 4.0f) {
            outDistance = t;
            return true;
        }
    }

    return false;
}

bool TransformGizmo::RaySphereTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                   const glm::vec3& center, float radius, float& outDistance) {
    glm::vec3 oc = rayOrigin - center;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return false;
    }

    float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (t < 0) {
        t = (-b + std::sqrt(discriminant)) / (2.0f * a);
    }

    if (t < 0) {
        return false;
    }

    outDistance = t;
    return true;
}

// =============================================================================
// Rendering
// =============================================================================

void TransformGizmo::Render(const Camera& camera) {
    Render(camera.GetView(), camera.GetProjection(), camera.GetPosition());
}

void TransformGizmo::Render(const glm::mat4& view, const glm::mat4& projection,
                            const glm::vec3& cameraPosition) {
    if (!m_visible || !m_enabled || !m_initialized) {
        return;
    }

    // Calculate screen-space scale
    float distToCamera = glm::length(m_position - cameraPosition);
    float scale = distToCamera * m_screenSize / 1000.0f * m_baseScale;

    // Setup OpenGL state
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    switch (m_mode) {
        case GizmoMode::Translate:
            RenderTranslateGizmo(view, projection, scale);
            break;
        case GizmoMode::Rotate:
            RenderRotateGizmo(view, projection, scale);
            break;
        case GizmoMode::Scale:
            RenderScaleGizmo(view, projection, scale);
            break;
    }

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void TransformGizmo::RenderTranslateGizmo(const glm::mat4& view, const glm::mat4& projection, float scale) {
    glm::mat4 orientation = GetGizmoOrientation();
    glm::mat4 baseTransform = glm::translate(glm::mat4(1.0f), m_position) * orientation;

    m_shader->Bind();

    // Render each axis arrow
    auto renderArrow = [&](GizmoAxis axis, const glm::vec3& dir, const glm::vec4& color) {
        bool highlighted = (m_hoveredAxis == axis || m_activeAxis == axis);
        glm::vec4 finalColor = GetAxisColor(axis, highlighted, m_activeAxis == axis);

        // Arrow shaft
        glm::mat4 arrowTransform = baseTransform;
        if (dir.x > 0.5f) {
            // X axis - default orientation
        } else if (dir.y > 0.5f) {
            arrowTransform *= glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0, 0, 1));
        } else {
            arrowTransform *= glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(0, 1, 0));
        }
        arrowTransform = glm::scale(arrowTransform, glm::vec3(scale));

        glm::mat4 mvp = projection * view * arrowTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", arrowTransform);
        m_shader->SetVec4("u_Color", finalColor);
        m_shader->SetBool("u_UseLighting", true);

        if (m_arrowMesh) {
            m_arrowMesh->Draw();
        }

        // Arrow head (cone)
        glm::mat4 coneTransform = arrowTransform;
        coneTransform = glm::translate(coneTransform, glm::vec3(0.85f, 0.0f, 0.0f));

        mvp = projection * view * coneTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", coneTransform);

        if (m_coneMesh) {
            m_coneMesh->Draw();
        }
    };

    renderArrow(GizmoAxis::X, glm::vec3(1, 0, 0), m_xAxisColor);
    renderArrow(GizmoAxis::Y, glm::vec3(0, 1, 0), m_yAxisColor);
    renderArrow(GizmoAxis::Z, glm::vec3(0, 0, 1), m_zAxisColor);

    // Render plane handles
    auto renderPlane = [&](GizmoAxis axis, const glm::mat4& planeRotation, const glm::vec4& color1, const glm::vec4& color2) {
        bool highlighted = (m_hoveredAxis == axis || m_activeAxis == axis);
        glm::vec4 finalColor = (color1 + color2) * 0.5f;
        finalColor.a = highlighted ? 0.6f : 0.3f;

        glm::mat4 planeTransform = baseTransform * planeRotation;
        planeTransform = glm::scale(planeTransform, glm::vec3(scale));

        glm::mat4 mvp = projection * view * planeTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", planeTransform);
        m_shader->SetVec4("u_Color", finalColor);
        m_shader->SetBool("u_UseLighting", false);

        if (m_planeMesh) {
            m_planeMesh->Draw();
        }
    };

    // XY plane (rotate so it faces Z)
    renderPlane(GizmoAxis::XY, glm::mat4(1.0f), m_xAxisColor, m_yAxisColor);

    // XZ plane (rotate around X by -90 degrees)
    renderPlane(GizmoAxis::XZ, glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(1, 0, 0)),
                m_xAxisColor, m_zAxisColor);

    // YZ plane (rotate around Y by 90 degrees)
    renderPlane(GizmoAxis::YZ, glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0, 1, 0)),
                m_yAxisColor, m_zAxisColor);
}

void TransformGizmo::RenderRotateGizmo(const glm::mat4& view, const glm::mat4& projection, float scale) {
    glm::mat4 orientation = GetGizmoOrientation();
    glm::mat4 baseTransform = glm::translate(glm::mat4(1.0f), m_position) * orientation;

    m_shader->Bind();

    auto renderRing = [&](GizmoAxis axis, const glm::mat4& ringRotation, const glm::vec4& color) {
        bool highlighted = (m_hoveredAxis == axis || m_activeAxis == axis);
        glm::vec4 finalColor = GetAxisColor(axis, highlighted, m_activeAxis == axis);

        glm::mat4 ringTransform = baseTransform * ringRotation;
        ringTransform = glm::scale(ringTransform, glm::vec3(scale));

        glm::mat4 mvp = projection * view * ringTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", ringTransform);
        m_shader->SetVec4("u_Color", finalColor);
        m_shader->SetBool("u_UseLighting", true);

        if (m_torusMesh) {
            m_torusMesh->Draw();
        }
    };

    // X rotation ring (normal along X) - rotate so ring is in YZ plane
    renderRing(GizmoAxis::X, glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0, 0, 1)),
               m_xAxisColor);

    // Y rotation ring (normal along Y) - default orientation
    renderRing(GizmoAxis::Y, glm::mat4(1.0f), m_yAxisColor);

    // Z rotation ring (normal along Z) - rotate so ring is in XY plane
    renderRing(GizmoAxis::Z, glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(1, 0, 0)),
               m_zAxisColor);

    // View-aligned ring (slightly larger)
    {
        bool highlighted = (m_hoveredAxis == GizmoAxis::View || m_activeAxis == GizmoAxis::View);
        glm::vec4 finalColor = m_viewAxisColor;
        if (highlighted) {
            finalColor *= m_highlightIntensity;
            finalColor.a = 1.0f;
        }

        // Create view-aligned rotation
        glm::mat4 viewRingTransform = glm::translate(glm::mat4(1.0f), m_position);
        glm::mat4 invView = glm::inverse(view);
        glm::mat3 viewRot = glm::mat3(invView);
        viewRingTransform *= glm::mat4(viewRot);
        viewRingTransform = glm::scale(viewRingTransform, glm::vec3(scale * 1.1f));

        glm::mat4 mvp = projection * view * viewRingTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", viewRingTransform);
        m_shader->SetVec4("u_Color", finalColor);
        m_shader->SetBool("u_UseLighting", false);

        if (m_torusMesh) {
            m_torusMesh->Draw();
        }
    }
}

void TransformGizmo::RenderScaleGizmo(const glm::mat4& view, const glm::mat4& projection, float scale) {
    glm::mat4 orientation = GetGizmoOrientation();
    glm::mat4 baseTransform = glm::translate(glm::mat4(1.0f), m_position) * orientation;

    m_shader->Bind();

    // Render center cube for uniform scale
    {
        bool highlighted = (m_hoveredAxis == GizmoAxis::XYZ || m_activeAxis == GizmoAxis::XYZ);
        glm::vec4 finalColor = m_centerColor;
        if (highlighted) {
            finalColor *= m_highlightIntensity;
        } else {
            finalColor.a = m_inactiveAlpha;
        }

        glm::mat4 cubeTransform = baseTransform;
        cubeTransform = glm::scale(cubeTransform, glm::vec3(scale));

        glm::mat4 mvp = projection * view * cubeTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", cubeTransform);
        m_shader->SetVec4("u_Color", finalColor);
        m_shader->SetBool("u_UseLighting", true);

        if (m_centerCubeMesh) {
            m_centerCubeMesh->Draw();
        }
    }

    // Render axis handles with cubes at ends
    auto renderScaleAxis = [&](GizmoAxis axis, const glm::vec3& dir, const glm::vec4& color) {
        bool highlighted = (m_hoveredAxis == axis || m_activeAxis == axis);
        glm::vec4 finalColor = GetAxisColor(axis, highlighted, m_activeAxis == axis);

        // Line
        glm::mat4 lineTransform = baseTransform;
        if (dir.y > 0.5f) {
            lineTransform *= glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0, 0, 1));
        } else if (dir.z > 0.5f) {
            lineTransform *= glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(0, 1, 0));
        }
        lineTransform = glm::scale(lineTransform, glm::vec3(scale));

        glm::mat4 mvp = projection * view * lineTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", lineTransform);
        m_shader->SetVec4("u_Color", finalColor);
        m_shader->SetBool("u_UseLighting", true);

        if (m_arrowMesh) {
            m_arrowMesh->Draw();
        }

        // End cube
        glm::mat4 cubeTransform = baseTransform;
        cubeTransform = glm::translate(cubeTransform, dir * m_handleLength * scale);
        cubeTransform = glm::scale(cubeTransform, glm::vec3(scale));

        mvp = projection * view * cubeTransform;
        m_shader->SetMat4("u_MVP", mvp);
        m_shader->SetMat4("u_Model", cubeTransform);

        if (m_scaleCubeMesh) {
            m_scaleCubeMesh->Draw();
        }
    };

    renderScaleAxis(GizmoAxis::X, glm::vec3(1, 0, 0), m_xAxisColor);
    renderScaleAxis(GizmoAxis::Y, glm::vec3(0, 1, 0), m_yAxisColor);
    renderScaleAxis(GizmoAxis::Z, glm::vec3(0, 0, 1), m_zAxisColor);
}

glm::vec4 TransformGizmo::GetAxisColor(GizmoAxis axis, bool highlighted, bool active) const {
    glm::vec4 color;

    switch (axis) {
        case GizmoAxis::X:
            color = m_xAxisColor;
            break;
        case GizmoAxis::Y:
            color = m_yAxisColor;
            break;
        case GizmoAxis::Z:
            color = m_zAxisColor;
            break;
        case GizmoAxis::XY:
            color = (m_xAxisColor + m_yAxisColor) * 0.5f;
            break;
        case GizmoAxis::XZ:
            color = (m_xAxisColor + m_zAxisColor) * 0.5f;
            break;
        case GizmoAxis::YZ:
            color = (m_yAxisColor + m_zAxisColor) * 0.5f;
            break;
        case GizmoAxis::XYZ:
            color = m_centerColor;
            break;
        case GizmoAxis::View:
            color = m_viewAxisColor;
            break;
        default:
            color = glm::vec4(1.0f);
            break;
    }

    if (active) {
        color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow when active
    } else if (highlighted) {
        color *= m_highlightIntensity;
        color.a = 1.0f;
    } else {
        color.a *= m_inactiveAlpha;
    }

    return color;
}

float TransformGizmo::ComputeScreenScale(const Camera& camera) const {
    float distToCamera = glm::length(m_position - camera.GetPosition());
    return distToCamera * m_screenSize / 1000.0f * m_baseScale;
}

glm::mat4 TransformGizmo::GetGizmoOrientation() const {
    if (m_space == GizmoSpace::World) {
        return glm::mat4(1.0f);
    } else {
        return glm::toMat4(m_rotation);
    }
}

glm::vec3 TransformGizmo::ScreenToWorldRay(const Camera& camera, const glm::vec2& screenPos,
                                           const glm::vec2& screenSize) const {
    return camera.ScreenToWorldRay(screenPos, screenSize);
}

// =============================================================================
// World-Space Snapping Implementation
// =============================================================================

bool TransformGizmo::IsSnappingActive() const {
    // If Ctrl is held and ctrlOverridesSnap is enabled, disable snapping
    if (m_ctrlPressed && m_worldSnap.ctrlOverridesSnap) {
        return false;
    }
    // Return true if any snapping mode is enabled
    return m_snapping.enabled || m_worldSnap.gridSnapEnabled || m_worldSnap.objectSnapEnabled;
}

glm::vec3 TransformGizmo::SnapToGrid(const glm::vec3& position) const {
    if (!m_worldSnap.gridSnapEnabled) {
        return position;
    }

    float gridStep = m_worldSnap.gridSize;
    if (m_worldSnap.gridSubdivisions > 1) {
        gridStep /= static_cast<float>(m_worldSnap.gridSubdivisions);
    }

    return glm::vec3(
        SnapToNearestGridLine(position.x, gridStep),
        SnapToNearestGridLine(position.y, gridStep),
        SnapToNearestGridLine(position.z, gridStep)
    );
}

glm::vec3 TransformGizmo::SnapToGridIntersection(const glm::vec3& position) const {
    return FindClosestGridIntersection(position);
}

float TransformGizmo::SnapToNearestGridLine(float value, float gridSize) const {
    if (gridSize <= 0.0f) return value;
    return std::round(value / gridSize) * gridSize;
}

glm::vec3 TransformGizmo::FindClosestGridIntersection(const glm::vec3& position) const {
    float gridStep = m_worldSnap.gridSize;
    if (m_worldSnap.gridSubdivisions > 1) {
        gridStep /= static_cast<float>(m_worldSnap.gridSubdivisions);
    }

    // Find the nearest grid intersection point in all three axes
    glm::vec3 snapped;
    snapped.x = std::round(position.x / gridStep) * gridStep;
    snapped.y = std::round(position.y / gridStep) * gridStep;
    snapped.z = std::round(position.z / gridStep) * gridStep;

    // Check if we're within snap distance
    float distSq = glm::length(snapped - position);
    if (distSq <= m_worldSnap.snapDistance) {
        return snapped;
    }

    return position;
}

glm::vec3 TransformGizmo::ApplyWorldSnap(const glm::vec3& worldPosition) {
    glm::vec3 result = worldPosition;

    // Apply grid snapping first
    if (m_worldSnap.gridSnapEnabled) {
        result = SnapToGrid(result);
    }

    return result;
}

SnapResult TransformGizmo::SnapToObject(const glm::vec3& position,
                                        const std::vector<SnapPoint>& snapPoints) const {
    SnapResult result;
    result.position = position;
    result.didSnap = false;

    if (!m_worldSnap.objectSnapEnabled || snapPoints.empty()) {
        return result;
    }

    float closestDistSq = m_worldSnap.objectSnapDistance * m_worldSnap.objectSnapDistance;
    const SnapPoint* closestPoint = nullptr;

    for (const auto& point : snapPoints) {
        // Check if this snap target type is enabled
        if (!HasSnapTarget(m_worldSnap.snapTargets, point.type)) {
            continue;
        }

        float distSq = glm::dot(point.position - position, point.position - position);
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestPoint = &point;
        }
    }

    if (closestPoint) {
        result.position = closestPoint->position;
        result.normal = closestPoint->normal;
        result.type = closestPoint->type;
        result.distance = std::sqrt(closestDistSq);
        result.didSnap = true;
    }

    return result;
}

std::vector<SnapPoint> TransformGizmo::GetMeshSnapPoints(
    const Mesh& mesh,
    const glm::mat4& transform,
    uint64_t objectId,
    SnapTargetType targets) {

    std::vector<SnapPoint> points;

    // For a full implementation, we would need access to the mesh's vertex data
    // Here we provide the bounding box points as a fallback
    glm::vec3 boundsMin = mesh.GetBoundsMin();
    glm::vec3 boundsMax = mesh.GetBoundsMax();

    // Add bounding box snap points
    if (HasSnapTarget(targets, SnapTargetType::BoundingBox)) {
        auto bbPoints = GetBoundsSnapPoints(boundsMin, boundsMax, transform, objectId);
        points.insert(points.end(), bbPoints.begin(), bbPoints.end());
    }

    // Add center point as a vertex snap target
    if (HasSnapTarget(targets, SnapTargetType::Vertex)) {
        glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(center, 1.0f));
        points.emplace_back(worldCenter, glm::vec3(0, 1, 0), SnapTargetType::Vertex, objectId);
    }

    return points;
}

std::vector<SnapPoint> TransformGizmo::GetBoundsSnapPoints(
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    const glm::mat4& transform,
    uint64_t objectId) {

    std::vector<SnapPoint> points;
    points.reserve(27); // 8 corners + 12 edge midpoints + 6 face centers + 1 center

    // Helper to transform a local point to world space
    auto toWorld = [&transform](const glm::vec3& local) -> glm::vec3 {
        return glm::vec3(transform * glm::vec4(local, 1.0f));
    };

    // 8 corner points
    glm::vec3 corners[8] = {
        {boundsMin.x, boundsMin.y, boundsMin.z},
        {boundsMax.x, boundsMin.y, boundsMin.z},
        {boundsMin.x, boundsMax.y, boundsMin.z},
        {boundsMax.x, boundsMax.y, boundsMin.z},
        {boundsMin.x, boundsMin.y, boundsMax.z},
        {boundsMax.x, boundsMin.y, boundsMax.z},
        {boundsMin.x, boundsMax.y, boundsMax.z},
        {boundsMax.x, boundsMax.y, boundsMax.z}
    };

    for (const auto& corner : corners) {
        points.emplace_back(toWorld(corner), glm::vec3(0, 1, 0), SnapTargetType::BoundingBox, objectId);
    }

    // 12 edge midpoints
    glm::vec3 edgeMidpoints[12] = {
        // Bottom face edges
        {(boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, boundsMin.z},
        {boundsMin.x, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f},
        {boundsMax.x, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f},
        {(boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, boundsMax.z},
        // Top face edges
        {(boundsMin.x + boundsMax.x) * 0.5f, boundsMax.y, boundsMin.z},
        {boundsMin.x, boundsMax.y, (boundsMin.z + boundsMax.z) * 0.5f},
        {boundsMax.x, boundsMax.y, (boundsMin.z + boundsMax.z) * 0.5f},
        {(boundsMin.x + boundsMax.x) * 0.5f, boundsMax.y, boundsMax.z},
        // Vertical edges
        {boundsMin.x, (boundsMin.y + boundsMax.y) * 0.5f, boundsMin.z},
        {boundsMax.x, (boundsMin.y + boundsMax.y) * 0.5f, boundsMin.z},
        {boundsMin.x, (boundsMin.y + boundsMax.y) * 0.5f, boundsMax.z},
        {boundsMax.x, (boundsMin.y + boundsMax.y) * 0.5f, boundsMax.z}
    };

    for (const auto& mid : edgeMidpoints) {
        points.emplace_back(toWorld(mid), glm::vec3(0, 1, 0), SnapTargetType::Edge, objectId);
    }

    // 6 face centers
    glm::vec3 faceCenters[6] = {
        {(boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f}, // Bottom
        {(boundsMin.x + boundsMax.x) * 0.5f, boundsMax.y, (boundsMin.z + boundsMax.z) * 0.5f}, // Top
        {boundsMin.x, (boundsMin.y + boundsMax.y) * 0.5f, (boundsMin.z + boundsMax.z) * 0.5f}, // Left
        {boundsMax.x, (boundsMin.y + boundsMax.y) * 0.5f, (boundsMin.z + boundsMax.z) * 0.5f}, // Right
        {(boundsMin.x + boundsMax.x) * 0.5f, (boundsMin.y + boundsMax.y) * 0.5f, boundsMin.z}, // Front
        {(boundsMin.x + boundsMax.x) * 0.5f, (boundsMin.y + boundsMax.y) * 0.5f, boundsMax.z}  // Back
    };

    glm::vec3 faceNormals[6] = {
        {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}
    };

    for (int i = 0; i < 6; ++i) {
        glm::vec3 worldNormal = glm::normalize(glm::vec3(transform * glm::vec4(faceNormals[i], 0.0f)));
        points.emplace_back(toWorld(faceCenters[i]), worldNormal, SnapTargetType::Face, objectId);
    }

    // Object center
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    points.emplace_back(toWorld(center), glm::vec3(0, 1, 0), SnapTargetType::BoundingBox, objectId);

    return points;
}

glm::quat TransformGizmo::SnapRotationToWorldAxes(const glm::quat& rotation) const {
    if (!m_worldSnap.worldAxisRotationSnap) {
        return rotation;
    }

    // Convert quaternion to Euler angles
    glm::vec3 euler = glm::degrees(glm::eulerAngles(rotation));

    // Snap each axis to the configured angle
    float snapAngle = m_worldSnap.worldRotationSnapAngle;
    euler.x = std::round(euler.x / snapAngle) * snapAngle;
    euler.y = std::round(euler.y / snapAngle) * snapAngle;
    euler.z = std::round(euler.z / snapAngle) * snapAngle;

    // Convert back to quaternion
    return glm::quat(glm::radians(euler));
}

glm::vec3 TransformGizmo::SnapScaleToRoundValues(const glm::vec3& scale) const {
    if (!m_worldSnap.roundScaleSnap) {
        return scale;
    }

    float increment = m_worldSnap.scaleSnapIncrement;
    if (increment <= 0.0f) {
        return scale;
    }

    return glm::vec3(
        std::round(scale.x / increment) * increment,
        std::round(scale.y / increment) * increment,
        std::round(scale.z / increment) * increment
    );
}

// =============================================================================
// Grid Rendering Implementation
// =============================================================================

static const char* s_gridVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_VP;

out vec4 v_Color;
out vec3 v_WorldPos;

void main() {
    gl_Position = u_VP * vec4(a_Position, 1.0);
    v_Color = a_Color;
    v_WorldPos = a_Position;
}
)";

static const char* s_gridFragmentShader = R"(
#version 330 core

in vec4 v_Color;
in vec3 v_WorldPos;

uniform vec3 u_CameraPos;
uniform float u_FadeStart;
uniform float u_FadeEnd;

out vec4 FragColor;

void main() {
    // Calculate distance-based fade
    float dist = length(v_WorldPos.xz - u_CameraPos.xz);
    float fade = 1.0 - smoothstep(u_FadeStart, u_FadeEnd, dist);

    FragColor = vec4(v_Color.rgb, v_Color.a * fade);

    // Discard fully transparent pixels
    if (FragColor.a < 0.01) {
        discard;
    }
}
)";

void TransformGizmo::InitializeGridResources() {
    if (m_gridVAO != 0) {
        return; // Already initialized
    }

    // Create grid shader
    m_gridShader = std::make_unique<Shader>();
    if (!m_gridShader->LoadFromSource(s_gridVertexShader, s_gridFragmentShader)) {
        m_gridShader.reset();
        return;
    }

    // Create grid buffers
    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);

    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_GRID_VERTICES * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);

    // Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    // Create snap indicator buffers
    glGenVertexArrays(1, &m_snapIndicatorVAO);
    glGenBuffers(1, &m_snapIndicatorVBO);

    glBindVertexArray(m_snapIndicatorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_snapIndicatorVBO);
    glBufferData(GL_ARRAY_BUFFER, 256 * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void TransformGizmo::DestroyGridResources() {
    if (m_gridVAO) {
        glDeleteVertexArrays(1, &m_gridVAO);
        m_gridVAO = 0;
    }
    if (m_gridVBO) {
        glDeleteBuffers(1, &m_gridVBO);
        m_gridVBO = 0;
    }
    if (m_snapIndicatorVAO) {
        glDeleteVertexArrays(1, &m_snapIndicatorVAO);
        m_snapIndicatorVAO = 0;
    }
    if (m_snapIndicatorVBO) {
        glDeleteBuffers(1, &m_snapIndicatorVBO);
        m_snapIndicatorVBO = 0;
    }
    m_gridShader.reset();
}

void TransformGizmo::GenerateGridLines(std::vector<float>& vertices,
                                        const glm::vec3& cameraPos,
                                        float gridExtent) const {
    vertices.clear();

    float gridSize = m_worldSnap.gridSize;
    int subdivisions = m_worldSnap.gridSubdivisions;
    float subGridSize = gridSize / static_cast<float>(subdivisions);

    // Calculate grid bounds centered on camera (snapped to grid)
    float centerX = std::round(cameraPos.x / gridSize) * gridSize;
    float centerZ = std::round(cameraPos.z / gridSize) * gridSize;

    float minX = centerX - gridExtent;
    float maxX = centerX + gridExtent;
    float minZ = centerZ - gridExtent;
    float maxZ = centerZ + gridExtent;

    // Grid is drawn on Y=0 plane (can be modified to use m_position.y)
    float gridY = 0.0f;

    auto addLine = [&vertices](const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color) {
        // Point 1
        vertices.push_back(p1.x);
        vertices.push_back(p1.y);
        vertices.push_back(p1.z);
        vertices.push_back(color.r);
        vertices.push_back(color.g);
        vertices.push_back(color.b);
        vertices.push_back(color.a);

        // Point 2
        vertices.push_back(p2.x);
        vertices.push_back(p2.y);
        vertices.push_back(p2.z);
        vertices.push_back(color.r);
        vertices.push_back(color.g);
        vertices.push_back(color.b);
        vertices.push_back(color.a);
    };

    // Draw subdivision lines first (they'll be under main grid)
    if (subdivisions > 1) {
        for (float x = minX; x <= maxX; x += subGridSize) {
            // Skip if this is a main grid line
            if (std::abs(std::fmod(x, gridSize)) < 0.001f) continue;

            addLine(glm::vec3(x, gridY, minZ), glm::vec3(x, gridY, maxZ), m_worldSnap.gridSubdivColor);
        }

        for (float z = minZ; z <= maxZ; z += subGridSize) {
            // Skip if this is a main grid line
            if (std::abs(std::fmod(z, gridSize)) < 0.001f) continue;

            addLine(glm::vec3(minX, gridY, z), glm::vec3(maxX, gridY, z), m_worldSnap.gridSubdivColor);
        }
    }

    // Draw main grid lines
    for (float x = std::floor(minX / gridSize) * gridSize; x <= maxX; x += gridSize) {
        glm::vec4 color = m_worldSnap.gridColor;
        // Highlight the Y axis (X=0)
        if (std::abs(x) < 0.001f) {
            color = glm::vec4(0.2f, 0.2f, 0.8f, 0.6f); // Blue for Z axis direction
        }
        addLine(glm::vec3(x, gridY, minZ), glm::vec3(x, gridY, maxZ), color);
    }

    for (float z = std::floor(minZ / gridSize) * gridSize; z <= maxZ; z += gridSize) {
        glm::vec4 color = m_worldSnap.gridColor;
        // Highlight the X axis (Z=0)
        if (std::abs(z) < 0.001f) {
            color = glm::vec4(0.8f, 0.2f, 0.2f, 0.6f); // Red for X axis direction
        }
        addLine(glm::vec3(minX, gridY, z), glm::vec3(maxX, gridY, z), color);
    }
}

void TransformGizmo::RenderGrid(const Camera& camera) {
    RenderGrid(camera.GetView(), camera.GetProjection(), camera.GetPosition());
}

void TransformGizmo::RenderGrid(const glm::mat4& view, const glm::mat4& projection,
                                const glm::vec3& cameraPosition) {
    if (!m_worldSnap.showGrid) {
        return;
    }

    // Initialize grid resources if needed
    if (m_gridVAO == 0) {
        InitializeGridResources();
        if (m_gridVAO == 0) {
            return; // Failed to initialize
        }
    }

    // Generate grid lines based on camera position
    std::vector<float> vertices;
    float gridExtent = GRID_FADE_END * 1.5f; // Extend slightly beyond fade distance
    GenerateGridLines(vertices, cameraPosition, gridExtent);

    if (vertices.empty()) {
        return;
    }

    m_gridVertexCount = vertices.size() / 7; // 7 floats per vertex

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());

    // Setup render state
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    // Render grid
    m_gridShader->Bind();
    glm::mat4 vp = projection * view;
    m_gridShader->SetMat4("u_VP", vp);
    m_gridShader->SetVec3("u_CameraPos", cameraPosition);
    m_gridShader->SetFloat("u_FadeStart", GRID_FADE_START);
    m_gridShader->SetFloat("u_FadeEnd", GRID_FADE_END);

    glBindVertexArray(m_gridVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_gridVertexCount));
    glBindVertexArray(0);

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void TransformGizmo::RenderSnapIndicator(const Camera& camera, const SnapResult& activeSnap) {
    if (!activeSnap.didSnap || !m_worldSnap.showSnapIndicators) {
        return;
    }

    if (m_snapIndicatorVAO == 0) {
        InitializeGridResources();
        if (m_snapIndicatorVAO == 0) {
            return;
        }
    }

    // Create a small cross/diamond indicator at the snap point
    std::vector<float> vertices;
    float size = 0.1f;
    glm::vec4 color = m_worldSnap.snapIndicatorColor;

    glm::vec3 pos = activeSnap.position;

    // Helper to add a line
    auto addLine = [&vertices, &color](const glm::vec3& p1, const glm::vec3& p2) {
        vertices.push_back(p1.x); vertices.push_back(p1.y); vertices.push_back(p1.z);
        vertices.push_back(color.r); vertices.push_back(color.g); vertices.push_back(color.b); vertices.push_back(color.a);
        vertices.push_back(p2.x); vertices.push_back(p2.y); vertices.push_back(p2.z);
        vertices.push_back(color.r); vertices.push_back(color.g); vertices.push_back(color.b); vertices.push_back(color.a);
    };

    // Draw a 3D cross at snap point
    addLine(pos + glm::vec3(-size, 0, 0), pos + glm::vec3(size, 0, 0));
    addLine(pos + glm::vec3(0, -size, 0), pos + glm::vec3(0, size, 0));
    addLine(pos + glm::vec3(0, 0, -size), pos + glm::vec3(0, 0, size));

    // Draw a small diamond around the point
    addLine(pos + glm::vec3(-size, 0, 0), pos + glm::vec3(0, size, 0));
    addLine(pos + glm::vec3(0, size, 0), pos + glm::vec3(size, 0, 0));
    addLine(pos + glm::vec3(size, 0, 0), pos + glm::vec3(0, -size, 0));
    addLine(pos + glm::vec3(0, -size, 0), pos + glm::vec3(-size, 0, 0));

    // Upload and render
    glBindBuffer(GL_ARRAY_BUFFER, m_snapIndicatorVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    m_lineShader->Bind();
    glm::mat4 mvp = camera.GetProjection() * camera.GetView();
    m_lineShader->SetMat4("u_MVP", mvp);

    glBindVertexArray(m_snapIndicatorVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 7));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
}

} // namespace Nova
