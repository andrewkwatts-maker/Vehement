#include "SchemaEditor.hpp"
#include "WebViewManager.hpp"
#include "JSBridge.hpp"
#include "../Editor.hpp"
#include "../../config/ConfigSchema.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <imgui.h>

namespace Vehement {
namespace WebEditor {

SchemaEditor::SchemaEditor(Editor* editor)
    : m_editor(editor)
{
}

SchemaEditor::~SchemaEditor() {
    Shutdown();
}

bool SchemaEditor::Initialize() {
    // Create web view
    WebViewConfig config;
    config.id = "schema_editor";
    config.title = "Schema Editor";
    config.width = 600;
    config.height = 800;
    config.debug = true;

    m_webView.reset(new WebView(config));

    // Create bridge
    m_bridge.reset(new JSBridge());
    SetupJSBridge();

    // Load HTML
    std::string htmlPath = WebViewManager::Instance().ResolvePath("editor/html/schema_editor.html");
    if (!m_webView->LoadFile(htmlPath)) {
        // Use inline HTML fallback
        m_webView->LoadHtml(R"(
<!DOCTYPE html>
<html>
<head>
    <link rel="stylesheet" href="editor.css">
    <script src="editor_core.js"></script>
    <script src="schema_form.js"></script>
</head>
<body class="schema-editor">
    <div id="toolbar">
        <button onclick="schemaEditor.undo()">Undo</button>
        <button onclick="schemaEditor.redo()">Redo</button>
        <button onclick="schemaEditor.save()">Save</button>
    </div>
    <div id="form-container"></div>
    <div id="errors-panel"></div>
    <script>
        var schemaEditor = new SchemaEditor('form-container', 'errors-panel');
    </script>
</body>
</html>
)");
    }

    return true;
}

void SchemaEditor::Shutdown() {
    m_webView.reset();
    m_bridge.reset();
    m_schemas.clear();
    ClearHistory();
}

void SchemaEditor::Update(float deltaTime) {
    if (m_webView) {
        m_webView->Update(deltaTime);
    }

    if (m_bridge) {
        m_bridge->ProcessPending();
    }
}

void SchemaEditor::Render() {
    if (!m_webView) return;

    WebViewManager::Instance().RenderImGuiInline(m_webView->GetId());
}

void SchemaEditor::SetupJSBridge() {
    if (!m_bridge) return;

    m_bridge->SetScriptExecutor([this](const std::string& script, JSCallback callback) {
        if (m_webView) {
            m_webView->ExecuteJS(script, [callback](const std::string& result) {
                if (callback) {
                    callback(JSResult::Success(JSValue::FromJson(result)));
                }
            });
        }
    });

    m_webView->SetMessageHandler([this](const std::string& type, const std::string& payload) {
        m_bridge->HandleIncomingMessage("{\"type\":\"" + type + "\",\"payload\":" + payload + "}");
    });

    RegisterBridgeFunctions();
}

void SchemaEditor::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    // Get schema definition
    m_bridge->RegisterFunction("schemaEditor.getSchema", [this](const auto& args) {
        if (m_currentTypeId.empty()) {
            return JSResult::Error("No schema loaded");
        }

        auto* schema = GetSchema(m_currentTypeId);
        if (!schema) {
            return JSResult::Error("Schema not found");
        }

        // Convert schema to JSON
        JSValue::Object obj;
        obj["id"] = schema->id;
        obj["name"] = schema->name;
        obj["description"] = schema->description;

        JSValue::Array fields;
        for (const auto& field : schema->fields) {
            JSValue::Object fieldObj;
            fieldObj["name"] = field.name;
            fieldObj["type"] = static_cast<int>(field.type);
            fieldObj["required"] = field.required;
            fieldObj["description"] = field.description;
            fieldObj["defaultValue"] = field.defaultValue;

            // Constraints
            JSValue::Object constraints;
            if (field.constraints.minValue) {
                constraints["minValue"] = *field.constraints.minValue;
            }
            if (field.constraints.maxValue) {
                constraints["maxValue"] = *field.constraints.maxValue;
            }
            if (!field.constraints.enumValues.empty()) {
                JSValue::Array enumVals;
                for (const auto& v : field.constraints.enumValues) {
                    enumVals.push_back(v);
                }
                constraints["enumValues"] = enumVals;
            }
            fieldObj["constraints"] = constraints;

            fields.push_back(fieldObj);
        }
        obj["fields"] = fields;

        return JSResult::Success(obj);
    });

