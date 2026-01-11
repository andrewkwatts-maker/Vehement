#include "SDFPrimitive.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Nova {

uint32_t SDFPrimitive::s_nextId = 1;

// ============================================================================
// SDFTransform Implementation
// ============================================================================

glm::mat4 SDFTransform::ToMatrix() const {
    glm::mat4 mat = glm::mat4(1.0f);
    mat = glm::translate(mat, position);
    mat = mat * glm::toMat4(rotation);
    mat = glm::scale(mat, scale);
    return mat;
}

glm::mat4 SDFTransform::ToInverseMatrix() const {
    glm::mat4 mat = glm::mat4(1.0f);
    mat = glm::scale(mat, 1.0f / scale);
    mat = mat * glm::toMat4(glm::conjugate(rotation));
    mat = glm::translate(mat, -position);
    return mat;
}

glm::vec3 SDFTransform::TransformPoint(const glm::vec3& point) const {
    return glm::vec3(ToMatrix() * glm::vec4(point, 1.0f));
}

glm::vec3 SDFTransform::InverseTransformPoint(const glm::vec3& point) const {
    glm::vec3 p = point - position;
    p = glm::inverse(rotation) * p;
    p = p / scale;
    return p;
}

SDFTransform SDFTransform::Lerp(const SDFTransform& a, const SDFTransform& b, float t) {
    SDFTransform result;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

// ============================================================================
// SDFPrimitive Implementation
// ============================================================================

SDFPrimitive::SDFPrimitive(SDFPrimitiveType type)
    : m_id(s_nextId++)
    , m_type(type) {
}

SDFPrimitive::SDFPrimitive(const std::string& name, SDFPrimitiveType type)
    : m_id(s_nextId++)
    , m_name(name)
    , m_type(type) {
}

float SDFPrimitive::EvaluateSDF(const glm::vec3& point) const {
    // Transform point to local space
    glm::vec3 p = m_localTransform.InverseTransformPoint(point);

    switch (m_type) {
        case SDFPrimitiveType::Sphere:
            return SDFEval::Sphere(p, m_parameters.radius);
        case SDFPrimitiveType::Box:
            return SDFEval::Box(p, m_parameters.dimensions * 0.5f);
        case SDFPrimitiveType::RoundedBox:
            return SDFEval::RoundedBox(p, m_parameters.dimensions * 0.5f, m_parameters.cornerRadius);
        case SDFPrimitiveType::Cylinder:
            return SDFEval::Cylinder(p, m_parameters.height, m_parameters.bottomRadius);
        case SDFPrimitiveType::Capsule:
            return SDFEval::Capsule(p, m_parameters.height, m_parameters.bottomRadius);
        case SDFPrimitiveType::Cone:
            return SDFEval::Cone(p, m_parameters.height, m_parameters.bottomRadius);
        case SDFPrimitiveType::Torus:
            return SDFEval::Torus(p, m_parameters.majorRadius, m_parameters.minorRadius);
        case SDFPrimitiveType::Plane:
            return SDFEval::Plane(p, glm::vec3(0, 1, 0), 0);
        case SDFPrimitiveType::Ellipsoid:
            return SDFEval::Ellipsoid(p, m_parameters.radii);
        case SDFPrimitiveType::Pyramid:
            return SDFEval::Pyramid(p, m_parameters.height, m_parameters.bottomRadius);
        case SDFPrimitiveType::Prism:
            return SDFEval::Prism(p, m_parameters.sides, m_parameters.bottomRadius, m_parameters.height);
        default:
            return 1e10f;
    }
}

glm::vec3 SDFPrimitive::CalculateNormal(const glm::vec3& point, float epsilon) const {
    glm::vec3 normal;
    normal.x = EvaluateSDF(point + glm::vec3(epsilon, 0, 0)) - EvaluateSDF(point - glm::vec3(epsilon, 0, 0));
    normal.y = EvaluateSDF(point + glm::vec3(0, epsilon, 0)) - EvaluateSDF(point - glm::vec3(0, epsilon, 0));
    normal.z = EvaluateSDF(point + glm::vec3(0, 0, epsilon)) - EvaluateSDF(point - glm::vec3(0, 0, epsilon));
    return glm::normalize(normal);
}

std::pair<glm::vec3, glm::vec3> SDFPrimitive::GetLocalBounds() const {
    glm::vec3 halfSize;

    switch (m_type) {
        case SDFPrimitiveType::Sphere:
            halfSize = glm::vec3(m_parameters.radius);
            break;
        case SDFPrimitiveType::Box:
        case SDFPrimitiveType::RoundedBox:
            halfSize = m_parameters.dimensions * 0.5f;
            break;
        case SDFPrimitiveType::Cylinder:
        case SDFPrimitiveType::Capsule:
            halfSize = glm::vec3(m_parameters.bottomRadius, m_parameters.height * 0.5f, m_parameters.bottomRadius);
            break;
        case SDFPrimitiveType::Cone:
            halfSize = glm::vec3(m_parameters.bottomRadius, m_parameters.height * 0.5f, m_parameters.bottomRadius);
            break;
        case SDFPrimitiveType::Torus:
            halfSize = glm::vec3(m_parameters.majorRadius + m_parameters.minorRadius,
                                 m_parameters.minorRadius,
                                 m_parameters.majorRadius + m_parameters.minorRadius);
            break;
        case SDFPrimitiveType::Ellipsoid:
            halfSize = m_parameters.radii;
            break;
        default:
            halfSize = glm::vec3(1.0f);
            break;
    }

    return {-halfSize, halfSize};
}

SDFPrimitive* SDFPrimitive::AddChild(std::unique_ptr<SDFPrimitive> child) {
    if (!child) return nullptr;
    child->m_parent = this;
    m_children.push_back(std::move(child));
    return m_children.back().get();
}

bool SDFPrimitive::RemoveChild(SDFPrimitive* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const auto& ptr) { return ptr.get() == child; });
    if (it != m_children.end()) {
        m_children.erase(it);
        return true;
    }
    return false;
}

