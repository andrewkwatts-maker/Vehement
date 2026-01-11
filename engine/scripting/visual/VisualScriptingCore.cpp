#include "VisualScriptingCore.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <regex>

namespace Nova {
namespace VisualScript {

// =============================================================================
// Port Implementation
// =============================================================================

Port::Port(const std::string& name, PortDirection direction, PortType type,
           const std::string& dataType)
    : m_name(name)
    , m_displayName(name)
    , m_direction(direction)
    , m_type(type)
    , m_dataType(dataType) {
}

bool Port::CanConnectTo(const Port& other) const {
    // Can't connect to self
    if (this == &other) return false;

    // Must be opposite directions
    if (m_direction == other.m_direction) return false;

    // Must match port types
    if (m_type != other.m_type) return false;

    // For data ports, check type compatibility
    if (m_type == PortType::Data) {
        if (m_dataType != "any" && other.m_dataType != "any") {
            // Basic type compatibility check
            if (m_dataType != other.m_dataType) {
                // Allow numeric conversions
                static const std::unordered_set<std::string> numericTypes = {
                    "int", "float", "double", "int32", "int64", "uint32", "uint64"
                };
                if (numericTypes.count(m_dataType) && numericTypes.count(other.m_dataType)) {
                    return true;
                }
                return false;
            }
        }
    }

    return true;
}

void Port::AddConnection(ConnectionPtr conn) {
    m_connections.push_back(conn);
}

void Port::RemoveConnection(ConnectionPtr conn) {
    m_connections.erase(
        std::remove(m_connections.begin(), m_connections.end(), conn),
        m_connections.end()
    );
}

// =============================================================================
// Node Implementation
// =============================================================================

size_t Node::s_nextId = 0;

Node::Node(const std::string& typeId, const std::string& displayName)
    : m_id("node_" + std::to_string(s_nextId++))
    , m_typeId(typeId)
    , m_displayName(displayName) {
}

void Node::AddInputPort(PortPtr port) {
    port->SetOwner(this);
    m_inputPorts.push_back(port);
}

void Node::AddOutputPort(PortPtr port) {
    port->SetOwner(this);
    m_outputPorts.push_back(port);
}

PortPtr Node::GetInputPort(const std::string& name) const {
    for (const auto& port : m_inputPorts) {
        if (port->GetName() == name) return port;
    }
    return nullptr;
}

PortPtr Node::GetOutputPort(const std::string& name) const {
    for (const auto& port : m_outputPorts) {
        if (port->GetName() == name) return port;
    }
    return nullptr;
}

bool Node::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    // Check required input connections
    for (const auto& port : m_inputPorts) {
        if (port->GetType() == PortType::Flow && !port->IsConnected()) {
            // Flow inputs can be entry points
            continue;
        }
        // Check for broken bindings
        if (port->GetType() == PortType::Binding) {
            const auto& binding = port->GetBindingRef();
            if (binding.state == BindingState::BrokenBinding) {
                errors.push_back("Node '" + m_displayName + "': Broken binding on port '" +
                                port->GetName() + "' - " + binding.warningMessage);
                valid = false;
            }
        }
    }

    return valid;
}

nlohmann::json Node::Serialize() const {
    nlohmann::json json;
    json["id"] = m_id;
    json["typeId"] = m_typeId;
    json["displayName"] = m_displayName;
    json["position"] = {m_position.x, m_position.y};

    // Serialize port default values and bindings
    nlohmann::json inputs = nlohmann::json::array();
    for (const auto& port : m_inputPorts) {
        nlohmann::json portJson;
        portJson["name"] = port->GetName();
        if (port->GetType() == PortType::Binding) {
            portJson["binding"] = port->GetBindingRef().path;
        }
        inputs.push_back(portJson);
    }
    json["inputs"] = inputs;

    return json;
}

void Node::Deserialize(const nlohmann::json& json) {
    if (json.contains("id")) m_id = json["id"].get<std::string>();
    if (json.contains("displayName")) m_displayName = json["displayName"].get<std::string>();
    if (json.contains("position")) {
        auto pos = json["position"];
        m_position = glm::vec2(pos[0].get<float>(), pos[1].get<float>());
    }
}

std::vector<BindingReference> Node::GetAllBindings() const {
    std::vector<BindingReference> bindings;
    for (const auto& port : m_inputPorts) {
        if (port->GetType() == PortType::Binding) {
            bindings.push_back(port->GetBindingRef());
        }
    }
    for (const auto& port : m_outputPorts) {
        if (port->GetType() == PortType::Binding) {
            bindings.push_back(port->GetBindingRef());
        }
    }
    return bindings;
}

void Node::UpdateBindingStates(const BindingRegistry& registry) {
    for (auto& port : m_inputPorts) {
        if (port->GetType() == PortType::Binding) {
            auto ref = port->GetBindingRef();
            if (!ref.path.empty()) {
                ref = registry.ResolveBinding(ref.path);
                port->SetBindingRef(ref);
            }
        }
    }
    for (auto& port : m_outputPorts) {
        if (port->GetType() == PortType::Binding) {
            auto ref = port->GetBindingRef();
            if (!ref.path.empty()) {
                ref = registry.ResolveBinding(ref.path);
                port->SetBindingRef(ref);
            }
        }
    }
}

// =============================================================================
// Connection Implementation
// =============================================================================

Connection::Connection(PortPtr source, PortPtr target)
    : m_source(source)
    , m_target(target) {
}

bool Connection::IsValid() const {
    return m_source && m_target && m_source->CanConnectTo(*m_target);
}

glm::vec4 Connection::GetColor() const {
    if (!m_source) return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

    switch (m_source->GetType()) {
        case PortType::Flow:
            return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        case PortType::Data:
            return glm::vec4(0.3f, 0.7f, 1.0f, 1.0f);  // Blue
        case PortType::Event:
            return glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);  // Orange
        case PortType::Binding: {
            // Color based on binding state
            if (m_source->GetBindingRef().state == BindingState::HardBinding) {
                return glm::vec4(0.2f, 0.9f, 0.3f, 1.0f);  // Green
            } else if (m_source->GetBindingRef().state == BindingState::LooseBinding) {
                return glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);  // Yellow
            } else if (m_source->GetBindingRef().state == BindingState::BrokenBinding) {
                return glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
            }
            return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray for unbound
        }
    }
    return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
}

