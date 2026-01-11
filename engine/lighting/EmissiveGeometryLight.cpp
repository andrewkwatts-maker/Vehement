#include "EmissiveGeometryLight.hpp"
#include "../materials/AdvancedMaterial.hpp"
#include <algorithm>
#include <cmath>

namespace Engine {

EmissiveGeometryLight::EmissiveGeometryLight() {
}

EmissiveGeometryLight::~EmissiveGeometryLight() {
}

void EmissiveGeometryLight::BuildLightSource() {
    m_emissiveTriangles.clear();
    m_triangleCDF.clear();
    m_totalPower = 0.0f;
    m_totalArea = 0.0f;

    if (mesh) {
        ExtractEmissiveTriangles();
    } else if (sdf) {
        // SDF light extraction would require marching cubes or similar
        // For now, treat as point light at SDF center
    }

    BuildCDF();
}

void EmissiveGeometryLight::ExtractEmissiveTriangles() {
    if (!mesh || !material) {
        return;
    }

    // Get emission properties
    glm::vec3 emission;
    if (useOverrideEmission) {
        emission = emissionColor * emissionStrength;
    } else {
        emission = material->GetEmissionColor();
    }

    // Skip if not emissive
    if (glm::length(emission) < 0.001f) {
        return;
    }

    // Extract triangles from mesh (simplified - assumes triangle mesh)
    // In real implementation, would iterate through mesh geometry
    // For now, create example triangles

    // Example: create a simple quad
    EmissiveTriangle tri;
    tri.v0 = glm::vec3(-0.5f, 0.0f, -0.5f);
    tri.v1 = glm::vec3(0.5f, 0.0f, -0.5f);
    tri.v2 = glm::vec3(0.5f, 0.0f, 0.5f);
    tri.n0 = tri.n1 = tri.n2 = glm::vec3(0.0f, 1.0f, 0.0f);
    tri.uv0 = glm::vec2(0.0f, 0.0f);
    tri.uv1 = glm::vec2(1.0f, 0.0f);
    tri.uv2 = glm::vec2(1.0f, 1.0f);

    // Transform to world space
    tri.v0 = glm::vec3(transform * glm::vec4(tri.v0, 1.0f));
    tri.v1 = glm::vec3(transform * glm::vec4(tri.v1, 1.0f));
    tri.v2 = glm::vec3(transform * glm::vec4(tri.v2, 1.0f));

    // Calculate area
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    tri.area = 0.5f * glm::length(glm::cross(edge1, edge2));

    tri.emission = emission;
    tri.power = glm::length(emission) * tri.area * glm::pi<float>();

    m_emissiveTriangles.push_back(tri);
    m_totalArea += tri.area;
    m_totalPower += tri.power;
}

void EmissiveGeometryLight::BuildCDF() {
    if (m_emissiveTriangles.empty()) {
        return;
    }

    m_triangleCDF.resize(m_emissiveTriangles.size());

    float cumulativePower = 0.0f;
    for (size_t i = 0; i < m_emissiveTriangles.size(); ++i) {
        cumulativePower += m_emissiveTriangles[i].power;
        m_triangleCDF[i] = cumulativePower;
    }

    // Normalize
    for (float& cdf : m_triangleCDF) {
        cdf /= cumulativePower;
    }
}

int EmissiveGeometryLight::SampleTriangleIndex(float u) const {
    // Binary search in CDF
    auto it = std::lower_bound(m_triangleCDF.begin(), m_triangleCDF.end(), u);
    int index = std::distance(m_triangleCDF.begin(), it);
    return glm::clamp(index, 0, static_cast<int>(m_emissiveTriangles.size()) - 1);
}

glm::vec3 EmissiveGeometryLight::SampleTriangle(const EmissiveTriangle& tri, float u, float v) const {
    // Uniform sampling of triangle
    float sqrtU = std::sqrt(u);
    float alpha = 1.0f - sqrtU;
    float beta = v * sqrtU;
    float gamma = 1.0f - alpha - beta;

    return alpha * tri.v0 + beta * tri.v1 + gamma * tri.v2;
}

glm::vec3 EmissiveGeometryLight::InterpolateNormal(const EmissiveTriangle& tri, float u, float v) const {
    float sqrtU = std::sqrt(u);
    float alpha = 1.0f - sqrtU;
    float beta = v * sqrtU;
    float gamma = 1.0f - alpha - beta;

    return glm::normalize(alpha * tri.n0 + beta * tri.n1 + gamma * tri.n2);
}

float EmissiveGeometryLight::SampleSurface(float u, float v, glm::vec3& outPosition,
                                          glm::vec3& outNormal, glm::vec3& outEmission) const {
    if (m_emissiveTriangles.empty()) {
        return 0.0f;
    }

    // Sample triangle based on power distribution
    int triIndex = SampleTriangleIndex(u);
    const EmissiveTriangle& tri = m_emissiveTriangles[triIndex];

    // Sample point on triangle
    outPosition = SampleTriangle(tri, u, v);
    outNormal = InterpolateNormal(tri, u, v);
    outEmission = tri.emission;

    // PDF is proportional to power
    float pdf = tri.power / m_totalPower;
    pdf /= tri.area;  // Convert to area measure

    return pdf;
}

glm::vec3 EmissiveGeometryLight::Evaluate(const glm::vec3& shadingPoint,
                                          const glm::vec3& shadingNormal,
                                          float time) const {
    if (m_emissiveTriangles.empty()) {
        return glm::vec3(0.0f);
    }

    // Monte Carlo integration over light surface
    glm::vec3 totalRadiance(0.0f);

    for (int i = 0; i < numSamples; ++i) {
        float u = static_cast<float>(i) / numSamples;
        float v = static_cast<float>((i * 7) % numSamples) / numSamples;  // Quasi-random

        glm::vec3 lightPos, lightNormal, emission;
        float pdf = SampleSurface(u, v, lightPos, lightNormal, emission);

        if (pdf > 0.0f) {
            glm::vec3 lightDir = glm::normalize(lightPos - shadingPoint);
            float distance = glm::length(lightPos - shadingPoint);

            // Geometric term
            float cosTheta = glm::max(glm::dot(shadingNormal, lightDir), 0.0f);
            float cosThetaLight = glm::max(glm::dot(lightNormal, -lightDir), 0.0f);

            if (cosTheta > 0.0f && cosThetaLight > 0.0f) {
                float geometricTerm = (cosTheta * cosThetaLight) / (distance * distance);
                totalRadiance += emission * geometricTerm / pdf;
            }
        }
    }

    return totalRadiance / static_cast<float>(numSamples);
}

float EmissiveGeometryLight::GetPDF(const glm::vec3& point) const {
    if (m_totalArea > 0.0f) {
        return 1.0f / m_totalArea;
    }
    return 0.0f;
}

float EmissiveGeometryLight::GetTotalPower() const {
    return m_totalPower;
}

EmissiveGeometryLight::AABB EmissiveGeometryLight::GetBounds() const {
    AABB bounds;
    bounds.min = glm::vec3(std::numeric_limits<float>::max());
    bounds.max = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& tri : m_emissiveTriangles) {
        bounds.min = glm::min(bounds.min, tri.v0);
        bounds.min = glm::min(bounds.min, tri.v1);
        bounds.min = glm::min(bounds.min, tri.v2);

        bounds.max = glm::max(bounds.max, tri.v0);
        bounds.max = glm::max(bounds.max, tri.v1);
        bounds.max = glm::max(bounds.max, tri.v2);
    }

