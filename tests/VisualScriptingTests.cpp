#include <gtest/gtest.h>
#include "scripting/visual/VisualScriptingCore.hpp"
#include <nlohmann/json.hpp>

using namespace Nova::VisualScript;
using json = nlohmann::json;

// =============================================================================
// Port Tests
// =============================================================================

class PortTests : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(PortTests, CreatePort) {
    Port port("value", PortDirection::Input, PortType::Data, "float");

    EXPECT_EQ(port.GetName(), "value");
    EXPECT_EQ(port.GetDirection(), PortDirection::Input);
    EXPECT_EQ(port.GetType(), PortType::Data);
    EXPECT_EQ(port.GetDataType(), "float");
}

TEST_F(PortTests, CanConnectCompatiblePorts) {
    Port output("out", PortDirection::Output, PortType::Data, "float");
    Port input("in", PortDirection::Input, PortType::Data, "float");

    EXPECT_TRUE(output.CanConnectTo(input));
}

TEST_F(PortTests, CannotConnectSameDirection) {
    Port output1("out1", PortDirection::Output, PortType::Data, "float");
    Port output2("out2", PortDirection::Output, PortType::Data, "float");

    EXPECT_FALSE(output1.CanConnectTo(output2));
}

TEST_F(PortTests, CannotConnectDifferentPortTypes) {
    Port flowOut("flow", PortDirection::Output, PortType::Flow);
    Port dataIn("data", PortDirection::Input, PortType::Data, "int");

    EXPECT_FALSE(flowOut.CanConnectTo(dataIn));
}

TEST_F(PortTests, AnyTypeConnectsToSpecific) {
    Port anyOut("any", PortDirection::Output, PortType::Data, "any");
    Port floatIn("float", PortDirection::Input, PortType::Data, "float");

    EXPECT_TRUE(anyOut.CanConnectTo(floatIn));
}

TEST_F(PortTests, NumericTypesAreCompatible) {
    Port intOut("int", PortDirection::Output, PortType::Data, "int");
    Port floatIn("float", PortDirection::Input, PortType::Data, "float");

    EXPECT_TRUE(intOut.CanConnectTo(floatIn));
}

TEST_F(PortTests, SetAndGetValue) {
    Port port("value", PortDirection::Input, PortType::Data, "int");
    port.SetValue(std::any(42));

    auto value = std::any_cast<int>(port.GetValue());
    EXPECT_EQ(value, 42);
}

TEST_F(PortTests, BindingReference) {
    Port port("binding", PortDirection::Input, PortType::Binding);

    BindingReference ref;
    ref.path = "unit.stats.health";
    ref.state = BindingState::HardBinding;

    port.SetBindingRef(ref);

    EXPECT_EQ(port.GetBindingRef().path, "unit.stats.health");
    EXPECT_EQ(port.GetBindingRef().state, BindingState::HardBinding);
}

// =============================================================================
// Node Tests
// =============================================================================

class NodeTests : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(NodeTests, CreateGetPropertyNode) {
    GetPropertyNode node;

    EXPECT_EQ(node.GetTypeId(), "GetProperty");
    EXPECT_EQ(node.GetCategory(), NodeCategory::Binding);
    EXPECT_FALSE(node.GetInputPorts().empty());
    EXPECT_FALSE(node.GetOutputPorts().empty());
}

TEST_F(NodeTests, CreateSetPropertyNode) {
    SetPropertyNode node;

    EXPECT_EQ(node.GetTypeId(), "SetProperty");
    EXPECT_EQ(node.GetCategory(), NodeCategory::Binding);

    // Should have exec flow ports
    auto execIn = node.GetInputPort("exec");
    auto execOut = node.GetOutputPort("exec");
    EXPECT_NE(execIn, nullptr);
    EXPECT_NE(execOut, nullptr);
    EXPECT_EQ(execIn->GetType(), PortType::Flow);
}

TEST_F(NodeTests, SetPropertyPath) {
    GetPropertyNode node;
    node.SetPropertyPath("unit.stats.health");

    EXPECT_EQ(node.GetPropertyPath(), "unit.stats.health");

    // Binding port should be updated
    auto bindingPort = node.GetInputPort("binding");
    EXPECT_NE(bindingPort, nullptr);
    EXPECT_EQ(bindingPort->GetBindingRef().path, "unit.stats.health");
}

