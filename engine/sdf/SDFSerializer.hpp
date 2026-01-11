#pragma once

#include "SDFPrimitive.hpp"
#include "SDFModel.hpp"
#include "SDFAnimation.hpp"
#include <string>
#include <memory>

namespace Nova {

/**
 * @brief JSON serializer for SDF models, poses, and animations
 *
 * Handles serialization to/from the game's unit/building/hero JSON format
 */
class SDFSerializer {
public:
    // =========================================================================
    // SDF Model Serialization
    // =========================================================================

    /**
     * @brief Serialize SDFModel to JSON string
     */
    static std::string ModelToJson(const SDFModel& model);

    /**
     * @brief Deserialize SDFModel from JSON string
     */
    static std::unique_ptr<SDFModel> ModelFromJson(const std::string& json);

    /**
     * @brief Save SDFModel to file
     */
    static bool SaveModel(const SDFModel& model, const std::string& path);

    /**
     * @brief Load SDFModel from file
     */
    static std::unique_ptr<SDFModel> LoadModel(const std::string& path);

    // =========================================================================
    // Primitive Serialization
    // =========================================================================

    /**
     * @brief Serialize SDFPrimitive to JSON string
     */
    static std::string PrimitiveToJson(const SDFPrimitive& primitive);

    /**
     * @brief Deserialize SDFPrimitive from JSON string
     */
    static std::unique_ptr<SDFPrimitive> PrimitiveFromJson(const std::string& json);

    // =========================================================================
    // Pose Serialization
    // =========================================================================

    /**
     * @brief Serialize pose to JSON string
     */
    static std::string PoseToJson(const SDFPose& pose);

    /**
     * @brief Deserialize pose from JSON string
     */
    static SDFPose PoseFromJson(const std::string& json);

    /**
     * @brief Save pose library to file
     */
    static bool SavePoseLibrary(const SDFPoseLibrary& library, const std::string& path);

    /**
     * @brief Load pose library from file
     */
    static bool LoadPoseLibrary(SDFPoseLibrary& library, const std::string& path);

    // =========================================================================
    // Animation Serialization
    // =========================================================================

    /**
     * @brief Serialize animation clip to JSON string
     */
    static std::string AnimationClipToJson(const SDFAnimationClip& clip);

    /**
     * @brief Deserialize animation clip from JSON string
     */
    static std::unique_ptr<SDFAnimationClip> AnimationClipFromJson(const std::string& json);

    /**
     * @brief Save animation clip to file
     */
    static bool SaveAnimationClip(const SDFAnimationClip& clip, const std::string& path);

    /**
     * @brief Load animation clip from file
     */
    static std::unique_ptr<SDFAnimationClip> LoadAnimationClip(const std::string& path);

    // =========================================================================
    // State Machine Serialization
    // =========================================================================

    /**
     * @brief Serialize state machine to JSON string
     */
    static std::string StateMachineToJson(const SDFAnimationStateMachine& stateMachine);

    /**
     * @brief Deserialize state machine from JSON string (clips must be loaded separately)
     */
    static std::unique_ptr<SDFAnimationStateMachine> StateMachineFromJson(
        const std::string& json,
        const std::unordered_map<std::string, std::shared_ptr<SDFAnimationClip>>& clips);

    // =========================================================================
    // Game Entity Integration
    // =========================================================================

    /**
     * @brief Create SDF definition section for unit/building/hero JSON
     *
     * Returns JSON object content for embedding in game entity config:
     * {
     *   "sdf_model": { ... },
     *   "sdf_poses": [ ... ],
     *   "sdf_animations": [ ... ],
     *   "sdf_state_machine": { ... }
     * }
     */
    static std::string CreateEntitySDFSection(
        const SDFModel& model,
        const SDFPoseLibrary* poseLibrary = nullptr,
        const std::vector<const SDFAnimationClip*>& animations = {},
        const SDFAnimationStateMachine* stateMachine = nullptr);

    /**
     * @brief Parse SDF definition from unit/building/hero JSON
     */
    struct EntitySDFData {
        std::unique_ptr<SDFModel> model;
        std::unique_ptr<SDFPoseLibrary> poseLibrary;
        std::vector<std::unique_ptr<SDFAnimationClip>> animations;
        std::unique_ptr<SDFAnimationStateMachine> stateMachine;
    };

    static EntitySDFData ParseEntitySDFSection(const std::string& json);

    /**
     * @brief Update existing entity JSON with SDF data
     *
     * Reads existing JSON file, adds/updates SDF section, writes back
     */
    static bool UpdateEntityJson(
        const std::string& jsonPath,
        const SDFModel& model,
        const SDFPoseLibrary* poseLibrary = nullptr,
        const std::vector<const SDFAnimationClip*>& animations = {},
        const SDFAnimationStateMachine* stateMachine = nullptr);

    /**
     * @brief Extract SDF data from entity JSON file
     */
    static EntitySDFData LoadEntitySDF(const std::string& jsonPath);

    // Helper functions (public for use by serialization helpers)
    static std::string TransformToJson(const SDFTransform& transform);
    static SDFTransform TransformFromJson(const std::string& json);

    static std::string MaterialToJson(const SDFMaterial& material);
    static SDFMaterial MaterialFromJson(const std::string& json);

    static std::string ParametersToJson(const SDFParameters& params);
    static SDFParameters ParametersFromJson(const std::string& json);

    static std::string Vec3ToJson(const glm::vec3& v);
    static glm::vec3 Vec3FromJson(const std::string& json);

    static std::string Vec4ToJson(const glm::vec4& v);
    static glm::vec4 Vec4FromJson(const std::string& json);

    static std::string QuatToJson(const glm::quat& q);
    static glm::quat QuatFromJson(const std::string& json);

    static const char* PrimitiveTypeToString(SDFPrimitiveType type);
    static SDFPrimitiveType PrimitiveTypeFromString(const std::string& str);

    static const char* CSGOperationToString(CSGOperation op);
    static CSGOperation CSGOperationFromString(const std::string& str);
};

} // namespace Nova
