#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Nova {

class Skeleton;

/**
 * @brief Bone mask for selective animation blending
 *
 * Allows controlling which bones are affected by an animation layer.
 * Supports per-bone weights, presets, and smooth falloff between
 * masked and unmasked regions.
 */
class BlendMask {
public:
    /**
     * @brief Preset mask types
     */
    enum class Preset {
        FullBody,           // All bones at full weight
        UpperBody,          // Spine and above
        LowerBody,          // Hips and below
        LeftArm,            // Left arm chain
        RightArm,           // Right arm chain
        LeftLeg,            // Left leg chain
        RightLeg,           // Right leg chain
        Head,               // Head and neck
        Spine,              // Spine chain
        Hands,              // Both hands
        Feet,               // Both feet
        Arms,               // Both arms
        Legs,               // Both legs
        Custom              // User-defined
    };

    /**
     * @brief Bone weight entry
     */
    struct BoneWeight {
        std::string boneName;
        int boneIndex = -1;     // Cached index
        float weight = 1.0f;
        bool includeChildren = true;
    };

    BlendMask() = default;
    explicit BlendMask(const std::string& name);
    explicit BlendMask(Preset preset);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set mask name
     */
    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    /**
     * @brief Set skeleton for bone mapping
     */
    void SetSkeleton(Skeleton* skeleton);
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

    /**
     * @brief Get preset type
     */
    [[nodiscard]] Preset GetPreset() const { return m_preset; }

    // =========================================================================
    // Bone Weight Management
    // =========================================================================

    /**
     * @brief Set weight for a specific bone
     * @param boneName Bone name
     * @param weight Weight (0-1)
     * @param includeChildren Apply to child bones
     */
    void SetBoneWeight(const std::string& boneName, float weight, bool includeChildren = false);

    /**
     * @brief Set weight by bone index
     */
    void SetBoneWeight(size_t boneIndex, float weight);

    /**
     * @brief Get weight for a bone
     */
    [[nodiscard]] float GetBoneWeight(const std::string& boneName) const;
    [[nodiscard]] float GetBoneWeight(size_t boneIndex) const;

    /**
     * @brief Check if bone is masked
     */
    [[nodiscard]] bool IsBoneMasked(const std::string& boneName) const;
    [[nodiscard]] bool IsBoneMasked(size_t boneIndex) const;

    /**
     * @brief Get all bone weights as array (for GPU upload or fast access)
     */
    [[nodiscard]] const std::vector<float>& GetWeights() const { return m_weights; }

    /**
     * @brief Clear all weights (set to 0)
     */
    void ClearWeights();

    /**
     * @brief Set all weights to 1
     */
    void SetAllWeights(float weight = 1.0f);

    // =========================================================================
    // Presets
    // =========================================================================

    /**
     * @brief Apply a preset
     */
    void ApplyPreset(Preset preset);

    /**
     * @brief Create mask from preset
     */
    static std::shared_ptr<BlendMask> CreateFromPreset(Preset preset, Skeleton* skeleton = nullptr);

    /**
     * @brief Get preset name
     */
    static const char* GetPresetName(Preset preset);

    /**
     * @brief Get all available presets
     */
    static std::vector<Preset> GetAvailablePresets();

    // =========================================================================
    // Hierarchy Operations
    // =========================================================================

    /**
     * @brief Set weight for a bone and all its descendants
     */
    void SetBranchWeight(const std::string& rootBone, float weight);

    /**
     * @brief Add feathering/falloff from specified bone
     * @param startBone Bone to start feathering from
     * @param levels Number of levels to feather
     * @param startWeight Weight at start bone
     * @param endWeight Weight at end of feather
     */
    void AddFeathering(const std::string& startBone, int levels,
                       float startWeight = 1.0f, float endWeight = 0.0f);

    /**
     * @brief Mirror mask (left to right)
     */
    void Mirror();

    // =========================================================================
    // Blending
    // =========================================================================

    /**
     * @brief Blend two masks
     */
    static std::shared_ptr<BlendMask> Blend(const BlendMask& a, const BlendMask& b, float t);

    /**
     * @brief Multiply mask weights
     */
    void Multiply(float factor);

    /**
     * @brief Add another mask (additive blend)
     */
    void Add(const BlendMask& other, float weight = 1.0f);

    /**
     * @brief Invert mask weights (1 - weight)
     */
    void Invert();

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Load from JSON
     */
    bool FromJson(const std::string& json);

    // =========================================================================
    // Runtime
    // =========================================================================

    /**
     * @brief Rebuild weight array from bone weights
     */
    void RebuildWeights();

    /**
     * @brief Check if mask is dirty (needs rebuild)
     */
    [[nodiscard]] bool IsDirty() const { return m_dirty; }

    // =========================================================================
    // Standard Bone Names
    // =========================================================================

    /**
     * @brief Standard humanoid bone names for auto-mapping
     */
    struct HumanoidBones {
        static const char* Hips;
        static const char* Spine;
        static const char* Spine1;
        static const char* Spine2;
        static const char* Neck;
        static const char* Head;

        static const char* LeftShoulder;
        static const char* LeftUpperArm;
        static const char* LeftLowerArm;
        static const char* LeftHand;

        static const char* RightShoulder;
        static const char* RightUpperArm;
        static const char* RightLowerArm;
        static const char* RightHand;

        static const char* LeftUpperLeg;
        static const char* LeftLowerLeg;
        static const char* LeftFoot;
        static const char* LeftToes;

        static const char* RightUpperLeg;
        static const char* RightLowerLeg;
        static const char* RightFoot;
        static const char* RightToes;
    };

    /**
     * @brief Try to map skeleton bones to humanoid standard
     */
    void AutoMapHumanoid();

private:
    void PropagateToChildren(int boneIndex, float weight);
    int FindChildBoneIndex(int parentIndex, int startFrom = 0) const;

    std::string m_name = "Unnamed Mask";
    Preset m_preset = Preset::Custom;
    Skeleton* m_skeleton = nullptr;

    std::vector<BoneWeight> m_boneWeights;
    std::vector<float> m_weights;  // Indexed by bone index

    std::unordered_map<std::string, size_t> m_boneNameToWeight;
    bool m_dirty = true;
};

/**
 * @brief Manager for blend mask presets
 */
class BlendMaskLibrary {
public:
    static BlendMaskLibrary& Instance();

    /**
     * @brief Register a mask preset
     */
    void RegisterMask(const std::string& name, std::shared_ptr<BlendMask> mask);

    /**
     * @brief Get mask by name
     */
    [[nodiscard]] std::shared_ptr<BlendMask> GetMask(const std::string& name) const;

    /**
     * @brief Check if mask exists
     */
    [[nodiscard]] bool HasMask(const std::string& name) const;

    /**
     * @brief Get all registered mask names
     */
    [[nodiscard]] std::vector<std::string> GetMaskNames() const;

    /**
     * @brief Remove a mask
     */
    void RemoveMask(const std::string& name);

    /**
     * @brief Clear all masks
     */
    void Clear();

    /**
     * @brief Load masks from JSON file
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Save masks to JSON file
     */
    bool SaveToFile(const std::string& path) const;

private:
    BlendMaskLibrary() = default;
    std::unordered_map<std::string, std::shared_ptr<BlendMask>> m_masks;
};

} // namespace Nova