    // Get document data
    m_bridge->RegisterFunction("schemaEditor.getData", [this](const auto& args) {
        return JSResult::Success(JSValue::FromJson(m_documentJson));
    });

    // Set value
    m_bridge->RegisterFunction("schemaEditor.setValue", [this](const auto& args) {
        if (args.size() < 2) {
            return JSResult::Error("Missing path and value");
        }

        std::string path = args[0].GetString();
        std::string value = JSON::Stringify(args[1]);

        if (SetValue(path, value)) {
            return JSResult::Success();
        }
        return JSResult::Error("Failed to set value");
    });

    // Get value
    m_bridge->RegisterFunction("schemaEditor.getValue", [this](const auto& args) {
        if (args.empty()) {
            return JSResult::Error("Missing path");
        }

        std::string value = GetValue(args[0].GetString());
        if (value.empty()) {
            return JSResult::Success();  // null
        }
        return JSResult::Success(JSValue::FromJson(value));
    });

    // Validate
    m_bridge->RegisterFunction("schemaEditor.validate", [this](const auto& args) {
        auto result = Validate();

        JSValue::Object obj;
        obj["valid"] = result.valid;

        JSValue::Array errors;
        for (const auto& err : result.errors) {
            errors.push_back(err);
        }
        obj["errors"] = errors;

        JSValue::Array warnings;
        for (const auto& warn : result.warnings) {
            warnings.push_back(warn);
        }
        obj["warnings"] = warnings;

        return JSResult::Success(obj);
    });

    // Undo
    m_bridge->RegisterFunction("schemaEditor.undo", [this](const auto& args) {
        if (CanUndo()) {
            Undo();
            return JSResult::Success();
        }
        return JSResult::Error("Nothing to undo");
    });

    // Redo
    m_bridge->RegisterFunction("schemaEditor.redo", [this](const auto& args) {
        if (CanRedo()) {
            Redo();
            return JSResult::Success();
        }
        return JSResult::Error("Nothing to redo");
    });

    // Save
    m_bridge->RegisterFunction("schemaEditor.save", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing file path");
        }

        if (SaveToFile(args[0].AsString())) {
            return JSResult::Success();
        }
        return JSResult::Error("Failed to save");
    });

    // Get errors
    m_bridge->RegisterFunction("schemaEditor.getErrors", [this](const auto& args) {
        JSValue::Array errors;
        for (const auto& err : m_errors) {
            JSValue::Object obj;
            obj["path"] = err.path;
            obj["message"] = err.message;
            obj["severity"] = static_cast<int>(err.severity);
            obj["line"] = err.line;
            obj["column"] = err.column;
            errors.push_back(obj);
        }
        return JSResult::Success(errors);
    });
}

void SchemaEditor::RegisterSchema(const std::string& typeId,
                                   const Config::ConfigSchemaDefinition& schema) {
    m_schemas[typeId] = schema;
}

const Config::ConfigSchemaDefinition* SchemaEditor::GetSchema(const std::string& typeId) const {
    auto it = m_schemas.find(typeId);
    return it != m_schemas.end() ? &it->second : nullptr;
}

std::vector<std::string> SchemaEditor::GetRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(m_schemas.size());
    for (const auto& [id, schema] : m_schemas) {
        types.push_back(id);
    }
    return types;
}

