#pragma once

#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace Engine {

// Forward declarations
class Texture;
class MaterialGraph;
class Shader;

/**
 * @brief Sellmeier equation coefficients for wavelength-dependent IOR
 */
struct SellmeierCoefficients {
    float B1 = 1.03961212f;
    float B2 = 0.231792344f;
    float B3 = 1.01046945f;
    float C1 = 0.00600069867f;
    float C2 = 0.0200179144f;
    float C3 = 103.560653f;

    // Calculate IOR at wavelength (nm)
    float CalculateIOR(float wavelength_nm) const;
};

/**
 * @brief Abbe number for dispersion calculation
 */
struct DispersionProperties {
    float abbeNumber = 55.0f;        // V_d (higher = less dispersion)
    float partialDispersion = 0.0f;   // P_g,F
    bool enableDispersion = false;

    SellmeierCoefficients sellmeier;

    // Get IOR at specific wavelength
    float GetIOR(float wavelength_nm, float baseIOR) const;
};

/**
 * @brief Subsurface scattering properties
 */
struct SubsurfaceScatteringProperties {
    bool enabled = false;
    float radius = 1.0f;              // mm
    glm::vec3 color{0.8f, 0.3f, 0.2f};
    float scatteringDensity = 0.5f;
    float scatteringAnisotropy = 0.0f; // -1 (back) to +1 (forward)
    glm::vec3 extinction{1.0f};        // Absorption + scattering
    glm::vec3 albedo{0.9f};            // Single scattering albedo
};

/**
 * @brief Volumetric scattering properties (Rayleigh, Mie)
 */
struct VolumetricScatteringProperties {
    enum class ScatteringType {
        None,
        Rayleigh,    // Atmospheric (wavelength^-4)
        Mie,         // Aerosols, fog
        HenyeyGreenstein,  // General phase function
        Mixed
    };

    ScatteringType type = ScatteringType::None;
    float density = 0.0f;
    float anisotropy = 0.0f;          // g in Henyey-Greenstein
    glm::vec3 scatteringCoefficient{0.0f};
    glm::vec3 absorptionCoefficient{0.0f};

    // Rayleigh-specific (atmosphere)
    float rayleighScale = 8000.0f;    // Scale height (meters)

    // Mie-specific
    float mieScale = 1200.0f;
    float turbidity = 2.0f;           // Atmospheric turbidity
};

/**
 * @brief Emission properties (blackbody, luminosity)
 */
struct EmissionProperties {
    bool enabled = false;

    // Blackbody radiation
    bool useBlackbody = false;
    float temperature = 6500.0f;      // Kelvin (1000-40000)
    float luminosity = 0.0f;          // cd/m² (candela per square meter)
    float luminousFlux = 0.0f;        // lumens (total power)

    // Manual emission
    glm::vec3 emissionColor{1.0f};
    float emissionStrength = 1.0f;    // Multiplier

    // Spectral power distribution
    std::vector<float> spectralPowerDistribution;  // Custom SPD curve

    // Textures
    Texture* emissionMap = nullptr;
    float emissionMapStrength = 1.0f;
};

/**
 * @brief Fluorescence properties (UV absorption → visible emission)
 */
struct FluorescenceProperties {
    bool enabled = false;
    float strength = 0.0f;
    float absorptionWavelength = 365.0f;  // nm (UV-A)
    float emissionWavelength = 520.0f;    // nm (green)
    float quantumEfficiency = 0.7f;       // Energy conversion efficiency
    glm::vec3 emissionColor{0.0f, 1.0f, 0.0f};
};

/**
 * @brief Anisotropic material properties (direction-dependent)
 */
struct AnisotropicProperties {
    bool enabled = false;
    glm::vec3 iorAnisotropic{1.5f};       // Per-axis IOR
    glm::vec3 roughnessAnisotropic{0.5f}; // Per-axis roughness
    glm::vec3 tangentDirection{1.0f, 0.0f, 0.0f};
    glm::vec3 bitangentDirection{0.0f, 1.0f, 0.0f};
    float anisotropicStrength = 0.0f;     // 0-1
};

/**
 * @brief Clear coat layer (car paint, lacquer)
 */
struct ClearCoatProperties {
    bool enabled = false;
    float strength = 1.0f;
    float roughness = 0.1f;
    float ior = 1.5f;
    glm::vec3 tint{1.0f};
    Texture* normalMap = nullptr;
};

/**
 * @brief Sheen properties (fabric, velvet)
 */
struct SheenProperties {
    bool enabled = false;
    float strength = 0.0f;
    glm::vec3 color{1.0f};
    float roughness = 0.5f;
};

/**
 * @brief Advanced material with physically-based properties
 */
class AdvancedMaterial {
public:
    AdvancedMaterial();
    ~AdvancedMaterial();

