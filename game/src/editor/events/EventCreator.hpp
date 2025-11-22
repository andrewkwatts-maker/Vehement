#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <variant>
#include <optional>

namespace Vehement {
namespace WebEditor {
    class JSBridge;
}

/**
 * @brief Comparison operators for conditions
 */
enum class ComparisonOp {
    Equal,
    NotEqual,
    LessThan,
    LessOrEqual,
    GreaterThan,
    GreaterOrEqual,
    Contains,
    StartsWith,
    EndsWith,
    Matches,      // Regex
    InRange,
    IsNull,
    IsNotNull,
    Changed,
    BitSet,
    BitClear
};

/**
 * @brief Logical operators for compound conditions
 */
enum class LogicalOp {
    And,
    Or,
    Not,
    Xor,
    Nand,
    Nor
};

/**
 * @brief Value types for condition comparisons
 */
using ConditionValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<int64_t>,
    std::vector<double>,
    std::vector<std::string>
>;

/**
 * @brief A single condition node in the condition tree
 */
struct ConditionNode {
    std::string id;
    std::string description;

    // For property conditions
    std::string propertyPath;
    ComparisonOp comparison = ComparisonOp::Equal;
    ConditionValue compareValue;
    ConditionValue rangeMin;
    ConditionValue rangeMax;

    // For compound conditions
    LogicalOp logicalOp = LogicalOp::And;
    std::vector<std::shared_ptr<ConditionNode>> children;

    // For Python conditions
    std::string pythonExpression;
    std::string pythonModule;
    std::string pythonFunction;

    // Metadata
    bool negated = false;
    bool enabled = true;
    std::vector<std::string> tags;

    // Visual editor position
    float posX = 0.0f;
    float posY = 0.0f;

    [[nodiscard]] bool IsCompound() const { return !children.empty(); }
    [[nodiscard]] bool IsPython() const {
        return !pythonExpression.empty() || !pythonFunction.empty();
    }
};

/**
 * @brief Timer configuration for timer-based events
 */
struct TimerConfig {
    enum class Type {
        OneShot,
        Repeating,
        RandomInterval
    };

    Type type = Type::OneShot;
    float interval = 1.0f;          // Base interval in seconds
    float randomMin = 0.0f;         // Minimum interval for random
    float randomMax = 0.0f;         // Maximum interval for random
    int maxRepetitions = -1;        // -1 for unlimited
    bool startPaused = false;
    float initialDelay = 0.0f;
};

/**
 * @brief Property watcher configuration
 */
struct PropertyWatcher {
    std::string id;
    std::string propertyPath;
    std::string sourceType;         // Entity type to watch
    std::string sourceId;           // Specific entity ID (empty for all)

    // Threshold triggers
    bool watchThreshold = false;
    double thresholdValue = 0.0;
    bool triggerAbove = true;       // Trigger when value goes above threshold

    // Rate of change trigger
    bool watchRateOfChange = false;
    double rateThreshold = 0.0;     // Change per second

    // Debounce
    float debounceTime = 0.0f;

    // Callback
    std::string callbackId;
};

/**
 * @brief Condition template for reuse
 */
struct ConditionTemplate {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::shared_ptr<ConditionNode> rootCondition;
    std::vector<std::string> parameterNames;  // Placeholders in the condition
    std::vector<std::string> tags;
    bool isBuiltIn = false;
};

/**
 * @brief Test result for condition testing
 */
struct ConditionTestResult {
    bool success = false;
    bool conditionResult = false;
    std::string errorMessage;
    float evaluationTimeMs = 0.0f;
    std::vector<std::pair<std::string, bool>> nodeResults;  // Node ID -> result
};

/**
 * @brief Event Creator
 *
 * Create new events based on conditions:
 * - Condition builder with visual AND/OR tree
 * - Property watchers with thresholds
 * - Timer-based events
 * - Compound condition editor
 * - Test condition button
 * - Save as template
 */
class EventCreator {
public:
    /**
     * @brief Configuration
     */
    struct Config {
        int maxConditionDepth = 10;
        int maxWatchers = 100;
        bool enablePythonConditions = true;
        std::string templatesPath = "config/condition_templates.json";
    };

    /**
     * @brief Callbacks
     */
    using OnConditionCreatedCallback = std::function<void(const std::shared_ptr<ConditionNode>&)>;
    using OnTemplateCreatedCallback = std::function<void(const ConditionTemplate&)>;
    using OnWatcherCreatedCallback = std::function<void(const PropertyWatcher&)>;