bool SDFPrimitive::RemoveChild(size_t index) {
    if (index >= m_children.size()) return false;
    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

SDFPrimitive* SDFPrimitive::FindChild(const std::string& name) {
    for (auto& child : m_children) {
        if (child->GetName() == name) return child.get();
        if (auto* found = child->FindChild(name)) return found;
    }
    return nullptr;
}

SDFPrimitive* SDFPrimitive::FindChildById(uint32_t id) {
    for (auto& child : m_children) {
        if (child->GetId() == id) return child.get();
        if (auto* found = child->FindChildById(id)) return found;
    }
    return nullptr;
}

SDFTransform SDFPrimitive::GetWorldTransform() const {
    if (!m_parent) return m_localTransform;

    SDFTransform parentWorld = m_parent->GetWorldTransform();
    SDFTransform result;
    result.position = parentWorld.TransformPoint(m_localTransform.position);
    result.rotation = parentWorld.rotation * m_localTransform.rotation;
    result.scale = parentWorld.scale * m_localTransform.scale;
    return result;
}

std::unique_ptr<SDFPrimitive> SDFPrimitive::Clone() const {
    auto clone = std::make_unique<SDFPrimitive>(m_name, m_type);
    clone->m_localTransform = m_localTransform;
    clone->m_parameters = m_parameters;
    clone->m_material = m_material;
    clone->m_csgOperation = m_csgOperation;
    clone->m_visible = m_visible;
    clone->m_locked = m_locked;

    for (const auto& child : m_children) {
        clone->AddChild(child->Clone());
    }

    return clone;
}

void SDFPrimitive::ForEach(const std::function<void(SDFPrimitive&)>& callback) {
    callback(*this);
    for (auto& child : m_children) {
        child->ForEach(callback);
    }
}

void SDFPrimitive::ForEach(const std::function<void(const SDFPrimitive&)>& callback) const {
    callback(*this);
    for (const auto& child : m_children) {
        const SDFPrimitive* constChild = child.get();
        constChild->ForEach(callback);
    }
}

// ============================================================================
// SDF Evaluation Functions
// ============================================================================

namespace SDFEval {

float Sphere(const glm::vec3& p, float radius) {
    return glm::length(p) - radius;
}

float Box(const glm::vec3& p, const glm::vec3& dimensions) {
    glm::vec3 q = glm::abs(p) - dimensions;
    return glm::length(glm::max(q, glm::vec3(0.0f))) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

float RoundedBox(const glm::vec3& p, const glm::vec3& dimensions, float radius) {
    glm::vec3 q = glm::abs(p) - dimensions + glm::vec3(radius);
    return glm::length(glm::max(q, glm::vec3(0.0f))) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f) - radius;
}

float Cylinder(const glm::vec3& p, float height, float radius) {
    glm::vec2 d = glm::abs(glm::vec2(glm::length(glm::vec2(p.x, p.z)), p.y)) - glm::vec2(radius, height * 0.5f);
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f)));
}

float Capsule(const glm::vec3& p, float height, float radius) {
    float halfHeight = height * 0.5f - radius;
    glm::vec3 pa = p - glm::vec3(0, -halfHeight, 0);
    glm::vec3 ba = glm::vec3(0, halfHeight * 2.0f, 0);
    float h = glm::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
    return glm::length(pa - ba * h) - radius;
}

float Cone(const glm::vec3& p, float height, float radius) {
    glm::vec2 c = glm::normalize(glm::vec2(radius, height));
    float q = glm::length(glm::vec2(p.x, p.z));
    return glm::dot(c, glm::vec2(q, p.y + height * 0.5f));
}

float Torus(const glm::vec3& p, float majorRadius, float minorRadius) {
    glm::vec2 q = glm::vec2(glm::length(glm::vec2(p.x, p.z)) - majorRadius, p.y);
    return glm::length(q) - minorRadius;
}

float Plane(const glm::vec3& p, const glm::vec3& normal, float offset) {
    return glm::dot(p, normal) + offset;
}