// =============================================================================
// Graph Implementation
// =============================================================================

Graph::Graph(const std::string& name) : m_name(name) {}

void Graph::AddNode(NodePtr node) {
    m_nodes.push_back(node);
}

void Graph::RemoveNode(NodePtr node) {
    // Remove all connections involving this node
    std::vector<ConnectionPtr> toRemove;
    for (const auto& conn : m_connections) {
        if (conn->GetSource()->GetOwner() == node.get() ||
            conn->GetTarget()->GetOwner() == node.get()) {
            toRemove.push_back(conn);
        }
    }
    for (const auto& conn : toRemove) {
        Disconnect(conn);
    }

    // Remove the node
    m_nodes.erase(std::remove(m_nodes.begin(), m_nodes.end(), node), m_nodes.end());
}

NodePtr Graph::FindNode(const std::string& id) const {
    for (const auto& node : m_nodes) {
        if (node->GetId() == id) return node;
    }
    return nullptr;
}

ConnectionPtr Graph::Connect(PortPtr source, PortPtr target) {
    if (!source->CanConnectTo(*target)) {
        return nullptr;
    }

    auto conn = std::make_shared<Connection>(source, target);
    source->AddConnection(conn);
    target->AddConnection(conn);
    m_connections.push_back(conn);
    return conn;
}

void Graph::Disconnect(ConnectionPtr connection) {
    if (!connection) return;

    connection->GetSource()->RemoveConnection(connection);
    connection->GetTarget()->RemoveConnection(connection);
    m_connections.erase(
        std::remove(m_connections.begin(), m_connections.end(), connection),
        m_connections.end()
    );
}

bool Graph::Validate(std::vector<std::string>& errors) const {
    bool valid = true;
    for (const auto& node : m_nodes) {
        if (!node->Validate(errors)) {
            valid = false;
        }
    }
    return valid;
}

std::vector<BindingReference> Graph::GetAllBindings() const {
    std::vector<BindingReference> bindings;
    for (const auto& node : m_nodes) {
        auto nodeBindings = node->GetAllBindings();
        bindings.insert(bindings.end(), nodeBindings.begin(), nodeBindings.end());
    }
    return bindings;
}

std::vector<BindingReference> Graph::GetBrokenBindings() const {
    auto all = GetAllBindings();
    std::vector<BindingReference> broken;
    std::copy_if(all.begin(), all.end(), std::back_inserter(broken),
        [](const BindingReference& ref) {
            return ref.state == BindingState::BrokenBinding;
        });
    return broken;
}

std::vector<BindingReference> Graph::GetLooseBindings() const {
    auto all = GetAllBindings();
    std::vector<BindingReference> loose;
    std::copy_if(all.begin(), all.end(), std::back_inserter(loose),
        [](const BindingReference& ref) {
            return ref.state == BindingState::LooseBinding;
        });
    return loose;
}

void Graph::UpdateBindingStates(const BindingRegistry& registry) {
    for (auto& node : m_nodes) {
        node->UpdateBindingStates(registry);
    }
}

