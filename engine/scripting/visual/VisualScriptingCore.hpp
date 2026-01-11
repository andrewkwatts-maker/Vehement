#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <any>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include "../../reflection/TypeInfo.hpp"
#include "../../reflection/TypeRegistry.hpp"
#include "../../reflection/Observable.hpp"

namespace Nova {
namespace VisualScript {

// Forward declarations
class Node;
class Graph;
class Port;
class Connection;
class BindingRegistry;
class AssetDiscovery;

using NodePtr = std::shared_ptr<Node>;
using GraphPtr = std::shared_ptr<Graph>;
using PortPtr = std::shared_ptr<Port>;
using ConnectionPtr = std::shared_ptr<Connection>;

// =============================================================================
// Binding Types - Loose vs Hard linking
// =============================================================================

/**
 * @brief Binding state for property references
 */
enum class BindingState {
    Unbound,        // No binding target
    LooseBinding,   // Target exists in JSON but not in code (yellow warning)
    HardBinding,    // Target exists in both JSON and reflected code (green)
    BrokenBinding,  // Was bound but target no longer exists (red warning)
    PendingBinding  // Binding waiting for async resolution
};

/**
 * @brief Warning levels for binding issues
 */
enum class BindingWarning {
    None,
    Info,           // Informational (e.g., using default value)
    Suggestion,     // Could be improved
    Warning,        // Loose binding - works but not type-safe
    Error           // Broken binding - will fail at runtime
};

/**
 * @brief Represents a binding reference that can be loose or hard
 */
struct BindingReference {
    std::string path;               // Full path (e.g., "humans.units.footman.stats.health")
    std::string displayName;        // Human-readable name
    BindingState state = BindingState::Unbound;
    BindingWarning warning = BindingWarning::None;
    std::string warningMessage;

    // Type information (if resolved)
    std::string expectedType;       // Type expected by the consumer
    std::string actualType;         // Type found in source (if resolved)
    bool typeCompatible = false;

    // Resolution info
    bool resolvedInCode = false;    // Has matching reflected property
    bool resolvedInAsset = false;   // Has matching asset config property
    std::string sourceAssetId;      // Which asset this comes from

    // Default value for loose bindings
    std::any defaultValue;

    bool IsValid() const {
        return state == BindingState::HardBinding || state == BindingState::LooseBinding;
    }

    bool NeedsAttention() const {
        return warning >= BindingWarning::Warning;
    }
};

// =============================================================================
// Port - Input/Output connection points on nodes
// =============================================================================

enum class PortDirection { Input, Output };
enum class PortType {
    Flow,           // Execution flow
    Data,           // Data value
    Event,          // Event trigger
    Binding         // Property binding reference
};

/**
 * @brief Connection point on a visual script node
 */
class Port {
public:
    Port(const std::string& name, PortDirection direction, PortType type,
         const std::string& dataType = "any");

    const std::string& GetName() const { return m_name; }
    const std::string& GetDisplayName() const { return m_displayName; }
    void SetDisplayName(const std::string& name) { m_displayName = name; }

    PortDirection GetDirection() const { return m_direction; }
    PortType GetType() const { return m_type; }
    const std::string& GetDataType() const { return m_dataType; }

    bool CanConnectTo(const Port& other) const;

    // Value for unconnected ports
    void SetDefaultValue(const std::any& value) { m_defaultValue = value; }
    const std::any& GetDefaultValue() const { return m_defaultValue; }

    // Current value (for data ports)
    void SetValue(const std::any& value) { m_value = value; }
    const std::any& GetValue() const { return m_value; }

    // Binding reference (for binding ports)
    void SetBindingRef(const BindingReference& ref) { m_bindingRef = ref; }
    const BindingReference& GetBindingRef() const { return m_bindingRef; }

