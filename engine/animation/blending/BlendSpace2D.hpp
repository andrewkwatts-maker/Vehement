#pragma once

#include "BlendNode.hpp"
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <functional>

namespace Nova {

class Animation;

/**
 * @brief 2D Blend Space for two-parameter animation blending
 *
 * Blends between animations positioned in a 2D parameter space.
 * Uses Delaunay triangulation to determine blend weights.
 *
 * Common use cases:
 * - Directional locomotion (X: strafe, Y: forward/back)
 * - Speed + turn rate blending
 * - Aiming (X: horizontal, Y: vertical)
 */
class BlendSpace2D {
public:
    /**
     * @brief Sample point in the 2D blend space
     */
    struct Sample {
        std::string clipId;
        const Animation* clip = nullptr;
        glm::vec2 position{0.0f};           // Position in parameter space
        float playbackSpeed = 1.0f;          // Speed multiplier

        // Motion data
        glm::vec2 motionDirection{0.0f, 1.0f}; // Direction of motion
        float motionSpeed = 0.0f;              // Motion speed
        float angularSpeed = 0.0f;             // Rotation speed
    };

    /**
     * @brief Triangle for triangulation
     */
    struct Triangle {
        std::array<size_t, 3> indices;
        glm::vec2 circumcenter;
        float circumradiusSq;
    };

    /**
     * @brief Blend result
     */
    struct BlendResult {
        AnimationPose pose;
        glm::vec3 rootMotionDelta{0.0f};
        glm::quat rootRotationDelta{1.0f, 0.0f, 0.0f, 0.0f};
        float normalizedTime = 0.0f;

        // Active sample weights
        std::vector<std::pair<size_t, float>> activeWeights;
    };

    /**
     * @brief Blend mode for the 2D space
     */
    enum class BlendMode {
        Directional,        // Standard directional blending
        FreeformDirectional, // Freeform with gradient bands
        FreeformCartesian    // Cartesian gradient computation
    };

    BlendSpace2D() = default;
    explicit BlendSpace2D(const std::string& name);

    // =========================================================================
    // Configuration
    // =========================================================================

    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    void SetParameterX(const std::string& name) { m_parameterX = name; }
    void SetParameterY(const std::string& name) { m_parameterY = name; }
    [[nodiscard]] const std::string& GetParameterX() const { return m_parameterX; }
    [[nodiscard]] const std::string& GetParameterY() const { return m_parameterY; }

    void SetParameterRangeX(float min, float max);
    void SetParameterRangeY(float min, float max);
    [[nodiscard]] glm::vec2 GetMinBounds() const { return m_minBounds; }
    [[nodiscard]] glm::vec2 GetMaxBounds() const { return m_maxBounds; }

    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

    void SetBlendMode(BlendMode mode) { m_blendMode = mode; }
    [[nodiscard]] BlendMode GetBlendMode() const { return m_blendMode; }

    // =========================================================================
    // Sample Management
    // =========================================================================

    /**
     * @brief Add a sample to the blend space
     * @return Index of added sample
     */
    size_t AddSample(const Animation* clip, const glm::vec2& position, float playbackSpeed = 1.0f);

    /**
     * @brief Add sample with full configuration
     */
    size_t AddSample(const Sample& sample);

    /**
     * @brief Remove sample at index
     */
    void RemoveSample(size_t index);

    /**
     * @brief Clear all samples
     */
    void ClearSamples();

    /**
     * @brief Get sample count
     */
    [[nodiscard]] size_t GetSampleCount() const { return m_samples.size(); }

    /**
     * @brief Get sample at index
     */
    [[nodiscard]] Sample& GetSample(size_t index) { return m_samples[index]; }
    [[nodiscard]] const Sample& GetSample(size_t index) const { return m_samples[index]; }

    /**
     * @brief Get all samples
     */
    [[nodiscard]] const std::vector<Sample>& GetSamples() const { return m_samples; }

    /**
     * @brief Update sample position
     */
    void SetSamplePosition(size_t index, const glm::vec2& position);

    // =========================================================================
    // Triangulation
    // =========================================================================

    /**
     * @brief Rebuild the Delaunay triangulation
     */
    void RebuildTriangulation();

    /**
     * @brief Check if triangulation needs rebuild
     */
    [[nodiscard]] bool IsTriangulationDirty() const { return m_triangulationDirty; }

