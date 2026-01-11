/**
 * @file EditorCommand.hpp
 * @brief Base command interface for editor undo/redo system
 *
 * Implements the Command pattern for all editor operations.
 * Supports command merging for continuous operations like dragging.
 */

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

class SceneNode;
class Scene;

/**
 * @brief Unique identifier for command types
 *
 * Used for command merging to identify compatible commands.
 */
using CommandTypeId = size_t;

/**
 * @brief Generate a unique type ID for a command class
 * @tparam T The command class type
 * @return Unique identifier for the command type
 */
template<typename T>
constexpr CommandTypeId GetCommandTypeId() {
    return typeid(T).hash_code();
}

/**
 * @brief Abstract base interface for all editor commands
 *
 * All editor operations that modify state should be implemented as commands
 * to support undo/redo functionality. Commands are immutable after creation -
 * Execute() and Undo() should not modify the command's internal state.
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Execute the command
     * @return true if execution succeeded
     */
    virtual bool Execute() = 0;

    /**
     * @brief Undo the command (reverse Execute)
     * @return true if undo succeeded
     */
    virtual bool Undo() = 0;

    /**
     * @brief Get human-readable command name for UI display
     * @return Command description (e.g., "Move Object", "Delete Selection")
     */
    [[nodiscard]] virtual std::string GetName() const = 0;

    /**
     * @brief Get the command type identifier for merging
     * @return Type ID unique to this command class
     */
    [[nodiscard]] virtual CommandTypeId GetTypeId() const = 0;

    /**
     * @brief Check if this command can be merged with another
     *
     * Merging combines continuous operations (like dragging) into a single
     * undoable action. Override to enable merging for specific command types.
     *
     * @param other The command to potentially merge with
     * @return true if commands can be merged
     */
    [[nodiscard]] virtual bool CanMergeWith(const ICommand& other) const {
        (void)other;
        return false;
    }

    /**
     * @brief Merge another command into this one
     *
     * Called when CanMergeWith returns true. The other command's changes
     * should be incorporated into this command.
     *
     * @param other The command to merge (will be discarded after merge)
     * @return true if merge succeeded
     */
    virtual bool MergeWith(const ICommand& other) {
        (void)other;
        return false;
    }

    /**
     * @brief Get timestamp when command was created
     * @return Creation time point
     */
    [[nodiscard]] std::chrono::steady_clock::time_point GetTimestamp() const {
        return m_timestamp;
    }

    /**
     * @brief Check if command is still within merge window
     * @param windowMs Maximum time difference in milliseconds for merging
     * @return true if command is recent enough for merging
     */
    [[nodiscard]] bool IsWithinMergeWindow(uint32_t windowMs = 500) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timestamp);
        return elapsed.count() < windowMs;
    }

protected:
    ICommand() : m_timestamp(std::chrono::steady_clock::now()) {}

    std::chrono::steady_clock::time_point m_timestamp;
};

/**
 * @brief Smart pointer type for commands
 */
using CommandPtr = std::unique_ptr<ICommand>;

// =============================================================================
// Transform Data Structures
// =============================================================================

/**
 * @brief Captured transform state for undo/redo
 */
struct TransformState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    bool operator==(const TransformState& other) const {
        return position == other.position &&
               rotation == other.rotation &&
               scale == other.scale;
    }

    bool operator!=(const TransformState& other) const {
        return !(*this == other);
    }
};

// =============================================================================
// Concrete Command Implementations
// =============================================================================

/**
 * @brief Command for transforming a scene node (position, rotation, scale)
 *
 * Supports merging for continuous drag operations. Multiple transform commands
 * on the same node within the merge window will be combined into one.
 */
class TransformCommand : public ICommand {
public:
    /**
     * @brief Create a transform command
     * @param node The node to transform
     * @param newState The new transform state
     */
    TransformCommand(SceneNode* node, const TransformState& newState);

    /**
     * @brief Create a transform command with explicit old state
     * @param node The node to transform
     * @param oldState The previous transform state
     * @param newState The new transform state
     */
    TransformCommand(SceneNode* node, const TransformState& oldState, const TransformState& newState);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

    /**
     * @brief Get the target node
     */
    [[nodiscard]] SceneNode* GetNode() const { return m_node; }

    /**
     * @brief Get the old transform state
     */
    [[nodiscard]] const TransformState& GetOldState() const { return m_oldState; }

    /**
     * @brief Get the new transform state
     */
    [[nodiscard]] const TransformState& GetNewState() const { return m_newState; }

private:
    SceneNode* m_node;
    TransformState m_oldState;
    TransformState m_newState;
};

/**
 * @brief Command for creating a new object in the scene
 *
 * Stores the created node for undo (removal) and redo (re-insertion).
 */
