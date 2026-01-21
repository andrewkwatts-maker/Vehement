/**
 * @file PrefabSystem.cpp
 * @brief Implementation of the comprehensive prefab system
 */

#include "PrefabSystem.hpp"
#include "../scene/Scene.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>

// Include nlohmann/json if available in project
// For this implementation, we'll use a simplified JSON approach
// In production, you would use the project's JSON library

namespace Nova {

// =============================================================================
// Static Members Initialization
// =============================================================================

std::atomic<PrefabId> Prefab::s_nextId{1};
std::atomic<uint64_t> PrefabInstance::s_nextInstanceId{1};

// =============================================================================
// Helper Functions (Internal)
// =============================================================================

namespace {

/**
 * @brief Get current timestamp in milliseconds
 */
uint64_t GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief Case-insensitive string comparison
 */
bool CaseInsensitiveCompare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Escape string for JSON
 */
std::string EscapeJsonString(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (c < 0x20) {
                    ss << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

/**
 * @brief Convert PropertyValue to JSON string representation
 */
std::string PropertyValueToJson(const PropertyValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        std::ostringstream ss;

        if constexpr (std::is_same_v<T, bool>) {
            ss << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, int>) {
            ss << arg;
        } else if constexpr (std::is_same_v<T, float>) {
            ss << std::fixed << arg;
        } else if constexpr (std::is_same_v<T, double>) {
            ss << std::fixed << arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            ss << "\"" << EscapeJsonString(arg) << "\"";
        } else if constexpr (std::is_same_v<T, glm::vec2>) {
            ss << "[" << arg.x << "," << arg.y << "]";
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            ss << "[" << arg.x << "," << arg.y << "," << arg.z << "]";
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            ss << "[" << arg.x << "," << arg.y << "," << arg.z << "," << arg.w << "]";
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            ss << "[" << arg.w << "," << arg.x << "," << arg.y << "," << arg.z << "]";
        }
        return ss.str();
    }, value);
}

/**
 * @brief Get type string for PropertyValue
 */
std::string GetPropertyValueType(const PropertyValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, int>) return "int";
        else if constexpr (std::is_same_v<T, float>) return "float";
        else if constexpr (std::is_same_v<T, double>) return "double";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, glm::vec2>) return "vec2";
        else if constexpr (std::is_same_v<T, glm::vec3>) return "vec3";
        else if constexpr (std::is_same_v<T, glm::vec4>) return "vec4";
        else if constexpr (std::is_same_v<T, glm::quat>) return "quat";
        else return "unknown";
    }, value);
}

} // anonymous namespace

// =============================================================================
// Prefab Implementation
// =============================================================================

Prefab::Prefab(const std::string& name)
    : m_id(s_nextId.fetch_add(1))
    , m_name(name)
    , m_lastModified(GetCurrentTimestamp())
{
    m_rootNode = std::make_unique<SceneNode>(name);
}

Prefab::Prefab(const std::string& name, const SceneNode& sourceNode)
    : m_id(s_nextId.fetch_add(1))
    , m_name(name)
    , m_lastModified(GetCurrentTimestamp())
{
    m_rootNode = CloneNode(&sourceNode);
    m_rootNode->SetName(name);
}

Prefab::~Prefab() = default;

void Prefab::SetName(const std::string& name) {
    if (m_name != name) {
        m_name = name;
        MarkModified();
    }
}

void Prefab::SetRootNode(std::unique_ptr<SceneNode> root) {
    m_rootNode = std::move(root);
    MarkModified();
}

std::unique_ptr<SceneNode> Prefab::CloneRootNode() const {
    if (!m_rootNode) {
        return nullptr;
    }
    return CloneNode(m_rootNode.get());
}

std::vector<PropertyPath> Prefab::GetAllNodePaths() const {
    std::vector<PropertyPath> paths;
    if (m_rootNode) {
        CollectNodePaths(m_rootNode.get(), "", paths);
    }
    return paths;
}

void Prefab::CollectNodePaths(const SceneNode* node, const std::string& prefix,
                               std::vector<PropertyPath>& paths) const {
    if (!node) return;

    std::string currentPath = prefix.empty() ? node->GetName() : prefix + "/" + node->GetName();
    paths.push_back(currentPath);

    for (const auto& child : node->GetChildren()) {
        CollectNodePaths(child.get(), currentPath, paths);
    }
}

std::unique_ptr<SceneNode> Prefab::CloneNode(const SceneNode* source) const {
    if (!source) return nullptr;

    auto clone = std::make_unique<SceneNode>(source->GetName());

    // Copy transform
    clone->SetPosition(source->GetPosition());
    clone->SetRotation(source->GetRotation());
    clone->SetScale(source->GetScale());

    // Copy visibility
    clone->SetVisible(source->IsVisible());

    // Copy mesh and material references
    if (source->HasMesh()) {
        clone->SetMesh(source->GetMesh());
    }
    if (source->HasMaterial()) {
        clone->SetMaterial(source->GetMaterial());
    }

    // Recursively clone children
    for (const auto& child : source->GetChildren()) {
        clone->AddChild(CloneNode(child.get()));
    }

    return clone;
}

void Prefab::AddTag(const std::string& tag) {
    if (!HasTag(tag)) {
        m_tags.push_back(tag);
        MarkModified();
    }
}

void Prefab::RemoveTag(const std::string& tag) {
    auto it = std::find(m_tags.begin(), m_tags.end(), tag);
    if (it != m_tags.end()) {
        m_tags.erase(it);
        MarkModified();
    }
}

bool Prefab::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void Prefab::MarkModified() {
    m_lastModified = GetCurrentTimestamp();
    m_isDirty = true;
    IncrementVersion();

    if (OnModified) {
        OnModified(this);
    }
}

void Prefab::SetVariantOverride(const PropertyPath& path, const PropertyValue& value) {
    // Find existing override
    for (auto& override : m_variantOverrides) {
        if (override.path == path) {
            override.value = value;
            override.timestamp = GetCurrentTimestamp();
            MarkModified();
            return;
        }
    }

    // Add new override
    PropertyOverride newOverride;
    newOverride.path = path;
    newOverride.value = value;
    newOverride.timestamp = GetCurrentTimestamp();
    m_variantOverrides.push_back(std::move(newOverride));
    MarkModified();
}

