#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

/**
 * @brief Base interface for all editor commands
 * Implements the Command pattern for undo/redo functionality
 */
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;

    /**
     * @brief Execute the command
     */
    virtual void Execute() = 0;

    /**
     * @brief Undo the command
     */
    virtual void Undo() = 0;

    /**
     * @brief Get a human-readable description of the command
     */
    virtual std::string GetDescription() const = 0;
};

/**
 * @brief Manages command history for undo/redo functionality
 */
class CommandHistory {
public:
    CommandHistory();
    ~CommandHistory();

    /**
     * @brief Execute a command and add it to the history
     * @param cmd Command to execute
     */
    void ExecuteCommand(std::unique_ptr<IEditorCommand> cmd);

    /**
     * @brief Undo the last command
     */
    void Undo();

    /**
     * @brief Redo the last undone command
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    bool CanUndo() const;

    /**
     * @brief Check if redo is available
     */
    bool CanRedo() const;

    /**
     * @brief Clear all command history
     */
    void Clear();

    /**
     * @brief Get the maximum history size
     */
    int GetMaxHistorySize() const { return m_maxHistorySize; }

    /**
     * @brief Set the maximum history size
     */
    void SetMaxHistorySize(int size) { m_maxHistorySize = size; }

    /**
     * @brief Get the current undo stack size
     */
    size_t GetUndoStackSize() const { return m_undoStack.size(); }

    /**
     * @brief Get the current redo stack size
     */
    size_t GetRedoStackSize() const { return m_redoStack.size(); }

private:
    std::vector<std::unique_ptr<IEditorCommand>> m_undoStack;
    std::vector<std::unique_ptr<IEditorCommand>> m_redoStack;
    int m_maxHistorySize = 100;
};

// =============================================================================
// Concrete Command Implementations
// =============================================================================

/**
 * @brief Command for painting terrain tiles
 */
class TerrainPaintCommand : public IEditorCommand {
public:
    TerrainPaintCommand(std::vector<int>& terrainTiles, int tileIndex, int oldValue, int newValue);

    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    std::vector<int>& m_terrainTiles;
    int m_tileIndex;
    int m_oldValue;
    int m_newValue;
};

/**
 * @brief Command for sculpting terrain height
 * Stores height changes for all affected tiles
 */
class TerrainSculptCommand : public IEditorCommand {
public:
    TerrainSculptCommand(std::vector<float>& terrainHeights);

    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

    /**
     * @brief Add a tile height change
     */
    void AddHeightChange(int tileIndex, float oldHeight, float newHeight);

private:
    std::vector<float>& m_terrainHeights;
    std::vector<int> m_affectedTiles;
    std::vector<float> m_oldHeights;
    std::vector<float> m_newHeights;
};

/**
 * @brief Command for placing objects
 */
class ObjectPlaceCommand : public IEditorCommand {
public:
    struct ObjectData {
        std::string type;
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
    };

    ObjectPlaceCommand(std::vector<ObjectData>& objects, const ObjectData& objectData);

    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    std::vector<ObjectData>& m_objects;
    ObjectData m_objectData;
    int m_objectIndex = -1; // Index of placed object
};

/**
 * @brief Command for deleting objects
 */
class ObjectDeleteCommand : public IEditorCommand {
public:
    ObjectDeleteCommand(std::vector<ObjectPlaceCommand::ObjectData>& objects, int objectIndex);

    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    std::vector<ObjectPlaceCommand::ObjectData>& m_objects;
    int m_objectIndex;
    ObjectPlaceCommand::ObjectData m_deletedObjectData;
    bool m_isDeleted = false;
};

/**
 * @brief Command for transforming objects (move/rotate/scale)
 */
class ObjectTransformCommand : public IEditorCommand {
public:
    ObjectTransformCommand(
        std::vector<ObjectPlaceCommand::ObjectData>& objects,
        int objectIndex,
        const glm::vec3& oldPosition,
        const glm::vec3& oldRotation,
        const glm::vec3& oldScale,
        const glm::vec3& newPosition,
        const glm::vec3& newRotation,
        const glm::vec3& newScale
    );

    void Execute() override;
    void Undo() override;
    std::string GetDescription() const override;

private:
    std::vector<ObjectPlaceCommand::ObjectData>& m_objects;
    int m_objectIndex;
    glm::vec3 m_oldPosition;
    glm::vec3 m_oldRotation;
    glm::vec3 m_oldScale;
    glm::vec3 m_newPosition;
    glm::vec3 m_newRotation;
    glm::vec3 m_newScale;
};
