#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class AnimationLayerStack;
class AnimationLayer;
class BlendMask;
}

namespace Vehement {

class Editor;

/**
 * @brief Editor for animation layer stack
 *
 * Provides:
 * - Reorderable layer list
 * - Per-layer weight slider
 * - Per-layer mask assignment
 * - Solo/mute layers
 */
class LayerStackEditor {
public:
    /**
     * @brief Layer display info
     */
    struct LayerItem {
        size_t index = 0;
        std::string name;
        float weight = 1.0f;
        bool enabled = true;
        bool solo = false;
        bool muted = false;
        std::string blendMode;
        std::string maskName;
        bool selected = false;
        bool dragging = false;
    };

    /**
     * @brief Blend mode option
     */
    struct BlendModeOption {
        std::string name;
        int value = 0;
    };

    explicit LayerStackEditor(Editor* editor);
    ~LayerStackEditor();

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    // =========================================================================
    // Data
    // =========================================================================

    void SetLayerStack(Nova::AnimationLayerStack* stack);
    [[nodiscard]] Nova::AnimationLayerStack* GetLayerStack() const { return m_layerStack; }

    // =========================================================================
    // Layer Operations
    // =========================================================================

    /**
     * @brief Add a new layer
     */
    void AddLayer(const std::string& name);

    /**
     * @brief Remove layer at index
     */
    void RemoveLayer(size_t index);

    /**
     * @brief Duplicate layer
     */
    void DuplicateLayer(size_t index);

    /**
     * @brief Move layer
     */
    void MoveLayer(size_t fromIndex, size_t toIndex);

    /**
     * @brief Select layer
     */
    void SelectLayer(size_t index);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected layer index
     */
    [[nodiscard]] int GetSelectedLayerIndex() const { return m_selectedLayerIndex; }

    // =========================================================================
    // Layer Properties
    // =========================================================================

    void SetLayerWeight(size_t index, float weight);
    void SetLayerEnabled(size_t index, bool enabled);
    void SetLayerBlendMode(size_t index, int blendMode);
    void SetLayerMask(size_t index, const std::string& maskName);

    void SoloLayer(size_t index);
    void MuteLayer(size_t index, bool muted);
    void ClearSolo();

    // =========================================================================
    // Display
    // =========================================================================

    [[nodiscard]] std::vector<LayerItem> GetLayerItems() const;
    [[nodiscard]] std::vector<BlendModeOption> GetBlendModeOptions() const;
    [[nodiscard]] std::vector<std::string> GetAvailableMasks() const;

    // =========================================================================
    // Input
    // =========================================================================

    bool OnMouseDown(const glm::vec2& pos, int button);
    void OnMouseMove(const glm::vec2& pos);
    void OnMouseUp(const glm::vec2& pos, int button);
    bool OnKeyDown(int key);

    // =========================================================================
    // Drag & Drop
    // =========================================================================

    void BeginDragLayer(size_t index);
    void UpdateDrag(const glm::vec2& pos);
    void EndDrag();
    [[nodiscard]] bool IsDragging() const { return m_isDragging; }

    // =========================================================================
    // Visibility & Layout
    // =========================================================================

    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    void SetPanelBounds(const glm::vec2& pos, const glm::vec2& size);
    void SetLayerHeight(float height) { m_layerHeight = height; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnLayerStackChanged;
    std::function<void(size_t)> OnLayerSelected;
    std::function<void(size_t)> OnEditLayerTree;
    std::function<void(size_t)> OnEditLayerMask;

private:
    int FindLayerAtPosition(const glm::vec2& pos) const;
    int FindDropPosition(const glm::vec2& pos) const;
    glm::vec2 GetLayerPosition(size_t index) const;

    Editor* m_editor = nullptr;
    Nova::AnimationLayerStack* m_layerStack = nullptr;

    bool m_visible = true;
    bool m_initialized = false;

    // Panel layout
    glm::vec2 m_panelPos{0.0f};
    glm::vec2 m_panelSize{250.0f, 400.0f};
    float m_layerHeight = 60.0f;

    // Selection
    int m_selectedLayerIndex = -1;

    // Drag state
    bool m_isDragging = false;
    int m_draggingLayerIndex = -1;
    glm::vec2 m_dragOffset{0.0f};
    int m_dropTargetIndex = -1;
};

} // namespace Vehement