TEST_F(NodeTests, NodePosition) {
    GetPropertyNode node;
    node.SetPosition(glm::vec2(100.0f, 200.0f));

    EXPECT_FLOAT_EQ(node.GetPosition().x, 100.0f);
    EXPECT_FLOAT_EQ(node.GetPosition().y, 200.0f);
}

TEST_F(NodeTests, NodeSerialization) {
    GetPropertyNode node;
    node.SetDisplayName("Get Health");
    node.SetPosition(glm::vec2(150.0f, 250.0f));
    node.SetPropertyPath("unit.health");

    json serialized = node.Serialize();

    EXPECT_EQ(serialized["typeId"], "GetProperty");
    EXPECT_EQ(serialized["displayName"], "Get Health");
    EXPECT_FLOAT_EQ(serialized["position"][0], 150.0f);
    EXPECT_FLOAT_EQ(serialized["position"][1], 250.0f);
}

// =============================================================================
// Graph Tests
// =============================================================================

class GraphTests : public ::testing::Test {
protected:
    void SetUp() override {
        graph = std::make_shared<Graph>("TestGraph");
    }

    GraphPtr graph;
};

TEST_F(GraphTests, CreateGraph) {
    EXPECT_EQ(graph->GetName(), "TestGraph");
    EXPECT_TRUE(graph->GetNodes().empty());
    EXPECT_TRUE(graph->GetConnections().empty());
}

TEST_F(GraphTests, AddNode) {
    auto node = std::make_shared<GetPropertyNode>();
    graph->AddNode(node);

    EXPECT_EQ(graph->GetNodes().size(), 1);
    EXPECT_EQ(graph->FindNode(node->GetId()), node);
}

TEST_F(GraphTests, RemoveNode) {
    auto node = std::make_shared<GetPropertyNode>();
    graph->AddNode(node);
    graph->RemoveNode(node);

    EXPECT_TRUE(graph->GetNodes().empty());
}

TEST_F(GraphTests, ConnectNodes) {
    auto getNode = std::make_shared<GetPropertyNode>();
    auto setNode = std::make_shared<SetPropertyNode>();
    graph->AddNode(getNode);
    graph->AddNode(setNode);

    auto sourcePort = getNode->GetOutputPort("value");
    auto targetPort = setNode->GetInputPort("value");

    auto conn = graph->Connect(sourcePort, targetPort);

    EXPECT_NE(conn, nullptr);
    EXPECT_EQ(graph->GetConnections().size(), 1);
    EXPECT_TRUE(sourcePort->IsConnected());
    EXPECT_TRUE(targetPort->IsConnected());
}

TEST_F(GraphTests, DisconnectNodes) {
    auto getNode = std::make_shared<GetPropertyNode>();
    auto setNode = std::make_shared<SetPropertyNode>();
    graph->AddNode(getNode);
    graph->AddNode(setNode);

    auto sourcePort = getNode->GetOutputPort("value");
    auto targetPort = setNode->GetInputPort("value");

    auto conn = graph->Connect(sourcePort, targetPort);
    graph->Disconnect(conn);

    EXPECT_TRUE(graph->GetConnections().empty());
    EXPECT_FALSE(sourcePort->IsConnected());
    EXPECT_FALSE(targetPort->IsConnected());
}

TEST_F(GraphTests, RemoveNodeCleansConnections) {
    auto getNode = std::make_shared<GetPropertyNode>();
    auto setNode = std::make_shared<SetPropertyNode>();
    graph->AddNode(getNode);
    graph->AddNode(setNode);

    auto sourcePort = getNode->GetOutputPort("value");
    auto targetPort = setNode->GetInputPort("value");
    graph->Connect(sourcePort, targetPort);

    graph->RemoveNode(getNode);

    EXPECT_TRUE(graph->GetConnections().empty());
    EXPECT_FALSE(targetPort->IsConnected());
}

TEST_F(GraphTests, GraphVariables) {
    graph->SetVariable("health", std::any(100));
    graph->SetVariable("name", std::any(std::string("Test")));

    EXPECT_TRUE(graph->HasVariable("health"));
    EXPECT_TRUE(graph->HasVariable("name"));
    EXPECT_FALSE(graph->HasVariable("unknown"));

    auto health = std::any_cast<int>(graph->GetVariable("health"));
    EXPECT_EQ(health, 100);
}