    // Connections
    const std::vector<ConnectionPtr>& GetConnections() const { return m_connections; }
    void AddConnection(ConnectionPtr conn);
    void RemoveConnection(ConnectionPtr conn);
    bool IsConnected() const { return !m_connections.empty(); }

    Node* GetOwner() const { return m_owner; }
    void SetOwner(Node* owner) { m_owner = owner; }

private:
    std::string m_name;
    std::string m_displayName;
    PortDirection m_direction;
    PortType m_type;
    std::string m_dataType;

    std::any m_defaultValue;
    std::any m_value;
    BindingReference m_bindingRef;

    std::vector<ConnectionPtr> m_connections;
    Node* m_owner = nullptr;
};

// =============================================================================
// Node - Base class for all visual script nodes
// =============================================================================

/**
 * @brief Categories for organizing nodes in the palette
 */
enum class NodeCategory {
    Flow,           // Control flow (if, for, while)
    Math,           // Mathematical operations
    Logic,          // Boolean logic
    Data,           // Data manipulation
    Event,          // Event handling
    Asset,          // Asset references
    Binding,        // Property binding
    Custom,         // User-defined
    Material,       // Material/shader nodes
    Animation,      // Animation nodes
    AI,             // AI/Behavior nodes
    Audio,          // Sound nodes
    Physics         // Physics nodes
};

/**
 * @brief Base class for visual script nodes
 */
class Node : public std::enable_shared_from_this<Node> {
public:
    Node(const std::string& typeId, const std::string& displayName);
    virtual ~Node() = default;

    // Identity
    const std::string& GetId() const { return m_id; }
    const std::string& GetTypeId() const { return m_typeId; }
    const std::string& GetDisplayName() const { return m_displayName; }
    void SetDisplayName(const std::string& name) { m_displayName = name; }

    NodeCategory GetCategory() const { return m_category; }
    void SetCategory(NodeCategory cat) { m_category = cat; }

    const std::string& GetDescription() const { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    // Position in editor
    glm::vec2 GetPosition() const { return m_position; }
    void SetPosition(const glm::vec2& pos) { m_position = pos; }

    // Ports
    void AddInputPort(PortPtr port);
    void AddOutputPort(PortPtr port);
    PortPtr GetInputPort(const std::string& name) const;
    PortPtr GetOutputPort(const std::string& name) const;
    const std::vector<PortPtr>& GetInputPorts() const { return m_inputPorts; }
    const std::vector<PortPtr>& GetOutputPorts() const { return m_outputPorts; }

    // Execution
    virtual void Execute(class ExecutionContext& context) = 0;
    virtual bool Validate(std::vector<std::string>& errors) const;

    // Serialization
    virtual nlohmann::json Serialize() const;
    virtual void Deserialize(const nlohmann::json& json);

    // Binding support
    std::vector<BindingReference> GetAllBindings() const;
    void UpdateBindingStates(const BindingRegistry& registry);

protected:
    std::string m_id;
    std::string m_typeId;
    std::string m_displayName;
    std::string m_description;
    NodeCategory m_category = NodeCategory::Custom;
    glm::vec2 m_position{0.0f};

    std::vector<PortPtr> m_inputPorts;
    std::vector<PortPtr> m_outputPorts;

    static size_t s_nextId;
};

// =============================================================================
// Connection - Links between ports
// =============================================================================

class Connection {
public:
    Connection(PortPtr source, PortPtr target);

    PortPtr GetSource() const { return m_source; }
    PortPtr GetTarget() const { return m_target; }

    bool IsValid() const;

    // Visual properties
    glm::vec4 GetColor() const;

private:
    PortPtr m_source;
    PortPtr m_target;
};

// =============================================================================
// Graph - Container for nodes and connections
// =============================================================================

class Graph {
public:
    Graph(const std::string& name);

    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    // Node management
    void AddNode(NodePtr node);
    void RemoveNode(NodePtr node);
    NodePtr FindNode(const std::string& id) const;
    const std::vector<NodePtr>& GetNodes() const { return m_nodes; }