void Prefab::RemoveVariantOverride(const PropertyPath& path) {
    auto it = std::remove_if(m_variantOverrides.begin(), m_variantOverrides.end(),
        [&path](const PropertyOverride& o) { return o.path == path; });

    if (it != m_variantOverrides.end()) {
        m_variantOverrides.erase(it, m_variantOverrides.end());
        MarkModified();
    }
}

std::string Prefab::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"formatVersion\": " << PrefabFormatVersion << ",\n";
    ss << "  \"id\": " << m_id << ",\n";
    ss << "  \"name\": \"" << EscapeJsonString(m_name) << "\",\n";
    ss << "  \"version\": " << m_version << ",\n";
    ss << "  \"lastModified\": " << m_lastModified << ",\n";

    // Thumbnail
    if (!m_thumbnailPath.empty()) {
        ss << "  \"thumbnailPath\": \"" << EscapeJsonString(m_thumbnailPath) << "\",\n";
    }

    // Tags
    ss << "  \"tags\": [";
    for (size_t i = 0; i < m_tags.size(); ++i) {
        ss << "\"" << EscapeJsonString(m_tags[i]) << "\"";
        if (i < m_tags.size() - 1) ss << ", ";
    }
    ss << "],\n";

    // Variant support
    if (m_basePrefabId != InvalidPrefabId) {
        ss << "  \"basePrefabId\": " << m_basePrefabId << ",\n";
        ss << "  \"variantOverrides\": [\n";
        for (size_t i = 0; i < m_variantOverrides.size(); ++i) {
            const auto& override = m_variantOverrides[i];
            ss << "    {\n";
            ss << "      \"path\": \"" << EscapeJsonString(override.path) << "\",\n";
            ss << "      \"type\": \"" << GetPropertyValueType(override.value) << "\",\n";
            ss << "      \"value\": " << PropertyValueToJson(override.value) << "\n";
            ss << "    }";
            if (i < m_variantOverrides.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "  ],\n";
    }

    // Root node serialization (simplified - full implementation would recurse)
    ss << "  \"rootNode\": ";
    if (m_rootNode) {
        SerializeNodeToJson(ss, m_rootNode.get(), 2);
    } else {
        ss << "null";
    }
    ss << "\n";

    ss << "}";
    return ss.str();
}

void Prefab::SerializeNodeToJson(std::ostringstream& ss, const SceneNode* node, int indent) const {
    std::string ind(indent * 2, ' ');

    ss << "{\n";
    ss << ind << "  \"name\": \"" << EscapeJsonString(node->GetName()) << "\",\n";

    // Transform
    const auto& pos = node->GetPosition();
    const auto& rot = node->GetRotation();
    const auto& scale = node->GetScale();

    ss << ind << "  \"position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
    ss << ind << "  \"rotation\": [" << rot.w << ", " << rot.x << ", " << rot.y << ", " << rot.z << "],\n";
    ss << ind << "  \"scale\": [" << scale.x << ", " << scale.y << ", " << scale.z << "],\n";
    ss << ind << "  \"visible\": " << (node->IsVisible() ? "true" : "false") << ",\n";

    // Children
    ss << ind << "  \"children\": [";
    const auto& children = node->GetChildren();
    if (!children.empty()) {
        ss << "\n";
        for (size_t i = 0; i < children.size(); ++i) {
            ss << ind << "    ";
            SerializeNodeToJson(ss, children[i].get(), indent + 2);
            if (i < children.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << ind << "  ";
    }
    ss << "]\n";
    ss << ind << "}";
}

bool Prefab::FromJson(const std::string& json) {
    // Simplified JSON parsing - in production, use nlohmann/json or similar
    // This is a basic implementation for demonstration

    // For now, we'll just validate the string is not empty
    if (json.empty()) {
        return false;
    }

    // FUTURE: Implement full JSON parsing with the project's JSON library
    // This would parse the JSON and populate all fields

    return true;
}

bool Prefab::SaveToFile(const std::string& path) {
    std::string savePath = path.empty() ? m_filePath : path;
    if (savePath.empty()) {
        return false;
    }

    std::ofstream file(savePath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJson();
    file.close();

    m_filePath = savePath;
    m_isDirty = false;

    return true;
}

bool Prefab::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    if (!FromJson(buffer.str())) {
        return false;
    }

    m_filePath = path;
    m_isDirty = false;

    return true;
}

// =============================================================================
// PrefabInstance Implementation
// =============================================================================

PrefabInstance::PrefabInstance(PrefabId prefabId, std::unique_ptr<SceneNode> rootNode)
    : m_instanceId(s_nextInstanceId.fetch_add(1))
    , m_prefabId(prefabId)
    , m_rootNode(std::move(rootNode))
{
}

PrefabInstance::~PrefabInstance() = default;

bool PrefabInstance::IsOverridden(const PropertyPath& path) const {
    return m_overrides.find(path) != m_overrides.end();
}

std::vector<PropertyPath> PrefabInstance::GetOverriddenPaths() const {
    std::vector<PropertyPath> paths;
    paths.reserve(m_overrides.size());
    for (const auto& [path, _] : m_overrides) {
        paths.push_back(path);
    }
    return paths;
}

std::optional<PropertyValue> PrefabInstance::GetOverride(const PropertyPath& path) const {
    auto it = m_overrides.find(path);
    if (it != m_overrides.end()) {
        return it->second.value;
    }
    return std::nullopt;
}

void PrefabInstance::ApplyOverride(const PropertyPath& path, const PropertyValue& value) {
    PropertyOverride override;
    override.path = path;
    override.value = value;
    override.timestamp = GetCurrentTimestamp();

    m_overrides[path] = std::move(override);

    // Apply to the actual node
    ApplyPropertyToNode(m_rootNode.get(), path, value);

    if (OnOverrideChanged) {
        OnOverrideChanged(this, path);
    }
}

bool PrefabInstance::RevertOverride(const PropertyPath& path) {
    auto it = m_overrides.find(path);
    if (it == m_overrides.end()) {
        return false;
    }

    m_overrides.erase(it);

    // Need to sync with prefab to restore original value
    // This would be called by the user after reverting

    if (OnOverrideChanged) {
        OnOverrideChanged(this, path);
    }

    return true;
}

void PrefabInstance::RevertAllOverrides() {
    m_overrides.clear();

    if (OnOverrideChanged) {
        OnOverrideChanged(this, "");
    }
}

void PrefabInstance::AddNestedInstance(std::unique_ptr<PrefabInstance> instance) {
    m_nestedInstances.push_back(std::move(instance));
}

std::unique_ptr<PrefabInstance> PrefabInstance::RemoveNestedInstance(uint64_t instanceId) {
    for (auto it = m_nestedInstances.begin(); it != m_nestedInstances.end(); ++it) {
        if ((*it)->GetInstanceId() == instanceId) {
            auto result = std::move(*it);
            m_nestedInstances.erase(it);
            return result;
        }
    }
    return nullptr;
}

PrefabInstance* PrefabInstance::FindNestedInstance(uint64_t instanceId) {
    for (auto& nested : m_nestedInstances) {
        if (nested->GetInstanceId() == instanceId) {
            return nested.get();
        }
        // Recursive search
        if (auto* found = nested->FindNestedInstance(instanceId)) {
            return found;
        }
    }
    return nullptr;
}

bool PrefabInstance::SyncWithPrefab(const Prefab& prefab) {
    if (prefab.GetId() != m_prefabId) {
        return false;
    }

    if (!NeedsSync(prefab)) {
        return false;
    }

    // Get the template from prefab
    const SceneNode* templateRoot = prefab.GetRootNode();
    if (!templateRoot || !m_rootNode) {
        return false;
    }

    // Sync non-overridden properties
    SyncNodeWithTemplate(m_rootNode.get(), templateRoot, "");

    m_sourceVersion = prefab.GetVersion();

    return true;
}

void PrefabInstance::SyncNodeWithTemplate(SceneNode* instance, const SceneNode* templateNode,
                                           const std::string& pathPrefix) {
    if (!instance || !templateNode) return;

    std::string currentPath = pathPrefix.empty() ? instance->GetName()
                                                  : pathPrefix + "/" + instance->GetName();

    // Sync position if not overridden
    if (!IsOverridden(currentPath + "/position")) {
        instance->SetPosition(templateNode->GetPosition());
    }

    // Sync rotation if not overridden
    if (!IsOverridden(currentPath + "/rotation")) {
        instance->SetRotation(templateNode->GetRotation());
    }

    // Sync scale if not overridden
    if (!IsOverridden(currentPath + "/scale")) {
        instance->SetScale(templateNode->GetScale());
    }

    // Sync visibility if not overridden
    if (!IsOverridden(currentPath + "/visible")) {
        instance->SetVisible(templateNode->IsVisible());
    }

    // Sync children
    const auto& templateChildren = templateNode->GetChildren();
    const auto& instanceChildren = instance->GetChildren();

    // Match children by name and sync
    for (size_t i = 0; i < templateChildren.size() && i < instanceChildren.size(); ++i) {
        SyncNodeWithTemplate(const_cast<SceneNode*>(instanceChildren[i].get()),
                            templateChildren[i].get(), currentPath);
    }
}

bool PrefabInstance::NeedsSync(const Prefab& prefab) const {
    return prefab.GetVersion() > m_sourceVersion;
}

void PrefabInstance::ApplyPropertyToNode(SceneNode* node, const PropertyPath& path,
                                          const PropertyValue& value) {
    if (!node) return;

    // Parse the path to find the target node and property
    auto [nodePath, propertyName] = ParsePropertyPath(path);

    SceneNode* targetNode = FindNodeByPath(nodePath);
    if (!targetNode) {
        targetNode = node;
    }

    // Apply the property value
    if (propertyName == "position" && std::holds_alternative<glm::vec3>(value)) {
        targetNode->SetPosition(std::get<glm::vec3>(value));
    } else if (propertyName == "rotation" && std::holds_alternative<glm::quat>(value)) {
        targetNode->SetRotation(std::get<glm::quat>(value));
    } else if (propertyName == "scale" && std::holds_alternative<glm::vec3>(value)) {
        targetNode->SetScale(std::get<glm::vec3>(value));
    } else if (propertyName == "visible" && std::holds_alternative<bool>(value)) {
        targetNode->SetVisible(std::get<bool>(value));
    } else if (propertyName == "name" && std::holds_alternative<std::string>(value)) {
        targetNode->SetName(std::get<std::string>(value));
    }
}

PropertyValue PrefabInstance::GetPropertyFromNode(const SceneNode* node,
                                                   const PropertyPath& path) const {
    if (!node) return std::string("");

    auto [nodePath, propertyName] = ParsePropertyPath(path);

    const SceneNode* targetNode = FindNodeByPath(nodePath);
    if (!targetNode) {
        targetNode = node;
    }

    if (propertyName == "position") {
        return targetNode->GetPosition();
    } else if (propertyName == "rotation") {
        return targetNode->GetRotation();
    } else if (propertyName == "scale") {
        return targetNode->GetScale();
    } else if (propertyName == "visible") {
        return targetNode->IsVisible();
    } else if (propertyName == "name") {
        return targetNode->GetName();
    }

    return std::string("");
}

SceneNode* PrefabInstance::FindNodeByPath(const PropertyPath& path) const {
    if (path.empty() || !m_rootNode) {
        return m_rootNode.get();
    }

    // Split path by '/'
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    if (parts.empty()) {
        return m_rootNode.get();
    }

    // Navigate to the node
    SceneNode* current = m_rootNode.get();
    for (size_t i = 0; i < parts.size(); ++i) {
        // Skip if this is the root node name
        if (i == 0 && current->GetName() == parts[i]) {
            continue;
        }

        current = current->FindChild(parts[i], false);
        if (!current) {
            return nullptr;
        }
    }

    return current;
}

std::string PrefabInstance::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"instanceId\": " << m_instanceId << ",\n";
    ss << "  \"prefabId\": " << m_prefabId << ",\n";
    ss << "  \"sourceVersion\": " << m_sourceVersion << ",\n";

    // Overrides
    ss << "  \"overrides\": [\n";
    size_t idx = 0;
    for (const auto& [path, override] : m_overrides) {
        ss << "    {\n";
        ss << "      \"path\": \"" << EscapeJsonString(path) << "\",\n";
        ss << "      \"type\": \"" << GetPropertyValueType(override.value) << "\",\n";
        ss << "      \"value\": " << PropertyValueToJson(override.value) << ",\n";
        ss << "      \"timestamp\": " << override.timestamp << "\n";
        ss << "    }";
        if (++idx < m_overrides.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Nested instances
    ss << "  \"nestedInstances\": [";
    for (size_t i = 0; i < m_nestedInstances.size(); ++i) {
        ss << "\n    " << m_nestedInstances[i]->ToJson();
        if (i < m_nestedInstances.size() - 1) ss << ",";
    }
    if (!m_nestedInstances.empty()) ss << "\n  ";
    ss << "]\n";

    ss << "}";
    return ss.str();
}

bool PrefabInstance::FromJson(const std::string& json) {
    // Simplified - would use proper JSON parsing in production
    if (json.empty()) {
        return false;
    }
    return true;
}

// =============================================================================
// PrefabRegistry Implementation (Singleton)
// =============================================================================

PrefabRegistry& PrefabRegistry::Instance() {
    static PrefabRegistry instance;
    return instance;
}

PrefabRegistry::PrefabRegistry() = default;

PrefabRegistry::~PrefabRegistry() {
    Shutdown();
}

bool PrefabRegistry::Initialize(const std::string& prefabDirectory) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    m_prefabDirectory = prefabDirectory;

    // Create directory if it doesn't exist
    if (!fs::exists(m_prefabDirectory)) {
        try {
            fs::create_directories(m_prefabDirectory);
        } catch (const fs::filesystem_error&) {
            return false;
        }
    }

    // Scan for existing prefabs
    ScanPrefabDirectory();

    m_initialized = true;
    return true;
}

void PrefabRegistry::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Save any dirty prefabs
    for (auto& [id, prefab] : m_prefabs) {
        if (prefab->IsDirty() && !prefab->GetFilePath().empty()) {
            prefab->SaveToFile();
        }
    }

    // Exit editing mode if active
    if (m_editingPrefab) {
        DiscardPrefabChanges();
    }

    m_instances.clear();
    m_prefabInstances.clear();
    m_prefabs.clear();
    m_pathToId.clear();
    m_eventCallbacks.clear();

    m_initialized = false;
}

void PrefabRegistry::ScanPrefabDirectory() {
    if (m_prefabDirectory.empty() || !fs::exists(m_prefabDirectory)) {
        return;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_prefabDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".prefab") {
                LoadPrefab(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
}

Prefab* PrefabRegistry::LoadPrefab(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already loaded
    auto pathIt = m_pathToId.find(path);
    if (pathIt != m_pathToId.end()) {
        auto prefabIt = m_prefabs.find(pathIt->second);
        if (prefabIt != m_prefabs.end()) {
            return prefabIt->second.get();
        }
    }

    // Load from file
    auto prefab = std::make_unique<Prefab>();
    if (!prefab->LoadFromFile(path)) {
        return nullptr;
    }

    PrefabId id = prefab->GetId();
    Prefab* result = prefab.get();

    m_prefabs[id] = std::move(prefab);
    m_pathToId[path] = id;
    m_fileTimestamps[path] = GetFileTimestamp(path);

    NotifyEvent(PrefabEventType::Loaded, id, path);

    return result;
}

Prefab* PrefabRegistry::CreatePrefab(const SceneNode& sourceNode, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto prefab = std::make_unique<Prefab>(name, sourceNode);
    PrefabId id = prefab->GetId();
    Prefab* result = prefab.get();

    // Set up modification callback
    prefab->OnModified = [this](Prefab* p) {
        // Auto-sync instances when prefab changes
        if (m_hotReloadEnabled) {
            SyncAllInstances(p->GetId());
        }
    };

    m_prefabs[id] = std::move(prefab);

    NotifyEvent(PrefabEventType::Created, id, name);

    return result;
}

Prefab* PrefabRegistry::CreateEmptyPrefab(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto prefab = std::make_unique<Prefab>(name);
    PrefabId id = prefab->GetId();
    Prefab* result = prefab.get();

    prefab->OnModified = [this](Prefab* p) {
        if (m_hotReloadEnabled) {
            SyncAllInstances(p->GetId());
        }
    };

    m_prefabs[id] = std::move(prefab);

    NotifyEvent(PrefabEventType::Created, id, name);

    return result;
}

bool PrefabRegistry::SavePrefab(Prefab* prefab, const std::string& path) {
    if (!prefab) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string savePath = path.empty() ? prefab->GetFilePath() : path;

    // Generate path if needed
    if (savePath.empty()) {
        savePath = m_prefabDirectory + "/" + prefab->GetName() + ".prefab";
    }

    if (!prefab->SaveToFile(savePath)) {
        return false;
    }

    m_pathToId[savePath] = prefab->GetId();
    m_fileTimestamps[savePath] = GetFileTimestamp(savePath);

    NotifyEvent(PrefabEventType::Saved, prefab->GetId(), savePath);

    return true;
}

bool PrefabRegistry::DeletePrefab(PrefabId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_prefabs.find(id);
    if (it == m_prefabs.end()) {
        return false;
    }

    // Remove from path mapping
    const std::string& path = it->second->GetFilePath();
    if (!path.empty()) {
        m_pathToId.erase(path);
        m_fileTimestamps.erase(path);
    }

    // Destroy all instances of this prefab
    auto instancesIt = m_prefabInstances.find(id);
    if (instancesIt != m_prefabInstances.end()) {
        for (uint64_t instanceId : instancesIt->second) {
            m_instances.erase(instanceId);
        }
        m_prefabInstances.erase(instancesIt);
    }

    std::string name = it->second->GetName();
    m_prefabs.erase(it);

    NotifyEvent(PrefabEventType::Deleted, id, name);

    return true;
}

Prefab* PrefabRegistry::GetPrefab(PrefabId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_prefabs.find(id);
    return it != m_prefabs.end() ? it->second.get() : nullptr;
}

const Prefab* PrefabRegistry::GetPrefab(PrefabId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_prefabs.find(id);
    return it != m_prefabs.end() ? it->second.get() : nullptr;
}

Prefab* PrefabRegistry::GetPrefabByPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pathToId.find(path);
    if (it != m_pathToId.end()) {
        auto prefabIt = m_prefabs.find(it->second);
        if (prefabIt != m_prefabs.end()) {
            return prefabIt->second.get();
        }
    }
    return nullptr;
}

std::vector<Prefab*> PrefabRegistry::GetAllPrefabs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Prefab*> result;
    result.reserve(m_prefabs.size());
    for (auto& [id, prefab] : m_prefabs) {
        result.push_back(prefab.get());
    }
    return result;
}

std::vector<const Prefab*> PrefabRegistry::GetAllPrefabs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const Prefab*> result;
    result.reserve(m_prefabs.size());
    for (const auto& [id, prefab] : m_prefabs) {
        result.push_back(prefab.get());
    }
    return result;
}

Prefab* PrefabRegistry::FindPrefabByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, prefab] : m_prefabs) {
        if (CaseInsensitiveCompare(prefab->GetName(), name)) {
            return prefab.get();
        }
    }
    return nullptr;
}