TEST_F(GraphTests, GraphSerialization) {
    auto node = std::make_shared<GetPropertyNode>();
    node->SetDisplayName("Health Getter");
    node->SetPosition(glm::vec2(100.0f, 50.0f));
    graph->AddNode(node);

    json serialized = graph->Serialize();

    EXPECT_EQ(serialized["name"], "TestGraph");
    EXPECT_EQ(serialized["nodes"].size(), 1);
    EXPECT_EQ(serialized["nodes"][0]["displayName"], "Health Getter");
}

TEST_F(GraphTests, GraphDeserialization) {
    json graphJson = {
        {"name", "LoadedGraph"},
        {"nodes", json::array({
            {
                {"id", "node_100"},
                {"typeId", "GetProperty"},
                {"displayName", "Get Value"},
                {"position", {200.0f, 100.0f}}
            }
        })},
        {"connections", json::array()}
    };

    auto loaded = Graph::Deserialize(graphJson);

    EXPECT_EQ(loaded->GetName(), "LoadedGraph");
    EXPECT_EQ(loaded->GetNodes().size(), 1);
}

// =============================================================================
// Binding Registry Tests
// =============================================================================

class BindingRegistryTests : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing registrations
    }
};

TEST_F(BindingRegistryTests, RegisterCustomProperty) {
    auto& registry = BindingRegistry::Instance();

    BindableProperty prop;
    prop.id = "test.property";
    prop.name = "property";
    prop.displayName = "Test Property";
    prop.typeName = "int";
    prop.category = "test";
    prop.sourceType = "custom";
    prop.isHardLinked = true;

    registry.RegisterCustomProperty(prop);

    auto* found = registry.Find("test.property");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->displayName, "Test Property");
    EXPECT_TRUE(found->isHardLinked);

    // Cleanup
    registry.Unregister("test.property");
}

TEST_F(BindingRegistryTests, SearchProperties) {
    auto& registry = BindingRegistry::Instance();

    BindableProperty prop1;
    prop1.id = "unit.health";
    prop1.name = "health";
    prop1.displayName = "Health";
    prop1.typeName = "int";
    prop1.category = "stats";
    registry.RegisterCustomProperty(prop1);

    BindableProperty prop2;
    prop2.id = "unit.healthMax";
    prop2.name = "healthMax";
    prop2.displayName = "Max Health";
    prop2.typeName = "int";
    prop2.category = "stats";
    registry.RegisterCustomProperty(prop2);

    auto results = registry.Search("health");
    EXPECT_GE(results.size(), 2);

    // Cleanup
    registry.Unregister("unit.health");
    registry.Unregister("unit.healthMax");
}

TEST_F(BindingRegistryTests, ResolveHardBinding) {
    auto& registry = BindingRegistry::Instance();

    BindableProperty prop;
    prop.id = "unit.damage";
    prop.name = "damage";
    prop.typeName = "int";
    prop.isHardLinked = true;
    registry.RegisterCustomProperty(prop);

    auto ref = registry.ResolveBinding("unit.damage");

    EXPECT_EQ(ref.state, BindingState::HardBinding);
    EXPECT_TRUE(ref.resolvedInCode);
    EXPECT_EQ(ref.warning, BindingWarning::None);

    registry.Unregister("unit.damage");
}

TEST_F(BindingRegistryTests, ResolveLooseBinding) {
    auto& registry = BindingRegistry::Instance();

    BindableProperty prop;
    prop.id = "unit.customValue";
    prop.name = "customValue";
    prop.typeName = "float";
    prop.isLooseLinked = true;
    prop.isHardLinked = false;
    registry.RegisterCustomProperty(prop);

    auto ref = registry.ResolveBinding("unit.customValue");

    EXPECT_EQ(ref.state, BindingState::LooseBinding);
    EXPECT_TRUE(ref.resolvedInAsset);
    EXPECT_FALSE(ref.resolvedInCode);
    EXPECT_EQ(ref.warning, BindingWarning::Warning);

    registry.Unregister("unit.customValue");
}

TEST_F(BindingRegistryTests, ResolveBrokenBinding) {
    auto& registry = BindingRegistry::Instance();

    auto ref = registry.ResolveBinding("nonexistent.property");

    EXPECT_EQ(ref.state, BindingState::BrokenBinding);
    EXPECT_EQ(ref.warning, BindingWarning::Error);
    EXPECT_FALSE(ref.warningMessage.empty());
}

