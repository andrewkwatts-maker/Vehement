#include "UIDataBinding.hpp"
#include "HTMLRenderer.hpp"

#include <algorithm>
#include <sstream>
#include <regex>

namespace Engine {
namespace UI {

// ObservableModel implementation
ObservableModel::ObservableModel() : m_data(nlohmann::json::object()) {}
ObservableModel::~ObservableModel() = default;

nlohmann::json ObservableModel::Get(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto* ptr = GetPointerAt(path);
    return ptr ? *ptr : nlohmann::json(nullptr);
}

void ObservableModel::Set(const std::string& path, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    nlohmann::json oldValue = Get(path);

    // Navigate to parent and set value
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    nlohmann::json* current = &m_data;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->contains(parts[i])) {
            (*current)[parts[i]] = nlohmann::json::object();
        }
        current = &(*current)[parts[i]];
    }

    if (!parts.empty()) {
        (*current)[parts.back()] = value;
    }

    // Notify watchers
    if (m_batchDepth > 0) {
        PropertyChangeEvent event;
        event.path = path;
        event.oldValue = oldValue;
        event.newValue = value;
        m_pendingChanges.push_back(event);
    } else {
        NotifyWatchers(path, oldValue, value);
        UpdateComputedDependencies(path);
    }
}

bool ObservableModel::Has(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return GetPointerAt(path) != nullptr;
}

void ObservableModel::Delete(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.empty()) return;

    nlohmann::json* current = &m_data;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->contains(parts[i])) return;
        current = &(*current)[parts[i]];
    }

    if (current->contains(parts.back())) {
        current->erase(parts.back());
    }
}

void ObservableModel::SetData(const nlohmann::json& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json oldData = m_data;
    m_data = data;

    // Notify root watchers
    NotifyWatchers("", oldData, data);
}

void ObservableModel::Merge(const nlohmann::json& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.merge_patch(data);
}

int ObservableModel::Watch(const std::string& path,
                          std::function<void(const nlohmann::json&, const nlohmann::json&)> callback,
                          bool deep) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Watcher watcher;
    watcher.id = m_nextWatcherId++;
    watcher.path = path;
    watcher.callback = callback;
    watcher.deep = deep;

    m_watchers.push_back(watcher);

    return watcher.id;
}

void ObservableModel::Unwatch(int watcherId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_watchers.erase(
        std::remove_if(m_watchers.begin(), m_watchers.end(),
            [watcherId](const Watcher& w) { return w.id == watcherId; }),
        m_watchers.end());
}

void ObservableModel::AddComputed(const std::string& name,
                                 const std::vector<std::string>& dependencies,
                                 std::function<nlohmann::json()> compute) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ComputedProperty prop;
    prop.name = name;
    prop.dependencies = dependencies;
    prop.compute = compute;
    prop.dirty = true;

    m_computed[name] = prop;
}

nlohmann::json ObservableModel::GetComputed(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_computed.find(name);
    if (it == m_computed.end()) {
        return nullptr;
    }

    if (it->second.dirty) {
        it->second.cachedValue = it->second.compute();
        it->second.dirty = false;
    }

    return it->second.cachedValue;
}

void ObservableModel::InvalidateComputed(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_computed.find(name);
    if (it != m_computed.end()) {
        it->second.dirty = true;
    }
}

void ObservableModel::BeginBatch() {
    m_batchDepth++;
}

void ObservableModel::EndBatch() {
    if (m_batchDepth > 0) {
        m_batchDepth--;

        if (m_batchDepth == 0) {
            // Process all pending changes
            for (const auto& change : m_pendingChanges) {
                NotifyWatchers(change.path, change.oldValue, change.newValue);
            }
            m_pendingChanges.clear();
        }
    }
}

void ObservableModel::NotifyWatchers(const std::string& path,
                                    const nlohmann::json& oldValue,
                                    const nlohmann::json& newValue) {
    for (auto& watcher : m_watchers) {
        bool shouldNotify = false;

        if (watcher.path.empty()) {
            // Root watcher
            shouldNotify = true;
        } else if (watcher.path == path) {
            // Exact match
            shouldNotify = true;
        } else if (watcher.deep) {
            // Check if changed path is a child of watched path
            shouldNotify = (path.find(watcher.path + ".") == 0);
        }

        if (shouldNotify && watcher.callback) {
            watcher.callback(oldValue, newValue);
        }
    }
}

void ObservableModel::UpdateComputedDependencies(const std::string& changedPath) {
    for (auto& [name, prop] : m_computed) {
        for (const auto& dep : prop.dependencies) {
            if (dep == changedPath || changedPath.find(dep + ".") == 0) {
                prop.dirty = true;
                break;
            }
        }
    }
}

