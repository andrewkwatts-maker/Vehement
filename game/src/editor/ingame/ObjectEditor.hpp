#pragma once

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <variant>

namespace Vehement {

// Forward declarations
class InGameEditor;

/**
 * @brief Object category types
 */
enum class ObjectCategory : uint8_t {
    Unit,
    Building,
    Ability,
    Upgrade,
    Item,
    Doodad
};

/**
 * @brief Stat modifier operation
 */
enum class StatOperation : uint8_t {
    Set,        // Set to value
    Add,        // Add value
    Multiply,   // Multiply by value
    Percent     // Modify by percentage
};

/**
 * @brief Stat modification
 */
struct StatModification {
    std::string statName;
    StatOperation operation = StatOperation::Set;
    float value = 0.0f;
};

/**
 * @brief Custom object definition (modification from template)
 */
struct CustomObject {
    std::string id;                     // Unique ID for this custom object
    std::string baseId;                 // ID of the template object
    ObjectCategory category;
    std::string name;
    std::string description;
    std::string iconPath;

    // Stat modifications from base
    std::vector<StatModification> statMods;

    // Custom properties
    std::unordered_map<std::string, std::string> properties;

    // For abilities
    std::string customScript;

    // Visual modifications
    std::string customModel;
    float modelScale = 1.0f;
    std::array<float, 4> tint = {1.0f, 1.0f, 1.0f, 1.0f};

    // Sound modifications
    std::string customSound;
};

/**
 * @brief Base object template (from game data)
 */
struct ObjectTemplate {
    std::string id;
    ObjectCategory category;
    std::string name;
    std::string description;
    std::string iconPath;
    std::string modelPath;

    // Stats (varies by category)
    std::unordered_map<std::string, float> stats;

    // Properties
    std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief Object editor command for undo/redo
 */
class ObjectEditorCommand {
public:
    virtual ~ObjectEditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
};

/**
 * @brief Object Editor - Edit game objects
 *
 * Allows modification of game objects with permission-based access:
 * - Unit stats modification (health, damage, speed, etc.)
 * - Building properties (cost, build time, abilities)
 * - Ability modifications (damage, cooldown, range)
 * - Upgrade changes
 * - Custom objects from templates
 */
class ObjectEditor {
public:
    ObjectEditor();
    ~ObjectEditor();

    // Non-copyable
    ObjectEditor(const ObjectEditor&) = delete;
    ObjectEditor& operator=(const ObjectEditor&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(InGameEditor& parent);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    void Update(float deltaTime);
    void Render();
    void ProcessInput();

    // =========================================================================
    // Template Management
    // =========================================================================

    void LoadTemplates();
    [[nodiscard]] const std::vector<ObjectTemplate>& GetTemplates(ObjectCategory category) const;
    [[nodiscard]] const ObjectTemplate* GetTemplate(const std::string& id) const;

    // =========================================================================
    // Custom Object Management
    // =========================================================================

    std::string CreateCustomObject(const std::string& baseId);
    void DeleteCustomObject(const std::string& id);
    void UpdateCustomObject(const std::string& id, const CustomObject& obj);
    [[nodiscard]] CustomObject* GetCustomObject(const std::string& id);
    [[nodiscard]] const std::vector<CustomObject>& GetCustomObjects() const { return m_customObjects; }

    void SelectObject(const std::string& id);
    [[nodiscard]] const std::string& GetSelectedObjectId() const { return m_selectedObjectId; }

    // =========================================================================
    // Stat Modification
    // =========================================================================

    void AddStatMod(const std::string& objectId, const StatModification& mod);
    void RemoveStatMod(const std::string& objectId, const std::string& statName);
    void ClearStatMods(const std::string& objectId);

    // Calculate effective stat value
    [[nodiscard]] float GetEffectiveStat(const std::string& objectId, const std::string& statName) const;

    // =========================================================================
    // Property Modification
    // =========================================================================

    void SetProperty(const std::string& objectId, const std::string& key, const std::string& value);
    void RemoveProperty(const std::string& objectId, const std::string& key);

    // =========================================================================
    // Visual Modification
    // =========================================================================

    void SetCustomModel(const std::string& objectId, const std::string& modelPath);
    void SetModelScale(const std::string& objectId, float scale);
    void SetTint(const std::string& objectId, float r, float g, float b, float a);

    // =========================================================================
    // Import/Export
    // =========================================================================

    bool ExportCustomObjects(const std::string& path) const;
    bool ImportCustomObjects(const std::string& path);

    // =========================================================================
    // Validation
    // =========================================================================

    [[nodiscard]] bool ValidateCustomObject(const std::string& id, std::string& error) const;
    [[nodiscard]] bool ValidateAll(std::vector<std::string>& errors) const;

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    void ExecuteCommand(std::unique_ptr<ObjectEditorCommand> command);
    void Undo();
    void Redo();
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    void ClearHistory();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnObjectCreated;
    std::function<void(const std::string&)> OnObjectModified;
    std::function<void(const std::string&)> OnObjectDeleted;
    std::function<void(const std::string&)> OnObjectSelected;

private:
    // Rendering helpers
    void RenderCategoryList();
    void RenderObjectList();
    void RenderObjectDetails();
    void RenderStatEditor();
    void RenderPropertyEditor();
    void RenderVisualEditor();
    void RenderPreview();

    // ID generation
    std::string GenerateCustomId(const std::string& baseId);

    // State
    bool m_initialized = false;
    InGameEditor* m_parent = nullptr;

    // Templates (from game data)
    std::unordered_map<ObjectCategory, std::vector<ObjectTemplate>> m_templates;

    // Custom objects
    std::vector<CustomObject> m_customObjects;

    // Selection
    ObjectCategory m_selectedCategory = ObjectCategory::Unit;
    std::string m_selectedObjectId;
    bool m_showingCustomOnly = false;

    // Filter
    std::string m_searchFilter;

    // Undo/Redo
    std::deque<std::unique_ptr<ObjectEditorCommand>> m_undoStack;
    std::deque<std::unique_ptr<ObjectEditorCommand>> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    // ID counter for custom objects
    int m_nextCustomId = 1;
};

// Helper functions
[[nodiscard]] const char* GetCategoryName(ObjectCategory category);
[[nodiscard]] const char* GetStatOperationName(StatOperation op);

} // namespace Vehement