nlohmann::json Graph::Serialize() const {
    nlohmann::json json;
    json["name"] = m_name;

    // Serialize nodes
    nlohmann::json nodes = nlohmann::json::array();
    for (const auto& node : m_nodes) {
        nodes.push_back(node->Serialize());
    }
    json["nodes"] = nodes;

    // Serialize connections
    nlohmann::json connections = nlohmann::json::array();
    for (const auto& conn : m_connections) {
        nlohmann::json connJson;
        connJson["sourceNode"] = conn->GetSource()->GetOwner()->GetId();
        connJson["sourcePort"] = conn->GetSource()->GetName();
        connJson["targetNode"] = conn->GetTarget()->GetOwner()->GetId();
        connJson["targetPort"] = conn->GetTarget()->GetName();
        connections.push_back(connJson);
    }
    json["connections"] = connections;

    // Serialize variables
    nlohmann::json vars = nlohmann::json::object();
    // Note: std::any serialization would need type-specific handling
    json["variables"] = vars;

    return json;
}

GraphPtr Graph::Deserialize(const nlohmann::json& json) {
    auto graph = std::make_shared<Graph>(json.value("name", "Untitled"));

    auto& factory = NodeFactory::Instance();

    // Deserialize nodes
    if (json.contains("nodes")) {
        for (const auto& nodeJson : json["nodes"]) {
            std::string typeId = nodeJson.value("typeId", "");
            auto node = factory.Create(typeId);
            if (node) {
                node->Deserialize(nodeJson);
                graph->AddNode(node);
            }
        }
    }

    // Deserialize connections
    if (json.contains("connections")) {
        for (const auto& connJson : json["connections"]) {
            std::string sourceNodeId = connJson.value("sourceNode", "");
            std::string sourcePortName = connJson.value("sourcePort", "");
            std::string targetNodeId = connJson.value("targetNode", "");
            std::string targetPortName = connJson.value("targetPort", "");

            auto sourceNode = graph->FindNode(sourceNodeId);
            auto targetNode = graph->FindNode(targetNodeId);

            if (sourceNode && targetNode) {
                auto sourcePort = sourceNode->GetOutputPort(sourcePortName);
                auto targetPort = targetNode->GetInputPort(targetPortName);
                if (sourcePort && targetPort) {
                    graph->Connect(sourcePort, targetPort);
                }
            }
        }
    }

    return graph;
}

void Graph::SetVariable(const std::string& name, const std::any& value) {
    m_variables[name] = value;
}

std::any Graph::GetVariable(const std::string& name) const {
    auto it = m_variables.find(name);
    return (it != m_variables.end()) ? it->second : std::any{};
}

bool Graph::HasVariable(const std::string& name) const {
    return m_variables.find(name) != m_variables.end();
}

std::vector<std::string> Graph::GetVariableNames() const {
    std::vector<std::string> names;
    names.reserve(m_variables.size());
    for (const auto& [name, _] : m_variables) {
        names.push_back(name);
    }
    return names;
}

// =============================================================================
// BindingRegistry Implementation
// =============================================================================

BindingRegistry& BindingRegistry::Instance() {
    static BindingRegistry instance;
    return instance;
}

void BindingRegistry::RegisterFromReflection(const Reflect::TypeInfo* typeInfo) {
    if (!typeInfo) return;

    std::string typePrefix = typeInfo->name + ".";

    for (const auto& prop : typeInfo->properties) {
        BindableProperty bindable;
        bindable.id = typePrefix + prop.name;
        bindable.name = prop.name;
        bindable.displayName = prop.name;  // Could be enhanced with metadata
        bindable.typeName = prop.typeName;
        bindable.category = typeInfo->name;
        bindable.sourceType = "reflected";
        bindable.sourceId = typeInfo->name;
        bindable.sourcePath = prop.name;
        bindable.readable = prop.getter != nullptr;
        bindable.writable = prop.setter != nullptr;
        bindable.observable = false;  // Would need Observable detection
        bindable.isHardLinked = true;
        bindable.isLooseLinked = false;

        m_properties[bindable.id] = bindable;
        IndexProperty(bindable);
        NotifyRegistered(bindable.id);
    }
}

void BindingRegistry::RegisterFromAsset(const std::string& assetId, const nlohmann::json& assetJson) {
    auto& discovery = AssetDiscovery::Instance();
    auto properties = discovery.ExtractProperties(assetId, assetJson);

    for (auto& prop : properties) {
        // Check if this property already has hard linking from reflection
        auto existing = Find(prop.id);
        if (existing && existing->isHardLinked) {
            // Update existing with asset info but keep hard link status
            existing->isLooseLinked = true;  // Now has both
            existing->sourceId = assetId;
            continue;
        }

        // New loose binding
        prop.isLooseLinked = true;
        prop.isHardLinked = false;
        m_properties[prop.id] = prop;
        IndexProperty(prop);
        NotifyRegistered(prop.id);
    }
}

void BindingRegistry::RegisterCustomProperty(const BindableProperty& property) {
    m_properties[property.id] = property;
    IndexProperty(property);
    NotifyRegistered(property.id);
}

void BindingRegistry::Unregister(const std::string& propertyId) {
    auto it = m_properties.find(propertyId);
    if (it != m_properties.end()) {
        UnindexProperty(propertyId);
        m_properties.erase(it);
        NotifyUnregistered(propertyId);
    }
}