bool SchemaEditor::LoadDocument(const std::string& typeId,
                                 const std::string& jsonData,
                                 const std::string& documentId) {
    auto* schema = GetSchema(typeId);
    if (!schema) {
        return false;
    }

    m_currentTypeId = typeId;
    m_documentId = documentId;
    m_documentJson = jsonData;
    m_isDirty = false;

    ClearHistory();
    ValidateDocument();

    // Notify web view
    if (m_bridge) {
        JSValue::Object data;
        data["typeId"] = typeId;
        data["documentId"] = documentId;
        m_bridge->EmitEvent("documentLoaded", data);
    }

    if (OnDocumentLoaded) {
        OnDocumentLoaded();
    }

    return true;
}

bool SchemaEditor::LoadFromFile(const std::string& typeId, const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return LoadDocument(typeId, buffer.str(), filePath);
}

std::string SchemaEditor::GetDocument() const {
    return m_documentJson;
}

bool SchemaEditor::SaveToFile(const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    // Pretty print
    JSValue doc = JSValue::FromJson(m_documentJson);
    file << JSON::Stringify(doc, true);

    m_isDirty = false;

    if (OnDocumentSaved) {
        OnDocumentSaved();
    }

    return true;
}

std::string SchemaEditor::GetValue(const std::string& path) const {
    JSValue doc = JSValue::FromJson(m_documentJson);

    // Parse path and traverse
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    const JSValue* current = &doc;
    for (const auto& p : parts) {
        if (current->IsObject()) {
            if (!current->HasProperty(p)) {
                return "";
            }
            current = &(*current)[p];
        } else if (current->IsArray()) {
            size_t index = std::stoul(p);
            if (index >= current->Size()) {
                return "";
            }
            current = &(*current)[index];
        } else {
            return "";
        }
    }

    return JSON::Stringify(*current);
}

bool SchemaEditor::SetValue(const std::string& path, const std::string& value, bool createPath) {
    // Save old value for undo
    std::string oldValue = GetValue(path);

    JSValue doc = JSValue::FromJson(m_documentJson);
    JSValue newVal = JSValue::FromJson(value);

    // Parse path
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    // Traverse and set
    JSValue* current = &doc;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& p = parts[i];
        if (current->IsObject()) {
            if (!current->HasProperty(p)) {
                if (!createPath) return false;
                (*current)[p] = JSValue::Object{};
            }
            current = &(*current)[p];
        } else if (current->IsArray()) {
            size_t index = std::stoul(p);
            if (index >= current->Size()) return false;
            // Note: Non-const access needed here
            current = &current->AsArray()[index];
        }
    }

    // Set final value
    if (current->IsObject()) {
        (*current)[parts.back()] = newVal;
    } else if (current->IsArray()) {
        size_t index = std::stoul(parts.back());
        if (index >= current->Size()) return false;
        current->AsArray()[index] = newVal;
    } else {
        return false;
    }

    m_documentJson = JSON::Stringify(doc);
    m_isDirty = true;

    // Push to undo stack
    EditOperation op;
    op.path = path;
    op.oldValue = oldValue;
    op.newValue = value;
    op.description = "Set " + path;
    PushEdit(op);

    // Validate if enabled
    if (m_realtimeValidation) {
        ValidateDocument();
    }

    if (OnValueChanged) {
        OnValueChanged(path, value);
    }

    return true;
}

bool SchemaEditor::DeleteValue(const std::string& path) {
    std::string oldValue = GetValue(path);
    if (oldValue.empty()) return false;

    JSValue doc = JSValue::FromJson(m_documentJson);

    // Parse path
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    // Traverse to parent
    JSValue* current = &doc;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& p = parts[i];
        if (current->IsObject() && current->HasProperty(p)) {
            current = &(*current)[p];
        } else {
            return false;
        }
    }

    // Remove from parent
    if (current->IsObject()) {
        auto& obj = current->AsObject();
        obj.erase(parts.back());
    } else if (current->IsArray()) {
        size_t index = std::stoul(parts.back());
        auto& arr = current->AsArray();
        if (index < arr.size()) {
            arr.erase(arr.begin() + index);
        }
    }

    m_documentJson = JSON::Stringify(doc);
    m_isDirty = true;

    EditOperation op;
    op.path = path;
    op.oldValue = oldValue;
    op.newValue = "null";
    op.description = "Delete " + path;
    PushEdit(op);

    if (m_realtimeValidation) {
        ValidateDocument();
    }

    return true;
}

