#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Engine {

// Forward declarations
class Mesh;
class SDFModel;
class AdvancedMaterial;

/**
 * @brief Emissive geometry light - any mesh or SDF can emit light
 *
 * Automatically extracts light sources from emissive materials on geometry
 * and performs importance sampling for direct lighting.
 */
class EmissiveGeometryLight {
public:
    EmissiveGeometryLight();
    ~EmissiveGeometryLight();

    // Geometry source
    Mesh* mesh = nullptr;
    SDFModel* sdf = nullptr;

    // Material (contains emission properties)
    std::shared_ptr<AdvancedMaterial> material;

    // Emission override
    bool useOverrideEmission = false;
    glm::vec3 emissionColor{1.0f};
    float emissionStrength = 1.0f;

    // Transform
    glm::mat4 transform{1.0f};

    // Sampling
    int numSamples = 32;           // For area integration
    bool useMIS = true;             // Multiple importance sampling

    /**
     * @brief Build light source data from mesh/SDF
     *
     * Extracts emissive triangles/primitives and builds sampling structure
     */
    void BuildLightSource();

    /**
     * @brief Sample random point on emissive surface
     *
     * @param u Random value [0,1]
     * @param v Random value [0,1]
     * @param outPosition Sampled position (world space)
     * @param outNormal Surface normal
     * @param outEmission Emission at sample point
     * @return PDF of sample
     */
    float SampleSurface(float u, float v, glm::vec3& outPosition,
                       glm::vec3& outNormal, glm::vec3& outEmission) const;

    /**
     * @brief Evaluate light contribution at shading point
     *
     * @param shadingPoint World position
     * @param shadingNormal Surface normal
     * @param time Current time
     * @return Light radiance
     */
    glm::vec3 Evaluate(const glm::vec3& shadingPoint, const glm::vec3& shadingNormal, float time) const;

    /**
     * @brief Get PDF for sampling a point
     *
     * @param point Sampled point
     * @return Probability density
     */
    float GetPDF(const glm::vec3& point) const;

    /**
     * @brief Get total emitted power
     *
     * @return Total flux (lumens)
     */
    float GetTotalPower() const;

    /**
     * @brief Get bounding box of emissive geometry
     *
     * @return AABB
     */
    struct AABB {
        glm::vec3 min;
        glm::vec3 max;
    };
    AABB GetBounds() const;

private:
    /**
     * @brief Emissive triangle for mesh lights
     */
    struct EmissiveTriangle {
        glm::vec3 v0, v1, v2;
        glm::vec3 n0, n1, n2;
        glm::vec2 uv0, uv1, uv2;
        float area;
        glm::vec3 emission;
        float power;  // Total emitted power from this triangle
    };

    std::vector<EmissiveTriangle> m_emissiveTriangles;
    std::vector<float> m_triangleCDF;  // Cumulative distribution for importance sampling
    float m_totalPower = 0.0f;
    float m_totalArea = 0.0f;

    void ExtractEmissiveTriangles();
    void BuildCDF();
    int SampleTriangleIndex(float u) const;
    glm::vec3 SampleTriangle(const EmissiveTriangle& tri, float u, float v) const;
    glm::vec3 InterpolateNormal(const EmissiveTriangle& tri, float u, float v) const;
};

/**
 * @brief Importance sampling for area lights with MIS
 */
class AreaLightSampler {
public:
    /**
     * @brief Sample area light with BRDF importance
     *
     * @param light Emissive geometry light
     * @param shadingPoint Shading point
     * @param shadingNormal Surface normal
     * @param roughness Surface roughness
     * @param u Random value [0,1]
     * @param v Random value [0,1]
     * @return Sampled light contribution
     */
    static glm::vec3 SampleWithMIS(const EmissiveGeometryLight& light,
                                    const glm::vec3& shadingPoint,
                                    const glm::vec3& shadingNormal,
                                    float roughness,
                                    float u, float v);

    /**
     * @brief Calculate MIS weight (balance heuristic)
     *
     * @param pdf0 PDF of first sampling strategy
     * @param pdf1 PDF of second sampling strategy
     * @return MIS weight
     */
    static float MISWeight(float pdf0, float pdf1);

    /**
     * @brief Power heuristic for MIS
     *
     * @param pdf0 PDF of first sampling strategy
     * @param pdf1 PDF of second sampling strategy
     * @param beta Power (typically 2)
     * @return MIS weight
     */
    static float MISWeightPower(float pdf0, float pdf1, float beta = 2.0f);
};

} // namespace Engine