BindableProperty* BindingRegistry::Find(const std::string& id) {
    auto it = m_properties.find(id);
    return (it != m_properties.end()) ? &it->second : nullptr;
}

const BindableProperty* BindingRegistry::Find(const std::string& id) const {
    auto it = m_properties.find(id);
    return (it != m_properties.end()) ? &it->second : nullptr;
}

std::vector<const BindableProperty*> BindingRegistry::Search(const std::string& query) const {
    std::vector<const BindableProperty*> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& [id, prop] : m_properties) {
        std::string lowerId = id;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        std::string lowerName = prop.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::string lowerDisplay = prop.displayName;
        std::transform(lowerDisplay.begin(), lowerDisplay.end(), lowerDisplay.begin(), ::tolower);

        // Match ID, name, display name, or tags
        if (lowerId.find(lowerQuery) != std::string::npos ||
            lowerName.find(lowerQuery) != std::string::npos ||
            lowerDisplay.find(lowerQuery) != std::string::npos) {
            results.push_back(&prop);
            continue;
        }

        // Check tags
        for (const auto& tag : prop.tags) {
            std::string lowerTag = tag;
            std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);
            if (lowerTag.find(lowerQuery) != std::string::npos) {
                results.push_back(&prop);
                break;
            }
        }
    }

    return results;
}

std::vector<const BindableProperty*> BindingRegistry::GetByCategory(const std::string& category) const {
    std::vector<const BindableProperty*> results;
    auto it = m_byCategory.find(category);
    if (it != m_byCategory.end()) {
        for (const auto& id : it->second) {
            auto prop = Find(id);
            if (prop) results.push_back(prop);
        }
    }
    return results;
}

std::vector<const BindableProperty*> BindingRegistry::GetByType(const std::string& typeName) const {
    std::vector<const BindableProperty*> results;
    auto it = m_byType.find(typeName);
    if (it != m_byType.end()) {
        for (const auto& id : it->second) {
            auto prop = Find(id);
            if (prop) results.push_back(prop);
        }
    }
    return results;
}

std::vector<const BindableProperty*> BindingRegistry::GetBySource(const std::string& sourceId) const {
    std::vector<const BindableProperty*> results;
    auto it = m_bySource.find(sourceId);
    if (it != m_bySource.end()) {
        for (const auto& id : it->second) {
            auto prop = Find(id);
            if (prop) results.push_back(prop);
        }
    }
    return results;
}

std::vector<const BindableProperty*> BindingRegistry::GetByTags(const std::vector<std::string>& tags) const {
    std::vector<const BindableProperty*> results;

    for (const auto& [id, prop] : m_properties) {
        bool hasAllTags = true;
        for (const auto& requiredTag : tags) {
            bool found = false;
            for (const auto& propTag : prop.tags) {
                if (propTag == requiredTag) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                hasAllTags = false;
                break;
            }
        }
        if (hasAllTags) {
            results.push_back(&prop);
        }
    }

    return results;
}

