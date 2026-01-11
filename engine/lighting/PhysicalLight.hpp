#pragma once

#include "LightMaterialFunction.hpp"
#include <glm/glm.hpp>
#include <string>
#include <memory>

namespace Engine {

// Forward declarations
class Mesh;
class SDFModel;

/**
 * @brief Physical light types with real-world units
 */
enum class LightType {
    Point,          // Omnidirectional point light (cd)
    Spot,           // Spotlight with cone (cd)
    Directional,    // Infinite directional light (lux)
    Area,           // Area light (cd/m²)
    Line,           // Line/tube light (cd/m)
    IES,            // IES photometric profile
    Sky,            // Sky/environment light
    Emissive        // Emissive mesh/SDF
};

/**
 * @brief Area light shapes
 */
enum class AreaLightShape {
    Rectangular,
    Disk,
    Sphere,
    Cylinder
};

/**
 * @brief Physical light with real-world units and material functions
 */
class PhysicalLight {
public:
    PhysicalLight(LightType type = LightType::Point);
    ~PhysicalLight();

    // Light type
    LightType type;

    // Transform
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};  // For spot/directional
    glm::vec3 scale{1.0f};                    // For area lights

    // Physical intensity units
    float intensity = 1000.0f;                // cd (candela) for point/spot
    float luminousFlux = 1000.0f;             // lm (lumens) for area lights
    float illuminance = 100.0f;               // lux for directional lights
    float luminance = 1000.0f;                // cd/m² for area lights

    // Color/Temperature
    bool useTemperature = false;
    float temperature = 6500.0f;              // K (Kelvin)
    glm::vec3 color{1.0f};                    // Tint/filter

    // Spotlight parameters
    float innerConeAngle = 30.0f;             // degrees
    float outerConeAngle = 45.0f;             // degrees
    float spotFalloff = 1.0f;                 // Falloff exponent

    // Area light parameters
    AreaLightShape areaShape = AreaLightShape::Rectangular;
    glm::vec2 areaSize{1.0f};                 // Width x Height (meters)
    float areaRadius = 0.5f;                  // For disk/sphere (meters)

    // Line light parameters
    float lineLength = 1.0f;                  // meters

    // Attenuation
    float range = 10.0f;                      // Max distance (meters)
    bool usePhysicalAttenuation = true;       // Inverse-square law
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.09f;
    float quadraticAttenuation = 0.032f;

    // IES profile
    std::string iesProfilePath;
    bool useIESProfile = false;

    // Material function
    LightMaterialFunction materialFunction;

    // Emissive geometry
    Mesh* emissiveMesh = nullptr;
    SDFModel* emissiveSDF = nullptr;

    // Shadows
    bool castShadows = true;
    float shadowBias = 0.005f;
    int shadowMapResolution = 1024;

    // Volumetric lighting
    bool enableVolumetric = false;
    float volumetricStrength = 1.0f;
    float volumetricScatteringFactor = 0.1f;

    // Methods
    /**
     * @brief Evaluate light contribution at world position
     *
     * @param worldPos World position to evaluate
     * @param normal Surface normal
     * @param time Current time
     * @return Light radiance (RGB)
     */
    glm::vec3 Evaluate(const glm::vec3& worldPos, const glm::vec3& normal, float time) const;

    /**
     * @brief Get light direction at world position
     *
     * @param worldPos World position
     * @return Direction from surface to light
     */
    glm::vec3 GetLightDirection(const glm::vec3& worldPos) const;

    /**
     * @brief Get attenuation factor at distance
     *
     * @param distance Distance from light
     * @return Attenuation multiplier (0-1)
     */
    float GetAttenuation(float distance) const;

    /**
     * @brief Get spot light attenuation (cone falloff)
     *
     * @param worldPos World position
     * @return Spot attenuation (0-1)
     */
    float GetSpotAttenuation(const glm::vec3& worldPos) const;

    /**
     * @brief Get light color (from temperature or direct color)
     *
     * @return RGB color
     */
    glm::vec3 GetColor() const;

    /**
     * @brief Get luminous intensity in direction
     *
     * @param direction Direction from light
     * @return Intensity (cd)
     */
    float GetIntensityInDirection(const glm::vec3& dir) const;

    /**
     * @brief Convert luminous flux to intensity
     *
     * @return Intensity (cd)
     */
    float LumensToIntensity() const;

    /**
     * @brief Sample random point on area light
     *
     * @param u Random value [0,1]
     * @param v Random value [0,1]
     * @return World space position
     */
    glm::vec3 SampleAreaLight(float u, float v) const;

    /**
     * @brief Get area light PDF
     *
     * @param worldPos Sample position
     * @return Probability density
     */
    float GetAreaLightPDF(const glm::vec3& worldPos) const;

    // Serialization
    void Save(const std::string& filepath) const;
    void Load(const std::string& filepath);

private:
    float CalculatePhysicalAttenuation(float distance) const;
    float CalculateSpotCone(const glm::vec3& lightToPoint) const;
    glm::vec3 SampleRectangularLight(float u, float v) const;
    glm::vec3 SampleDiskLight(float u, float v) const;
    glm::vec3 SampleSphereLight(float u, float v) const;
};

/**
 * @brief Physical light presets with real-world values
 */
class PhysicalLightPresets {
public:
    // Common bulbs
    static PhysicalLight CreateIncandescentBulb60W();
    static PhysicalLight CreateIncandescentBulb100W();
    static PhysicalLight CreateLEDBulb10W();
    static PhysicalLight CreateCFLBulb15W();

    // Candles and flames
    static PhysicalLight CreateCandle();
    static PhysicalLight CreateTorch();

    // Professional lighting
    static PhysicalLight CreateStudioLight();
    static PhysicalLight CreateSpotlight();
    static PhysicalLight CreateSoftbox();

    // Environmental
    static PhysicalLight CreateSunlight();
    static PhysicalLight CreateMoonlight();
    static PhysicalLight CreateCloudyDay();

    // Special effects
    static PhysicalLight CreateNeonTube(const glm::vec3& color);
    static PhysicalLight CreateStreetLight();
    static PhysicalLight CreateCarHeadlight();
    static PhysicalLight CreateFireplace();

    // Display lights
    static PhysicalLight CreateMonitor();
    static PhysicalLight CreateTV();
};

} // namespace Engine