nlohmann::json* ObservableModel::GetPointerAt(const std::string& path) {
    if (path.empty()) return &m_data;

    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    nlohmann::json* current = &m_data;
    for (const auto& p : parts) {
        if (!current->contains(p)) return nullptr;
        current = &(*current)[p];
    }

    return current;
}

const nlohmann::json* ObservableModel::GetPointerAt(const std::string& path) const {
    return const_cast<ObservableModel*>(this)->GetPointerAt(path);
}

// UIDataBinding implementation
UIDataBinding::UIDataBinding() = default;
UIDataBinding::~UIDataBinding() { Shutdown(); }

void UIDataBinding::Initialize() {
    if (m_initialized) return;

    m_defaultModel = std::make_unique<ObservableModel>();

    // Register default formatters
    RegisterFormatter("currency", [](const nlohmann::json& value) {
        if (value.is_number()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "$%.2f", value.get<double>());
            return std::string(buf);
        }
        return value.dump();
    });

    RegisterFormatter("percent", [](const nlohmann::json& value) {
        if (value.is_number()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f%%", value.get<double>() * 100);
            return std::string(buf);
        }
        return value.dump();
    });

    RegisterFormatter("uppercase", [](const nlohmann::json& value) {
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        }
        return value.dump();
    });

    RegisterFormatter("lowercase", [](const nlohmann::json& value) {
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        }
        return value.dump();
    });

    m_initialized = true;
}

void UIDataBinding::Shutdown() {
    m_bindings.clear();
    m_models.clear();
    m_defaultModel.reset();
    m_formatters.clear();
    m_validators.clear();
    m_initialized = false;
}

void UIDataBinding::Update() {
    // Update dirty bindings
    for (int id : m_dirtyBindings) {
        auto it = m_bindings.find(id);
        if (it != m_bindings.end()) {
            UpdateBinding(it->second);
        }
    }
    m_dirtyBindings.clear();
}

ObservableModel* UIDataBinding::CreateModel(const std::string& name) {
    auto model = std::make_unique<ObservableModel>();
    ObservableModel* ptr = model.get();
    m_models[name] = std::move(model);
    return ptr;
}

ObservableModel* UIDataBinding::GetModel(const std::string& name) {
    if (name.empty()) return m_defaultModel.get();

    auto it = m_models.find(name);
    if (it != m_models.end()) {
        return it->second.get();
    }
    return nullptr;
}

void UIDataBinding::RemoveModel(const std::string& name) {
    m_models.erase(name);
}

int UIDataBinding::Bind(DOMElement* element, const std::string& attribute,
                       const std::string& path, const std::string& modelName) {
    int id = m_nextBindingId++;

    ElementBinding binding;
    binding.element = element;
    binding.attribute = attribute;
    binding.path = path;
    binding.twoWay = false;

    m_bindings[id] = binding;

    // Set up watcher
    auto* model = GetModel(modelName);
    if (model) {
        model->Watch(path, [this, id](const nlohmann::json&, const nlohmann::json&) {
            m_dirtyBindings.push_back(id);
        });

        // Initial update
        UpdateBinding(m_bindings[id]);
    }

    return id;
}

int UIDataBinding::BindTwoWay(DOMElement* element, const std::string& attribute,
                             const std::string& path, const std::string& modelName) {
    int id = Bind(element, attribute, path, modelName);

    if (id > 0) {
        m_bindings[id].twoWay = true;
    }

    return id;
}

int UIDataBinding::BindExpression(DOMElement* element, const std::string& attribute,
                                 const std::string& expression, const std::string& modelName) {
    int id = m_nextBindingId++;

    ElementBinding binding;
    binding.element = element;
    binding.attribute = attribute;
    binding.twoWay = false;
    binding.expression = ParseExpression(expression);

    m_bindings[id] = binding;

    // Set up watchers for dependencies
    auto* model = GetModel(modelName);
    if (model) {
        for (const auto& dep : binding.expression.dependencies) {
            model->Watch(dep, [this, id](const nlohmann::json&, const nlohmann::json&) {
                m_dirtyBindings.push_back(id);
            });
        }

        // Initial update
        UpdateBinding(m_bindings[id]);
    }

    return id;
}

void UIDataBinding::Unbind(int bindingId) {
    m_bindings.erase(bindingId);
}

void UIDataBinding::UnbindElement(DOMElement* element) {
    for (auto it = m_bindings.begin(); it != m_bindings.end();) {
        if (it->second.element == element) {
            it = m_bindings.erase(it);
        } else {
            ++it;
        }
    }
}

void UIDataBinding::UnbindPath(const std::string& path) {
    for (auto it = m_bindings.begin(); it != m_bindings.end();) {
        if (it->second.path == path) {
            it = m_bindings.erase(it);
        } else {
            ++it;
        }
    }
}