std::vector<std::string> BindingRegistry::GetCategories() const {
    std::vector<std::string> categories;
    categories.reserve(m_byCategory.size());
    for (const auto& [cat, _] : m_byCategory) {
        categories.push_back(cat);
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

std::vector<std::string> BindingRegistry::GetAllIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_properties.size());
    for (const auto& [id, _] : m_properties) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

BindingReference BindingRegistry::ResolveBinding(const std::string& path) const {
    BindingReference ref;
    ref.path = path;

    auto prop = Find(path);
    if (prop) {
        ref.displayName = prop->displayName;
        ref.actualType = prop->typeName;
        ref.resolvedInCode = prop->isHardLinked;
        ref.resolvedInAsset = prop->isLooseLinked;
        ref.sourceAssetId = prop->sourceId;

        if (prop->isHardLinked) {
            ref.state = BindingState::HardBinding;
            ref.warning = BindingWarning::None;
        } else if (prop->isLooseLinked) {
            ref.state = BindingState::LooseBinding;
            ref.warning = BindingWarning::Warning;
            ref.warningMessage = "Loose binding - property defined in asset but not in code. "
                                "Add to reflected type for type safety.";
        }
    } else {
        ref.state = BindingState::BrokenBinding;
        ref.warning = BindingWarning::Error;
        ref.warningMessage = "Property '" + path + "' not found in registry.";
    }

    return ref;
}

BindingState BindingRegistry::GetBindingState(const std::string& path) const {
    auto prop = Find(path);
    if (!prop) return BindingState::BrokenBinding;
    if (prop->isHardLinked) return BindingState::HardBinding;
    if (prop->isLooseLinked) return BindingState::LooseBinding;
    return BindingState::Unbound;
}

std::vector<BindingReference> BindingRegistry::ValidateBindings(const std::vector<std::string>& paths) const {
    std::vector<BindingReference> results;
    results.reserve(paths.size());
    for (const auto& path : paths) {
        results.push_back(ResolveBinding(path));
    }
    return results;
}

Reflect::ObserverConnection BindingRegistry::OnPropertyRegistered(PropertyChangedCallback callback) {
    auto connected = std::make_shared<bool>(true);
    m_onRegistered.push_back({std::move(callback), connected});
    return Reflect::ObserverConnection(connected);
}

Reflect::ObserverConnection BindingRegistry::OnPropertyUnregistered(PropertyChangedCallback callback) {
    auto connected = std::make_shared<bool>(true);
    m_onUnregistered.push_back({std::move(callback), connected});
    return Reflect::ObserverConnection(connected);
}

void BindingRegistry::RefreshFromAssets(const std::filesystem::path& assetPath) {
    AssetDiscovery::Instance().ScanDirectory(assetPath, true);
}

void BindingRegistry::RefreshFromReflection() {
    // Would iterate over TypeRegistry and register all reflected types
    // Implementation depends on TypeRegistry implementation
}

void BindingRegistry::NotifyRegistered(const std::string& id) {
    for (auto& [callback, connected] : m_onRegistered) {
        if (*connected && callback) {
            callback(id);
        }
    }
}

void BindingRegistry::NotifyUnregistered(const std::string& id) {
    for (auto& [callback, connected] : m_onUnregistered) {
        if (*connected && callback) {
            callback(id);
        }
    }
}

void BindingRegistry::IndexProperty(const BindableProperty& prop) {
    m_byCategory[prop.category].push_back(prop.id);
    m_byType[prop.typeName].push_back(prop.id);
    m_bySource[prop.sourceId].push_back(prop.id);
}

void BindingRegistry::UnindexProperty(const std::string& id) {
    auto prop = Find(id);
    if (!prop) return;

    auto removeFromIndex = [&id](std::vector<std::string>& vec) {
        vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    };

    if (m_byCategory.count(prop->category)) {
        removeFromIndex(m_byCategory[prop->category]);
    }
    if (m_byType.count(prop->typeName)) {
        removeFromIndex(m_byType[prop->typeName]);
    }
    if (m_bySource.count(prop->sourceId)) {
        removeFromIndex(m_bySource[prop->sourceId]);
    }
}

// =============================================================================
// AssetDiscovery Implementation
// =============================================================================

AssetDiscovery& AssetDiscovery::Instance() {
    static AssetDiscovery instance;
    return instance;
}

void AssetDiscovery::ScanDirectory(const std::filesystem::path& directory, bool recursive) {
    if (!std::filesystem::exists(directory)) return;

    auto scanFile = [this](const std::filesystem::path& path) {
        if (path.extension() == ".json") {
            ScanAsset(path);
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                scanFile(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                scanFile(entry.path());
            }
        }
    }
}

void AssetDiscovery::ScanAsset(const std::filesystem::path& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return;

        nlohmann::json json;
        file >> json;

        // Determine asset ID from file or JSON content
        std::string assetId;
        if (json.contains("id")) {
            assetId = json["id"].get<std::string>();
        } else {
            assetId = filepath.stem().string();
        }

        // Build full path prefix from directory structure
        std::string prefix;
        if (json.contains("race")) {
            prefix = json["race"].get<std::string>() + ".";
        }
        if (json.contains("type")) {
            prefix += json["type"].get<std::string>() + "s.";
        }
        prefix += assetId;

        auto& registry = BindingRegistry::Instance();
        registry.RegisterFromAsset(prefix, json);

        m_discoveredAssets.insert(assetId);

    } catch (const std::exception& e) {
        // Log error but continue scanning
    }
}

std::vector<BindableProperty> AssetDiscovery::ExtractProperties(
    const std::string& assetId,
    const nlohmann::json& json,
    const std::string& prefix) {

    std::vector<BindableProperty> properties;

    std::string fullPrefix = prefix.empty() ? assetId : prefix;

    std::function<void(const nlohmann::json&, const std::string&)> extractRecursive =
        [&](const nlohmann::json& obj, const std::string& path) {

        if (obj.is_object()) {
            for (auto& [key, value] : obj.items()) {
                std::string newPath = path.empty() ? key : path + "." + key;

                BindableProperty prop;
                prop.id = fullPrefix + "." + newPath;
                prop.name = key;
                prop.displayName = key;
                prop.category = assetId;
                prop.sourceType = "asset";
                prop.sourceId = assetId;
                prop.sourcePath = newPath;
                prop.readable = true;
                prop.writable = true;

                // Determine type from JSON value
                if (value.is_number_integer()) {
                    prop.typeName = "int";
                    prop.defaultValue = value.get<int>();
                } else if (value.is_number_float()) {
                    prop.typeName = "float";
                    prop.defaultValue = value.get<float>();
                } else if (value.is_boolean()) {
                    prop.typeName = "bool";
                    prop.defaultValue = value.get<bool>();
                } else if (value.is_string()) {
                    prop.typeName = "string";
                    prop.defaultValue = value.get<std::string>();
                } else if (value.is_array()) {
                    prop.typeName = "array";
                    // Arrays are complex - could recurse into them
                } else if (value.is_object()) {
                    prop.typeName = "object";
                    // Recurse into nested objects
                    extractRecursive(value, newPath);
                    continue;  // Don't add object itself as property
                }

                // Extract tags from key name
                if (key.find("health") != std::string::npos ||
                    key.find("damage") != std::string::npos ||
                    key.find("armor") != std::string::npos) {
                    prop.tags.push_back("combat");
                }
                if (key.find("speed") != std::string::npos ||
                    key.find("range") != std::string::npos) {
                    prop.tags.push_back("movement");
                }
                if (key.find("cost") != std::string::npos ||
                    key.find("gold") != std::string::npos ||
                    key.find("food") != std::string::npos) {
                    prop.tags.push_back("economy");
                }

                properties.push_back(prop);
            }
        }
    };

    extractRecursive(json, "");

    return properties;
}

