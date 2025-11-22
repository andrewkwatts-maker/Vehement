#pragma once

#include "../../../engine/reflection/TypeInfo.hpp"
#include "../../../engine/reflection/TypeRegistry.hpp"
#include "../../../engine/events/PropertyWatcher.hpp"
#include "../../../engine/events/EventBinding.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <deque>
#include <unordered_map>

namespace Vehement {
namespace Editor {

/**
 * @brief Enhanced property inspector with observable support
 *
 * Features:
 * - Show observable properties
 * - Inline binding creation
 * - Property history graph
 * - Real-time value updates
 */
class PropertyInspector {
public:
    /**
     * @brief Configuration
     */
    struct Config {
        bool showObservableIndicator = true;
        bool enableHistoryGraph = true;
        size_t maxHistoryPoints = 100;
        float historyUpdateInterval = 0.1f;  // Seconds
        bool showBindingCreation = true;
        bool groupByCategory = true;
    };

    PropertyInspector();
    ~PropertyInspector();

    /**
     * @brief Initialize the inspector
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Set the object to inspect
     */
    void SetInspectedObject(void* object, const Nova::Reflect::TypeInfo* typeInfo);

    /**
     * @brief Clear the inspected object
     */
    void ClearInspectedObject();

    /**
     * @brief Update inspector
     */
    void Update(float deltaTime);

    /**
     * @brief Render the inspector UI
     */
    void Render();

    // =========================================================================
    // Visibility
    // =========================================================================

    void Show() { m_visible = true; }
    void Hide() { m_visible = false; }
    void Toggle() { m_visible = !m_visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    // =========================================================================
    // Property Watching
    // =========================================================================

    /**
     * @brief Start watching a property for changes
     */
    void WatchProperty(const std::string& propertyPath);

    /**
     * @brief Stop watching a property
     */
    void UnwatchProperty(const std::string& propertyPath);

    /**
     * @brief Check if a property is being watched
     */
    [[nodiscard]] bool IsPropertyWatched(const std::string& propertyPath) const;

    /**
     * @brief Get property watcher instance
     */
    [[nodiscard]] Nova::Events::PropertyWatcher& GetPropertyWatcher() { return m_watcher; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string& propertyPath, const std::any& newValue)> OnPropertyChanged;
    std::function<void(const std::string& propertyPath)> OnCreateBindingRequested;

private:
    // Rendering helpers
    void RenderHeader();
    void RenderPropertyList();
    void RenderProperty(const Nova::Reflect::PropertyInfo& prop, void* object);
    void RenderPropertyValue(const Nova::Reflect::PropertyInfo& prop, void* object);
    void RenderPropertyHistory(const std::string& propertyPath);
    void RenderBindingButton(const std::string& propertyPath);
    void RenderHistoryPopup(const std::string& propertyPath);

    // Value editors
    bool RenderBoolEditor(const std::string& label, bool& value);
    bool RenderIntEditor(const std::string& label, int& value, const Nova::Reflect::PropertyInfo& prop);
    bool RenderFloatEditor(const std::string& label, float& value, const Nova::Reflect::PropertyInfo& prop);
    bool RenderStringEditor(const std::string& label, std::string& value);
    bool RenderVec3Editor(const std::string& label, float* values);
    bool RenderColorEditor(const std::string& label, float* values);

    // History management
    void RecordPropertyValue(const std::string& propertyPath, const std::any& value);
    void UpdatePropertyHistory(float deltaTime);

    // State
    bool m_initialized = false;
    bool m_visible = true;
    Config m_config;

    // Inspected object
    void* m_inspectedObject = nullptr;
    const Nova::Reflect::TypeInfo* m_typeInfo = nullptr;

    // Property watching
    Nova::Events::PropertyWatcher m_watcher;
    std::unordered_map<std::string, std::string> m_watchIds;  // propertyPath -> watchId

    // Property history
    struct PropertyHistory {
        std::deque<std::pair<float, float>> values;  // time, value
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool isNumeric = true;
    };
    std::unordered_map<std::string, PropertyHistory> m_propertyHistory;
    float m_historyTimer = 0.0f;
    float m_totalTime = 0.0f;

    // UI state
    std::string m_selectedProperty;
    std::string m_searchFilter;
    bool m_showOnlyObservable = false;
    bool m_showReadOnly = true;
    std::string m_expandedCategory;
    std::string m_historyPopupProperty;
};

} // namespace Editor
} // namespace Vehement
