#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include "../../../engine/procedural/WorldTemplate.hpp"
#include "../../../engine/procedural/ProcGenGraph.hpp"

namespace Vehement {

// Forward declarations
class WorldTemplateLibrary;

/**
 * @brief World size preset
 */
enum class WorldSizePreset {
    Small,      // 2500x2500m
    Medium,     // 5000x5000m
    Large,      // 10000x10000m
    Huge,       // 20000x20000m
    Custom
};

/**
 * @brief World creation parameters
 */
struct WorldCreationParams {
    std::string templateId;
    std::string worldName = "New World";
    int seed = 0;
    bool randomSeed = true;

    // Size
    WorldSizePreset sizePreset = WorldSizePreset::Medium;
    glm::ivec2 customSize = glm::ivec2(5000, 5000);

    // Advanced settings
    int erosionIterations = 100;
    float resourceDensity = 1.0f;
    float structureDensity = 1.0f;
    bool useRealWorldData = false;
    float terrainRoughness = 1.0f;
    float waterLevel = 0.0f;

    // Output
    std::string savePath;
    bool generateImmediately = true;

    /**
     * @brief Get actual world size based on preset
     */
    glm::ivec2 GetWorldSize() const;

    /**
     * @brief Generate random seed
     */
    void GenerateRandomSeed();
};

/**
 * @brief Modal dialog for creating new worlds from templates
 *
 * Features:
 * - Template selection with thumbnail previews
 * - Template info display (name, description, biomes, features)
 * - Seed input (random button + manual entry)
 * - World size configuration
 * - Advanced settings collapsible section
 * - Integration with WorldTemplate and ProcGenGraph
 */
class NewWorldDialog {
public:
    NewWorldDialog();
    ~NewWorldDialog();

    /**
     * @brief Show the dialog
     */
    void Show();

    /**
     * @brief Hide the dialog
     */
    void Hide();

    /**
     * @brief Check if dialog is visible
     */
    bool IsVisible() const { return m_isVisible; }

    /**
     * @brief Render the dialog UI
     * Call this every frame when visible
     */
    void Render();

    /**
     * @brief Set callback for when world is created
     */
    void SetOnWorldCreatedCallback(std::function<void(const WorldCreationParams&)> callback) {
        m_onWorldCreated = callback;
    }

    /**
     * @brief Set template library
     */
    void SetTemplateLibrary(std::shared_ptr<WorldTemplateLibrary> library) {
        m_templateLibrary = library;
    }

    /**
     * @brief Reset dialog to default state
     */
    void Reset();

private:
    bool m_isVisible = false;
    WorldCreationParams m_params;
    std::shared_ptr<WorldTemplateLibrary> m_templateLibrary;
    std::function<void(const WorldCreationParams&)> m_onWorldCreated;

    // UI state
    int m_selectedTemplateIndex = 0;
    std::vector<std::string> m_availableTemplateIds;
    std::shared_ptr<Nova::ProcGen::WorldTemplate> m_currentTemplate;
    bool m_showAdvancedSettings = false;
    char m_seedBuffer[32] = "12345";
    char m_worldNameBuffer[256] = "New World";

    // Validation
    bool m_hasValidationError = false;
    std::string m_validationError;

    /**
     * @brief Refresh template list
     */
    void RefreshTemplateList();

    /**
     * @brief Load selected template
     */
    void LoadSelectedTemplate();

    /**
     * @brief Render template selection UI
     */
    void RenderTemplateSelection();

    /**
     * @brief Render template info panel
     */
    void RenderTemplateInfo();

    /**
     * @brief Render template preview thumbnail
     */
    void RenderTemplatePreview();

    /**
     * @brief Render basic settings
     */
    void RenderBasicSettings();

    /**
     * @brief Render world size selection
     */
    void RenderWorldSizeSelection();

    /**
     * @brief Render advanced settings section
     */
    void RenderAdvancedSettings();

    /**
     * @brief Render action buttons
     */
    void RenderActionButtons();

    /**
     * @brief Validate input parameters
     */
    bool ValidateParams();

    /**
     * @brief Create world from current parameters
     */
    void CreateWorld();

    /**
     * @brief Create empty world (no template)
     */
    void CreateEmptyWorld();

    /**
     * @brief Get world size description string
     */
    std::string GetWorldSizeDescription(WorldSizePreset preset) const;

    /**
     * @brief Generate random seed value
     */
    int GenerateRandomSeed();
};

/**
 * @brief Helper functions for world creation
 */
namespace WorldCreationUtils {
    /**
     * @brief Convert world size preset to actual size
     */
    glm::ivec2 PresetToSize(WorldSizePreset preset);

    /**
     * @brief Get preset name
     */
    const char* GetPresetName(WorldSizePreset preset);

    /**
     * @brief Parse seed from string
     */
    int ParseSeed(const std::string& seedStr);
}

} // namespace Vehement