void UIDataBinding::SetValue(const std::string& path, const nlohmann::json& value,
                            const std::string& modelName) {
    auto* model = GetModel(modelName);
    if (model) {
        model->Set(path, value);
    }
}

nlohmann::json UIDataBinding::GetValue(const std::string& path, const std::string& modelName) const {
    auto* model = const_cast<UIDataBinding*>(this)->GetModel(modelName);
    if (model) {
        return model->Get(path);
    }
    return nullptr;
}

void UIDataBinding::SetValues(const std::unordered_map<std::string, nlohmann::json>& values,
                             const std::string& modelName) {
    auto* model = GetModel(modelName);
    if (model) {
        model->BeginBatch();
        for (const auto& [path, value] : values) {
            model->Set(path, value);
        }
        model->EndBatch();
    }
}

void UIDataBinding::DefineComputed(const std::string& name,
                                  const std::vector<std::string>& dependencies,
                                  std::function<nlohmann::json()> compute,
                                  const std::string& modelName) {
    auto* model = GetModel(modelName);
    if (model) {
        model->AddComputed(name, dependencies, compute);
    }
}

nlohmann::json UIDataBinding::GetComputed(const std::string& name, const std::string& modelName) {
    auto* model = GetModel(modelName);
    if (model) {
        return model->GetComputed(name);
    }
    return nullptr;
}

int UIDataBinding::Watch(const std::string& path,
                        std::function<void(const nlohmann::json&, const nlohmann::json&)> callback,
                        bool deep, const std::string& modelName) {
    auto* model = GetModel(modelName);
    if (model) {
        return model->Watch(path, callback, deep);
    }
    return 0;
}

void UIDataBinding::Unwatch(int watcherId) {
    // Check all models for this watcher
    m_defaultModel->Unwatch(watcherId);
    for (auto& [name, model] : m_models) {
        model->Unwatch(watcherId);
    }
}

void UIDataBinding::HandleElementInput(DOMElement* element, const nlohmann::json& value) {
    // Find two-way bindings for this element
    for (auto& [id, binding] : m_bindings) {
        if (binding.element == element && binding.twoWay) {
            SetValue(binding.path, value);
        }
    }
}

void UIDataBinding::HandleElementEvent(DOMElement* element, const std::string& eventType,
                                       const nlohmann::json& eventData) {
    if (eventType == "input" || eventType == "change") {
        nlohmann::json value = GetValueFromElement(element, "value");
        HandleElementInput(element, value);
    }
}

std::string UIDataBinding::ProcessTemplate(const std::string& html, ObservableModel* model) {
    std::string result = html;

    // Process {{ expression }} bindings
    std::regex exprRegex(R"(\{\{([^}]+)\}\})");
    std::smatch match;

    std::string::const_iterator searchStart = result.cbegin();
    std::string processed;

    while (std::regex_search(searchStart, result.cend(), match, exprRegex)) {
        processed += std::string(searchStart, match[0].first);

        std::string expression = match[1].str();
        // Trim whitespace
        size_t start = expression.find_first_not_of(" \t");
        size_t end = expression.find_last_not_of(" \t");
        if (start != std::string::npos) {
            expression = expression.substr(start, end - start + 1);
        }

        // Evaluate expression
        nlohmann::json value = model->Get(expression);
        if (value.is_string()) {
            processed += value.get<std::string>();
        } else if (!value.is_null()) {
            processed += value.dump();
        }

        searchStart = match.suffix().first;
    }
    processed += std::string(searchStart, result.cend());

    return processed;
}

void UIDataBinding::BindTemplate(DOMElement* root, const std::string& modelName) {
    if (!root) return;

    // Process text content
    if (!root->textContent.empty() && root->textContent.find("{{") != std::string::npos) {
        // Create binding for text content
        BindExpression(root, "textContent", root->textContent, modelName);
    }

    // Process attributes
    for (const auto& [name, value] : root->attributes) {
        if (value.find("{{") != std::string::npos) {
            BindExpression(root, name, value, modelName);
        }
    }

    // Recurse to children
    for (auto& child : root->children) {
        BindTemplate(child.get(), modelName);
    }
}

void UIDataBinding::RegisterFormatter(const std::string& name,
                                     std::function<std::string(const nlohmann::json&)> formatter) {
    m_formatters[name] = formatter;
}

std::string UIDataBinding::Format(const std::string& formatterName, const nlohmann::json& value) {
    auto it = m_formatters.find(formatterName);
    if (it != m_formatters.end()) {
        return it->second(value);
    }

    if (value.is_string()) {
        return value.get<std::string>();
    }
    return value.dump();
}