void AssetDiscovery::StartWatching(const std::filesystem::path& directory) {
    // Would implement file watcher - platform specific
    m_watching = true;
}

void AssetDiscovery::StopWatching() {
    m_watching = false;
}

std::vector<std::string> AssetDiscovery::GetDiscoveredAssets() const {
    return std::vector<std::string>(m_discoveredAssets.begin(), m_discoveredAssets.end());
}

// =============================================================================
// NodeFactory Implementation
// =============================================================================

NodeFactory& NodeFactory::Instance() {
    static NodeFactory instance;
    return instance;
}

void NodeFactory::Register(const std::string& typeId, Creator creator, NodeCategory category,
                          const std::string& displayName, const std::string& description) {
    m_creators[typeId] = creator;
    m_nodeInfo[typeId] = {typeId, displayName, description, category};
}

NodePtr NodeFactory::Create(const std::string& typeId) const {
    auto it = m_creators.find(typeId);
    if (it != m_creators.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> NodeFactory::GetNodeTypes() const {
    std::vector<std::string> types;
    types.reserve(m_creators.size());
    for (const auto& [id, _] : m_creators) {
        types.push_back(id);
    }
    return types;
}

std::vector<std::string> NodeFactory::GetNodeTypesByCategory(NodeCategory category) const {
    std::vector<std::string> types;
    for (const auto& [id, info] : m_nodeInfo) {
        if (info.category == category) {
            types.push_back(id);
        }
    }
    return types;
}

const NodeFactory::NodeInfo* NodeFactory::GetNodeInfo(const std::string& typeId) const {
    auto it = m_nodeInfo.find(typeId);
    return (it != m_nodeInfo.end()) ? &it->second : nullptr;
}

std::vector<NodeFactory::NodeInfo> NodeFactory::SearchNodes(const std::string& query) const {
    std::vector<NodeInfo> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& [id, info] : m_nodeInfo) {
        std::string lowerId = id;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        std::string lowerName = info.displayName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerId.find(lowerQuery) != std::string::npos ||
            lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(info);
        }
    }

    return results;
}

// =============================================================================
// ExecutionContext Implementation
// =============================================================================

ExecutionContext::ExecutionContext(Graph* graph) : m_graph(graph) {}

void ExecutionContext::SetVariable(const std::string& name, const std::any& value) {
    m_variables[name] = value;
    if (m_graph) {
        m_graph->SetVariable(name, value);
    }
}

std::any ExecutionContext::GetVariable(const std::string& name) const {
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        return it->second;
    }
    if (m_graph) {
        return m_graph->GetVariable(name);
    }
    return std::any{};
}

std::any ExecutionContext::ResolveBinding(const BindingReference& ref) const {
    if (!ref.IsValid()) {
        return ref.defaultValue;
    }

    // For runtime binding resolution, we'd use reflection to access the actual property
    if (m_dataContext && m_dataContextType) {
        // Navigate the property path
        auto parts = std::vector<std::string>{};
        std::stringstream ss(ref.path);
        std::string part;
        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }

        // Use reflection to get the value
        void* currentObj = m_dataContext;
        const Reflect::TypeInfo* currentType = m_dataContextType;

        for (size_t i = 0; i < parts.size() && currentObj && currentType; ++i) {
            for (const auto& prop : currentType->properties) {
                if (prop.name == parts[i]) {
                    if (prop.getter) {
                        return prop.getter(currentObj);
                    }
                    break;
                }
            }
        }
    }

    return ref.defaultValue;
}

