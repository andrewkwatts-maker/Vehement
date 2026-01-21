#pragma once

#include "BuildingComponentSystem.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Nova {
namespace Buildings {

/**
 * @brief Represents a single corner point in the wall system
 */
struct WallCorner {
    glm::vec3 position{0, 0, 0};
    std::string id;  // Unique identifier

    // Optional per-corner customization
    float heightMultiplier = 1.0f;
    float thicknessMultiplier = 1.0f;
};

/**
 * @brief Represents a wall segment connecting two corners
 */
struct WallSegment {
    std::string id;
    std::string startCornerId;
    std::string endCornerId;

    // Spline control for curved walls
    enum class CurveType {
        Straight,      // No curve
        Bezier,        // Cubic Bezier curve
        Catmull        // Catmull-Rom spline
    };

    CurveType curveType = CurveType::Straight;
    std::vector<glm::vec3> controlPoints;  // Additional points for curves
    float curvature = 0.0f;  // -1.0 to 1.0, affects control point offset

    // Wall properties
    float baseHeight = 3.0f;
    float baseThickness = 0.5f;
    int styleVariant = 0;  // Different visual styles per level

    // Gate attachment
    struct GateAttachment {
        std::string gateComponentId;
        float positionAlongWall = 0.5f;  // 0.0 to 1.0
        glm::vec3 offset{0, 0, 0};
    };
    std::optional<GateAttachment> gate;

    // Generate mesh points along segment
    std::vector<glm::vec3> GenerateWallPath(int subdivisions = 10) const;

    nlohmann::json Serialize() const;
    static WallSegment Deserialize(const nlohmann::json& json);
};

/**
 * @brief Wall system configuration based on building level
 */
struct WallSystemConfig {
    int buildingLevel;

    // Corner constraints
    int minCorners = 3;
    int maxCorners = 4;

    // Wall properties by level
    float wallHeight = 3.0f;
    float wallThickness = 0.5f;
    int availableStyles = 1;  // Number of style variants unlocked

    // Area requirements
    float minInternalArea = 20.0f;  // Minimum enclosed area
    float maxInternalArea = 200.0f; // Maximum based on building footprint

    // Curve support
    bool allowCurvedWalls = false;
    int maxControlPointsPerSegment = 2;

    // Material/appearance
    std::string wallMaterial = "stone";
    glm::vec3 baseColor{0.6f, 0.6f, 0.6f};

    static WallSystemConfig GetConfigForLevel(int level);
};

/**
 * @brief Manages wall corner placement and editing
 */
class WallPlacementController {
public:
    enum class PlacementMode {
        PlacingCorners,      // Placing corner points
        EditingCurve,        // Adjusting wall curve between corners
        EditingCorner,       // Moving existing corner
        Finished             // Walls finalized
    };

    WallPlacementController(BuildingInstancePtr building);

    // Configuration
    void SetBuildingLevel(int level);
    int GetBuildingLevel() const { return m_config.buildingLevel; }
    const WallSystemConfig& GetConfig() const { return m_config; }

    // Corner placement
    void StartPlacingCorners();
    bool CanPlaceCorner() const;
    bool PlaceCorner(const glm::vec3& position);
    void RemoveLastCorner();
    void FinishCornerPlacement();

    const std::vector<WallCorner>& GetCorners() const { return m_corners; }
    int GetCornerCount() const { return static_cast<int>(m_corners.size()); }

    // Curve editing
    void StartEditingSegmentCurve(const std::string& segmentId);
    void SetSegmentCurvature(const std::string& segmentId, float curvature);
    void AddControlPoint(const std::string& segmentId, const glm::vec3& point);
    void FinishCurveEditing();

    // Corner editing
    void SelectCorner(const std::string& cornerId);
    void MoveSelectedCorner(const glm::vec3& newPosition);
    void DeleteSelectedCorner();

    // Segments
    const std::vector<WallSegment>& GetSegments() const { return m_segments; }
    WallSegment* GetSegment(const std::string& segmentId);

    // Validation
    bool ValidateWallSystem(std::vector<std::string>& errors) const;
    float CalculateInternalArea() const;
    bool IsPointInsideWalls(const glm::vec3& point) const;

    // Preview
    struct PreviewState {
        glm::vec3 nextCornerPosition{0, 0, 0};
        bool valid = false;
        std::vector<std::string> errors;
        glm::vec4 glowColor{0.0f, 1.0f, 0.0f, 0.5f};
    };
    void UpdatePreview(const glm::vec3& mousePosition);
    const PreviewState& GetPreview() const { return m_preview; }