std::vector<Prefab*> PrefabRegistry::FindPrefabsByTag(const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Prefab*> result;
    for (auto& [id, prefab] : m_prefabs) {
        if (prefab->HasTag(tag)) {
            result.push_back(prefab.get());
        }
    }
    return result;
}

std::vector<Prefab*> PrefabRegistry::FindPrefabs(
    const std::function<bool(const Prefab&)>& predicate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Prefab*> result;
    for (auto& [id, prefab] : m_prefabs) {
        if (predicate(*prefab)) {
            result.push_back(prefab.get());
        }
    }
    return result;
}

size_t PrefabRegistry::GetPrefabCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prefabs.size();
}

PrefabInstance* PrefabRegistry::InstantiatePrefab(Prefab* prefab, SceneNode* parent,
                                                   const glm::vec3& position) {
    if (!prefab) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Clone the prefab's root node
    auto rootNode = prefab->CloneRootNode();
    if (!rootNode) return nullptr;

    // Set initial position
    rootNode->SetPosition(position);

    // Add to parent if provided
    SceneNode* nodePtr = rootNode.get();

    // Create instance
    auto instance = std::make_unique<PrefabInstance>(prefab->GetId(), std::move(rootNode));
    instance->SetSourceVersion(prefab->GetVersion());

    uint64_t instanceId = instance->GetInstanceId();
    PrefabInstance* result = instance.get();

    // Store instance
    m_instances[instanceId] = std::move(instance);
    m_prefabInstances[prefab->GetId()].insert(instanceId);

    // Add node to parent
    if (parent) {
        // Note: In a real implementation, we'd need to transfer ownership properly
        // This is simplified for demonstration
    }

    NotifyEvent(PrefabEventType::InstanceCreated, prefab->GetId(),
                std::to_string(instanceId));

    return result;
}