int SchemaEditor::AddArrayItem(const std::string& arrayPath, const std::string& value) {
    JSValue doc = JSValue::FromJson(m_documentJson);
    JSValue newVal = JSValue::FromJson(value);

    // Navigate to array
    std::vector<std::string> parts;
    std::istringstream iss(arrayPath);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    JSValue* current = &doc;
    for (const auto& p : parts) {
        if (current->IsObject() && current->HasProperty(p)) {
            current = &(*current)[p];
        } else {
            return -1;
        }
    }

    if (!current->IsArray()) {
        return -1;
    }

    auto& arr = current->AsArray();
    arr.push_back(newVal);
    int index = static_cast<int>(arr.size()) - 1;

    m_documentJson = JSON::Stringify(doc);
    m_isDirty = true;

    EditOperation op;
    op.path = arrayPath + "." + std::to_string(index);
    op.oldValue = "null";
    op.newValue = value;
    op.description = "Add item to " + arrayPath;
    PushEdit(op);

    if (m_realtimeValidation) {
        ValidateDocument();
    }

    return index;
}

bool SchemaEditor::RemoveArrayItem(const std::string& arrayPath, int index) {
    return DeleteValue(arrayPath + "." + std::to_string(index));
}

bool SchemaEditor::MoveArrayItem(const std::string& arrayPath, int fromIndex, int toIndex) {
    JSValue doc = JSValue::FromJson(m_documentJson);

    // Navigate to array
    std::vector<std::string> parts;
    std::istringstream iss(arrayPath);
    std::string part;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    JSValue* current = &doc;
    for (const auto& p : parts) {
        if (current->IsObject() && current->HasProperty(p)) {
            current = &(*current)[p];
        } else {
            return false;
        }
    }

    if (!current->IsArray()) {
        return false;
    }

    auto& arr = current->AsArray();
    if (fromIndex < 0 || fromIndex >= static_cast<int>(arr.size()) ||
        toIndex < 0 || toIndex >= static_cast<int>(arr.size())) {
        return false;
    }

    // Move element
    JSValue item = arr[fromIndex];
    arr.erase(arr.begin() + fromIndex);
    arr.insert(arr.begin() + toIndex, item);

    m_documentJson = JSON::Stringify(doc);
    m_isDirty = true;

    return true;
}

Config::ValidationResult SchemaEditor::Validate() const {
    Config::ValidationResult result;

    auto* schema = GetSchema(m_currentTypeId);
    if (!schema) {
        result.AddError("", "No schema loaded");
        return result;
    }

    JSValue doc = JSValue::FromJson(m_documentJson);

    // Validate required fields
    for (const auto& field : schema->fields) {
        if (field.required && !doc.HasProperty(field.name)) {
            result.AddError(field.name, "Required field is missing");
        }
    }

    // Validate field types and constraints
    for (const auto& field : schema->fields) {
        if (!doc.HasProperty(field.name)) continue;

        const JSValue& value = doc[field.name];
        std::string fieldPath = field.name;

        // Type validation
        switch (field.type) {
            case Config::SchemaFieldType::String:
                if (!value.IsString()) {
                    result.AddError(fieldPath, "Expected string");
                }
                break;
            case Config::SchemaFieldType::Integer:
            case Config::SchemaFieldType::Float:
                if (!value.IsNumber()) {
                    result.AddError(fieldPath, "Expected number");
                } else {
                    double num = value.AsNumber();
                    if (field.constraints.minValue && num < *field.constraints.minValue) {
                        result.AddError(fieldPath, "Value below minimum");
                    }
                    if (field.constraints.maxValue && num > *field.constraints.maxValue) {
                        result.AddError(fieldPath, "Value above maximum");
                    }
                }
                break;
            case Config::SchemaFieldType::Boolean:
                if (!value.IsBool()) {
                    result.AddError(fieldPath, "Expected boolean");
                }
                break;
            case Config::SchemaFieldType::Array:
                if (!value.IsArray()) {
                    result.AddError(fieldPath, "Expected array");
                }
                break;
            case Config::SchemaFieldType::Object:
                if (!value.IsObject()) {
                    result.AddError(fieldPath, "Expected object");
                }
                break;
            case Config::SchemaFieldType::Enum:
                if (value.IsString()) {
                    const auto& enumValues = field.constraints.enumValues;
                    if (std::find(enumValues.begin(), enumValues.end(), value.AsString()) == enumValues.end()) {
                        result.AddError(fieldPath, "Invalid enum value");
                    }
                }
                break;
            default:
                break;
        }
    }

    return result;
}

