#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class BlendMask;
class Skeleton;
}

namespace Vehement {

class Editor;

/**
 * @brief Visual editor for blend masks
 *
 * Provides:
 * - Skeleton visualization
 * - Click bones to toggle mask
 * - Gradient falloff editor
 * - Preset management
 */
class BlendMaskEditor {
public:
    /**
     * @brief Bone display info
     */
    struct BoneDisplay {
        std::string name;
        int index = -1;
        int parentIndex = -1;
        float weight = 0.0f;
        bool selected = false;
        bool hovered = false;

        glm::vec2 screenPos{0.0f};
        glm::vec2 parentScreenPos{0.0f};
    };

    /**
     * @brief Preset info
     */
    struct PresetInfo {
        std::string name;
        Nova::BlendMask::Preset type;
    };

    explicit BlendMaskEditor(Editor* editor);
    ~BlendMaskEditor();

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

    void SetMask(Nova::BlendMask* mask);
    [[nodiscard]] Nova::BlendMask* GetMask() const { return m_mask; }

    void SetSkeleton(Nova::Skeleton* skeleton);
    [[nodiscard]] Nova::Skeleton* GetSkeleton() const { return m_skeleton; }

    // =========================================================================
    // Editing
    // =========================================================================

    /**
     * @brief Set weight for a bone
     */
    void SetBoneWeight(const std::string& boneName, float weight);
    void SetBoneWeight(int boneIndex, float weight);

    /**
     * @brief Set weight for bone and children
     */
    void SetBranchWeight(const std::string& boneName, float weight);

    /**
     * @brief Toggle bone (1.0 if 0, 0 if >0)
     */
    void ToggleBone(const std::string& boneName);
    void ToggleBone(int boneIndex);

    /**
     * @brief Select/deselect bone
     */
    void SelectBone(int boneIndex);
    void ClearSelection();

    /**
     * @brief Get selected bone index
     */
    [[nodiscard]] int GetSelectedBoneIndex() const { return m_selectedBoneIndex; }

    /**
     * @brief Apply preset
     */
    void ApplyPreset(Nova::BlendMask::Preset preset);

    /**
     * @brief Get available presets
     */
    [[nodiscard]] std::vector<PresetInfo> GetPresets() const;

    /**
     * @brief Clear all weights
     */
    void ClearAllWeights();

    /**
     * @brief Set all weights to 1
     */
    void SetAllWeights();

    /**
     * @brief Invert mask
     */
    void InvertMask();

    /**
     * @brief Mirror mask
     */
    void MirrorMask();

    // =========================================================================
    // Feathering
    // =========================================================================

    /**
     * @brief Add feathering from selected bone
     */
    void AddFeathering(int levels, float startWeight, float endWeight);

    /**
     * @brief Set feather levels
     */
    void SetFeatherLevels(int levels) { m_featherLevels = levels; }
    [[nodiscard]] int GetFeatherLevels() const { return m_featherLevels; }

    // =========================================================================
    // Display
    // =========================================================================

    [[nodiscard]] std::vector<BoneDisplay> GetBoneDisplays() const;

    /**
     * @brief Set skeleton pose for preview
     */
    void SetPoseTime(float normalizedTime);

    /**
     * @brief Set view rotation
     */
    void SetViewRotation(float yaw, float pitch);

    /**
     * @brief Set zoom
     */
    void SetZoom(float zoom);

    // =========================================================================
    // Input
    // =========================================================================

    bool OnMouseDown(const glm::vec2& pos, int button);
    void OnMouseMove(const glm::vec2& pos);
    void OnMouseUp(const glm::vec2& pos, int button);
    bool OnKeyDown(int key);

    // =========================================================================
    // Visibility & Layout
    // =========================================================================

    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    void SetCanvasBounds(const glm::vec2& pos, const glm::vec2& size);

    // =========================================================================
    // Preset Management
    // =========================================================================

    /**
     * @brief Save current mask as preset
     */
    void SaveAsPreset(const std::string& name);

    /**
     * @brief Load preset
     */
    void LoadPreset(const std::string& name);

    /**
     * @brief Get custom preset names
     */
    [[nodiscard]] std::vector<std::string> GetCustomPresetNames() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnMaskChanged;
    std::function<void(int)> OnBoneSelected;

private:
    glm::vec2 BoneToScreen(int boneIndex) const;
    int FindBoneAtPosition(const glm::vec2& pos) const;
    void UpdateBonePositions();

    Editor* m_editor = nullptr;
    Nova::BlendMask* m_mask = nullptr;
    Nova::Skeleton* m_skeleton = nullptr;

    bool m_visible = true;
    bool m_initialized = false;

    // Canvas layout
    glm::vec2 m_canvasPos{50.0f, 50.0f};
    glm::vec2 m_canvasSize{300.0f, 500.0f};
    float m_boneRadius = 10.0f;

    // View
    float m_viewYaw = 0.0f;
    float m_viewPitch = 0.0f;
    float m_zoom = 1.0f;

    // Selection
    int m_selectedBoneIndex = -1;
    int m_hoveredBoneIndex = -1;

    // Feathering
    int m_featherLevels = 2;

    // Cached bone positions
    std::vector<glm::vec2> m_boneScreenPositions;
};

} // namespace Vehement