PrefabInstance* PrefabRegistry::InstantiatePrefab(PrefabId prefabId, SceneNode* parent,
                                                   const glm::vec3& position) {
    Prefab* prefab = GetPrefab(prefabId);
    return InstantiatePrefab(prefab, parent, position);
}

std::unique_ptr<SceneNode> PrefabRegistry::UnpackPrefab(PrefabInstance* instance) {
    if (!instance) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t instanceId = instance->GetInstanceId();
    PrefabId prefabId = instance->GetPrefabId();

    // Get the root node
    auto rootNode = instance->ReleaseRootNode();

    // Remove from tracking
    m_instances.erase(instanceId);
    m_prefabInstances[prefabId].erase(instanceId);

    NotifyEvent(PrefabEventType::InstanceDestroyed, prefabId,
                std::to_string(instanceId));

    return rootNode;
}

void PrefabRegistry::DestroyInstance(uint64_t instanceId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_instances.find(instanceId);
    if (it == m_instances.end()) return;

    PrefabId prefabId = it->second->GetPrefabId();

    m_instances.erase(it);
    m_prefabInstances[prefabId].erase(instanceId);

    NotifyEvent(PrefabEventType::InstanceDestroyed, prefabId,
                std::to_string(instanceId));
}

std::vector<PrefabInstance*> PrefabRegistry::GetInstancesOf(PrefabId prefabId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PrefabInstance*> result;

    auto it = m_prefabInstances.find(prefabId);
    if (it != m_prefabInstances.end()) {
        result.reserve(it->second.size());
        for (uint64_t instanceId : it->second) {
            auto instanceIt = m_instances.find(instanceId);
            if (instanceIt != m_instances.end()) {
                result.push_back(instanceIt->second.get());
            }
        }
    }

    return result;
}

