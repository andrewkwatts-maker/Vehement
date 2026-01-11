#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <functional>

namespace Engine {

// Forward declarations
class Texture;
class MaterialGraph;

/**
 * @brief Light material function for procedural intensity modulation
 *
 * Supports various modulation types including textures, procedural animations,
 * and custom shader graphs.
 */
class LightMaterialFunction {
public:
    /**
     * @brief Modulation types
     */
    enum class ModulationType {
        Constant,       // Fixed intensity
        Pulse,          // Periodic pulse (sine wave)
        Flicker,        // Random flickering
        Fire,           // Flame-like animation
        Neon,           // Electric neon flicker
        Strobe,         // Sharp on/off strobe
        Breathe,        // Smooth breathing effect
        Wave,           // Traveling wave
        UVTexture,      // 2D texture mapping
        LatLong,        // Spherical lat/long mapping (360°)
        Gobo,           // Projected cookie (spotlight)
        Custom          // User shader graph
    };

    LightMaterialFunction();
    ~LightMaterialFunction();

    // Configuration
    ModulationType type = ModulationType::Constant;

    // Animation parameters
    float frequency = 1.0f;         // Hz
    float amplitude = 1.0f;         // Modulation strength
    float offset = 0.0f;            // Time offset
    float phase = 0.0f;             // Phase shift (0-1)
    float dutyCycle = 0.5f;         // For pulse/strobe (0-1)

    // Noise parameters
    float noiseScale = 1.0f;
    float noiseStrength = 0.2f;
    int noiseOctaves = 3;

    // Textures
    Texture* modulationTexture = nullptr;
    bool modulationTextureEnabled = false;
    glm::vec2 textureScale{1.0f};
    glm::vec2 textureOffset{0.0f};
    float textureRotation = 0.0f;

    // Custom graph
    std::shared_ptr<MaterialGraph> customGraph;

    // Color modulation
    bool useColorModulation = false;
    glm::vec3 colorTint{1.0f};
    glm::vec3 colorVariation{0.0f};

    // Methods
    /**
     * @brief Evaluate modulation at UV coordinates and time
     *
     * @param uv UV coordinates
     * @param time Time in seconds
     * @return Modulation value (0-1)
     */
    float Evaluate(const glm::vec2& uv, float time) const;

    /**
     * @brief Evaluate modulation with direction (for spherical mapping)
     *
     * @param direction 3D direction vector
     * @param time Time in seconds
     * @return Modulation value (0-1)
     */
    float EvaluateDirection(const glm::vec3& direction, float time) const;

    /**
     * @brief Evaluate color modulation
     *
     * @param uv UV coordinates
     * @param time Time in seconds
     * @return Color multiplier
     */
    glm::vec3 EvaluateColor(const glm::vec2& uv, float time) const;

    /**
     * @brief Get modulation value for point light (spherical)
     *
     * @param worldPos World position
     * @param lightPos Light position
     * @param time Time in seconds
     * @return Modulation value
     */
    float EvaluatePointLight(const glm::vec3& worldPos, const glm::vec3& lightPos, float time) const;

    /**
     * @brief Get modulation value for spot light (with gobo)
     *
     * @param worldPos World position
     * @param lightPos Light position
     * @param lightDir Light direction
     * @param time Time in seconds
     * @return Modulation value
     */
    float EvaluateSpotLight(const glm::vec3& worldPos, const glm::vec3& lightPos,
                            const glm::vec3& lightDir, float time) const;

    // Serialization
    void Save(const std::string& filepath) const;
    void Load(const std::string& filepath);

private:
    // Animation functions
    float PulseAnimation(float time) const;
    float FlickerAnimation(float time) const;
    float FireAnimation(const glm::vec2& uv, float time) const;
    float NeonAnimation(float time) const;
    float StrobeAnimation(float time) const;
    float BreatheAnimation(float time) const;
    float WaveAnimation(const glm::vec2& uv, float time) const;

    // Noise functions
    float PerlinNoise(const glm::vec2& p) const;
    float PerlinNoise(const glm::vec3& p) const;
    float FractalNoise(const glm::vec2& p, int octaves) const;

    // UV mapping
    glm::vec2 TransformUV(const glm::vec2& uv) const;
    glm::vec2 DirectionToLatLong(const glm::vec3& direction) const;
    glm::vec2 DirectionToUV(const glm::vec3& direction) const;
};

/**
 * @brief Preset light material functions
 */
class LightMaterialFunctionPresets {
public:
    static LightMaterialFunction CreateCandle();
    static LightMaterialFunction CreateFireplace();
    static LightMaterialFunction CreateFlashlightFlicker();
    static LightMaterialFunction CreateNeonSign();
    static LightMaterialFunction CreateStrobe(float frequency);
    static LightMaterialFunction CreatePulse(float frequency);
    static LightMaterialFunction CreateBreathe(float frequency);
    static LightMaterialFunction CreateLightning();
    static LightMaterialFunction CreateTV();
    static LightMaterialFunction CreateEmergencyLight();
    static LightMaterialFunction CreateDiscoLight();
    static LightMaterialFunction CreateGobo(Texture* goboTexture);
};

} // namespace Engine