    return bounds;
}

// AreaLightSampler implementation
glm::vec3 AreaLightSampler::SampleWithMIS(const EmissiveGeometryLight& light,
                                          const glm::vec3& shadingPoint,
                                          const glm::vec3& shadingNormal,
                                          float roughness,
                                          float u, float v) {
    // Sample light
    glm::vec3 lightPos, lightNormal, emission;
    float lightPDF = light.SampleSurface(u, v, lightPos, lightNormal, emission);

    if (lightPDF <= 0.0f) {
        return glm::vec3(0.0f);
    }

    glm::vec3 lightDir = glm::normalize(lightPos - shadingPoint);
    float distance = glm::length(lightPos - shadingPoint);

    // BRDF sampling PDF (simplified)
    float cosTheta = glm::max(glm::dot(shadingNormal, lightDir), 0.0f);
    float brdfPDF = cosTheta / glm::pi<float>();

    // MIS weight
    float weight = MISWeight(lightPDF, brdfPDF);

    // Geometric term
    float cosThetaLight = glm::max(glm::dot(lightNormal, -lightDir), 0.0f);
    float geometricTerm = (cosTheta * cosThetaLight) / (distance * distance);

    return emission * geometricTerm * weight / lightPDF;
}

float AreaLightSampler::MISWeight(float pdf0, float pdf1) {
    // Balance heuristic
    return pdf0 / (pdf0 + pdf1);
}

float AreaLightSampler::MISWeightPower(float pdf0, float pdf1, float beta) {
    // Power heuristic
    float f0 = std::pow(pdf0, beta);
    float f1 = std::pow(pdf1, beta);
    return f0 / (f0 + f1);
}

} // namespace Engine