PrefabInstance* PrefabRegistry::GetInstance(uint64_t instanceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_instances.find(instanceId);
    return it != m_instances.end() ? it->second.get() : nullptr;
}

size_t PrefabRegistry::GetInstanceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_instances.size();
}

bool PrefabRegistry::UpdatePrefabFromInstance(PrefabInstance* instance) {
    if (!instance) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto prefabIt = m_prefabs.find(instance->GetPrefabId());
    if (prefabIt == m_prefabs.end()) return false;

    Prefab* prefab = prefabIt->second.get();

    // Apply all overrides from instance to prefab
    for (const PropertyPath& path : instance->GetOverriddenPaths()) {
        auto value = instance->GetOverride(path);
        if (value) {
            // Apply the override to the prefab's template node
            // This would need proper implementation based on the property type
        }
    }

    prefab->MarkModified();

    // Clear instance overrides since they're now in the prefab
    instance->RevertAllOverrides();
    instance->SetSourceVersion(prefab->GetVersion());

    // Sync all other instances
    SyncAllInstances(prefab->GetId());

    return true;
}

Prefab* PrefabRegistry::CreateVariant(Prefab* basePrefab, const std::string& variantName) {
    if (!basePrefab) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create new prefab with copied content
    auto variant = std::make_unique<Prefab>(variantName, *basePrefab->GetRootNode());
    variant->SetBasePrefabId(basePrefab->GetId());

    PrefabId id = variant->GetId();
    Prefab* result = variant.get();

    variant->OnModified = [this](Prefab* p) {
        if (m_hotReloadEnabled) {
            SyncAllInstances(p->GetId());
        }
    };

    m_prefabs[id] = std::move(variant);

    NotifyEvent(PrefabEventType::Created, id, variantName);

    return result;
}