Config::ValidationResult SchemaEditor::ValidateValue(const std::string& path,
                                                      const std::string& value) const {
    // TODO: Validate specific value against schema field
    return Config::ValidationResult{};
}

bool SchemaEditor::IsValid() const {
    return m_errors.empty();
}

void SchemaEditor::Undo() {
    if (m_undoStack.empty()) return;

    EditOperation op = m_undoStack.top();
    m_undoStack.pop();

    // Apply reverse
    SetValue(op.path, op.oldValue, true);

    // Remove the new edit that was pushed and move to redo
    if (!m_undoStack.empty()) {
        m_undoStack.pop();
    }
    m_redoStack.push(op);

    if (m_bridge) {
        m_bridge->EmitEvent("documentChanged", {});
    }
}

void SchemaEditor::Redo() {
    if (m_redoStack.empty()) return;

    EditOperation op = m_redoStack.top();
    m_redoStack.pop();

    SetValue(op.path, op.newValue, true);

    if (m_bridge) {
        m_bridge->EmitEvent("documentChanged", {});
    }
}

std::vector<std::string> SchemaEditor::GetUndoHistory() const {
    std::vector<std::string> history;
    std::stack<EditOperation> temp = m_undoStack;
    while (!temp.empty()) {
        history.push_back(temp.top().description);
        temp.pop();
    }
    return history;
}

void SchemaEditor::ClearHistory() {
    while (!m_undoStack.empty()) m_undoStack.pop();
    while (!m_redoStack.empty()) m_redoStack.pop();
}

std::vector<DiffEntry> SchemaEditor::ComputeDiff(const std::string& leftJson,
                                                  const std::string& rightJson) const {
    std::vector<DiffEntry> diffs;

    JSValue left = JSValue::FromJson(leftJson);
    JSValue right = JSValue::FromJson(rightJson);

    std::function<void(const JSValue&, const JSValue&, const std::string&)> compare;
    compare = [&](const JSValue& l, const JSValue& r, const std::string& path) {
        if (l.IsObject() && r.IsObject()) {
            const auto& lObj = l.AsObject();
            const auto& rObj = r.AsObject();

            // Check for removed/modified
            for (const auto& [key, val] : lObj) {
                std::string newPath = path.empty() ? key : path + "." + key;
                auto it = rObj.find(key);
                if (it == rObj.end()) {
                    DiffEntry entry;
                    entry.path = newPath;
                    entry.leftValue = JSON::Stringify(val);
                    entry.type = DiffEntry::Type::Removed;
                    diffs.push_back(entry);
                } else {
                    compare(val, it->second, newPath);
                }
            }

            // Check for added
            for (const auto& [key, val] : rObj) {
                std::string newPath = path.empty() ? key : path + "." + key;
                if (lObj.find(key) == lObj.end()) {
                    DiffEntry entry;
                    entry.path = newPath;
                    entry.rightValue = JSON::Stringify(val);
                    entry.type = DiffEntry::Type::Added;
                    diffs.push_back(entry);
                }
            }
        } else {
            std::string leftStr = JSON::Stringify(l);
            std::string rightStr = JSON::Stringify(r);

            if (leftStr != rightStr) {
                DiffEntry entry;
                entry.path = path;
                entry.leftValue = leftStr;
                entry.rightValue = rightStr;
                entry.type = DiffEntry::Type::Modified;
                diffs.push_back(entry);
            }
        }
    };

    compare(left, right, "");
    return diffs;
}