void ExecutionContext::WriteBinding(const BindingReference& ref, const std::any& value) {
    if (!ref.IsValid() || !ref.resolvedInCode) {
        ReportWarning("Cannot write to binding: " + ref.path);
        return;
    }

    if (m_dataContext && m_dataContextType) {
        // Similar path navigation as ResolveBinding
        auto parts = std::vector<std::string>{};
        std::stringstream ss(ref.path);
        std::string part;
        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }

        void* currentObj = m_dataContext;
        const Reflect::TypeInfo* currentType = m_dataContextType;

        // Navigate to parent, then set on last property
        for (size_t i = 0; i < parts.size() - 1 && currentObj && currentType; ++i) {
            for (const auto& prop : currentType->properties) {
                if (prop.name == parts[i]) {
                    if (prop.getter) {
                        auto result = prop.getter(currentObj);
                        // Would need to extract pointer from std::any
                    }
                    break;
                }
            }
        }

        // Set the final property
        if (currentObj && currentType && !parts.empty()) {
            for (const auto& prop : currentType->properties) {
                if (prop.name == parts.back()) {
//                    if (prop.setter) {
//                        prop.setter(currentObj, value);
//                    }
//                    break;
                }
            }
        }
    }
}

void ExecutionContext::SetDataContext(void* context, const Reflect::TypeInfo* type) {
    m_dataContext = context;
    m_dataContextType = type;
}

void ExecutionContext::ReportError(const std::string& error) {
    m_errors.push_back(error);
}

void ExecutionContext::ReportWarning(const std::string& warning) {
    m_warnings.push_back(warning);
}

// =============================================================================
// Standard Node Implementations
// =============================================================================

GetPropertyNode::GetPropertyNode() : Node("GetProperty", "Get Property") {
    SetCategory(NodeCategory::Binding);
    SetDescription("Gets the value of a bound property");

    AddInputPort(std::make_shared<Port>("binding", PortDirection::Input, PortType::Binding));
    AddOutputPort(std::make_shared<Port>("value", PortDirection::Output, PortType::Data, "any"));
}

void GetPropertyNode::SetPropertyPath(const std::string& path) {
    m_propertyPath = path;
    auto& registry = BindingRegistry::Instance();
    auto ref = registry.ResolveBinding(path);
    GetInputPort("binding")->SetBindingRef(ref);
}

void GetPropertyNode::Execute(ExecutionContext& context) {
    auto& ref = GetInputPort("binding")->GetBindingRef();
    auto value = context.ResolveBinding(ref);
    GetOutputPort("value")->SetValue(value);
}

SetPropertyNode::SetPropertyNode() : Node("SetProperty", "Set Property") {
    SetCategory(NodeCategory::Binding);
    SetDescription("Sets the value of a bound property");

    AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
    AddInputPort(std::make_shared<Port>("binding", PortDirection::Input, PortType::Binding));
    AddInputPort(std::make_shared<Port>("value", PortDirection::Input, PortType::Data, "any"));
    AddOutputPort(std::make_shared<Port>("exec", PortDirection::Output, PortType::Flow));
}

void SetPropertyNode::SetPropertyPath(const std::string& path) {
    m_propertyPath = path;
    auto& registry = BindingRegistry::Instance();
    auto ref = registry.ResolveBinding(path);
    GetInputPort("binding")->SetBindingRef(ref);
}

void SetPropertyNode::Execute(ExecutionContext& context) {
    auto& ref = GetInputPort("binding")->GetBindingRef();
    auto& value = GetInputPort("value")->GetValue();
    context.WriteBinding(ref, value);
}

OnPropertyChangedNode::OnPropertyChangedNode() : Node("OnPropertyChanged", "On Property Changed") {
    SetCategory(NodeCategory::Event);
    SetDescription("Fires when a bound property value changes");

    AddInputPort(std::make_shared<Port>("binding", PortDirection::Input, PortType::Binding));
    AddOutputPort(std::make_shared<Port>("exec", PortDirection::Output, PortType::Flow));
    AddOutputPort(std::make_shared<Port>("oldValue", PortDirection::Output, PortType::Data, "any"));
    AddOutputPort(std::make_shared<Port>("newValue", PortDirection::Output, PortType::Data, "any"));
}

void OnPropertyChangedNode::SetPropertyPath(const std::string& path) {
    m_propertyPath = path;
    auto& registry = BindingRegistry::Instance();
    auto ref = registry.ResolveBinding(path);
    GetInputPort("binding")->SetBindingRef(ref);
}

void OnPropertyChangedNode::Execute(ExecutionContext& context) {
    // This node is event-driven - execution happens via subscription
}

AssetReferenceNode::AssetReferenceNode() : Node("AssetReference", "Asset Reference") {
    SetCategory(NodeCategory::Asset);
    SetDescription("References an asset config by ID");

    AddOutputPort(std::make_shared<Port>("asset", PortDirection::Output, PortType::Binding));
}

void AssetReferenceNode::SetAssetId(const std::string& id) {
    m_assetId = id;
    BindingReference ref;
    ref.path = id;
    ref.displayName = id;
    ref.state = BindingState::LooseBinding;
    ref.resolvedInAsset = true;
    GetOutputPort("asset")->SetBindingRef(ref);
}