void UIDataBinding::RegisterValidator(const std::string& path,
                                     std::function<std::string(const nlohmann::json&)> validator) {
    m_validators[path] = validator;
}

std::string UIDataBinding::Validate(const std::string& path, const nlohmann::json& value) {
    auto it = m_validators.find(path);
    if (it != m_validators.end()) {
        return it->second(value);
    }
    return "";
}

std::vector<std::pair<std::string, std::string>> UIDataBinding::ValidateAll(const std::string& modelName) {
    std::vector<std::pair<std::string, std::string>> errors;

    auto* model = GetModel(modelName);
    if (!model) return errors;

    for (const auto& [path, validator] : m_validators) {
        nlohmann::json value = model->Get(path);
        std::string error = validator(value);
        if (!error.empty()) {
            errors.emplace_back(path, error);
        }
    }

    return errors;
}

std::vector<std::string> UIDataBinding::GetBoundPaths() const {
    std::vector<std::string> paths;

    for (const auto& [id, binding] : m_bindings) {
        if (!binding.path.empty()) {
            paths.push_back(binding.path);
        }
    }

    // Remove duplicates
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    return paths;
}

nlohmann::json UIDataBinding::DumpState() const {
    nlohmann::json state;

    state["bindingCount"] = m_bindings.size();
    state["modelCount"] = m_models.size() + 1; // +1 for default

    nlohmann::json bindings = nlohmann::json::array();
    for (const auto& [id, binding] : m_bindings) {
        nlohmann::json b;
        b["id"] = id;
        b["path"] = binding.path;
        b["attribute"] = binding.attribute;
        b["twoWay"] = binding.twoWay;
        bindings.push_back(b);
    }
    state["bindings"] = bindings;

    return state;
}

void UIDataBinding::UpdateBinding(ElementBinding& binding) {
    if (!binding.element) return;

    nlohmann::json value;

    if (!binding.expression.expression.empty()) {
        // Evaluate expression
        value = EvaluateExpression(binding.expression, m_defaultModel.get());
    } else if (!binding.path.empty()) {
        value = GetValue(binding.path);
    }

    ApplyValueToElement(binding.element, binding.attribute, value);
}

void UIDataBinding::ApplyValueToElement(DOMElement* element, const std::string& attribute,
                                       const nlohmann::json& value) {
    if (!element) return;

    std::string strValue;
    if (value.is_string()) {
        strValue = value.get<std::string>();
    } else if (!value.is_null()) {
        strValue = value.dump();
    }

    if (attribute == "textContent") {
        element->textContent = strValue;
    } else if (attribute == "innerHTML") {
        element->innerHTML = strValue;
    } else {
        element->SetAttribute(attribute, strValue);
    }
}

nlohmann::json UIDataBinding::GetValueFromElement(DOMElement* element, const std::string& attribute) {
    if (!element) return nullptr;

    if (attribute == "textContent") {
        return element->textContent;
    } else if (attribute == "innerHTML") {
        return element->innerHTML;
    } else {
        return element->GetAttribute(attribute);
    }
}

BindingExpression UIDataBinding::ParseExpression(const std::string& expression) {
    BindingExpression expr;
    expr.expression = expression;

    // Extract dependencies (simple implementation)
    // Look for identifiers that could be data paths
    std::regex pathRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z_][a-zA-Z0-9_]*)*)\b)");
    std::smatch match;

    std::string::const_iterator searchStart = expression.cbegin();
    while (std::regex_search(searchStart, expression.cend(), match, pathRegex)) {
        std::string dep = match[1].str();
        // Exclude JavaScript keywords
        if (dep != "true" && dep != "false" && dep != "null" && dep != "undefined") {
            expr.dependencies.push_back(dep);
        }
        searchStart = match.suffix().first;
    }

    // Remove duplicates
    std::sort(expr.dependencies.begin(), expr.dependencies.end());
    expr.dependencies.erase(std::unique(expr.dependencies.begin(), expr.dependencies.end()),
                           expr.dependencies.end());

    return expr;
}

nlohmann::json UIDataBinding::EvaluateExpression(const BindingExpression& expr, ObservableModel* model) {
    if (!model) return nullptr;

    // Simple implementation - just get single values
    if (expr.dependencies.size() == 1) {
        return model->Get(expr.dependencies[0]);
    }

    // For complex expressions, we'd need a proper expression evaluator
    // This is a simplified concatenation
    std::string result;
    for (const auto& dep : expr.dependencies) {
        nlohmann::json value = model->Get(dep);
        if (value.is_string()) {
            result += value.get<std::string>();
        } else if (!value.is_null()) {
            result += value.dump();
        }
    }

    return result;
}

} // namespace UI
} // namespace Engine