    // Basic PBR properties
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float specular = 0.5f;            // Dielectric specular strength
    glm::vec3 f0{0.04f};              // Base reflectivity

    // Optical properties
    float ior = 1.5f;                 // Base refractive index
    float transmission = 0.0f;        // 0 = opaque, 1 = transparent
    float thickness = 1.0f;           // For thin surfaces
    glm::vec3 transmittanceColor{1.0f};
    float transmittanceDistance = 1.0f;

    // Advanced properties
    DispersionProperties dispersion;
    SubsurfaceScatteringProperties subsurface;
    VolumetricScatteringProperties volumetric;
    EmissionProperties emission;
    FluorescenceProperties fluorescence;
    AnisotropicProperties anisotropic;
    ClearCoatProperties clearCoat;
    SheenProperties sheen;

    // Texture maps
    Texture* albedoMap = nullptr;
    Texture* normalMap = nullptr;
    Texture* metallicMap = nullptr;
    Texture* roughnessMap = nullptr;
    Texture* aoMap = nullptr;
    Texture* heightMap = nullptr;
    Texture* iorMap = nullptr;         // Spatially-varying IOR
    Texture* transmissionMap = nullptr;
    Texture* thicknessMap = nullptr;

    // Material graph (visual scripting)
    std::shared_ptr<MaterialGraph> materialGraph;
    bool useGraphShader = false;

    // Shader
    std::shared_ptr<Shader> shader;
    std::string shaderPath;

    // Material properties
    std::string name = "Unnamed Material";
    bool doubleSided = false;
    bool castShadows = true;
    bool receiveShadows = true;

    // Alpha blending
    enum class BlendMode {
        Opaque,
        Masked,
        Translucent,
        Additive,
        Multiply
    };
    BlendMode blendMode = BlendMode::Opaque;
    float alphaCutoff = 0.5f;         // For masked mode

    // Methods
    void SetAlbedo(const glm::vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetIOR(float value);
    void SetEmission(const glm::vec3& color, float strength);
    void SetTemperature(float kelvin);

    // Calculate derived properties
    glm::vec3 GetF0() const;
    float GetIORAtWavelength(float wavelength_nm) const;
    glm::vec3 GetEmissionColor(float time = 0.0f) const;
    float GetEmissionIntensity() const;

    // Material graph
    void SetMaterialGraph(std::shared_ptr<MaterialGraph> graph);
    void CompileGraphToShader();

    // Texture management
    void SetAlbedoMap(Texture* texture);
    void SetNormalMap(Texture* texture);
    void SetMetallicMap(Texture* texture);
    void SetRoughnessMap(Texture* texture);
    void SetEmissionMap(Texture* texture);
    void SetIORMap(Texture* texture);

    // Serialization
    void Save(const std::string& filepath) const;
    void Load(const std::string& filepath);

    // Material presets
    static AdvancedMaterial CreateGlass(float ior = 1.5f);
    static AdvancedMaterial CreateWater();
    static AdvancedMaterial CreateGold();
    static AdvancedMaterial CreateCopper();
    static AdvancedMaterial CreateDiamond();
    static AdvancedMaterial CreatePlastic(const glm::vec3& color);
    static AdvancedMaterial CreateSkin();
    static AdvancedMaterial CreateMarble();
    static AdvancedMaterial CreateWax(float temperature = 1800.0f);
    static AdvancedMaterial CreateEmissive(float temperature, float luminosity);
    static AdvancedMaterial CreateNeon(const glm::vec3& color);
    static AdvancedMaterial CreateVelvet(const glm::vec3& color);
    static AdvancedMaterial CreateCarPaint(const glm::vec3& color);

    // Validation
    bool Validate() const;
    std::vector<std::string> GetValidationErrors() const;

private:
    void InitializeDefaultShader();
    void CalculateF0();
    void UpdateShaderUniforms();

    mutable std::vector<std::string> m_validationErrors;
};

/**
 * @brief Material library for managing multiple materials
 */
class MaterialLibrary {
public:
    static MaterialLibrary& GetInstance();

    void AddMaterial(const std::string& name, std::shared_ptr<AdvancedMaterial> material);
    std::shared_ptr<AdvancedMaterial> GetMaterial(const std::string& name) const;
    void RemoveMaterial(const std::string& name);

    std::vector<std::string> GetMaterialNames() const;
    void Clear();

    // Load material library from file
    void LoadLibrary(const std::string& filepath);
    void SaveLibrary(const std::string& filepath) const;

    // Presets
    void LoadDefaultPresets();

private:
    MaterialLibrary() = default;
    std::map<std::string, std::shared_ptr<AdvancedMaterial>> m_materials;
};

} // namespace Engine