void AssetReferenceNode::Execute(ExecutionContext& context) {
    // Asset reference is primarily for binding discovery
}

// =============================================================================
// Event System Implementation
// =============================================================================

EventChannel::EventChannel(const std::string& name) : m_name(name) {}

void EventChannel::Publish(const std::any& data) {
    // Copy subscribers to avoid mutation during iteration
    std::vector<Subscriber> toCall;
    for (const auto& [callback, connected] : m_subscribers) {
        if (*connected && callback) {
            toCall.push_back(callback);
        }
    }

    for (const auto& callback : toCall) {
        callback(data);
    }
}

Reflect::ObserverConnection EventChannel::Subscribe(Subscriber callback) {
    auto connected = std::make_shared<bool>(true);
    m_subscribers.push_back({std::move(callback), connected});
    return Reflect::ObserverConnection(connected);
}

size_t EventChannel::GetSubscriberCount() const {
    size_t count = 0;
    for (const auto& [_, connected] : m_subscribers) {
        if (*connected) ++count;
    }
    return count;
}

VisualScriptEventBus& VisualScriptEventBus::Instance() {
    static VisualScriptEventBus instance;
    return instance;
}

EventChannel* VisualScriptEventBus::GetOrCreateChannel(const std::string& name) {
    auto it = m_channels.find(name);
    if (it == m_channels.end()) {
        m_channels[name] = std::make_unique<EventChannel>(name);
        return m_channels[name].get();
    }
    return it->second.get();
}

EventChannel* VisualScriptEventBus::GetChannel(const std::string& name) {
    auto it = m_channels.find(name);
    return (it != m_channels.end()) ? it->second.get() : nullptr;
}

void VisualScriptEventBus::Publish(const std::string& channelName, const std::any& data) {
    auto* channel = GetChannel(channelName);
    if (channel) {
        channel->Publish(data);
    }
}

Reflect::ObserverConnection VisualScriptEventBus::Subscribe(
    const std::string& channelName,
    EventChannel::Subscriber callback) {
    auto* channel = GetOrCreateChannel(channelName);
    return channel->Subscribe(std::move(callback));
}

std::vector<std::string> VisualScriptEventBus::GetChannelNames() const {
    std::vector<std::string> names;
    names.reserve(m_channels.size());
    for (const auto& [name, _] : m_channels) {
        names.push_back(name);
    }
    return names;
}

PublishEventNode::PublishEventNode() : Node("PublishEvent", "Publish Event") {
    SetCategory(NodeCategory::Event);
    SetDescription("Publishes an event to a channel");

    AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
    AddInputPort(std::make_shared<Port>("data", PortDirection::Input, PortType::Data, "any"));
    AddOutputPort(std::make_shared<Port>("exec", PortDirection::Output, PortType::Flow));
}

void PublishEventNode::Execute(ExecutionContext& context) {
    auto& data = GetInputPort("data")->GetValue();
    VisualScriptEventBus::Instance().Publish(m_channel, data);
}

SubscribeEventNode::SubscribeEventNode() : Node("SubscribeEvent", "Subscribe to Event") {
    SetCategory(NodeCategory::Event);
    SetDescription("Subscribes to events on a channel");

    AddOutputPort(std::make_shared<Port>("exec", PortDirection::Output, PortType::Flow));
    AddOutputPort(std::make_shared<Port>("data", PortDirection::Output, PortType::Data, "any"));
}

void SubscribeEventNode::Execute(ExecutionContext& context) {
    // Subscription is set up when the graph is activated, not during execution
}

// =============================================================================
// Node Registration
// =============================================================================

namespace {
    struct NodeRegistrar {
        NodeRegistrar() {
            auto& factory = NodeFactory::Instance();

            // Binding nodes
            factory.Register("GetProperty", []() { return std::make_shared<GetPropertyNode>(); },
                NodeCategory::Binding, "Get Property", "Gets the value of a bound property");
            factory.Register("SetProperty", []() { return std::make_shared<SetPropertyNode>(); },
                NodeCategory::Binding, "Set Property", "Sets the value of a bound property");
            factory.Register("OnPropertyChanged", []() { return std::make_shared<OnPropertyChangedNode>(); },
                NodeCategory::Event, "On Property Changed", "Fires when a property changes");

            // Asset nodes
            factory.Register("AssetReference", []() { return std::make_shared<AssetReferenceNode>(); },
                NodeCategory::Asset, "Asset Reference", "References an asset by ID");

            // Event nodes
            factory.Register("PublishEvent", []() { return std::make_shared<PublishEventNode>(); },
                NodeCategory::Event, "Publish Event", "Publishes data to an event channel");
            factory.Register("SubscribeEvent", []() { return std::make_shared<SubscribeEventNode>(); },
                NodeCategory::Event, "Subscribe Event", "Receives events from a channel");
        }
    };

    static NodeRegistrar s_nodeRegistrar;
}

} // namespace VisualScript
} // namespace Nova