void SchemaEditor::ShowDiff(const std::string& otherJson) {
    m_showDiff = true;
    m_diffJson = otherJson;
    m_diffEntries = ComputeDiff(m_documentJson, otherJson);

    if (m_bridge) {
        JSValue::Array entries;
        for (const auto& entry : m_diffEntries) {
            JSValue::Object obj;
            obj["path"] = entry.path;
            obj["leftValue"] = entry.leftValue;
            obj["rightValue"] = entry.rightValue;
            obj["type"] = static_cast<int>(entry.type);
            entries.push_back(obj);
        }
        m_bridge->SendMessage("showDiff", JSValue(entries));
    }
}

void SchemaEditor::HideDiff() {
    m_showDiff = false;
    m_diffJson.clear();
    m_diffEntries.clear();

    if (m_bridge) {
        m_bridge->SendMessage("hideDiff", {});
    }
}

void SchemaEditor::FocusField(const std::string& path) {
    m_focusedPath = path;

    if (m_bridge) {
        m_bridge->CallJS("schemaEditor.focusField", {JSValue(path)});
    }
}

void SchemaEditor::ExpandAll() {
    if (m_bridge) {
        m_bridge->CallJS("schemaEditor.expandAll", {});
    }
}

void SchemaEditor::CollapseAll() {
    if (m_bridge) {
        m_bridge->CallJS("schemaEditor.collapseAll", {});
    }
}

void SchemaEditor::PushEdit(const EditOperation& op) {
    // Clear redo stack
    while (!m_redoStack.empty()) m_redoStack.pop();

    // Push new edit
    m_undoStack.push(op);

    // Limit history size
    if (m_undoStack.size() > MAX_UNDO_HISTORY) {
        // Remove oldest (can't easily do this with stack, would need deque)
    }
}

void SchemaEditor::ValidateDocument() {
    m_errors.clear();

    auto result = Validate();

    for (const auto& err : result.errors) {
        ValidationError ve;
        // Parse path from error string "[path] message"
        size_t bracketEnd = err.find(']');
        if (bracketEnd != std::string::npos) {
            ve.path = err.substr(1, bracketEnd - 1);
            ve.message = err.substr(bracketEnd + 2);
        } else {
            ve.message = err;
        }
        ve.severity = ValidationError::Severity::Error;
        m_errors.push_back(ve);
    }

    for (const auto& warn : result.warnings) {
        ValidationError ve;
        size_t bracketEnd = warn.find(']');
        if (bracketEnd != std::string::npos) {
            ve.path = warn.substr(1, bracketEnd - 1);
            ve.message = warn.substr(bracketEnd + 2);
        } else {
            ve.message = warn;
        }
        ve.severity = ValidationError::Severity::Warning;
        m_errors.push_back(ve);
    }

    UpdateErrorHighlighting();

    if (OnValidationChanged) {
        OnValidationChanged(m_errors);
    }
}

void SchemaEditor::UpdateErrorHighlighting() {
    if (!m_bridge) return;

    JSValue::Array errorPaths;
    for (const auto& err : m_errors) {
        if (err.severity == ValidationError::Severity::Error) {
            errorPaths.push_back(err.path);
        }
    }

    m_bridge->SendMessage("highlightErrors", JSValue(errorPaths));
}

