#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <any>
#include <mutex>

#include <nlohmann/json.hpp>

namespace Engine {
namespace UI {

// Forward declarations
class DOMElement;

/**
 * @brief Observable property change event
 */
struct PropertyChangeEvent {
    std::string path;
    nlohmann::json oldValue;
    nlohmann::json newValue;
    std::string source;
};

/**
 * @brief Computed property definition
 */
struct ComputedProperty {
    std::string name;
    std::vector<std::string> dependencies;
    std::function<nlohmann::json()> compute;
    nlohmann::json cachedValue;
    bool dirty = true;
};

/**
 * @brief Watcher definition
 */
struct Watcher {
    int id;
    std::string path;
    std::function<void(const nlohmann::json&, const nlohmann::json&)> callback;
    bool deep = false;
    bool immediate = false;
};

/**
 * @brief Binding expression
 */
struct BindingExpression {
    std::string expression;
    std::vector<std::string> dependencies;
    std::function<nlohmann::json(const nlohmann::json&)> evaluate;
};

/**
 * @brief Element binding
 */
struct ElementBinding {
    DOMElement* element = nullptr;
    std::string attribute;  // "textContent", "value", "checked", etc.
    std::string path;       // Data path
    bool twoWay = false;
    BindingExpression expression;
};

/**
 * @brief Observable data model
 */
class ObservableModel {
public:
    ObservableModel();
    ~ObservableModel();

    /**
     * @brief Get value at path
     */
    nlohmann::json Get(const std::string& path) const;

    /**
     * @brief Set value at path
     */
    void Set(const std::string& path, const nlohmann::json& value);

    /**
     * @brief Check if path exists
     */
    bool Has(const std::string& path) const;

    /**
     * @brief Delete value at path
     */
    void Delete(const std::string& path);

    /**
     * @brief Get all data as JSON
     */
    const nlohmann::json& GetData() const { return m_data; }

    /**
     * @brief Set all data
     */
    void SetData(const nlohmann::json& data);

    /**
     * @brief Merge data
     */
    void Merge(const nlohmann::json& data);

    /**
     * @brief Watch for changes
     */
    int Watch(const std::string& path,
             std::function<void(const nlohmann::json&, const nlohmann::json&)> callback,
             bool deep = false);

    /**
     * @brief Remove watcher
     */
    void Unwatch(int watcherId);

    /**
     * @brief Add computed property
     */
    void AddComputed(const std::string& name,
                    const std::vector<std::string>& dependencies,
                    std::function<nlohmann::json()> compute);

    /**
     * @brief Get computed property value
     */
    nlohmann::json GetComputed(const std::string& name);

    /**
     * @brief Invalidate computed property
     */
    void InvalidateComputed(const std::string& name);

    /**
     * @brief Begin batch update
     */
    void BeginBatch();

    /**
     * @brief End batch update
     */
    void EndBatch();

private:
    void NotifyWatchers(const std::string& path,
                       const nlohmann::json& oldValue,
                       const nlohmann::json& newValue);
    void UpdateComputedDependencies(const std::string& changedPath);
    nlohmann::json* GetPointerAt(const std::string& path);
    const nlohmann::json* GetPointerAt(const std::string& path) const;

    nlohmann::json m_data;
    std::vector<Watcher> m_watchers;
    std::unordered_map<std::string, ComputedProperty> m_computed;
    int m_nextWatcherId = 1;
    int m_batchDepth = 0;
    std::vector<PropertyChangeEvent> m_pendingChanges;
    mutable std::mutex m_mutex;
};

/**
 * @brief Reactive Data Binding System
 *
 * Provides two-way data binding between game data and UI elements,
 * observable data models, automatic UI updates, computed properties,
 * and watchers.
 */
class UIDataBinding {
public:
    UIDataBinding();
    ~UIDataBinding();

    /**
     * @brief Initialize the data binding system
     */
    void Initialize();

    /**
     * @brief Shutdown the data binding system
     */
    void Shutdown();

    /**
     * @brief Update bindings (call each frame)
     */
    void Update();

    // Model Management

    /**
     * @brief Create a new observable model
     * @param name Model name
     * @return Pointer to model
     */
    ObservableModel* CreateModel(const std::string& name);

    /**
     * @brief Get existing model
     */
    ObservableModel* GetModel(const std::string& name);

    /**
     * @brief Remove model
     */
    void RemoveModel(const std::string& name);

    /**
     * @brief Get default model
     */
    ObservableModel* GetDefaultModel() { return m_defaultModel.get(); }

    // Binding Operations

    /**
     * @brief Bind element to data path (one-way)
     * @param element DOM element
     * @param attribute Element attribute to bind
     * @param path Data path (e.g., "player.health")
     * @param modelName Model name (empty for default)
     * @return Binding ID
     */
    int Bind(DOMElement* element, const std::string& attribute,
            const std::string& path, const std::string& modelName = "");

    /**
     * @brief Bind element with two-way binding
     */
    int BindTwoWay(DOMElement* element, const std::string& attribute,
                  const std::string& path, const std::string& modelName = "");