    /**
     * @brief Get triangulation for visualization
     */
    [[nodiscard]] const std::vector<Triangle>& GetTriangles() const { return m_triangles; }

    /**
     * @brief Get triangle containing a point
     * @return Triangle index or -1 if outside
     */
    [[nodiscard]] int FindContainingTriangle(const glm::vec2& position) const;

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Evaluate the blend space
     * @param posX X parameter value
     * @param posY Y parameter value
     * @param deltaTime Time since last evaluation
     * @return Blended result
     */
    [[nodiscard]] BlendResult Evaluate(float posX, float posY, float deltaTime);

    /**
     * @brief Preview at position without advancing time
     */
    [[nodiscard]] AnimationPose Preview(const glm::vec2& position, float normalizedTime) const;

    /**
     * @brief Get sample weights at a position
     */
    [[nodiscard]] std::vector<float> GetSampleWeights(const glm::vec2& position) const;

    /**
     * @brief Reset playback state
     */
    void Reset();

    // =========================================================================
    // Root Motion
    // =========================================================================

    void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }
    [[nodiscard]] bool IsRootMotionEnabled() const { return m_rootMotionEnabled; }

    void SetRootBoneName(const std::string& name) { m_rootBoneName = name; }

    /**
     * @brief Compute motion data for all samples
     */
    void ComputeMotionData();

    // =========================================================================
    // Preview Grid
    // =========================================================================

    /**
     * @brief Generate preview grid for visualization
     * @param gridSize Number of cells in each dimension
     * @return Grid of preview poses
     */
    [[nodiscard]] std::vector<std::vector<AnimationPose>> GeneratePreviewGrid(
        size_t gridSize, float normalizedTime) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] std::string ToJson() const;
    bool FromJson(const std::string& json);

    // =========================================================================
    // Build Blend Node
    // =========================================================================

    [[nodiscard]] std::unique_ptr<Blend2DNode> CreateBlendNode() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnLoopComplete;

private:
    // Delaunay triangulation using Bowyer-Watson algorithm
    void BowyerWatson();

    // Compute barycentric coordinates
    glm::vec3 ComputeBarycentric(const glm::vec2& p, const Triangle& tri) const;

    // Check if point is inside circumcircle
    bool IsInsideCircumcircle(const glm::vec2& p, const Triangle& tri) const;

    // Calculate circumcircle
    void CalculateCircumcircle(Triangle& tri) const;

    // Weight computation for different blend modes
    void ComputeWeightsDirectional(const glm::vec2& pos, std::vector<float>& weights) const;
    void ComputeWeightsFreeform(const glm::vec2& pos, std::vector<float>& weights) const;

    std::string m_name;
    std::string m_parameterX = "X";
    std::string m_parameterY = "Y";
    glm::vec2 m_minBounds{-1.0f, -1.0f};
    glm::vec2 m_maxBounds{1.0f, 1.0f};

    std::vector<Sample> m_samples;
    std::vector<Triangle> m_triangles;

    Skeleton* m_skeleton = nullptr;
    std::string m_rootBoneName = "root";

    // State
    float m_currentTime = 0.0f;
    float m_normalizedTime = 0.0f;

    // Settings
    bool m_rootMotionEnabled = true;
    bool m_triangulationDirty = true;
    BlendMode m_blendMode = BlendMode::Directional;
};

/**
 * @brief Builder for BlendSpace2D
 */
class BlendSpace2DBuilder {
public:
    BlendSpace2DBuilder& SetName(const std::string& name);
    BlendSpace2DBuilder& SetParameters(const std::string& paramX, const std::string& paramY);
    BlendSpace2DBuilder& SetBoundsX(float min, float max);
    BlendSpace2DBuilder& SetBoundsY(float min, float max);
    BlendSpace2DBuilder& SetSkeleton(Skeleton* skeleton);
    BlendSpace2DBuilder& AddSample(const Animation* clip, float x, float y, float speed = 1.0f);
    BlendSpace2DBuilder& SetBlendMode(BlendSpace2D::BlendMode mode);
    BlendSpace2DBuilder& EnableRootMotion(bool enabled = true);

    [[nodiscard]] std::unique_ptr<BlendSpace2D> Build();

private:
    std::unique_ptr<BlendSpace2D> m_blendSpace = std::make_unique<BlendSpace2D>();
};

} // namespace Nova