std::string SchemaEditor::GenerateFormHtml(const Config::ConfigSchemaDefinition& schema) {
    std::ostringstream html;

    html << "<form class='schema-form' id='schema-form-" << schema.id << "'>\n";
    html << "<h2>" << schema.name << "</h2>\n";
    html << "<p class='description'>" << schema.description << "</p>\n";

    for (const auto& field : schema.fields) {
        html << GenerateFieldHtml(field, field.name);
    }

    html << "</form>\n";

    return html.str();
}

std::string SchemaEditor::GenerateFieldHtml(const Config::SchemaField& field,
                                             const std::string& path) {
    std::ostringstream html;

    html << "<div class='form-field' data-path='" << path << "'>\n";
    html << "  <label for='" << path << "'>" << field.name;
    if (field.required) html << " <span class='required'>*</span>";
    html << "</label>\n";

    if (!field.description.empty()) {
        html << "  <span class='help-text'>" << field.description << "</span>\n";
    }

    switch (field.type) {
        case Config::SchemaFieldType::String:
            html << "  <input type='text' id='" << path << "' name='" << path << "'"
                 << " data-type='string'>\n";
            break;

        case Config::SchemaFieldType::Integer:
            html << "  <input type='number' id='" << path << "' name='" << path << "'"
                 << " data-type='integer' step='1'";
            if (field.constraints.minValue) {
                html << " min='" << *field.constraints.minValue << "'";
            }
            if (field.constraints.maxValue) {
                html << " max='" << *field.constraints.maxValue << "'";
            }
            html << ">\n";
            break;

        case Config::SchemaFieldType::Float:
            html << "  <input type='number' id='" << path << "' name='" << path << "'"
                 << " data-type='float' step='0.01'";
            if (field.constraints.minValue) {
                html << " min='" << *field.constraints.minValue << "'";
            }
            if (field.constraints.maxValue) {
                html << " max='" << *field.constraints.maxValue << "'";
            }
            html << ">\n";
            break;

        case Config::SchemaFieldType::Boolean:
            html << "  <input type='checkbox' id='" << path << "' name='" << path << "'"
                 << " data-type='boolean'>\n";
            break;

        case Config::SchemaFieldType::Enum:
            html << "  <select id='" << path << "' name='" << path << "' data-type='enum'>\n";
            for (const auto& opt : field.constraints.enumValues) {
                html << "    <option value='" << opt << "'>" << opt << "</option>\n";
            }
            html << "  </select>\n";
            break;

        case Config::SchemaFieldType::Vector3:
            html << "  <div class='vector3-input' data-type='vector3'>\n";
            html << "    <input type='number' id='" << path << ".x' placeholder='X' step='0.1'>\n";
            html << "    <input type='number' id='" << path << ".y' placeholder='Y' step='0.1'>\n";
            html << "    <input type='number' id='" << path << ".z' placeholder='Z' step='0.1'>\n";
            html << "  </div>\n";
            break;

        case Config::SchemaFieldType::Color:
            html << "  <input type='color' id='" << path << "' name='" << path << "'"
                 << " data-type='color'>\n";
            break;

        case Config::SchemaFieldType::Array:
            html << "  <div class='array-field' id='" << path << "' data-type='array'>\n";
            html << "    <div class='array-items'></div>\n";
            html << "    <button type='button' class='add-item'>Add Item</button>\n";
            html << "  </div>\n";
            break;

        case Config::SchemaFieldType::Object:
            html << "  <div class='object-field' id='" << path << "' data-type='object'>\n";
            for (const auto& subField : field.inlineFields) {
                html << GenerateFieldHtml(subField, path + "." + subField.name);
            }
            html << "  </div>\n";
            break;

        default:
            html << "  <input type='text' id='" << path << "' name='" << path << "'>\n";
            break;
    }

    html << "  <span class='error-message'></span>\n";
    html << "</div>\n";

    return html.str();
}

} // namespace WebEditor
} // namespace Vehement