    EventCreator();
    ~EventCreator();

    // Non-copyable
    EventCreator(const EventCreator&) = delete;
    EventCreator& operator=(const EventCreator&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the event creator
     * @param bridge Reference to JSBridge
     * @param config Configuration
     * @return true if successful
     */
    bool Initialize(WebEditor::JSBridge& bridge, const Config& config = {});

    /**
     * @brief Shutdown
     */
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Rendering
    // =========================================================================

    /**
     * @brief Update state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the condition builder UI (ImGui)
     */
    void Render();

    /**
     * @brief Render the visual condition tree (WebView)
     */
    void RenderWebEditor(WebEditor::WebViewManager& webViewManager);

    // =========================================================================
    // Condition Building
    // =========================================================================

    /**
     * @brief Create a new property condition
     */
    std::shared_ptr<ConditionNode> CreatePropertyCondition(
        const std::string& propertyPath,
        ComparisonOp comparison,
        const ConditionValue& value);

    /**
     * @brief Create a compound condition (AND/OR/NOT)
     */
    std::shared_ptr<ConditionNode> CreateCompoundCondition(
        LogicalOp op,
        const std::vector<std::shared_ptr<ConditionNode>>& children);

    /**
     * @brief Create a Python condition
     */
    std::shared_ptr<ConditionNode> CreatePythonCondition(
        const std::string& expression);

    /**
     * @brief Add child to compound condition
     */
    void AddConditionChild(const std::string& parentId,
                           std::shared_ptr<ConditionNode> child);

    /**
     * @brief Remove child from compound condition
     */
    void RemoveConditionChild(const std::string& parentId,
                              const std::string& childId);

    /**
     * @brief Get condition by ID
     */
    [[nodiscard]] std::shared_ptr<ConditionNode> GetCondition(const std::string& id) const;

    /**
     * @brief Get the current root condition being edited
     */
    [[nodiscard]] std::shared_ptr<ConditionNode> GetCurrentCondition() const {
        return m_currentCondition;
    }

    /**
     * @brief Set the current condition for editing
     */
    void SetCurrentCondition(std::shared_ptr<ConditionNode> condition);

    /**
     * @brief Clear the current condition
     */
    void ClearCurrentCondition();

    // =========================================================================
    // Condition Testing
    // =========================================================================

    /**
     * @brief Test condition against sample data
     * @param condition Condition to test
     * @param sampleData JSON sample data
     * @return Test result
     */
    ConditionTestResult TestCondition(
        const std::shared_ptr<ConditionNode>& condition,
        const std::string& sampleDataJson);

    /**
     * @brief Test current condition
     */
    ConditionTestResult TestCurrentCondition(const std::string& sampleDataJson);

    /**
     * @brief Set sample data for testing
     */
    void SetSampleData(const std::string& json);

    /**
     * @brief Get sample data
     */
    [[nodiscard]] const std::string& GetSampleData() const { return m_sampleData; }

    // =========================================================================
    // Property Watchers
    // =========================================================================

    /**
     * @brief Create a property watcher
     */
    PropertyWatcher CreateWatcher(const std::string& propertyPath,
                                   const std::string& sourceType);

    /**
     * @brief Configure watcher threshold
     */
    void SetWatcherThreshold(const std::string& watcherId,
                             double threshold, bool triggerAbove);

    /**
     * @brief Configure watcher rate of change
     */
    void SetWatcherRateOfChange(const std::string& watcherId, double rateThreshold);

    /**
     * @brief Delete a watcher
     */
    void DeleteWatcher(const std::string& watcherId);

    /**
     * @brief Get all watchers
     */
    [[nodiscard]] std::vector<PropertyWatcher> GetWatchers() const;

    // =========================================================================
    // Timer-Based Events
    // =========================================================================

    /**
     * @brief Create a timer event
     */
    std::string CreateTimerEvent(const std::string& eventName,
                                  const TimerConfig& config,
                                  const std::shared_ptr<ConditionNode>& condition);

    /**
     * @brief Delete a timer event
     */
    void DeleteTimerEvent(const std::string& timerId);

    /**
     * @brief Pause a timer
     */
    void PauseTimer(const std::string& timerId);

    /**
     * @brief Resume a timer
     */
    void ResumeTimer(const std::string& timerId);

    /**
     * @brief Get timer info
     */
    [[nodiscard]] std::optional<TimerConfig> GetTimerConfig(const std::string& timerId) const;

    // =========================================================================
    // Templates
    // =========================================================================

    /**
     * @brief Save current condition as template
     */
    ConditionTemplate SaveAsTemplate(const std::string& name,
                                      const std::string& category,
                                      const std::string& description);

    /**
     * @brief Load a template
     */
    void LoadTemplate(const std::string& templateId);

    /**
     * @brief Get all templates
     */
    [[nodiscard]] std::vector<ConditionTemplate> GetTemplates() const;

    /**
     * @brief Get templates by category
     */
    [[nodiscard]] std::vector<ConditionTemplate> GetTemplatesByCategory(
        const std::string& category) const;

    /**
     * @brief Delete a template
     */
    bool DeleteTemplate(const std::string& templateId);

    /**
     * @brief Import templates from file
     */
    bool ImportTemplates(const std::string& path);

    /**
     * @brief Export templates to file
     */
    bool ExportTemplates(const std::string& path);

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Export condition to JSON
     */
    [[nodiscard]] std::string ConditionToJson(
        const std::shared_ptr<ConditionNode>& condition) const;

    /**
     * @brief Import condition from JSON
     */
    std::shared_ptr<ConditionNode> ConditionFromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    OnConditionCreatedCallback OnConditionCreated;
    OnTemplateCreatedCallback OnTemplateCreated;
    OnWatcherCreatedCallback OnWatcherCreated;
    std::function<void(const ConditionTestResult&)> OnConditionTested;

private:
    // ImGui rendering helpers
    void RenderConditionTree();
    void RenderConditionNode(const std::shared_ptr<ConditionNode>& node, int depth);
    void RenderPropertyConditionEditor();
    void RenderCompoundConditionEditor();
    void RenderPythonConditionEditor();
    void RenderWatcherPanel();
    void RenderTimerPanel();
    void RenderTemplatePanel();
    void RenderTestPanel();
    void RenderSaveTemplateDialog();

    // Condition tree manipulation
    std::shared_ptr<ConditionNode> FindNode(const std::shared_ptr<ConditionNode>& root,
                                              const std::string& id);
    void RemoveNodeRecursive(std::shared_ptr<ConditionNode>& root, const std::string& id);

    // JSBridge registration
    void RegisterBridgeFunctions();

    // Utility
    std::string GenerateId(const std::string& prefix);
    std::string ComparisonOpToString(ComparisonOp op) const;
    ComparisonOp StringToComparisonOp(const std::string& str) const;
    std::string LogicalOpToString(LogicalOp op) const;
    LogicalOp StringToLogicalOp(const std::string& str) const;
    std::string ValueToString(const ConditionValue& value) const;

    // Timer update
    void UpdateTimers(float deltaTime);

    // State
    bool m_initialized = false;
    Config m_config;
    WebEditor::JSBridge* m_bridge = nullptr;

    // Current condition being edited
    std::shared_ptr<ConditionNode> m_currentCondition;
    std::string m_selectedNodeId;

    // All conditions (for lookup)
    std::unordered_map<std::string, std::shared_ptr<ConditionNode>> m_conditions;

    // Property watchers
    std::unordered_map<std::string, PropertyWatcher> m_watchers;

    // Timers
    struct TimerState {
        std::string id;
        std::string eventName;
        TimerConfig config;
        std::shared_ptr<ConditionNode> condition;
        float currentTime = 0.0f;
        int executionCount = 0;
        bool paused = false;
    };
    std::unordered_map<std::string, TimerState> m_timers;

    // Templates
    std::unordered_map<std::string, ConditionTemplate> m_templates;

    // Sample data for testing
    std::string m_sampleData;

    // UI state
    bool m_showSaveTemplateDialog = false;
    char m_templateName[256] = "";
    char m_templateCategory[128] = "";
    char m_templateDescription[512] = "";

    // New condition dialog state
    int m_newConditionType = 0;  // 0=property, 1=compound, 2=python
    char m_newPropertyPath[256] = "";
    int m_newComparisonOp = 0;
    char m_newCompareValue[256] = "";
    char m_newPythonExpression[1024] = "";

    // Web view ID
    std::string m_webViewId = "condition_builder";
};

} // namespace Vehement