size_t PrefabRegistry::SyncAllInstances(PrefabId prefabId) {
    // Don't lock here - GetInstancesOf and GetPrefab will lock
    std::vector<PrefabInstance*> instances = GetInstancesOf(prefabId);
    const Prefab* prefab = GetPrefab(prefabId);

    if (!prefab) return 0;

    size_t synced = 0;
    for (PrefabInstance* instance : instances) {
        if (instance->SyncWithPrefab(*prefab)) {
            ++synced;
        }
    }

    return synced;
}

bool PrefabRegistry::OpenPrefabForEditing(Prefab* prefab) {
    if (!prefab || m_editingPrefab) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create backup
    m_editingBackup = std::make_unique<Prefab>(prefab->GetName(), *prefab->GetRootNode());
    m_editingBackup->SetFilePath(prefab->GetFilePath());

    // Create isolated editing scene
    m_editingScene = std::make_unique<Scene>();
    m_editingScene->Initialize();

    // Clone prefab content into scene
    if (prefab->GetRootNode()) {
        auto rootClone = prefab->CloneRootNode();
        // Would add to editing scene's root
    }

    m_editingPrefab = prefab;

    return true;
}

bool PrefabRegistry::SavePrefabChanges() {
    if (!m_editingPrefab) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Save the prefab
    if (!m_editingPrefab->GetFilePath().empty()) {
        m_editingPrefab->SaveToFile();
    }

    // Sync all instances
    SyncAllInstances(m_editingPrefab->GetId());

    // Cleanup
    m_editingBackup.reset();
    m_editingScene.reset();
    m_editingPrefab = nullptr;

    return true;
}

void PrefabRegistry::DiscardPrefabChanges() {
    if (!m_editingPrefab) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Restore from backup
    if (m_editingBackup) {
        m_editingPrefab->SetRootNode(m_editingBackup->CloneRootNode());
    }

    // Cleanup
    m_editingBackup.reset();
    m_editingScene.reset();
    m_editingPrefab = nullptr;
}

void PrefabRegistry::CheckForFileChanges() {
    if (!m_hotReloadEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [path, lastTimestamp] : m_fileTimestamps) {
        uint64_t currentTimestamp = GetFileTimestamp(path);
        if (currentTimestamp > lastTimestamp) {
            // File changed, reload
            auto it = m_pathToId.find(path);
            if (it != m_pathToId.end()) {
                auto prefabIt = m_prefabs.find(it->second);
                if (prefabIt != m_prefabs.end()) {
                    prefabIt->second->LoadFromFile(path);
                    lastTimestamp = currentTimestamp;

                    // Sync instances
                    SyncAllInstances(it->second);

                    NotifyEvent(PrefabEventType::Reloaded, it->second, path);
                }
            }
        }
    }
}

uint32_t PrefabRegistry::RegisterEventCallback(PrefabEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t id = m_nextCallbackId.fetch_add(1);
    m_eventCallbacks[id] = std::move(callback);
    return id;
}

void PrefabRegistry::UnregisterEventCallback(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCallbacks.erase(id);
}

void PrefabRegistry::NotifyEvent(PrefabEventType type, PrefabId prefabId, const std::string& data) {
    // Copy callbacks to avoid issues if callback modifies the map
    std::vector<PrefabEventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks.reserve(m_eventCallbacks.size());
        for (const auto& [id, cb] : m_eventCallbacks) {
            callbacks.push_back(cb);
        }
    }

    for (const auto& callback : callbacks) {
        if (callback) {
            callback(type, prefabId, data);
        }
    }
}

uint64_t PrefabRegistry::GetFileTimestamp(const std::string& path) const {
    try {
        if (fs::exists(path)) {
            auto ftime = fs::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::clock_cast<std::chrono::system_clock>(ftime));
            return sctp.time_since_epoch().count();
        }
    } catch (const fs::filesystem_error&) {
        // Handle error
    }
    return 0;
}

// =============================================================================
// Command Implementations
// =============================================================================

CreatePrefabCommand::CreatePrefabCommand(const SceneNode& sourceNode, const std::string& name)
    : m_prefabName(name)
{
    // Clone the source node for storage
    m_sourceClone = CloneNodeDeep(&sourceNode);
}

bool CreatePrefabCommand::Execute() {
    if (!m_sourceClone) return false;

    m_createdPrefab = PrefabRegistry::Instance().CreatePrefab(*m_sourceClone, m_prefabName);
    if (m_createdPrefab) {
        m_createdId = m_createdPrefab->GetId();
        return true;
    }
    return false;
}

bool CreatePrefabCommand::Undo() {
    if (m_createdId == InvalidPrefabId) return false;

    PrefabRegistry::Instance().DeletePrefab(m_createdId);
    m_createdPrefab = nullptr;
    return true;
}

std::string CreatePrefabCommand::GetName() const {
    return "Create Prefab: " + m_prefabName;
}

CommandTypeId CreatePrefabCommand::GetTypeId() const {
    return GetCommandTypeId<CreatePrefabCommand>();
}

// Helper function for cloning
std::unique_ptr<SceneNode> CreatePrefabCommand::CloneNodeDeep(const SceneNode* source) {
    if (!source) return nullptr;

    auto clone = std::make_unique<SceneNode>(source->GetName());
    clone->SetPosition(source->GetPosition());
    clone->SetRotation(source->GetRotation());
    clone->SetScale(source->GetScale());
    clone->SetVisible(source->IsVisible());

    if (source->HasMesh()) {
        clone->SetMesh(source->GetMesh());
    }
    if (source->HasMaterial()) {
        clone->SetMaterial(source->GetMaterial());
    }

    for (const auto& child : source->GetChildren()) {
        clone->AddChild(CloneNodeDeep(child.get()));
    }

    return clone;
}

// -----------------------------------------------------------------------------

InstantiatePrefabCommand::InstantiatePrefabCommand(Prefab* prefab, SceneNode* parent,
                                                     const glm::vec3& position)
    : m_prefab(prefab)
    , m_parent(parent)
    , m_position(position)
{
}

bool InstantiatePrefabCommand::Execute() {
    m_createdInstance = PrefabRegistry::Instance().InstantiatePrefab(m_prefab, m_parent, m_position);
    if (m_createdInstance) {
        m_instanceId = m_createdInstance->GetInstanceId();
        return true;
    }
    return false;
}