    // Connection management
    ConnectionPtr Connect(PortPtr source, PortPtr target);
    void Disconnect(ConnectionPtr connection);
    const std::vector<ConnectionPtr>& GetConnections() const { return m_connections; }

    // Validation
    bool Validate(std::vector<std::string>& errors) const;
    std::vector<BindingReference> GetAllBindings() const;
    std::vector<BindingReference> GetBrokenBindings() const;
    std::vector<BindingReference> GetLooseBindings() const;

    // Update binding states from registry
    void UpdateBindingStates(const BindingRegistry& registry);

    // Serialization
    nlohmann::json Serialize() const;
    static GraphPtr Deserialize(const nlohmann::json& json);

    // Variables (local to this graph)
    void SetVariable(const std::string& name, const std::any& value);
    std::any GetVariable(const std::string& name) const;
    bool HasVariable(const std::string& name) const;
    std::vector<std::string> GetVariableNames() const;

private:
    std::string m_name;
    std::vector<NodePtr> m_nodes;
    std::vector<ConnectionPtr> m_connections;
    std::unordered_map<std::string, std::any> m_variables;
};

// =============================================================================
// Bindable Property Descriptor - Describes what can be bound to
// =============================================================================

/**
 * @brief Describes a property that can be bound in visual scripting
 */
struct BindableProperty {
    std::string id;                 // Unique ID (e.g., "humans.units.footman.stats.health")
    std::string name;               // Short name (e.g., "health")
    std::string displayName;        // Display name (e.g., "Health")
    std::string description;
    std::string typeName;           // Type (e.g., "int", "float", "string")
    std::string category;           // Category for grouping
    std::vector<std::string> tags;  // Searchable tags

    // Source info
    std::string sourceType;         // "reflected", "asset", "variable", "custom"
    std::string sourceId;           // Asset ID or type name
    std::string sourcePath;         // Path within source

    // Access
    bool readable = true;
    bool writable = true;
    bool observable = false;        // Supports change notifications

    // Default/constraints
    std::any defaultValue;
    std::optional<float> minValue;
    std::optional<float> maxValue;
    std::vector<std::string> enumValues;  // For enum types

    // Binding state
    bool isHardLinked = false;      // Has code-level reflection
    bool isLooseLinked = false;     // Defined in JSON but not code
};

// =============================================================================
// Binding Registry - Central registry of all bindable properties
// =============================================================================

/**
 * @brief Central registry for discovering and resolving bindings
 *
 * Combines:
 * - Reflected C++ types (hard bindings)
 * - Asset config properties (can be hard or loose)
 * - Custom variables (always loose until promoted)
 */
class BindingRegistry {
public:
    static BindingRegistry& Instance();

    // Registration
    void RegisterFromReflection(const Reflect::TypeInfo* typeInfo);
    void RegisterFromAsset(const std::string& assetId, const nlohmann::json& assetJson);
    void RegisterCustomProperty(const BindableProperty& property);
    void Unregister(const std::string& propertyId);

    // Discovery
    BindableProperty* Find(const std::string& id);
    const BindableProperty* Find(const std::string& id) const;

    std::vector<const BindableProperty*> Search(const std::string& query) const;
    std::vector<const BindableProperty*> GetByCategory(const std::string& category) const;
    std::vector<const BindableProperty*> GetByType(const std::string& typeName) const;
    std::vector<const BindableProperty*> GetBySource(const std::string& sourceId) const;
    std::vector<const BindableProperty*> GetByTags(const std::vector<std::string>& tags) const;

    std::vector<std::string> GetCategories() const;
    std::vector<std::string> GetAllIds() const;

    // Binding resolution
    BindingReference ResolveBinding(const std::string& path) const;
    BindingState GetBindingState(const std::string& path) const;

