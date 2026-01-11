#pragma once

#include "VisualScriptingCore.hpp"
#include <cmath>

namespace Nova {
namespace VisualScript {

// =============================================================================
// Flow Control Nodes
// =============================================================================

/**
 * @brief Branch node - if/else flow control
 */
class BranchNode : public Node {
public:
    BranchNode() : Node("Branch", "Branch") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Executes True or False branch based on condition");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("condition", PortDirection::Input, PortType::Data, "bool"));
        AddOutputPort(std::make_shared<Port>("true", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("false", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {
        auto condPort = GetInputPort("condition");
        bool condition = false;
        try {
            condition = std::any_cast<bool>(condPort->GetValue());
        } catch (...) {}
        // Flow would continue through appropriate output
    }
};

/**
 * @brief Sequence node - executes multiple outputs in order
 */
class SequenceNode : public Node {
public:
    SequenceNode() : Node("Sequence", "Sequence") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Executes outputs in sequential order");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("then0", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("then1", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("then2", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {
        // Execute each output in order
    }
};

/**
 * @brief ForLoop node - iterate a fixed number of times
 */
class ForLoopNode : public Node {
public:
    ForLoopNode() : Node("ForLoop", "For Loop") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Executes loop body for each iteration");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("start", PortDirection::Input, PortType::Data, "int"));
        AddInputPort(std::make_shared<Port>("end", PortDirection::Input, PortType::Data, "int"));
        AddOutputPort(std::make_shared<Port>("loopBody", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("index", PortDirection::Output, PortType::Data, "int"));
        AddOutputPort(std::make_shared<Port>("completed", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {
        // Loop implementation
    }
};

/**
 * @brief ForEachLoop node - iterate over array
 */
class ForEachLoopNode : public Node {
public:
    ForEachLoopNode() : Node("ForEachLoop", "For Each Loop") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Executes loop body for each element in array");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("array", PortDirection::Input, PortType::Data, "array"));
        AddOutputPort(std::make_shared<Port>("loopBody", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("element", PortDirection::Output, PortType::Data, "any"));
        AddOutputPort(std::make_shared<Port>("index", PortDirection::Output, PortType::Data, "int"));
        AddOutputPort(std::make_shared<Port>("completed", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {}
};

/**
 * @brief WhileLoop node - loop while condition is true
 */
class WhileLoopNode : public Node {
public:
    WhileLoopNode() : Node("WhileLoop", "While Loop") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Executes loop body while condition is true");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("condition", PortDirection::Input, PortType::Data, "bool"));
        AddOutputPort(std::make_shared<Port>("loopBody", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("completed", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {}
};

/**
 * @brief Gate node - controls flow based on state
 */
class GateNode : public Node {
public:
    GateNode() : Node("Gate", "Gate") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Allows or blocks execution flow");

        AddInputPort(std::make_shared<Port>("enter", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("open", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("close", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("toggle", PortDirection::Input, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("exit", PortDirection::Output, PortType::Flow));

        m_isOpen = false;
    }

    void Execute(ExecutionContext& context) override {}

private:
    bool m_isOpen;
};

// =============================================================================
// Math Nodes
// =============================================================================

/**
 * @brief Add node
 */
class AddNode : public Node {
public:
    AddNode() : Node("Add", "Add") {
        SetCategory(NodeCategory::Math);
        SetDescription("Adds two values");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float a = 0, b = 0;
        try { a = std::any_cast<float>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<float>(GetInputPort("b")->GetValue()); } catch (...) {}
        GetOutputPort("result")->SetValue(std::any(a + b));
    }
};

/**
 * @brief Subtract node
 */
class SubtractNode : public Node {
public:
    SubtractNode() : Node("Subtract", "Subtract") {
        SetCategory(NodeCategory::Math);
        SetDescription("Subtracts second value from first");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float a = 0, b = 0;
        try { a = std::any_cast<float>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<float>(GetInputPort("b")->GetValue()); } catch (...) {}
        GetOutputPort("result")->SetValue(std::any(a - b));
    }
};

/**
 * @brief Multiply node
 */
class MultiplyNode : public Node {
public:
    MultiplyNode() : Node("Multiply", "Multiply") {
        SetCategory(NodeCategory::Math);
        SetDescription("Multiplies two values");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float a = 1, b = 1;
        try { a = std::any_cast<float>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<float>(GetInputPort("b")->GetValue()); } catch (...) {}
        GetOutputPort("result")->SetValue(std::any(a * b));
    }
};

/**
 * @brief Divide node
 */
class DivideNode : public Node {
public:
    DivideNode() : Node("Divide", "Divide") {
        SetCategory(NodeCategory::Math);
        SetDescription("Divides first value by second");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float a = 0, b = 1;
        try { a = std::any_cast<float>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<float>(GetInputPort("b")->GetValue()); } catch (...) {}
        if (b == 0) {
            context.ReportError("Division by zero");
            return;
        }
        GetOutputPort("result")->SetValue(std::any(a / b));
    }
};

/**
 * @brief Clamp node
 */
class ClampNode : public Node {
public:
    ClampNode() : Node("Clamp", "Clamp") {
        SetCategory(NodeCategory::Math);
        SetDescription("Clamps value between min and max");

        AddInputPort(std::make_shared<Port>("value", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("min", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("max", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float value = 0, minVal = 0, maxVal = 1;
        try { value = std::any_cast<float>(GetInputPort("value")->GetValue()); } catch (...) {}
        try { minVal = std::any_cast<float>(GetInputPort("min")->GetValue()); } catch (...) {}
        try { maxVal = std::any_cast<float>(GetInputPort("max")->GetValue()); } catch (...) {}
        float result = std::max(minVal, std::min(maxVal, value));
        GetOutputPort("result")->SetValue(std::any(result));
    }
};

/**
 * @brief Lerp node - linear interpolation
 */
class LerpNode : public Node {
public:
    LerpNode() : Node("Lerp", "Lerp") {
        SetCategory(NodeCategory::Math);
        SetDescription("Linearly interpolates between A and B");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("alpha", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float a = 0, b = 1, alpha = 0.5f;
        try { a = std::any_cast<float>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<float>(GetInputPort("b")->GetValue()); } catch (...) {}
        try { alpha = std::any_cast<float>(GetInputPort("alpha")->GetValue()); } catch (...) {}
        float result = a + alpha * (b - a);
        GetOutputPort("result")->SetValue(std::any(result));
    }
};

/**
 * @brief Random node
 */
class RandomNode : public Node {
public:
    RandomNode() : Node("Random", "Random") {
        SetCategory(NodeCategory::Math);
        SetDescription("Generates random value between min and max");

        AddInputPort(std::make_shared<Port>("min", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("max", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {
        float minVal = 0, maxVal = 1;
        try { minVal = std::any_cast<float>(GetInputPort("min")->GetValue()); } catch (...) {}
        try { maxVal = std::any_cast<float>(GetInputPort("max")->GetValue()); } catch (...) {}
        float t = static_cast<float>(rand()) / RAND_MAX;
        float result = minVal + t * (maxVal - minVal);
        GetOutputPort("result")->SetValue(std::any(result));
    }
};

// =============================================================================
// Logic Nodes
// =============================================================================

/**
 * @brief AND node
 */
class AndNode : public Node {
public:
    AndNode() : Node("And", "AND") {
        SetCategory(NodeCategory::Logic);
        SetDescription("Logical AND of two booleans");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "bool"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "bool"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "bool"));
    }

    void Execute(ExecutionContext& context) override {
        bool a = false, b = false;
        try { a = std::any_cast<bool>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<bool>(GetInputPort("b")->GetValue()); } catch (...) {}
        GetOutputPort("result")->SetValue(std::any(a && b));
    }
};

/**
 * @brief OR node
 */
class OrNode : public Node {
public:
    OrNode() : Node("Or", "OR") {
        SetCategory(NodeCategory::Logic);
        SetDescription("Logical OR of two booleans");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "bool"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "bool"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "bool"));
    }

    void Execute(ExecutionContext& context) override {
        bool a = false, b = false;
        try { a = std::any_cast<bool>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<bool>(GetInputPort("b")->GetValue()); } catch (...) {}
        GetOutputPort("result")->SetValue(std::any(a || b));
    }
};

/**
 * @brief NOT node
 */
class NotNode : public Node {
public:
    NotNode() : Node("Not", "NOT") {
        SetCategory(NodeCategory::Logic);
        SetDescription("Logical NOT of boolean");

        AddInputPort(std::make_shared<Port>("input", PortDirection::Input, PortType::Data, "bool"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "bool"));
    }

    void Execute(ExecutionContext& context) override {
        bool input = false;
        try { input = std::any_cast<bool>(GetInputPort("input")->GetValue()); } catch (...) {}
        GetOutputPort("result")->SetValue(std::any(!input));
    }
};

/**
 * @brief Compare node
 */
class CompareNode : public Node {
public:
    enum class Operation { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

    CompareNode() : Node("Compare", "Compare") {
        SetCategory(NodeCategory::Logic);
        SetDescription("Compares two values");

        AddInputPort(std::make_shared<Port>("a", PortDirection::Input, PortType::Data, "float"));
        AddInputPort(std::make_shared<Port>("b", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("result", PortDirection::Output, PortType::Data, "bool"));
    }

    void SetOperation(Operation op) { m_operation = op; }

    void Execute(ExecutionContext& context) override {
        float a = 0, b = 0;
        try { a = std::any_cast<float>(GetInputPort("a")->GetValue()); } catch (...) {}
        try { b = std::any_cast<float>(GetInputPort("b")->GetValue()); } catch (...) {}

        bool result = false;
        switch (m_operation) {
            case Operation::Equal: result = (a == b); break;
            case Operation::NotEqual: result = (a != b); break;
            case Operation::Less: result = (a < b); break;
            case Operation::LessEqual: result = (a <= b); break;
            case Operation::Greater: result = (a > b); break;
            case Operation::GreaterEqual: result = (a >= b); break;
        }
        GetOutputPort("result")->SetValue(std::any(result));
    }

private:
    Operation m_operation = Operation::Equal;
};

// =============================================================================
// Data Nodes
// =============================================================================

/**
 * @brief Get Variable node
 */
class GetVariableNode : public Node {
public:
    GetVariableNode() : Node("GetVariable", "Get Variable") {
        SetCategory(NodeCategory::Data);
        SetDescription("Gets a variable value from the graph");

        AddOutputPort(std::make_shared<Port>("value", PortDirection::Output, PortType::Data, "any"));
    }

    void SetVariableName(const std::string& name) { m_variableName = name; }

    void Execute(ExecutionContext& context) override {
        auto value = context.GetVariable(m_variableName);
        GetOutputPort("value")->SetValue(value);
    }

private:
    std::string m_variableName;
};

/**
 * @brief Set Variable node
 */
class SetVariableNode : public Node {
public:
    SetVariableNode() : Node("SetVariable", "Set Variable") {
        SetCategory(NodeCategory::Data);
        SetDescription("Sets a variable value in the graph");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("value", PortDirection::Input, PortType::Data, "any"));
        AddOutputPort(std::make_shared<Port>("exec", PortDirection::Output, PortType::Flow));
    }

    void SetVariableName(const std::string& name) { m_variableName = name; }

    void Execute(ExecutionContext& context) override {
        auto& value = GetInputPort("value")->GetValue();
        context.SetVariable(m_variableName, value);
    }

private:
    std::string m_variableName;
};

/**
 * @brief Make Array node
 */
class MakeArrayNode : public Node {
public:
    MakeArrayNode() : Node("MakeArray", "Make Array") {
        SetCategory(NodeCategory::Data);
        SetDescription("Creates an array from inputs");

        AddInputPort(std::make_shared<Port>("element0", PortDirection::Input, PortType::Data, "any"));
        AddInputPort(std::make_shared<Port>("element1", PortDirection::Input, PortType::Data, "any"));
        AddInputPort(std::make_shared<Port>("element2", PortDirection::Input, PortType::Data, "any"));
        AddOutputPort(std::make_shared<Port>("array", PortDirection::Output, PortType::Data, "array"));
    }

    void Execute(ExecutionContext& context) override {
        std::vector<std::any> array;
        for (const auto& port : GetInputPorts()) {
            if (port->GetValue().has_value()) {
                array.push_back(port->GetValue());
            }
        }
        GetOutputPort("array")->SetValue(std::any(array));
    }
};

/**
 * @brief Get Array Element node
 */
class GetArrayElementNode : public Node {
public:
    GetArrayElementNode() : Node("GetArrayElement", "Get Array Element") {
        SetCategory(NodeCategory::Data);
        SetDescription("Gets element at index from array");

        AddInputPort(std::make_shared<Port>("array", PortDirection::Input, PortType::Data, "array"));
        AddInputPort(std::make_shared<Port>("index", PortDirection::Input, PortType::Data, "int"));
        AddOutputPort(std::make_shared<Port>("element", PortDirection::Output, PortType::Data, "any"));
    }

    void Execute(ExecutionContext& context) override {
        try {
            auto array = std::any_cast<std::vector<std::any>>(GetInputPort("array")->GetValue());
            int index = std::any_cast<int>(GetInputPort("index")->GetValue());
            if (index >= 0 && index < static_cast<int>(array.size())) {
                GetOutputPort("element")->SetValue(array[index]);
            } else {
                context.ReportError("Array index out of bounds");
            }
        } catch (...) {
            context.ReportError("Invalid array or index");
        }
    }
};

/**
 * @brief Print/Log node
 */
class PrintNode : public Node {
public:
    PrintNode() : Node("Print", "Print") {
        SetCategory(NodeCategory::Data);
        SetDescription("Prints value to console/log");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("message", PortDirection::Input, PortType::Data, "string"));
        AddOutputPort(std::make_shared<Port>("exec", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {
        // Would log the message
    }
};

// =============================================================================
// Timer/Delay Nodes
// =============================================================================

/**
 * @brief Delay node
 */
class DelayNode : public Node {
public:
    DelayNode() : Node("Delay", "Delay") {
        SetCategory(NodeCategory::Flow);
        SetDescription("Delays execution by specified time");

        AddInputPort(std::make_shared<Port>("exec", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("duration", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("completed", PortDirection::Output, PortType::Flow));
    }

    void Execute(ExecutionContext& context) override {
        // Would start a timer and trigger output after duration
    }
};

/**
 * @brief Timer node - recurring
 */
class TimerNode : public Node {
public:
    TimerNode() : Node("Timer", "Timer") {
        SetCategory(NodeCategory::Event);
        SetDescription("Fires repeatedly at interval");

        AddInputPort(std::make_shared<Port>("start", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("stop", PortDirection::Input, PortType::Flow));
        AddInputPort(std::make_shared<Port>("interval", PortDirection::Input, PortType::Data, "float"));
        AddOutputPort(std::make_shared<Port>("tick", PortDirection::Output, PortType::Flow));
        AddOutputPort(std::make_shared<Port>("elapsed", PortDirection::Output, PortType::Data, "float"));
    }

    void Execute(ExecutionContext& context) override {}
};

// =============================================================================
// Node Registration
// =============================================================================

inline void RegisterStandardNodes() {
    auto& factory = NodeFactory::Instance();

    // Flow control
    factory.Register("Branch", []() { return std::make_shared<BranchNode>(); },
        NodeCategory::Flow, "Branch", "Conditional branch");
    factory.Register("Sequence", []() { return std::make_shared<SequenceNode>(); },
        NodeCategory::Flow, "Sequence", "Execute in sequence");
    factory.Register("ForLoop", []() { return std::make_shared<ForLoopNode>(); },
        NodeCategory::Flow, "For Loop", "Loop from start to end");
    factory.Register("ForEachLoop", []() { return std::make_shared<ForEachLoopNode>(); },
        NodeCategory::Flow, "For Each Loop", "Loop over array");
    factory.Register("WhileLoop", []() { return std::make_shared<WhileLoopNode>(); },
        NodeCategory::Flow, "While Loop", "Loop while true");
    factory.Register("Gate", []() { return std::make_shared<GateNode>(); },
        NodeCategory::Flow, "Gate", "Control flow gate");
    factory.Register("Delay", []() { return std::make_shared<DelayNode>(); },
        NodeCategory::Flow, "Delay", "Delay execution");

    // Math
    factory.Register("Add", []() { return std::make_shared<AddNode>(); },
        NodeCategory::Math, "Add", "Add two numbers");
    factory.Register("Subtract", []() { return std::make_shared<SubtractNode>(); },
        NodeCategory::Math, "Subtract", "Subtract two numbers");
    factory.Register("Multiply", []() { return std::make_shared<MultiplyNode>(); },
        NodeCategory::Math, "Multiply", "Multiply two numbers");
    factory.Register("Divide", []() { return std::make_shared<DivideNode>(); },
        NodeCategory::Math, "Divide", "Divide two numbers");
    factory.Register("Clamp", []() { return std::make_shared<ClampNode>(); },
        NodeCategory::Math, "Clamp", "Clamp value to range");
    factory.Register("Lerp", []() { return std::make_shared<LerpNode>(); },
        NodeCategory::Math, "Lerp", "Linear interpolation");
    factory.Register("Random", []() { return std::make_shared<RandomNode>(); },
        NodeCategory::Math, "Random", "Random number");

    // Logic
    factory.Register("And", []() { return std::make_shared<AndNode>(); },
        NodeCategory::Logic, "AND", "Logical AND");
    factory.Register("Or", []() { return std::make_shared<OrNode>(); },
        NodeCategory::Logic, "OR", "Logical OR");
    factory.Register("Not", []() { return std::make_shared<NotNode>(); },
        NodeCategory::Logic, "NOT", "Logical NOT");
    factory.Register("Compare", []() { return std::make_shared<CompareNode>(); },
        NodeCategory::Logic, "Compare", "Compare values");

    // Data
    factory.Register("GetVariable", []() { return std::make_shared<GetVariableNode>(); },
        NodeCategory::Data, "Get Variable", "Get graph variable");
    factory.Register("SetVariable", []() { return std::make_shared<SetVariableNode>(); },
        NodeCategory::Data, "Set Variable", "Set graph variable");
    factory.Register("MakeArray", []() { return std::make_shared<MakeArrayNode>(); },
        NodeCategory::Data, "Make Array", "Create array");
    factory.Register("GetArrayElement", []() { return std::make_shared<GetArrayElementNode>(); },
        NodeCategory::Data, "Get Array Element", "Get element from array");
    factory.Register("Print", []() { return std::make_shared<PrintNode>(); },
        NodeCategory::Data, "Print", "Print to log");

    // Events
    factory.Register("Timer", []() { return std::make_shared<TimerNode>(); },
        NodeCategory::Event, "Timer", "Recurring timer");
}

} // namespace VisualScript
} // namespace Nova