    /**
     * @brief Bind with expression
     * @param element DOM element
     * @param attribute Element attribute
     * @param expression Expression string (e.g., "player.health + ' / ' + player.maxHealth")
     * @param modelName Model name
     * @return Binding ID
     */
    int BindExpression(DOMElement* element, const std::string& attribute,
                      const std::string& expression, const std::string& modelName = "");

    /**
     * @brief Remove binding
     */
    void Unbind(int bindingId);

    /**
     * @brief Remove all bindings for element
     */
    void UnbindElement(DOMElement* element);

    /**
     * @brief Remove all bindings for path
     */
    void UnbindPath(const std::string& path);

    // Data Operations

    /**
     * @brief Set data value
     */
    void SetValue(const std::string& path, const nlohmann::json& value,
                 const std::string& modelName = "");

    /**
     * @brief Get data value
     */
    nlohmann::json GetValue(const std::string& path, const std::string& modelName = "") const;

    /**
     * @brief Batch set multiple values
     */
    void SetValues(const std::unordered_map<std::string, nlohmann::json>& values,
                  const std::string& modelName = "");

    // Computed Properties

    /**
     * @brief Define computed property
     * @param name Property name
     * @param dependencies List of paths this depends on
     * @param compute Computation function
     * @param modelName Model name
     */
    void DefineComputed(const std::string& name,
                       const std::vector<std::string>& dependencies,
                       std::function<nlohmann::json()> compute,
                       const std::string& modelName = "");

    /**
     * @brief Get computed property value
     */
    nlohmann::json GetComputed(const std::string& name, const std::string& modelName = "");

    // Watchers

    /**
     * @brief Watch for changes at path
     * @param path Data path
     * @param callback Callback function (oldValue, newValue)
     * @param options Watch options
     * @param modelName Model name
     * @return Watcher ID
     */
    int Watch(const std::string& path,
             std::function<void(const nlohmann::json&, const nlohmann::json&)> callback,
             bool deep = false, const std::string& modelName = "");

    /**
     * @brief Remove watcher
     */
    void Unwatch(int watcherId);

    // Event Handling

    /**
     * @brief Handle input from UI element (for two-way binding)
     */
    void HandleElementInput(DOMElement* element, const nlohmann::json& value);

    /**
     * @brief Handle element event
     */
    void HandleElementEvent(DOMElement* element, const std::string& eventType,
                           const nlohmann::json& eventData);

    // Template Support

    /**
     * @brief Parse binding expressions in template
     * @param html HTML template string
     * @param model Model to bind to
     * @return Processed HTML
     */
    std::string ProcessTemplate(const std::string& html, ObservableModel* model);

    /**
     * @brief Bind template element (handles {{ }} expressions)
     */
    void BindTemplate(DOMElement* root, const std::string& modelName = "");

    // Formatting

    /**
     * @brief Register value formatter
     * @param name Formatter name
     * @param formatter Formatter function
     */
    void RegisterFormatter(const std::string& name,
                          std::function<std::string(const nlohmann::json&)> formatter);

    /**
     * @brief Apply formatter to value
     */
    std::string Format(const std::string& formatterName, const nlohmann::json& value);

    // Validation

    /**
     * @brief Register validator
     * @param path Data path
     * @param validator Validation function (returns error message or empty string)
     */
    void RegisterValidator(const std::string& path,
                          std::function<std::string(const nlohmann::json&)> validator);

    /**
     * @brief Validate value
     */
    std::string Validate(const std::string& path, const nlohmann::json& value);

    /**
     * @brief Validate all data
     */
    std::vector<std::pair<std::string, std::string>> ValidateAll(const std::string& modelName = "");

    // Debugging

    /**
     * @brief Get binding count
     */
    size_t GetBindingCount() const { return m_bindings.size(); }

    /**
     * @brief Get all binding paths
     */
    std::vector<std::string> GetBoundPaths() const;

    /**
     * @brief Dump binding state (for debugging)
     */
    nlohmann::json DumpState() const;

private:
    void UpdateBinding(ElementBinding& binding);
    void ApplyValueToElement(DOMElement* element, const std::string& attribute,
                            const nlohmann::json& value);
    nlohmann::json GetValueFromElement(DOMElement* element, const std::string& attribute);
    BindingExpression ParseExpression(const std::string& expression);
    nlohmann::json EvaluateExpression(const BindingExpression& expr, ObservableModel* model);

    std::unique_ptr<ObservableModel> m_defaultModel;
    std::unordered_map<std::string, std::unique_ptr<ObservableModel>> m_models;

    std::unordered_map<int, ElementBinding> m_bindings;
    int m_nextBindingId = 1;

    std::unordered_map<std::string, std::function<std::string(const nlohmann::json&)>> m_formatters;
    std::unordered_map<std::string, std::function<std::string(const nlohmann::json&)>> m_validators;

    std::vector<int> m_dirtyBindings;
    bool m_initialized = false;
};

} // namespace UI
} // namespace Engine