    // Validation
    std::vector<BindingReference> ValidateBindings(const std::vector<std::string>& paths) const;

    // Change notification
    using PropertyChangedCallback = std::function<void(const std::string& propertyId)>;
    Reflect::ObserverConnection OnPropertyRegistered(PropertyChangedCallback callback);
    Reflect::ObserverConnection OnPropertyUnregistered(PropertyChangedCallback callback);

    // Scan for changes
    void RefreshFromAssets(const std::filesystem::path& assetPath);
    void RefreshFromReflection();

private:
    BindingRegistry() = default;

    std::unordered_map<std::string, BindableProperty> m_properties;
    std::unordered_map<std::string, std::vector<std::string>> m_byCategory;
    std::unordered_map<std::string, std::vector<std::string>> m_byType;
    std::unordered_map<std::string, std::vector<std::string>> m_bySource;

    // Change callbacks
    std::vector<std::pair<PropertyChangedCallback, std::shared_ptr<bool>>> m_onRegistered;
    std::vector<std::pair<PropertyChangedCallback, std::shared_ptr<bool>>> m_onUnregistered;

    void NotifyRegistered(const std::string& id);
    void NotifyUnregistered(const std::string& id);
    void IndexProperty(const BindableProperty& prop);
    void UnindexProperty(const std::string& id);
};

// =============================================================================
// Asset Discovery Service - Scans assets for bindable properties
// =============================================================================

class AssetDiscovery {
public:
    static AssetDiscovery& Instance();

    /**
     * @brief Scan asset directory and register all bindable properties
     */
    void ScanDirectory(const std::filesystem::path& directory, bool recursive = true);

    /**
     * @brief Scan a single asset file
     */
    void ScanAsset(const std::filesystem::path& filepath);

    /**
     * @brief Extract bindable properties from an asset JSON
     */
    std::vector<BindableProperty> ExtractProperties(const std::string& assetId,
                                                    const nlohmann::json& json,
                                                    const std::string& prefix = "");

    /**
     * @brief Watch for asset file changes
     */
    void StartWatching(const std::filesystem::path& directory);
    void StopWatching();

    /**
     * @brief Get all discovered asset IDs
     */
    std::vector<std::string> GetDiscoveredAssets() const;

private:
    AssetDiscovery() = default;

    std::unordered_set<std::string> m_discoveredAssets;
    bool m_watching = false;
};

// =============================================================================
// Node Factory - Creates nodes from type IDs
// =============================================================================

class NodeFactory {
public:
    using Creator = std::function<NodePtr()>;

    static NodeFactory& Instance();

    void Register(const std::string& typeId, Creator creator, NodeCategory category,
                  const std::string& displayName, const std::string& description = "");

    NodePtr Create(const std::string& typeId) const;

    std::vector<std::string> GetNodeTypes() const;
    std::vector<std::string> GetNodeTypesByCategory(NodeCategory category) const;

    struct NodeInfo {
        std::string typeId;
        std::string displayName;
        std::string description;
        NodeCategory category;
    };

    const NodeInfo* GetNodeInfo(const std::string& typeId) const;
    std::vector<NodeInfo> SearchNodes(const std::string& query) const;

private:
    NodeFactory() = default;

    std::unordered_map<std::string, Creator> m_creators;
    std::unordered_map<std::string, NodeInfo> m_nodeInfo;
};

// =============================================================================
// Execution Context - Runtime context for graph execution
// =============================================================================

class ExecutionContext {
public:
    ExecutionContext(Graph* graph);

    Graph* GetGraph() const { return m_graph; }

    // Variable access
    void SetVariable(const std::string& name, const std::any& value);
    std::any GetVariable(const std::string& name) const;

    // Binding resolution at runtime
    std::any ResolveBinding(const BindingReference& ref) const;
    void WriteBinding(const BindingReference& ref, const std::any& value);