float Ellipsoid(const glm::vec3& p, const glm::vec3& radii) {
    float k0 = glm::length(p / radii);
    float k1 = glm::length(p / (radii * radii));
    return k0 * (k0 - 1.0f) / k1;
}

float Pyramid(const glm::vec3& p, float height, float baseSize) {
    glm::vec3 pos = p + glm::vec3(0, height * 0.5f, 0);
    float m2 = height * height + 0.25f;

    glm::vec2 pxz = glm::vec2(std::abs(pos.x), std::abs(pos.z));
    if (pxz.y > pxz.x) std::swap(pxz.x, pxz.y);
    pxz -= glm::vec2(baseSize * 0.5f);

    glm::vec3 q = glm::vec3(pxz.y, height * pxz.x - baseSize * 0.5f * pos.y, height * pos.y + baseSize * 0.5f * pxz.x);

    float s = std::max(-q.x, 0.0f);
    float t = glm::clamp((q.y - 0.5f * baseSize * q.z) / (m2 + 0.25f * baseSize * baseSize), 0.0f, 1.0f);

    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + 0.5f * t) * (q.x + 0.5f * t) + (q.y - m2 * t) * (q.y - m2 * t);

    float d2 = std::max(-q.y, q.x * m2 + q.y * 0.5f) < 0.0f ? 0.0f : std::min(a, b);

    return std::sqrt((d2 + q.z * q.z) / m2) * (std::max(q.z, -pos.y) < 0.0f ? -1.0f : 1.0f);
}

float Prism(const glm::vec3& p, int sides, float radius, float height) {
    const float PI = 3.14159265359f;
    float angle = PI / static_cast<float>(sides);
    glm::vec2 cs = glm::vec2(std::cos(angle), std::sin(angle));

    glm::vec2 pxz = glm::vec2(p.x, p.z);
    float phi = std::atan2(pxz.y, pxz.x);
    float sector = std::floor(phi / (2.0f * angle) + 0.5f);
    float sectorAngle = sector * 2.0f * angle;

    glm::vec2 rotated;
    rotated.x = pxz.x * std::cos(-sectorAngle) - pxz.y * std::sin(-sectorAngle);
    rotated.y = pxz.x * std::sin(-sectorAngle) + pxz.y * std::cos(-sectorAngle);

    glm::vec2 d = glm::abs(glm::vec2(rotated.x, p.y)) - glm::vec2(radius * cs.x, height * 0.5f);
    return std::min(std::max(d.x, d.y), 0.0f) + glm::length(glm::max(d, glm::vec2(0.0f)));
}

float Union(float d1, float d2) {
    return std::min(d1, d2);
}

float Subtraction(float d1, float d2) {
    return std::max(-d1, d2);
}

float Intersection(float d1, float d2) {
    return std::max(d1, d2);
}

float SmoothUnion(float d1, float d2, float k) {
    float h = glm::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, d1, h) - k * h * (1.0f - h);
}

float SmoothSubtraction(float d1, float d2, float k) {
    float h = glm::clamp(0.5f - 0.5f * (d2 + d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, -d1, h) + k * h * (1.0f - h);
}

float SmoothIntersection(float d1, float d2, float k) {
    float h = glm::clamp(0.5f - 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return glm::mix(d2, d1, h) + k * h * (1.0f - h);
}

// Exponential smooth union - creates very organic, flowing blends
// More expensive but produces smoother transitions than polynomial
float ExponentialSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0f) return std::min(d1, d2);
    float res = std::exp2(-k * d1) + std::exp2(-k * d2);
    return -std::log2(res) / k;
}

// Power smooth union - adjustable blend sharpness via exponent
// k controls blend radius, uses power function for smooth transition
float PowerSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0f) return std::min(d1, d2);
    float a = std::pow(d1, k);
    float b = std::pow(d2, k);
    return std::pow((a * b) / (a + b), 1.0f / k);
}

// Cubic smooth union - smoother than quadratic (standard SmoothUnion)
// Produces more organic transitions with better C2 continuity
float CubicSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0f) return std::min(d1, d2);
    float h = std::max(k - std::abs(d1 - d2), 0.0f) / k;
    float m = h * h * h * 0.5f;
    float s = m * k * (1.0f / 3.0f);
    return (d1 < d2) ? d1 - s : d2 - s;
}

// Distance-aware smooth union - prevents unwanted blending when parts are far apart
// Critical for character animation to prevent fingers/limbs from merging
float DistanceAwareSmoothUnion(float d1, float d2, float k, float minDist) {
    float dist = std::abs(d1 - d2);
    if (dist > minDist) {
        // Too far apart, use hard union
        return std::min(d1, d2);
    }
    // Close enough, apply smooth blending with falloff
    float falloff = 1.0f - (dist / minDist);
    float effectiveK = k * falloff;
    return SmoothUnion(d1, d2, effectiveK);
}

} // namespace SDFEval

} // namespace Nova
