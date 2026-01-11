#pragma once

#include "core/SettingsManager.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Nova {

/**
 * @brief Performance impact indicator
 */
enum class PerformanceImpact {
    None,      // No performance impact
    Low,       // Minimal impact (green)
    Medium,    // Moderate impact (yellow)
    High       // Heavy impact (red)
};

/**
 * @brief Settings change preview info
 */
struct SettingPreview {
    std::string name;
    std::string oldValue;
    std::string newValue;
    PerformanceImpact impact;
};

/**
 * @brief Performance stats for display
 */
struct PerformanceStats {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    float cullingTimeMs = 0.0f;
    float lightingTimeMs = 0.0f;
    float renderingTimeMs = 0.0f;
    int gpuMemoryUsedMB = 0;
    int gpuMemoryTotalMB = 0;

    // Caching stats
    int bricksCached = 0;
    int bricksTotal = 0;
    int cacheMemoryUsedMB = 0;
    int cacheMemoryTotalMB = 0;
    int dedupSavingsPercent = 0;
};

/**
 * @brief Settings tab enumeration
 */
enum class SettingsTab {
    Rendering,
    Lighting,
    Materials,
    LOD,
    Caching,
    Performance
};

/**
 * @brief Comprehensive settings menu for Nova3D engine
 *
 * Features:
 * - 6 organized tabs for all engine settings
 * - Quality preset system (Low, Medium, High, Ultra, Custom)
 * - JSON save/load functionality
 * - Real-time preview of changes
 * - Performance impact indicators
 * - Comprehensive tooltips
 * - Validation and error handling
 */
class SettingsMenuComplete {
public:
    SettingsMenuComplete();
    ~SettingsMenuComplete();

    /**
     * @brief Initialize the settings menu
     */
    void Initialize();

    /**
     * @brief Render the settings menu
     */
    void Render(bool* isOpen);

    /**
     * @brief Update performance stats (call every frame)
     */
    void UpdateStats(const PerformanceStats& stats);

    /**
     * @brief Set custom performance stats callback
     */
    void SetStatsCallback(std::function<PerformanceStats()> callback) {
        m_statsCallback = callback;
    }

    /**
     * @brief Check if settings have been modified
     */
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }

    /**
     * @brief Get current settings
     */
    const CompleteSettings& GetSettings() const { return m_currentSettings; }

private:
    // Tab rendering functions
    void RenderMenuBar();
    void RenderTabBar();
    void RenderRenderingTab();
    void RenderLightingTab();
    void RenderMaterialsTab();
    void RenderLODTab();
    void RenderCachingTab();
    void RenderPerformanceTab();
    void RenderControlButtons();

    // Rendering tab sections
    void RenderRenderingBackend();
    void RenderSDFRasterizer();
    void RenderPolygonRasterizer();
    void RenderGPUDriven();
    void RenderAsyncCompute();

    // Lighting tab sections
    void RenderClusteredLighting();
    void RenderShadowSettings();
    void RenderGlobalIllumination();
    void RenderLightTypes();

    // Materials tab sections
    void RenderPhysicalProperties();
    void RenderTextureQuality();
    void RenderShaderCompilation();

    // LOD tab sections
    void RenderGlobalLOD();
    void RenderDistanceThresholds();
    void RenderTransitionSettings();
    void RenderPerTypeSettings();

    // Caching tab sections
    void RenderSDFBrickCache();
    void RenderShaderCache();
    void RenderLightCache();

    // Performance tab sections
    void RenderThreadPool();
    void RenderMemorySettings();
    void RenderProfilingSettings();
    void RenderCurrentStats();

    // Utility functions
    void ApplyPreset(QualityPreset preset);
    void SaveSettings();
    void LoadSettings();
    void ResetToDefault();
    void ApplyChanges();
    void DiscardChanges();
    void MarkAsModified();
    void ClearModifiedFlag();

    // Validation
    bool ValidateSettings();
    void ShowValidationErrors();

    // Preview system
    void UpdatePreview();
    void RenderPreviewPanel();
    PerformanceImpact EstimateImpact(const std::string& settingName);

    // Helper UI functions
    void RenderSectionHeader(const char* title);
    void RenderSettingRow(const char* label, float labelWidth = 200.0f);
    void RenderPerformanceIndicator(PerformanceImpact impact);
    void RenderTooltip(const char* text);
    void RenderProgressBar(float fraction, const char* overlay = nullptr);
    bool RenderButton(const char* label, const char* tooltip = nullptr);

    // State
    SettingsTab m_currentTab = SettingsTab::Rendering;
    CompleteSettings m_currentSettings;
    CompleteSettings m_originalSettings;
    bool m_hasUnsavedChanges = false;
    bool m_showValidationErrors = false;
    std::vector<std::string> m_validationErrors;

    // Preview
    std::vector<SettingPreview> m_previews;
    bool m_showPreview = false;

    // Performance stats
    PerformanceStats m_stats;
    std::function<PerformanceStats()> m_statsCallback;

    // UI state
    int m_selectedBackend = 0;
    int m_selectedGIMethod = 3;
    int m_selectedLODQuality = 3;
    int m_selectedUpdateFreq = 0;
    bool m_showResetConfirm = false;
    bool m_showLoadDialog = false;
    bool m_showSaveDialog = false;
    char m_saveFilepath[256] = "assets/config/user_settings.json";
    char m_loadFilepath[256] = "assets/config/user_settings.json";

    // Performance impact cache
    std::unordered_map<std::string, PerformanceImpact> m_impactCache;
};

/**
 * @brief Helper functions for UI rendering
 */
namespace SettingsUI {
    void BeginSettingGroup(const char* label);
    void EndSettingGroup();
    void Separator();
    void HelpMarker(const char* desc);
    void ColoredText(float r, float g, float b, const char* fmt, ...);
}

} // namespace Nova