    // State
    PlacementMode GetMode() const { return m_mode; }
    bool IsFinished() const { return m_mode == PlacementMode::Finished; }

    // Serialization
    nlohmann::json Serialize() const;
    static std::shared_ptr<WallPlacementController> Deserialize(const nlohmann::json& json,
                                                                BuildingInstancePtr building);

private:
    BuildingInstancePtr m_building;
    WallSystemConfig m_config;
    PlacementMode m_mode = PlacementMode::PlacingCorners;

    std::vector<WallCorner> m_corners;
    std::vector<WallSegment> m_segments;

    std::string m_selectedCornerId;
    std::string m_editingSegmentId;

    PreviewState m_preview;

    // Helper methods
    void RegenerateSegments();
    void UpdateSegmentProperties();
    bool CheckCornerValidity(const glm::vec3& position, std::vector<std::string>& errors) const;
    bool CheckAreaConstraints(std::vector<std::string>& errors) const;
    glm::vec3 ClampToBuildingBounds(const glm::vec3& position) const;

    static size_t s_nextCornerId;
    static size_t s_nextSegmentId;
};

/**
 * @brief Visual renderer for wall placement system
 */
class WallPlacementVisualizer {
public:
    WallPlacementVisualizer() = default;

    // Render wall system
    void RenderWalls(const std::vector<WallSegment>& segments,
                    const WallSystemConfig& config,
                    float alpha = 1.0f);

    // Render corners
    void RenderCorners(const std::vector<WallCorner>& corners,
                      const std::string& selectedCornerId = "",
                      bool showLabels = true);

    // Render preview
    void RenderPreview(const WallPlacementController::PreviewState& preview,
                      const std::vector<WallCorner>& existingCorners);

    // Render curve editing handles
    void RenderCurveHandles(const WallSegment& segment,
                           const std::vector<WallCorner>& corners);

    // Render area visualization
    void RenderAreaBounds(const std::vector<WallCorner>& corners,
                         float internalArea,
                         bool valid = true);

    // Render grid snapped to building bounds
    void RenderBuildingBounds(const glm::vec3& min, const glm::vec3& max);

    // Configuration
    void SetWireframeMode(bool enabled) { m_wireframeMode = enabled; }
    void SetShowDimensions(bool show) { m_showDimensions = show; }
    void SetCornerSize(float size) { m_cornerSize = size; }

private:
    bool m_wireframeMode = false;
    bool m_showDimensions = true;
    float m_cornerSize = 0.3f;

    // Helper rendering methods
    void RenderWallMesh(const std::vector<glm::vec3>& path,
                       float height, float thickness,
                       const WallSystemConfig& config);
    void RenderBezierCurve(const glm::vec3& p0, const glm::vec3& p1,
                          const glm::vec3& c0, const glm::vec3& c1,
                          int subdivisions = 20);
};

/**
 * @brief Gate component that attaches to walls
 */
class WallGateComponent : public BuildingComponent {
public:
    WallGateComponent(const std::string& id, const std::string& name);

    // Gate-specific properties
    float GetWidth() const { return m_width; }
    void SetWidth(float width) { m_width = width; }

    float GetHeight() const { return m_height; }
    void SetHeight(float height) { m_height = height; }

    // Must attach to wall segment
    bool CanAttachToWall(const WallSegment& segment) const;
    glm::vec3 CalculateAttachmentPosition(const WallSegment& segment,
                                         float positionAlongWall,
                                         const std::vector<WallCorner>& corners) const;

private:
    float m_width = 2.0f;
    float m_height = 3.0f;
};

/**
 * @brief Helper for polygon area calculation
 */
class PolygonAreaCalculator {
public:
    // Calculate area of polygon defined by corners (using shoelace formula)
    static float CalculateArea(const std::vector<WallCorner>& corners);

    // Check if point is inside polygon
    static bool IsPointInside(const glm::vec3& point, const std::vector<WallCorner>& corners);

    // Check if polygon is convex
    static bool IsConvex(const std::vector<WallCorner>& corners);

    // Get polygon centroid
    static glm::vec3 GetCentroid(const std::vector<WallCorner>& corners);

    // Check if polygon self-intersects
    static bool HasSelfIntersection(const std::vector<WallCorner>& corners);
};

} // namespace Buildings
} // namespace Nova