class CreateObjectCommand : public ICommand {
public:
    /**
     * @brief Create a command for adding a new node
     * @param scene The scene to add the node to
     * @param node The node to add (ownership transferred)
     * @param parent Optional parent node (nullptr for root)
     */
    CreateObjectCommand(Scene* scene, std::unique_ptr<SceneNode> node, SceneNode* parent = nullptr);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the created node (valid after Execute)
     */
    [[nodiscard]] SceneNode* GetCreatedNode() const { return m_nodePtr; }

private:
    Scene* m_scene;
    std::unique_ptr<SceneNode> m_ownedNode;  // Owned when not in scene
    SceneNode* m_nodePtr = nullptr;           // Always points to the node
    SceneNode* m_parent = nullptr;
    std::string m_nodeName;
};

/**
 * @brief Command for deleting an object from the scene
 *
 * Preserves the deleted node and its parent for undo restoration.
 */
class DeleteObjectCommand : public ICommand {
public:
    /**
     * @brief Create a delete command
     * @param scene The scene containing the node
     * @param node The node to delete
     */
    DeleteObjectCommand(Scene* scene, SceneNode* node);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    Scene* m_scene;
    std::unique_ptr<SceneNode> m_ownedNode;  // Owned when removed from scene
    SceneNode* m_nodePtr = nullptr;           // Original pointer
    SceneNode* m_parent = nullptr;            // Parent for restoration
    size_t m_siblingIndex = 0;                // Position among siblings
    std::string m_nodeName;
};

/**
 * @brief Command for renaming a node
 */
class RenameCommand : public ICommand {
public:
    /**
     * @brief Create a rename command
     * @param node The node to rename
     * @param newName The new name
     */
    RenameCommand(SceneNode* node, const std::string& newName);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    SceneNode* m_node;
    std::string m_oldName;
    std::string m_newName;
};

/**
 * @brief Command for reparenting a node in the hierarchy
 */
class ReparentCommand : public ICommand {
public:
    /**
     * @brief Create a reparent command
     * @param node The node to reparent
     * @param newParent The new parent (nullptr for root)
     */
    ReparentCommand(SceneNode* node, SceneNode* newParent);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    SceneNode* m_node;
    SceneNode* m_oldParent;
    SceneNode* m_newParent;
    size_t m_oldSiblingIndex = 0;
};

/**
 * @brief Command that groups multiple commands as a single undoable operation
 *
 * Useful for complex operations that modify multiple objects atomically.
 */
class CompositeCommand : public ICommand {
public:
    /**
     * @brief Create an empty composite command
     * @param name Display name for the composite operation
     */
    explicit CompositeCommand(const std::string& name);

    /**
     * @brief Create a composite command with initial commands
     * @param name Display name for the composite operation
     * @param commands Initial commands to include
     */
    CompositeCommand(const std::string& name, std::vector<CommandPtr> commands);

    /**
     * @brief Add a command to the composite
     * @param command Command to add (ownership transferred)
     */
    void AddCommand(CommandPtr command);

    /**
     * @brief Get the number of sub-commands
     */
    [[nodiscard]] size_t GetCommandCount() const { return m_commands.size(); }

    /**
     * @brief Check if composite is empty
     */
    [[nodiscard]] bool IsEmpty() const { return m_commands.empty(); }

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    std::string m_name;
    std::vector<CommandPtr> m_commands;
    size_t m_executedCount = 0;  // Track partial execution for rollback
};

/**
 * @brief Command for setting a property value (generic)
 * @tparam T The property value type
 */
template<typename T>
class PropertyCommand : public ICommand {
public:
    using Getter = std::function<T()>;
    using Setter = std::function<void(const T&)>;

    /**
     * @brief Create a property command
     * @param name Display name for the property change
     * @param getter Function to get current value
     * @param setter Function to set new value
     * @param newValue The new value to set
     */
    PropertyCommand(const std::string& name, Getter getter, Setter setter, const T& newValue)
        : m_name(name)
        , m_getter(std::move(getter))
        , m_setter(std::move(setter))
        , m_newValue(newValue)
        , m_oldValue(m_getter())
    {}

    bool Execute() override {
        m_setter(m_newValue);
        return true;
    }

    bool Undo() override {
        m_setter(m_oldValue);
        return true;
    }

    [[nodiscard]] std::string GetName() const override {
        return "Set " + m_name;
    }

    [[nodiscard]] CommandTypeId GetTypeId() const override {
        return GetCommandTypeId<PropertyCommand<T>>();
    }

private:
    std::string m_name;
    Getter m_getter;
    Setter m_setter;
    T m_oldValue;
    T m_newValue;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Capture the current transform state of a node
 * @param node The node to capture
 * @return Transform state snapshot
 */
TransformState CaptureTransformState(const SceneNode* node);

/**
 * @brief Apply a transform state to a node
 * @param node The node to modify
 * @param state The transform state to apply
 */
void ApplyTransformState(SceneNode* node, const TransformState& state);

} // namespace Nova