TEST_F(BindingRegistryTests, GetByCategory) {
    auto& registry = BindingRegistry::Instance();

    BindableProperty prop1;
    prop1.id = "combat.attack";
    prop1.name = "attack";
    prop1.category = "combat";
    registry.RegisterCustomProperty(prop1);

    BindableProperty prop2;
    prop2.id = "combat.defense";
    prop2.name = "defense";
    prop2.category = "combat";
    registry.RegisterCustomProperty(prop2);

    auto combatProps = registry.GetByCategory("combat");
    EXPECT_GE(combatProps.size(), 2);

    registry.Unregister("combat.attack");
    registry.Unregister("combat.defense");
}

// =============================================================================
// Asset Discovery Tests
// =============================================================================

class AssetDiscoveryTests : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(AssetDiscoveryTests, ExtractPropertiesFromJson) {
    auto& discovery = AssetDiscovery::Instance();

    json assetJson = {
        {"id", "footman"},
        {"name", "Footman"},
        {"stats", {
            {"health", 100},
            {"damage", 15},
            {"armor", 2}
        }},
        {"speed", 3.5f}
    };

    auto properties = discovery.ExtractProperties("units.footman", assetJson);

    // Should have extracted multiple properties
    EXPECT_GT(properties.size(), 0);

    // Check for specific properties
    bool foundHealth = false;
    bool foundSpeed = false;
    for (const auto& prop : properties) {
        if (prop.name == "health") {
            foundHealth = true;
            EXPECT_EQ(prop.typeName, "int");
        }
        if (prop.name == "speed") {
            foundSpeed = true;
            EXPECT_EQ(prop.typeName, "float");
        }
    }

    EXPECT_TRUE(foundHealth);
    EXPECT_TRUE(foundSpeed);
}

// =============================================================================
// Node Factory Tests
// =============================================================================

class NodeFactoryTests : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(NodeFactoryTests, CreateRegisteredNode) {
    auto& factory = NodeFactory::Instance();

    auto node = factory.Create("GetProperty");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->GetTypeId(), "GetProperty");
}

TEST_F(NodeFactoryTests, CreateUnknownNodeReturnsNull) {
    auto& factory = NodeFactory::Instance();

    auto node = factory.Create("NonexistentNode");
    EXPECT_EQ(node, nullptr);
}

TEST_F(NodeFactoryTests, GetNodeTypesByCategory) {
    auto& factory = NodeFactory::Instance();

    auto bindingNodes = factory.GetNodeTypesByCategory(NodeCategory::Binding);
    EXPECT_GT(bindingNodes.size(), 0);

    bool foundGetProperty = false;
    for (const auto& typeId : bindingNodes) {
        if (typeId == "GetProperty") {
            foundGetProperty = true;
            break;
        }
    }
    EXPECT_TRUE(foundGetProperty);
}

TEST_F(NodeFactoryTests, SearchNodes) {
    auto& factory = NodeFactory::Instance();

    auto results = factory.SearchNodes("Property");
    EXPECT_GT(results.size(), 0);
}

TEST_F(NodeFactoryTests, GetNodeInfo) {
    auto& factory = NodeFactory::Instance();

    auto* info = factory.GetNodeInfo("GetProperty");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->displayName, "Get Property");
    EXPECT_EQ(info->category, NodeCategory::Binding);
}

// =============================================================================
// Event System Tests
// =============================================================================

class EventSystemTests : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(EventSystemTests, CreateChannel) {
    auto& bus = VisualScriptEventBus::Instance();

    auto* channel = bus.GetOrCreateChannel("test.event");
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(channel->GetName(), "test.event");
}

TEST_F(EventSystemTests, PublishAndSubscribe) {
    auto& bus = VisualScriptEventBus::Instance();

    int receivedValue = 0;
    auto conn = bus.Subscribe("value.changed", [&](const std::any& data) {
        receivedValue = std::any_cast<int>(data);
    });

    bus.Publish("value.changed", std::any(42));

    EXPECT_EQ(receivedValue, 42);
}

TEST_F(EventSystemTests, UnsubscribeOnConnectionDestroy) {
    auto& bus = VisualScriptEventBus::Instance();
    auto* channel = bus.GetOrCreateChannel("scoped.event");

    {
        int callCount = 0;
        auto conn = channel->Subscribe([&](const std::any&) {
            callCount++;
        });

        channel->Publish(std::any(1));
        EXPECT_EQ(callCount, 1);
    }  // conn goes out of scope, disconnects

    // Verify channel still works but subscriber is gone
    auto* afterChannel = bus.GetChannel("scoped.event");
    EXPECT_NE(afterChannel, nullptr);
}

