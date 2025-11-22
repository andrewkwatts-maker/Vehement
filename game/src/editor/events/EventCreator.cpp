#include "EventCreator.hpp"
#include "../web/JSBridge.hpp"
#include "../web/WebViewManager.hpp"
#include <imgui.h>
#include <sstream>
#include <fstream>
#include <random>
#include <chrono>

namespace Vehement {

EventCreator::EventCreator() = default;

EventCreator::~EventCreator() {
    if (m_initialized) {
        Shutdown();
    }
}

bool EventCreator::Initialize(WebEditor::JSBridge& bridge, const Config& config) {
    if (m_initialized) {
        return false;
    }

    m_bridge = &bridge;
    m_config = config;

    // Register JSBridge functions
    RegisterBridgeFunctions();

    // Load templates if file exists
    ImportTemplates(m_config.templatesPath);

    m_initialized = true;
    return true;
}

void EventCreator::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Export templates
    ExportTemplates(m_config.templatesPath);

    // Clear state
    m_currentCondition.reset();
    m_conditions.clear();
    m_watchers.clear();
    m_timers.clear();
    m_templates.clear();

    m_initialized = false;
}

void EventCreator::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update timers
    UpdateTimers(deltaTime);
}

void EventCreator::Render() {
    if (!m_initialized) {
        return;
    }

    ImGui::Begin("Event Creator");

    // Tab bar for different sections
    if (ImGui::BeginTabBar("EventCreatorTabs")) {
        if (ImGui::BeginTabItem("Condition Builder")) {
            RenderConditionTree();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Property Watchers")) {
            RenderWatcherPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Timers")) {
            RenderTimerPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Templates")) {
            RenderTemplatePanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Test")) {
            RenderTestPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // Dialogs
    if (m_showSaveTemplateDialog) {
        RenderSaveTemplateDialog();
    }

    ImGui::End();
}

void EventCreator::RenderWebEditor(WebEditor::WebViewManager& webViewManager) {
    if (!m_initialized) {
        return;
    }

    // Create web view if not exists
    if (!webViewManager.HasWebView(m_webViewId)) {
        WebEditor::WebViewConfig config;
        config.id = m_webViewId;
        config.title = "Condition Builder";
        config.width = 800;
        config.height = 600;
        config.debug = true;

        auto* webView = webViewManager.CreateWebView(config);
        if (webView) {
            webView->LoadFile("editor/html/condition_builder.html");
        }
    }

    webViewManager.RenderImGuiWindow(m_webViewId, "Visual Condition Builder", nullptr);
}

void EventCreator::RenderConditionTree() {
    // Toolbar
    if (ImGui::Button("+ Property")) {
        m_newConditionType = 0;
        ImGui::OpenPopup("New Condition");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ AND")) {
        auto node = CreateCompoundCondition(LogicalOp::And, {});
        if (!m_currentCondition) {
            m_currentCondition = node;
        } else {
            AddConditionChild(m_selectedNodeId.empty() ?
                             m_currentCondition->id : m_selectedNodeId, node);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+ OR")) {
        auto node = CreateCompoundCondition(LogicalOp::Or, {});
        if (!m_currentCondition) {
            m_currentCondition = node;
        } else {
            AddConditionChild(m_selectedNodeId.empty() ?
                             m_currentCondition->id : m_selectedNodeId, node);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+ NOT")) {
        auto node = CreateCompoundCondition(LogicalOp::Not, {});
        if (!m_currentCondition) {
            m_currentCondition = node;
        } else {
            AddConditionChild(m_selectedNodeId.empty() ?
                             m_currentCondition->id : m_selectedNodeId, node);
        }
    }
    if (m_config.enablePythonConditions) {
        ImGui::SameLine();
        if (ImGui::Button("+ Python")) {
            m_newConditionType = 2;
            ImGui::OpenPopup("New Condition");
        }
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    if (ImGui::Button("Clear")) {
        ClearCurrentCondition();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Template")) {
        m_showSaveTemplateDialog = true;
    }

    ImGui::Separator();

    // New condition popup
    if (ImGui::BeginPopup("New Condition")) {
        if (m_newConditionType == 0) {
            RenderPropertyConditionEditor();
        } else if (m_newConditionType == 2) {
            RenderPythonConditionEditor();
        }
        ImGui::EndPopup();
    }

    // Condition tree display
    ImGui::BeginChild("ConditionTreeView", ImVec2(0, 300), true);
    if (m_currentCondition) {
        RenderConditionNode(m_currentCondition, 0);
    } else {
        ImGui::TextDisabled("No condition. Click + to add.");
    }
    ImGui::EndChild();

    // JSON preview
    ImGui::Text("JSON Preview:");
    ImGui::BeginChild("JSONPreview", ImVec2(0, 150), true);
    if (m_currentCondition) {
        std::string json = ConditionToJson(m_currentCondition);
        ImGui::TextWrapped("%s", json.c_str());
    }
    ImGui::EndChild();
}

void EventCreator::RenderConditionNode(const std::shared_ptr<ConditionNode>& node, int depth) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
    if (node->id == m_selectedNodeId) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (!node->IsCompound() && !node->IsPython()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    // Node color based on type
    ImVec4 color;
    std::string label;
    if (node->IsCompound()) {
        color = ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
        label = LogicalOpToString(node->logicalOp);
    } else if (node->IsPython()) {
        color = ImVec4(0.7f, 0.5f, 1.0f, 1.0f);
        label = "Python: " + node->pythonExpression.substr(0, 30);
    } else {
        color = ImVec4(0.7f, 1.0f, 0.7f, 1.0f);
        label = node->propertyPath + " " +
                ComparisonOpToString(node->comparison) + " " +
                ValueToString(node->compareValue);
    }

    if (node->negated) {
        label = "NOT (" + label + ")";
    }

    if (!node->enabled) {
        color.w = 0.5f;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    bool opened = ImGui::TreeNodeEx(node->id.c_str(), flags, "%s", label.c_str());
    ImGui::PopStyleColor();

    if (ImGui::IsItemClicked()) {
        m_selectedNodeId = node->id;
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            if (m_currentCondition && m_currentCondition->id == node->id) {
                ClearCurrentCondition();
            } else {
                RemoveNodeRecursive(m_currentCondition, node->id);
            }
        }
        bool negated = node->negated;
        if (ImGui::MenuItem("Negate", nullptr, &negated)) {
            node->negated = negated;
        }
        bool enabled = node->enabled;
        if (ImGui::MenuItem("Enabled", nullptr, &enabled)) {
            node->enabled = enabled;
        }
        ImGui::EndPopup();
    }

    if (opened) {
        // Render children for compound nodes
        if (node->IsCompound()) {
            for (const auto& child : node->children) {
                RenderConditionNode(child, depth + 1);
            }
        }
        ImGui::TreePop();
    }
}

void EventCreator::RenderPropertyConditionEditor() {
    ImGui::Text("Property Condition");
    ImGui::Separator();

    ImGui::InputText("Property Path", m_newPropertyPath, sizeof(m_newPropertyPath));

    const char* opNames[] = {
        "==", "!=", "<", "<=", ">", ">=",
        "contains", "startsWith", "endsWith", "matches",
        "inRange", "isNull", "isNotNull", "changed"
    };
    ImGui::Combo("Operator", &m_newComparisonOp, opNames, IM_ARRAYSIZE(opNames));

    ComparisonOp op = static_cast<ComparisonOp>(m_newComparisonOp);
    if (op != ComparisonOp::IsNull && op != ComparisonOp::IsNotNull &&
        op != ComparisonOp::Changed) {
        ImGui::InputText("Value", m_newCompareValue, sizeof(m_newCompareValue));
    }

    ImGui::Separator();

    if (ImGui::Button("Create")) {
        auto node = CreatePropertyCondition(
            m_newPropertyPath,
            op,
            std::string(m_newCompareValue)
        );

        if (!m_currentCondition) {
            m_currentCondition = node;
        } else if (!m_selectedNodeId.empty()) {
            AddConditionChild(m_selectedNodeId, node);
        }

        // Reset form
        m_newPropertyPath[0] = '\0';
        m_newCompareValue[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
}

void EventCreator::RenderCompoundConditionEditor() {
    ImGui::Text("Compound Condition");
    // This is handled by the + AND/OR/NOT buttons
}

void EventCreator::RenderPythonConditionEditor() {
    ImGui::Text("Python Condition");
    ImGui::Separator();

    ImGui::InputTextMultiline("Expression", m_newPythonExpression,
                              sizeof(m_newPythonExpression),
                              ImVec2(400, 100));

    ImGui::Separator();

    if (ImGui::Button("Create")) {
        auto node = CreatePythonCondition(m_newPythonExpression);

        if (!m_currentCondition) {
            m_currentCondition = node;
        } else if (!m_selectedNodeId.empty()) {
            AddConditionChild(m_selectedNodeId, node);
        }

        m_newPythonExpression[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
}

void EventCreator::RenderWatcherPanel() {
    if (ImGui::Button("+ New Watcher")) {
        ImGui::OpenPopup("New Watcher");
    }

    ImGui::Separator();

    // Watcher list
    for (auto& [id, watcher] : m_watchers) {
        if (ImGui::CollapsingHeader(watcher.propertyPath.c_str())) {
            ImGui::PushID(id.c_str());

            ImGui::Text("ID: %s", id.c_str());
            ImGui::Text("Source: %s:%s", watcher.sourceType.c_str(),
                       watcher.sourceId.empty() ? "*" : watcher.sourceId.c_str());

            ImGui::Separator();

            ImGui::Checkbox("Watch Threshold", &watcher.watchThreshold);
            if (watcher.watchThreshold) {
                ImGui::InputDouble("Threshold", &watcher.thresholdValue);
                ImGui::Checkbox("Trigger Above", &watcher.triggerAbove);
            }

            ImGui::Checkbox("Watch Rate of Change", &watcher.watchRateOfChange);
            if (watcher.watchRateOfChange) {
                ImGui::InputDouble("Rate Threshold", &watcher.rateThreshold);
            }

            ImGui::InputFloat("Debounce (s)", &watcher.debounceTime);

            if (ImGui::Button("Delete")) {
                DeleteWatcher(id);
            }

            ImGui::PopID();
        }
    }

    // New watcher popup
    if (ImGui::BeginPopup("New Watcher")) {
        static char propertyPath[256] = "";
        static char sourceType[128] = "";

        ImGui::InputText("Property Path", propertyPath, sizeof(propertyPath));
        ImGui::InputText("Source Type", sourceType, sizeof(sourceType));

        if (ImGui::Button("Create")) {
            CreateWatcher(propertyPath, sourceType);
            propertyPath[0] = '\0';
            sourceType[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EventCreator::RenderTimerPanel() {
    if (ImGui::Button("+ New Timer")) {
        ImGui::OpenPopup("New Timer");
    }

    ImGui::Separator();

    // Timer list
    for (auto& [id, timer] : m_timers) {
        if (ImGui::CollapsingHeader(timer.eventName.c_str())) {
            ImGui::PushID(id.c_str());

            ImGui::Text("ID: %s", id.c_str());

            const char* typeNames[] = {"One-Shot", "Repeating", "Random Interval"};
            ImGui::Text("Type: %s", typeNames[static_cast<int>(timer.config.type)]);

            ImGui::Text("Interval: %.2f s", timer.config.interval);
            ImGui::Text("Executions: %d / %d", timer.executionCount,
                       timer.config.maxRepetitions < 0 ? -1 : timer.config.maxRepetitions);

            // Progress bar for next trigger
            float progress = timer.currentTime / timer.config.interval;
            ImGui::ProgressBar(progress, ImVec2(-1, 0),
                              timer.paused ? "Paused" : "Running");

            if (timer.paused) {
                if (ImGui::Button("Resume")) {
                    ResumeTimer(id);
                }
            } else {
                if (ImGui::Button("Pause")) {
                    PauseTimer(id);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                DeleteTimerEvent(id);
            }

            ImGui::PopID();
        }
    }

    // New timer popup
    if (ImGui::BeginPopup("New Timer")) {
        static char eventName[256] = "";
        static int timerType = 0;
        static float interval = 1.0f;
        static float randomMin = 0.5f;
        static float randomMax = 2.0f;
        static int maxReps = -1;

        ImGui::InputText("Event Name", eventName, sizeof(eventName));

        const char* typeNames[] = {"One-Shot", "Repeating", "Random Interval"};
        ImGui::Combo("Type", &timerType, typeNames, IM_ARRAYSIZE(typeNames));

        if (timerType != 2) {
            ImGui::InputFloat("Interval (s)", &interval);
        } else {
            ImGui::InputFloat("Min Interval", &randomMin);
            ImGui::InputFloat("Max Interval", &randomMax);
        }

        if (timerType == 1) {
            ImGui::InputInt("Max Repetitions (-1 = unlimited)", &maxReps);
        }

        if (ImGui::Button("Create")) {
            TimerConfig config;
            config.type = static_cast<TimerConfig::Type>(timerType);
            config.interval = interval;
            config.randomMin = randomMin;
            config.randomMax = randomMax;
            config.maxRepetitions = maxReps;

            CreateTimerEvent(eventName, config, nullptr);

            eventName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EventCreator::RenderTemplatePanel() {
    // Template list
    std::unordered_set<std::string> categories;
    for (const auto& [id, tmpl] : m_templates) {
        categories.insert(tmpl.category);
    }

    for (const auto& category : categories) {
        if (ImGui::CollapsingHeader(category.c_str())) {
            auto templates = GetTemplatesByCategory(category);
            for (const auto& tmpl : templates) {
                ImGui::PushID(tmpl.id.c_str());

                if (ImGui::Selectable(tmpl.name.c_str())) {
                    LoadTemplate(tmpl.id);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", tmpl.description.c_str());
                    ImGui::EndTooltip();
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                if (!tmpl.isBuiltIn && ImGui::SmallButton("X")) {
                    DeleteTemplate(tmpl.id);
                }

                ImGui::PopID();
            }
        }
    }
}

void EventCreator::RenderTestPanel() {
    ImGui::Text("Test Condition");
    ImGui::Separator();

    // Sample data input
    ImGui::Text("Sample Data (JSON):");
    static char sampleBuffer[4096] = "{}";
    if (ImGui::InputTextMultiline("##SampleData", sampleBuffer, sizeof(sampleBuffer),
                                   ImVec2(-1, 200))) {
        m_sampleData = sampleBuffer;
    }

    ImGui::Separator();

    if (ImGui::Button("Test Condition")) {
        auto result = TestCurrentCondition(m_sampleData);

        if (OnConditionTested) {
            OnConditionTested(result);
        }
    }

    // Show last test result
    static ConditionTestResult lastResult;
    static bool hasResult = false;

    if (ImGui::Button("Test Condition")) {
        lastResult = TestCurrentCondition(m_sampleData);
        hasResult = true;
    }

    if (hasResult) {
        ImGui::Separator();
        ImGui::Text("Result:");

        if (lastResult.success) {
            ImVec4 color = lastResult.conditionResult ?
                ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
                ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

            ImGui::TextColored(color, "Condition: %s",
                              lastResult.conditionResult ? "TRUE" : "FALSE");
            ImGui::Text("Evaluation time: %.3f ms", lastResult.evaluationTimeMs);

            // Node-by-node results
            if (ImGui::CollapsingHeader("Node Results")) {
                for (const auto& [nodeId, nodeResult] : lastResult.nodeResults) {
                    ImVec4 nodeColor = nodeResult ?
                        ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
                        ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
                    ImGui::TextColored(nodeColor, "%s: %s",
                                      nodeId.c_str(),
                                      nodeResult ? "TRUE" : "FALSE");
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                              "Error: %s", lastResult.errorMessage.c_str());
        }
    }
}

void EventCreator::RenderSaveTemplateDialog() {
    ImGui::OpenPopup("Save as Template");
    if (ImGui::BeginPopupModal("Save as Template", &m_showSaveTemplateDialog)) {
        ImGui::InputText("Name", m_templateName, sizeof(m_templateName));
        ImGui::InputText("Category", m_templateCategory, sizeof(m_templateCategory));
        ImGui::InputTextMultiline("Description", m_templateDescription,
                                  sizeof(m_templateDescription), ImVec2(400, 100));

        ImGui::Separator();

        if (ImGui::Button("Save")) {
            SaveAsTemplate(m_templateName, m_templateCategory, m_templateDescription);
            m_templateName[0] = '\0';
            m_templateCategory[0] = '\0';
            m_templateDescription[0] = '\0';
            m_showSaveTemplateDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_showSaveTemplateDialog = false;
        }

        ImGui::EndPopup();
    }
}

// Condition Building
std::shared_ptr<ConditionNode> EventCreator::CreatePropertyCondition(
    const std::string& propertyPath,
    ComparisonOp comparison,
    const ConditionValue& value) {

    auto node = std::make_shared<ConditionNode>();
    node->id = GenerateId("cond");
    node->propertyPath = propertyPath;
    node->comparison = comparison;
    node->compareValue = value;

    m_conditions[node->id] = node;

    if (OnConditionCreated) {
        OnConditionCreated(node);
    }

    return node;
}

std::shared_ptr<ConditionNode> EventCreator::CreateCompoundCondition(
    LogicalOp op,
    const std::vector<std::shared_ptr<ConditionNode>>& children) {

    auto node = std::make_shared<ConditionNode>();
    node->id = GenerateId("cond");
    node->logicalOp = op;
    node->children = children;

    m_conditions[node->id] = node;

    if (OnConditionCreated) {
        OnConditionCreated(node);
    }

    return node;
}

std::shared_ptr<ConditionNode> EventCreator::CreatePythonCondition(
    const std::string& expression) {

    auto node = std::make_shared<ConditionNode>();
    node->id = GenerateId("cond");
    node->pythonExpression = expression;

    m_conditions[node->id] = node;

    if (OnConditionCreated) {
        OnConditionCreated(node);
    }

    return node;
}

void EventCreator::AddConditionChild(const std::string& parentId,
                                      std::shared_ptr<ConditionNode> child) {
    auto parent = GetCondition(parentId);
    if (parent && parent->IsCompound()) {
        parent->children.push_back(child);
    }
}

void EventCreator::RemoveConditionChild(const std::string& parentId,
                                         const std::string& childId) {
    auto parent = GetCondition(parentId);
    if (parent) {
        auto& children = parent->children;
        children.erase(
            std::remove_if(children.begin(), children.end(),
                          [&childId](const auto& c) { return c->id == childId; }),
            children.end()
        );
    }
}

std::shared_ptr<ConditionNode> EventCreator::GetCondition(const std::string& id) const {
    auto it = m_conditions.find(id);
    return it != m_conditions.end() ? it->second : nullptr;
}

void EventCreator::SetCurrentCondition(std::shared_ptr<ConditionNode> condition) {
    m_currentCondition = condition;
}

void EventCreator::ClearCurrentCondition() {
    m_currentCondition.reset();
    m_selectedNodeId.clear();
}

// Testing
ConditionTestResult EventCreator::TestCondition(
    const std::shared_ptr<ConditionNode>& condition,
    const std::string& sampleDataJson) {

    ConditionTestResult result;
    auto start = std::chrono::high_resolution_clock::now();

    if (!condition) {
        result.success = false;
        result.errorMessage = "No condition provided";
        return result;
    }

    // In a real implementation, this would evaluate the condition
    // against the parsed JSON data using Python or native evaluation
    result.success = true;
    result.conditionResult = true;  // Placeholder

    auto end = std::chrono::high_resolution_clock::now();
    result.evaluationTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

    return result;
}

ConditionTestResult EventCreator::TestCurrentCondition(const std::string& sampleDataJson) {
    return TestCondition(m_currentCondition, sampleDataJson);
}

void EventCreator::SetSampleData(const std::string& json) {
    m_sampleData = json;
}

// Property Watchers
PropertyWatcher EventCreator::CreateWatcher(const std::string& propertyPath,
                                              const std::string& sourceType) {
    PropertyWatcher watcher;
    watcher.id = GenerateId("watch");
    watcher.propertyPath = propertyPath;
    watcher.sourceType = sourceType;

    m_watchers[watcher.id] = watcher;

    if (OnWatcherCreated) {
        OnWatcherCreated(watcher);
    }

    return watcher;
}

void EventCreator::SetWatcherThreshold(const std::string& watcherId,
                                         double threshold, bool triggerAbove) {
    auto it = m_watchers.find(watcherId);
    if (it != m_watchers.end()) {
        it->second.watchThreshold = true;
        it->second.thresholdValue = threshold;
        it->second.triggerAbove = triggerAbove;
    }
}

void EventCreator::SetWatcherRateOfChange(const std::string& watcherId,
                                           double rateThreshold) {
    auto it = m_watchers.find(watcherId);
    if (it != m_watchers.end()) {
        it->second.watchRateOfChange = true;
        it->second.rateThreshold = rateThreshold;
    }
}

void EventCreator::DeleteWatcher(const std::string& watcherId) {
    m_watchers.erase(watcherId);
}

std::vector<PropertyWatcher> EventCreator::GetWatchers() const {
    std::vector<PropertyWatcher> result;
    result.reserve(m_watchers.size());
    for (const auto& [id, watcher] : m_watchers) {
        result.push_back(watcher);
    }
    return result;
}

// Timer-Based Events
std::string EventCreator::CreateTimerEvent(const std::string& eventName,
                                             const TimerConfig& config,
                                             const std::shared_ptr<ConditionNode>& condition) {
    TimerState timer;
    timer.id = GenerateId("timer");
    timer.eventName = eventName;
    timer.config = config;
    timer.condition = condition;
    timer.currentTime = 0.0f;
    timer.executionCount = 0;
    timer.paused = config.startPaused;

    m_timers[timer.id] = timer;
    return timer.id;
}

void EventCreator::DeleteTimerEvent(const std::string& timerId) {
    m_timers.erase(timerId);
}

void EventCreator::PauseTimer(const std::string& timerId) {
    auto it = m_timers.find(timerId);
    if (it != m_timers.end()) {
        it->second.paused = true;
    }
}

void EventCreator::ResumeTimer(const std::string& timerId) {
    auto it = m_timers.find(timerId);
    if (it != m_timers.end()) {
        it->second.paused = false;
    }
}

std::optional<TimerConfig> EventCreator::GetTimerConfig(const std::string& timerId) const {
    auto it = m_timers.find(timerId);
    if (it != m_timers.end()) {
        return it->second.config;
    }
    return std::nullopt;
}

void EventCreator::UpdateTimers(float deltaTime) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    for (auto& [id, timer] : m_timers) {
        if (timer.paused) continue;

        timer.currentTime += deltaTime;

        float targetInterval = timer.config.interval;
        if (timer.config.type == TimerConfig::Type::RandomInterval) {
            std::uniform_real_distribution<float> dist(timer.config.randomMin,
                                                        timer.config.randomMax);
            targetInterval = dist(gen);
        }

        if (timer.currentTime >= targetInterval) {
            timer.currentTime = 0.0f;
            timer.executionCount++;

            // Fire event (in real implementation)
            // ...

            // Check max repetitions
            if (timer.config.type == TimerConfig::Type::OneShot ||
                (timer.config.maxRepetitions >= 0 &&
                 timer.executionCount >= timer.config.maxRepetitions)) {
                timer.paused = true;
            }
        }
    }
}

// Templates
ConditionTemplate EventCreator::SaveAsTemplate(const std::string& name,
                                                 const std::string& category,
                                                 const std::string& description) {
    ConditionTemplate tmpl;
    tmpl.id = GenerateId("tmpl");
    tmpl.name = name;
    tmpl.category = category;
    tmpl.description = description;
    tmpl.rootCondition = m_currentCondition;
    tmpl.isBuiltIn = false;

    m_templates[tmpl.id] = tmpl;

    if (OnTemplateCreated) {
        OnTemplateCreated(tmpl);
    }

    return tmpl;
}

void EventCreator::LoadTemplate(const std::string& templateId) {
    auto it = m_templates.find(templateId);
    if (it != m_templates.end()) {
        // Deep copy the condition tree
        m_currentCondition = ConditionFromJson(ConditionToJson(it->second.rootCondition));
    }
}

std::vector<ConditionTemplate> EventCreator::GetTemplates() const {
    std::vector<ConditionTemplate> result;
    result.reserve(m_templates.size());
    for (const auto& [id, tmpl] : m_templates) {
        result.push_back(tmpl);
    }
    return result;
}

std::vector<ConditionTemplate> EventCreator::GetTemplatesByCategory(
    const std::string& category) const {

    std::vector<ConditionTemplate> result;
    for (const auto& [id, tmpl] : m_templates) {
        if (tmpl.category == category) {
            result.push_back(tmpl);
        }
    }
    return result;
}

bool EventCreator::DeleteTemplate(const std::string& templateId) {
    auto it = m_templates.find(templateId);
    if (it != m_templates.end() && !it->second.isBuiltIn) {
        m_templates.erase(it);
        return true;
    }
    return false;
}

bool EventCreator::ImportTemplates(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    // Parse JSON and populate m_templates
    return true;
}

bool EventCreator::ExportTemplates(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    // Export m_templates to JSON
    return true;
}

// Serialization
std::string EventCreator::ConditionToJson(
    const std::shared_ptr<ConditionNode>& condition) const {

    if (!condition) {
        return "null";
    }

    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << condition->id << "\",\n";

    if (condition->IsCompound()) {
        ss << "  \"type\": \"compound\",\n";
        ss << "  \"operator\": \"" << LogicalOpToString(condition->logicalOp) << "\",\n";
        ss << "  \"children\": [\n";
        for (size_t i = 0; i < condition->children.size(); ++i) {
            if (i > 0) ss << ",\n";
            ss << ConditionToJson(condition->children[i]);
        }
        ss << "\n  ]";
    } else if (condition->IsPython()) {
        ss << "  \"type\": \"python\",\n";
        ss << "  \"expression\": \"" << condition->pythonExpression << "\"";
    } else {
        ss << "  \"type\": \"property\",\n";
        ss << "  \"propertyPath\": \"" << condition->propertyPath << "\",\n";
        ss << "  \"comparison\": \"" << ComparisonOpToString(condition->comparison) << "\",\n";
        ss << "  \"value\": \"" << ValueToString(condition->compareValue) << "\"";
    }

    ss << ",\n  \"negated\": " << (condition->negated ? "true" : "false");
    ss << ",\n  \"enabled\": " << (condition->enabled ? "true" : "false");
    ss << "\n}";

    return ss.str();
}

std::shared_ptr<ConditionNode> EventCreator::ConditionFromJson(const std::string& json) {
    // Placeholder - would use a proper JSON parser
    auto node = std::make_shared<ConditionNode>();
    node->id = GenerateId("cond");
    return node;
}

// Private helpers
void EventCreator::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    m_bridge->RegisterFunction("conditionBuilder.getCondition", [this](const auto&) {
        if (m_currentCondition) {
            return WebEditor::JSResult::Success(
                WebEditor::JSValue(ConditionToJson(m_currentCondition)));
        }
        return WebEditor::JSResult::Success(WebEditor::JSValue());
    });

    m_bridge->RegisterFunction("conditionBuilder.test", [this](const auto& args) {
        std::string sampleData = args.empty() ? "{}" : args[0].GetString();
        auto result = TestCurrentCondition(sampleData);
        WebEditor::JSValue::Object obj;
        obj["success"] = result.success;
        obj["result"] = result.conditionResult;
        obj["error"] = result.errorMessage;
        obj["timeMs"] = result.evaluationTimeMs;
        return WebEditor::JSResult::Success(WebEditor::JSValue(std::move(obj)));
    });
}

std::shared_ptr<ConditionNode> EventCreator::FindNode(
    const std::shared_ptr<ConditionNode>& root,
    const std::string& id) {

    if (!root) return nullptr;
    if (root->id == id) return root;

    for (const auto& child : root->children) {
        auto found = FindNode(child, id);
        if (found) return found;
    }

    return nullptr;
}

void EventCreator::RemoveNodeRecursive(std::shared_ptr<ConditionNode>& root,
                                        const std::string& id) {
    if (!root) return;

    auto& children = root->children;
    children.erase(
        std::remove_if(children.begin(), children.end(),
                      [&id](const auto& c) { return c->id == id; }),
        children.end()
    );

    for (auto& child : children) {
        RemoveNodeRecursive(child, id);
    }
}

std::string EventCreator::GenerateId(const std::string& prefix) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << prefix << "_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string EventCreator::ComparisonOpToString(ComparisonOp op) const {
    switch (op) {
        case ComparisonOp::Equal: return "==";
        case ComparisonOp::NotEqual: return "!=";
        case ComparisonOp::LessThan: return "<";
        case ComparisonOp::LessOrEqual: return "<=";
        case ComparisonOp::GreaterThan: return ">";
        case ComparisonOp::GreaterOrEqual: return ">=";
        case ComparisonOp::Contains: return "contains";
        case ComparisonOp::StartsWith: return "startsWith";
        case ComparisonOp::EndsWith: return "endsWith";
        case ComparisonOp::Matches: return "matches";
        case ComparisonOp::InRange: return "inRange";
        case ComparisonOp::IsNull: return "isNull";
        case ComparisonOp::IsNotNull: return "isNotNull";
        case ComparisonOp::Changed: return "changed";
        case ComparisonOp::BitSet: return "bitSet";
        case ComparisonOp::BitClear: return "bitClear";
        default: return "==";
    }
}

ComparisonOp EventCreator::StringToComparisonOp(const std::string& str) const {
    if (str == "==") return ComparisonOp::Equal;
    if (str == "!=") return ComparisonOp::NotEqual;
    if (str == "<") return ComparisonOp::LessThan;
    if (str == "<=") return ComparisonOp::LessOrEqual;
    if (str == ">") return ComparisonOp::GreaterThan;
    if (str == ">=") return ComparisonOp::GreaterOrEqual;
    if (str == "contains") return ComparisonOp::Contains;
    if (str == "startsWith") return ComparisonOp::StartsWith;
    if (str == "endsWith") return ComparisonOp::EndsWith;
    if (str == "matches") return ComparisonOp::Matches;
    if (str == "inRange") return ComparisonOp::InRange;
    if (str == "isNull") return ComparisonOp::IsNull;
    if (str == "isNotNull") return ComparisonOp::IsNotNull;
    if (str == "changed") return ComparisonOp::Changed;
    if (str == "bitSet") return ComparisonOp::BitSet;
    if (str == "bitClear") return ComparisonOp::BitClear;
    return ComparisonOp::Equal;
}

std::string EventCreator::LogicalOpToString(LogicalOp op) const {
    switch (op) {
        case LogicalOp::And: return "AND";
        case LogicalOp::Or: return "OR";
        case LogicalOp::Not: return "NOT";
        case LogicalOp::Xor: return "XOR";
        case LogicalOp::Nand: return "NAND";
        case LogicalOp::Nor: return "NOR";
        default: return "AND";
    }
}

LogicalOp EventCreator::StringToLogicalOp(const std::string& str) const {
    if (str == "AND") return LogicalOp::And;
    if (str == "OR") return LogicalOp::Or;
    if (str == "NOT") return LogicalOp::Not;
    if (str == "XOR") return LogicalOp::Xor;
    if (str == "NAND") return LogicalOp::Nand;
    if (str == "NOR") return LogicalOp::Nor;
    return LogicalOp::And;
}

std::string EventCreator::ValueToString(const ConditionValue& value) const {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else {
            return "[array]";
        }
    }, value);
}

} // namespace Vehement
