#pragma once

#include "../engine/core/PropertySystem.hpp"
#include "PropertyOverrideUI.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Nova3D {

// Forward declarations
class Light;
class LightFunction;
class IESProfile;

// Light type enumeration
enum class LightType {
    Directional,
    Point,
    Spot,
    Area,
    Tube,
    Emissive,
    IES,
    Volumetric
};

// Physical light units
enum class LightUnit {
    Candela,        // cd (luminous intensity)
    Lumen,          // lm (luminous flux)
    Lux,            // lx (illuminance)
    Nits            // cd/m² (luminance)
};

// Light function animation types
enum class LightFunctionType {
    Constant,
    Sine,
    Pulse,
    Flicker,
    Strobe,
    Breath,
    Fire,
    Lightning,
    Custom,
    Texture
};

// Texture mapping modes
enum class LightTextureMapping {
    UV,
    Spherical,
    Cylindrical,
    LatLong,
    Gobo
};

// Light preset data
struct LightPreset {
    std::string name;
    LightType type;
    float temperature;
    float intensity;
    glm::vec3 color;
    float radius;
    float coneAngle;
};

// Light editor panel
class LightEditorPanel {
public:
    LightEditorPanel();
    ~LightEditorPanel();

    void Initialize();
    void Shutdown();

    void RenderUI();

    // Light management
    void SetCurrentLight(Light* light);
    Light* GetCurrentLight() const { return m_currentLight; }

    void SaveLight();
    void SaveLightAs();
    void LoadLight(const std::string& filepath);
    void ResetLight();
    void DuplicateLight();

    // Preset management
    void ApplyPreset(const std::string& presetName);
    void SaveAsPreset(const std::string& presetName);

    // Panel state
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }

    // Property level editing
    void SetEditLevel(PropertyLevel level) { m_editLevel = level; }
    PropertyLevel GetEditLevel() const { return m_editLevel; }

private:
    // Tab rendering
    void RenderBasicPropertiesTab();
    void RenderPhysicalUnitsTab();
    void RenderFunctionTab();
    void RenderTextureTab();
    void RenderIESTab();
    void RenderShadowsTab();
    void RenderPreviewTab();

    // Basic properties
    void RenderLightTypeSelector();
    void RenderColorProperties();
    void RenderIntensityProperties();
    void RenderRangeProperties();
    void RenderShapeProperties();

    // Physical units
    void RenderUnitSelector();
    void RenderCandelaControls();
    void RenderLumenControls();
    void RenderLuxControls();
    void RenderNitsControls();
    void RenderTemperatureControls();

    // Light function
    void RenderFunctionTypeSelector();
    void RenderFunctionParameters();
    void RenderFunctionPreview();

    // Texture mapping
    void RenderTextureMappingSelector();
    void RenderUVMappingControls();
    void RenderSphericalMappingControls();
    void RenderGoboControls();

    // IES profile
    void RenderIESProfileSelector();
    void RenderIESPreview();

    // Shadows
    void RenderShadowSettings();
    void RenderCascadeSettings();

    // Preview
    void UpdatePreview();
    void RenderPreviewControls();
    void RenderLightPreview();

    // Preset management
    void LoadPresets();
    void SavePresets();

    // UI helpers
    void RenderLightSelector();
    void RenderPresetDropdown();
    void RenderToolbar();
    void RenderStatusBar();

    // Helper functions
    const char* LightTypeToString(LightType type) const;
    const char* LightUnitToString(LightUnit unit) const;
    const char* LightFunctionTypeToString(LightFunctionType type) const;
    glm::vec3 TemperatureToColor(float kelvin) const;

private:
    bool m_isOpen = true;
    PropertyLevel m_editLevel = PropertyLevel::Asset;

    // Current light
    Light* m_currentLight = nullptr;
    PropertyContainer* m_lightProperties = nullptr;

    // Light library
    std::vector<Light*> m_lightLibrary;
    std::vector<LightPreset> m_presets;

    // IES profiles
    std::vector<std::shared_ptr<IESProfile>> m_iesProfiles;
    int m_currentIESProfile = 0;

    // Preview
    ImTextureID m_previewTexture = nullptr;
    int m_previewSize = 256;

    // UI state
    bool m_showOnlyOverridden = false;

    // Temporary values for editing
    struct {
        LightType type = LightType::Point;
        LightUnit unit = LightUnit::Candela;
        LightFunctionType functionType = LightFunctionType::Constant;
        LightTextureMapping textureMapping = LightTextureMapping::UV;

        glm::vec3 color = glm::vec3(1.0f);
        float temperature = 6500.0f;
        bool useTemperature = true;

        float intensity = 1000.0f;
        float candela = 1000.0f;
        float lumen = 1000.0f;
        float lux = 100.0f;
        float nits = 100.0f;

        float radius = 10.0f;
        float coneAngleInner = 30.0f;
        float coneAngleOuter = 45.0f;
        float length = 5.0f;
        glm::vec2 areaSize = glm::vec2(1.0f);

        // Light function parameters
        float functionFrequency = 1.0f;
        float functionAmplitude = 1.0f;
        float functionPhase = 0.0f;
        float functionOffset = 0.0f;
        std::string functionTexture;

        // Texture mapping
        std::string lightTexture;
        glm::vec2 textureScale = glm::vec2(1.0f);
        glm::vec2 textureOffset = glm::vec2(0.0f);
        float textureRotation = 0.0f;

        // Shadows
        bool castShadows = true;
        int shadowMapSize = 1024;
        float shadowBias = 0.001f;
        float shadowNormalBias = 0.01f;
        int cascadeCount = 4;
        float cascadeSplitLambda = 0.5f;

        // Emissive geometry
        bool useEmissiveGeometry = false;
        std::string emissiveMesh;
    } m_tempValues;
};

// Preset definitions
namespace LightPresets {
    extern const LightPreset Daylight;
    extern const LightPreset Tungsten;
    extern const LightPreset Fluorescent;
    extern const LightPreset LED;
    extern const LightPreset Candle;
    extern const LightPreset Fire;
    extern const LightPreset Moonlight;
    extern const LightPreset Streetlight;
}

} // namespace Nova3D