bool InstantiatePrefabCommand::Undo() {
    if (m_instanceId == 0) return false;

    PrefabRegistry::Instance().DestroyInstance(m_instanceId);
    m_createdInstance = nullptr;
    return true;
}

std::string InstantiatePrefabCommand::GetName() const {
    return "Instantiate Prefab: " + (m_prefab ? m_prefab->GetName() : "Unknown");
}

CommandTypeId InstantiatePrefabCommand::GetTypeId() const {
    return GetCommandTypeId<InstantiatePrefabCommand>();
}

// -----------------------------------------------------------------------------

UnpackPrefabCommand::UnpackPrefabCommand(PrefabInstance* instance)
    : m_instanceId(instance ? instance->GetInstanceId() : 0)
    , m_prefabId(instance ? instance->GetPrefabId() : InvalidPrefabId)
{
    if (instance) {
        m_overridesJson = instance->ToJson();
    }
}

bool UnpackPrefabCommand::Execute() {
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (!instance) return false;

    auto unpackedNode = PrefabRegistry::Instance().UnpackPrefab(instance);
    m_unpackedNode = unpackedNode.get();
    // Note: In real implementation, the node would be transferred to the scene

    return m_unpackedNode != nullptr;
}

bool UnpackPrefabCommand::Undo() {
    // Would need to re-create the prefab instance and restore overrides
    // This is complex and would require careful state management
    return false;  // Simplified - unpacking is not easily undoable
}

std::string UnpackPrefabCommand::GetName() const {
    return "Unpack Prefab";
}

CommandTypeId UnpackPrefabCommand::GetTypeId() const {
    return GetCommandTypeId<UnpackPrefabCommand>();
}

// -----------------------------------------------------------------------------

OverridePrefabPropertyCommand::OverridePrefabPropertyCommand(PrefabInstance* instance,
                                                               const PropertyPath& path,
                                                               const PropertyValue& newValue)
    : m_instanceId(instance ? instance->GetInstanceId() : 0)
    , m_path(path)
    , m_newValue(newValue)
{
    if (instance) {
        m_hadPreviousOverride = instance->IsOverridden(path);
        if (m_hadPreviousOverride) {
            auto existing = instance->GetOverride(path);
            if (existing) {
                m_oldValue = *existing;
            }
        }
    }
}

bool OverridePrefabPropertyCommand::Execute() {
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (!instance) return false;

    instance->ApplyOverride(m_path, m_newValue);
    return true;
}

bool OverridePrefabPropertyCommand::Undo() {
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (!instance) return false;

    if (m_hadPreviousOverride) {
        instance->ApplyOverride(m_path, m_oldValue);
    } else {
        instance->RevertOverride(m_path);
    }
    return true;
}

std::string OverridePrefabPropertyCommand::GetName() const {
    return "Override Property: " + m_path;
}

CommandTypeId OverridePrefabPropertyCommand::GetTypeId() const {
    return GetCommandTypeId<OverridePrefabPropertyCommand>();
}

bool OverridePrefabPropertyCommand::CanMergeWith(const ICommand& other) const {
    if (GetTypeId() != other.GetTypeId()) return false;

    const auto* otherCmd = static_cast<const OverridePrefabPropertyCommand*>(&other);
    return m_instanceId == otherCmd->m_instanceId &&
           m_path == otherCmd->m_path &&
           IsWithinMergeWindow();
}

bool OverridePrefabPropertyCommand::MergeWith(const ICommand& other) {
    const auto* otherCmd = static_cast<const OverridePrefabPropertyCommand*>(&other);
    m_newValue = otherCmd->m_newValue;
    return true;
}

// -----------------------------------------------------------------------------

RevertOverrideCommand::RevertOverrideCommand(PrefabInstance* instance, const PropertyPath& path)
    : m_instanceId(instance ? instance->GetInstanceId() : 0)
    , m_path(path)
{
    if (instance) {
        auto existing = instance->GetOverride(path);
        if (existing) {
            m_removedOverride.path = path;
            m_removedOverride.value = *existing;
            m_removedOverride.timestamp = GetCurrentTimestamp();
        }
    }
}

bool RevertOverrideCommand::Execute() {
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (!instance) return false;

    return instance->RevertOverride(m_path);
}

bool RevertOverrideCommand::Undo() {
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (!instance) return false;

    instance->ApplyOverride(m_removedOverride.path, m_removedOverride.value);
    return true;
}

std::string RevertOverrideCommand::GetName() const {
    return "Revert Override: " + m_path;
}

CommandTypeId RevertOverrideCommand::GetTypeId() const {
    return GetCommandTypeId<RevertOverrideCommand>();
}

// -----------------------------------------------------------------------------

ApplyToPrefabCommand::ApplyToPrefabCommand(PrefabInstance* instance)
    : m_instanceId(instance ? instance->GetInstanceId() : 0)
    , m_prefabId(instance ? instance->GetPrefabId() : InvalidPrefabId)
{
    if (instance) {
        // Store current prefab state for undo
        Prefab* prefab = PrefabRegistry::Instance().GetPrefab(m_prefabId);
        if (prefab) {
            m_oldPrefabJson = prefab->ToJson();
        }

        // Store overrides being applied
        for (const auto& path : instance->GetOverriddenPaths()) {
            auto value = instance->GetOverride(path);
            if (value) {
                PropertyOverride override;
                override.path = path;
                override.value = *value;
                override.timestamp = GetCurrentTimestamp();
                m_appliedOverrides.push_back(std::move(override));
            }
        }
    }
}

bool ApplyToPrefabCommand::Execute() {
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (!instance) return false;

    return PrefabRegistry::Instance().UpdatePrefabFromInstance(instance);
}

bool ApplyToPrefabCommand::Undo() {
    Prefab* prefab = PrefabRegistry::Instance().GetPrefab(m_prefabId);
    if (!prefab) return false;

    // Restore prefab from backup JSON
    prefab->FromJson(m_oldPrefabJson);

    // Restore overrides to instance
    PrefabInstance* instance = PrefabRegistry::Instance().GetInstance(m_instanceId);
    if (instance) {
        for (const auto& override : m_appliedOverrides) {
            instance->ApplyOverride(override.path, override.value);
        }
    }

    return true;
}