    // External data context (e.g., selected unit)
    void SetDataContext(void* context, const Reflect::TypeInfo* type);
    void* GetDataContext() const { return m_dataContext; }
    const Reflect::TypeInfo* GetDataContextType() const { return m_dataContextType; }

    // Error handling
    void ReportError(const std::string& error);
    void ReportWarning(const std::string& warning);
    const std::vector<std::string>& GetErrors() const { return m_errors; }
    const std::vector<std::string>& GetWarnings() const { return m_warnings; }

private:
    Graph* m_graph;
    std::unordered_map<std::string, std::any> m_variables;

    void* m_dataContext = nullptr;
    const Reflect::TypeInfo* m_dataContextType = nullptr;

    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;
};

// =============================================================================
// Standard Node Types
// =============================================================================

/**
 * @brief Node that reads a bound property value
 */
class GetPropertyNode : public Node {
public:
    GetPropertyNode();

    void SetPropertyPath(const std::string& path);
    const std::string& GetPropertyPath() const { return m_propertyPath; }

    void Execute(ExecutionContext& context) override;

private:
    std::string m_propertyPath;
};

/**
 * @brief Node that writes to a bound property
 */
class SetPropertyNode : public Node {
public:
    SetPropertyNode();

    void SetPropertyPath(const std::string& path);
    const std::string& GetPropertyPath() const { return m_propertyPath; }

    void Execute(ExecutionContext& context) override;

private:
    std::string m_propertyPath;
};

/**
 * @brief Node that observes property changes
 */
class OnPropertyChangedNode : public Node {
public:
    OnPropertyChangedNode();

    void SetPropertyPath(const std::string& path);
    void Execute(ExecutionContext& context) override;

private:
    std::string m_propertyPath;
};

/**
 * @brief Node that references an asset config
 */
class AssetReferenceNode : public Node {
public:
    AssetReferenceNode();

    void SetAssetId(const std::string& id);
    const std::string& GetAssetId() const { return m_assetId; }

    void Execute(ExecutionContext& context) override;

private:
    std::string m_assetId;
};

// =============================================================================
// Pub/Sub Event System for Visual Scripting
// =============================================================================

/**
 * @brief Event channel for pub/sub messaging
 */
class EventChannel {
public:
    EventChannel(const std::string& name);

    const std::string& GetName() const { return m_name; }

    void Publish(const std::any& data);

    using Subscriber = std::function<void(const std::any&)>;
    Reflect::ObserverConnection Subscribe(Subscriber callback);

    size_t GetSubscriberCount() const;

private:
    std::string m_name;
    std::vector<std::pair<Subscriber, std::shared_ptr<bool>>> m_subscribers;
};

/**
 * @brief Global event bus for visual scripting pub/sub
 */
class VisualScriptEventBus {
public:
    static VisualScriptEventBus& Instance();

    EventChannel* GetOrCreateChannel(const std::string& name);
    EventChannel* GetChannel(const std::string& name);

    void Publish(const std::string& channelName, const std::any& data);
    Reflect::ObserverConnection Subscribe(const std::string& channelName,
                                         EventChannel::Subscriber callback);

    std::vector<std::string> GetChannelNames() const;

private:
    VisualScriptEventBus() = default;
    std::unordered_map<std::string, std::unique_ptr<EventChannel>> m_channels;
};

/**
 * @brief Node that publishes to an event channel
 */
class PublishEventNode : public Node {
public:
    PublishEventNode();
    void SetChannel(const std::string& channel) { m_channel = channel; }
    void Execute(ExecutionContext& context) override;

private:
    std::string m_channel;
};

/**
 * @brief Node that subscribes to an event channel
 */
class SubscribeEventNode : public Node {
public:
    SubscribeEventNode();
    void SetChannel(const std::string& channel) { m_channel = channel; }
    void Execute(ExecutionContext& context) override;

private:
    std::string m_channel;
};

} // namespace VisualScript
} // namespace Nova