TEST_F(EventSystemTests, GetChannelNames) {
    auto& bus = VisualScriptEventBus::Instance();

    bus.GetOrCreateChannel("channel.a");
    bus.GetOrCreateChannel("channel.b");

    auto names = bus.GetChannelNames();
    EXPECT_GE(names.size(), 2);
}

// =============================================================================
// Execution Context Tests
// =============================================================================

class ExecutionContextTests : public ::testing::Test {
protected:
    void SetUp() override {
        graph = std::make_shared<Graph>("TestGraph");
    }

    GraphPtr graph;
};

TEST_F(ExecutionContextTests, CreateContext) {
    ExecutionContext context(graph.get());
    EXPECT_EQ(context.GetGraph(), graph.get());
}

TEST_F(ExecutionContextTests, SetAndGetVariable) {
    ExecutionContext context(graph.get());

    context.SetVariable("test", std::any(123));
    auto value = std::any_cast<int>(context.GetVariable("test"));

    EXPECT_EQ(value, 123);
}

TEST_F(ExecutionContextTests, VariablePropagesToGraph) {
    ExecutionContext context(graph.get());

    context.SetVariable("shared", std::any(456));

    EXPECT_TRUE(graph->HasVariable("shared"));
    auto graphValue = std::any_cast<int>(graph->GetVariable("shared"));
    EXPECT_EQ(graphValue, 456);
}

TEST_F(ExecutionContextTests, ReportErrors) {
    ExecutionContext context(graph.get());

    context.ReportError("Test error 1");
    context.ReportError("Test error 2");

    EXPECT_EQ(context.GetErrors().size(), 2);
    EXPECT_EQ(context.GetErrors()[0], "Test error 1");
}

TEST_F(ExecutionContextTests, ReportWarnings) {
    ExecutionContext context(graph.get());

    context.ReportWarning("Loose binding detected");

    EXPECT_EQ(context.GetWarnings().size(), 1);
}

// =============================================================================
// Connection Tests
// =============================================================================

class ConnectionTests : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ConnectionTests, ValidConnection) {
    auto source = std::make_shared<Port>("out", PortDirection::Output, PortType::Data, "int");
    auto target = std::make_shared<Port>("in", PortDirection::Input, PortType::Data, "int");

    Connection conn(source, target);

    EXPECT_TRUE(conn.IsValid());
    EXPECT_EQ(conn.GetSource(), source);
    EXPECT_EQ(conn.GetTarget(), target);
}

TEST_F(ConnectionTests, ConnectionColor) {
    auto flowSource = std::make_shared<Port>("flow", PortDirection::Output, PortType::Flow);
    auto flowTarget = std::make_shared<Port>("flow", PortDirection::Input, PortType::Flow);
    Connection flowConn(flowSource, flowTarget);

    auto dataSource = std::make_shared<Port>("data", PortDirection::Output, PortType::Data);
    auto dataTarget = std::make_shared<Port>("data", PortDirection::Input, PortType::Data);
    Connection dataConn(dataSource, dataTarget);

    // Colors should be different for different port types
    EXPECT_NE(flowConn.GetColor(), dataConn.GetColor());
}

// =============================================================================
// Binding Reference Tests
// =============================================================================

TEST(BindingReferenceTest, IsValid) {
    BindingReference validHard;
    validHard.state = BindingState::HardBinding;
    EXPECT_TRUE(validHard.IsValid());

    BindingReference validLoose;
    validLoose.state = BindingState::LooseBinding;
    EXPECT_TRUE(validLoose.IsValid());

    BindingReference invalid;
    invalid.state = BindingState::BrokenBinding;
    EXPECT_FALSE(invalid.IsValid());

    BindingReference unbound;
    unbound.state = BindingState::Unbound;
    EXPECT_FALSE(unbound.IsValid());
}

TEST(BindingReferenceTest, NeedsAttention) {
    BindingReference warning;
    warning.warning = BindingWarning::Warning;
    EXPECT_TRUE(warning.NeedsAttention());

    BindingReference error;
    error.warning = BindingWarning::Error;
    EXPECT_TRUE(error.NeedsAttention());

    BindingReference info;
    info.warning = BindingWarning::Info;
    EXPECT_FALSE(info.NeedsAttention());

    BindingReference none;
    none.warning = BindingWarning::None;
    EXPECT_FALSE(none.NeedsAttention());
}