std::string ApplyToPrefabCommand::GetName() const {
    return "Apply Changes to Prefab";
}

CommandTypeId ApplyToPrefabCommand::GetTypeId() const {
    return GetCommandTypeId<ApplyToPrefabCommand>();
}

// =============================================================================
// Utility Function Implementations
// =============================================================================

Prefab* CreatePrefabFromSelection(const std::vector<SceneNode*>& selectedNodes,
                                   const std::string& name,
                                   CommandHistory* history) {
    if (selectedNodes.empty()) return nullptr;

    // If single node, create prefab directly
    if (selectedNodes.size() == 1) {
        if (history) {
            auto cmd = std::make_unique<CreatePrefabCommand>(*selectedNodes[0], name);
            history->ExecuteCommand(std::move(cmd));
            // Would return the created prefab from the command
        }
        return PrefabRegistry::Instance().CreatePrefab(*selectedNodes[0], name);
    }

    // Multiple nodes - create a parent node to contain them
    auto groupNode = std::make_unique<SceneNode>(name);

    // Calculate center position
    glm::vec3 center(0.0f);
    for (const SceneNode* node : selectedNodes) {
        center += node->GetWorldPosition();
    }
    center /= static_cast<float>(selectedNodes.size());

    groupNode->SetPosition(center);

    // Clone selected nodes as children
    for (const SceneNode* node : selectedNodes) {
        auto clone = std::make_unique<SceneNode>(node->GetName());
        clone->SetPosition(node->GetPosition() - center);
        clone->SetRotation(node->GetRotation());
        clone->SetScale(node->GetScale());
        clone->SetVisible(node->IsVisible());

        if (node->HasMesh()) {
            clone->SetMesh(node->GetMesh());
        }
        if (node->HasMaterial()) {
            clone->SetMaterial(node->GetMaterial());
        }

        groupNode->AddChild(std::move(clone));
    }

    return PrefabRegistry::Instance().CreatePrefab(*groupNode, name);
}

PrefabInstance* FindInstanceForNode(const SceneNode* node) {
    if (!node) return nullptr;

    // Search through all instances
    auto& registry = PrefabRegistry::Instance();

    // Get all prefabs and check their instances
    for (Prefab* prefab : registry.GetAllPrefabs()) {
        for (PrefabInstance* instance : registry.GetInstancesOf(prefab->GetId())) {
            // Check if node is part of this instance's hierarchy
            const SceneNode* root = instance->GetRootNode();
            if (root == node) {
                return instance;
            }

            // Check children
            bool found = false;
            root->ForEach([node, &found](const SceneNode& n) {
                if (&n == node) {
                    found = true;
                }
            });

            if (found) {
                return instance;
            }
        }
    }

    return nullptr;
}

bool IsPartOfPrefab(const SceneNode* node) {
    return FindInstanceForNode(node) != nullptr;
}

SceneNode* GetPrefabRoot(SceneNode* node) {
    PrefabInstance* instance = FindInstanceForNode(node);
    if (instance) {
        return instance->GetRootNode();
    }
    return nullptr;
}

PropertyPath BuildPropertyPath(const SceneNode* root, const SceneNode* target,
                                const std::string& propertyName) {
    if (!root || !target) return propertyName;

    if (root == target) {
        return root->GetName() + "/" + propertyName;
    }

    // Build path from root to target
    std::vector<std::string> pathParts;
    const SceneNode* current = target;

    while (current && current != root) {
        pathParts.push_back(current->GetName());
        current = current->GetParent();
    }

    if (!current) {
        // Target is not a descendant of root
        return target->GetName() + "/" + propertyName;
    }

    pathParts.push_back(root->GetName());

    // Reverse to get root-to-target order
    std::reverse(pathParts.begin(), pathParts.end());

    // Build path string
    std::string path;
    for (size_t i = 0; i < pathParts.size(); ++i) {
        path += pathParts[i];
        if (i < pathParts.size() - 1) {
            path += "/";
        }
    }
    path += "/" + propertyName;

    return path;
}

std::pair<std::string, std::string> ParsePropertyPath(const PropertyPath& path) {
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos) {
        return {"", path};
    }
    return {path.substr(0, lastSlash), path.substr(lastSlash + 1)};
}

std::string PropertyValueToString(const PropertyValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        std::ostringstream ss;

        if constexpr (std::is_same_v<T, bool>) {
            ss << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, int>) {
            ss << arg;
        } else if constexpr (std::is_same_v<T, float>) {
            ss << arg;
        } else if constexpr (std::is_same_v<T, double>) {
            ss << arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            ss << arg;
        } else if constexpr (std::is_same_v<T, glm::vec2>) {
            ss << arg.x << "," << arg.y;
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            ss << arg.x << "," << arg.y << "," << arg.z;
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            ss << arg.x << "," << arg.y << "," << arg.z << "," << arg.w;
        } else if constexpr (std::is_same_v<T, glm::quat>) {
            ss << arg.w << "," << arg.x << "," << arg.y << "," << arg.z;
        }
        return ss.str();
    }, value);
}

PropertyValue StringToPropertyValue(const std::string& str, const std::string& typeHint) {
    if (typeHint == "bool") {
        return str == "true" || str == "1";
    } else if (typeHint == "int") {
        return std::stoi(str);
    } else if (typeHint == "float") {
        return std::stof(str);
    } else if (typeHint == "double") {
        return std::stod(str);
    } else if (typeHint == "vec2") {
        glm::vec2 v;
        std::sscanf(str.c_str(), "%f,%f", &v.x, &v.y);
        return v;
    } else if (typeHint == "vec3") {
        glm::vec3 v;
        std::sscanf(str.c_str(), "%f,%f,%f", &v.x, &v.y, &v.z);
        return v;
    } else if (typeHint == "vec4") {
        glm::vec4 v;
        std::sscanf(str.c_str(), "%f,%f,%f,%f", &v.x, &v.y, &v.z, &v.w);
        return v;
    } else if (typeHint == "quat") {
        glm::quat q;
        std::sscanf(str.c_str(), "%f,%f,%f,%f", &q.w, &q.x, &q.y, &q.z);
        return q;
    }
    return str;  // Default to string
}

} // namespace Nova
