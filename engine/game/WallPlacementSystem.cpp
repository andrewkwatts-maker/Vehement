#include "WallPlacementSystem.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {
namespace Buildings {

size_t WallPlacementController::s_nextCornerId = 0;
size_t WallPlacementController::s_nextSegmentId = 0;

// =============================================================================
// WallSegment Implementation
// =============================================================================

std::vector<glm::vec3> WallSegment::GenerateWallPath(int subdivisions) const {
    std::vector<glm::vec3> path;

    if (curveType == CurveType::Straight) {
        // Simple straight line - will need corner positions from caller
        path.push_back(glm::vec3(0, 0, 0)); // Placeholder
        path.push_back(glm::vec3(1, 0, 0)); // Placeholder
    } else if (curveType == CurveType::Bezier && controlPoints.size() >= 2) {
        // Cubic Bezier curve
        glm::vec3 p0(0, 0, 0); // Start (from corner)
        glm::vec3 p3(1, 0, 0); // End (from corner)
        glm::vec3 p1 = controlPoints[0];
        glm::vec3 p2 = controlPoints[1];

        for (int i = 0; i <= subdivisions; ++i) {
            float t = static_cast<float>(i) / subdivisions;
            float t2 = t * t;
            float t3 = t2 * t;
            float mt = 1.0f - t;
            float mt2 = mt * mt;
            float mt3 = mt2 * mt;

            glm::vec3 point = mt3 * p0 + 3.0f * mt2 * t * p1 + 3.0f * mt * t2 * p2 + t3 * p3;
            path.push_back(point);
        }
    }

    return path;
}

nlohmann::json WallSegment::Serialize() const {
    nlohmann::json json;
    json["id"] = id;
    json["startCornerId"] = startCornerId;
    json["endCornerId"] = endCornerId;
    json["curveType"] = static_cast<int>(curveType);

    nlohmann::json controlPointsJson = nlohmann::json::array();
    for (const auto& cp : controlPoints) {
        controlPointsJson.push_back({cp.x, cp.y, cp.z});
    }
    json["controlPoints"] = controlPointsJson;

    json["curvature"] = curvature;
    json["baseHeight"] = baseHeight;
    json["baseThickness"] = baseThickness;
    json["styleVariant"] = styleVariant;

    if (gate) {
        nlohmann::json gateJson;
        gateJson["gateComponentId"] = gate->gateComponentId;
        gateJson["positionAlongWall"] = gate->positionAlongWall;
        gateJson["offset"] = {gate->offset.x, gate->offset.y, gate->offset.z};
        json["gate"] = gateJson;
    }

    return json;
}

WallSegment WallSegment::Deserialize(const nlohmann::json& json) {
    WallSegment segment;
    segment.id = json.value("id", "");
    segment.startCornerId = json.value("startCornerId", "");
    segment.endCornerId = json.value("endCornerId", "");
    segment.curveType = static_cast<CurveType>(json.value("curveType", 0));

    if (json.contains("controlPoints")) {
        for (const auto& cpJson : json["controlPoints"]) {
            segment.controlPoints.push_back(glm::vec3(cpJson[0], cpJson[1], cpJson[2]));
        }
    }

    segment.curvature = json.value("curvature", 0.0f);
    segment.baseHeight = json.value("baseHeight", 3.0f);
    segment.baseThickness = json.value("baseThickness", 0.5f);
    segment.styleVariant = json.value("styleVariant", 0);

    if (json.contains("gate")) {
        WallSegment::GateAttachment gateAttachment;
        gateAttachment.gateComponentId = json["gate"].value("gateComponentId", "");
        gateAttachment.positionAlongWall = json["gate"].value("positionAlongWall", 0.5f);
        auto offsetJson = json["gate"]["offset"];
        gateAttachment.offset = glm::vec3(offsetJson[0], offsetJson[1], offsetJson[2]);
        segment.gate = gateAttachment;
    }

    return segment;
}

// =============================================================================
// WallSystemConfig Implementation
// =============================================================================

WallSystemConfig WallSystemConfig::GetConfigForLevel(int level) {
    WallSystemConfig config;
    config.buildingLevel = level;

    // Scale properties with level
    if (level <= 2) {
        config.maxCorners = 4;
        config.wallHeight = 2.5f;
        config.wallThickness = 0.4f;
        config.availableStyles = 1;
        config.allowCurvedWalls = false;
        config.wallMaterial = "wood";
        config.baseColor = glm::vec3(0.55f, 0.45f, 0.35f);
    } else if (level <= 4) {
        config.maxCorners = 6;
        config.wallHeight = 3.5f;
        config.wallThickness = 0.6f;
        config.availableStyles = 2;
        config.allowCurvedWalls = true;
        config.maxControlPointsPerSegment = 2;
        config.wallMaterial = "stone";
        config.baseColor = glm::vec3(0.6f, 0.6f, 0.6f);
    } else if (level <= 7) {
        config.maxCorners = 8;
        config.wallHeight = 4.5f;
        config.wallThickness = 0.8f;
        config.availableStyles = 3;
        config.allowCurvedWalls = true;
        config.maxControlPointsPerSegment = 3;
        config.wallMaterial = "reinforced_stone";
        config.baseColor = glm::vec3(0.55f, 0.55f, 0.6f);
    } else {
        config.maxCorners = 12;
        config.wallHeight = 6.0f;
        config.wallThickness = 1.0f;
        config.availableStyles = 4;
        config.allowCurvedWalls = true;
        config.maxControlPointsPerSegment = 4;
        config.wallMaterial = "fortified_stone";
        config.baseColor = glm::vec3(0.5f, 0.5f, 0.55f);
    }

    config.minInternalArea = 20.0f + (level * 5.0f);
    config.maxInternalArea = 100.0f + (level * 30.0f);

    return config;
}

// =============================================================================
// WallPlacementController Implementation
// =============================================================================

WallPlacementController::WallPlacementController(BuildingInstancePtr building)
    : m_building(building) {
    m_config = WallSystemConfig::GetConfigForLevel(building->GetLevel());
}

void WallPlacementController::SetBuildingLevel(int level) {
    m_config = WallSystemConfig::GetConfigForLevel(level);
    UpdateSegmentProperties();
}

void WallPlacementController::StartPlacingCorners() {
    m_mode = PlacementMode::PlacingCorners;
    m_corners.clear();
    m_segments.clear();
}

bool WallPlacementController::CanPlaceCorner() const {
    return m_mode == PlacementMode::PlacingCorners &&
           GetCornerCount() < m_config.maxCorners;
}

bool WallPlacementController::PlaceCorner(const glm::vec3& position) {
    if (!CanPlaceCorner()) {
        return false;
    }

    std::vector<std::string> errors;
    if (!CheckCornerValidity(position, errors)) {
        return false;
    }

    WallCorner corner;
    corner.position = ClampToBuildingBounds(position);
    corner.id = "corner_" + std::to_string(s_nextCornerId++);

    m_corners.push_back(corner);

    // Update segments if we have at least 2 corners
    if (m_corners.size() >= 2) {
        RegenerateSegments();
    }

    return true;
}

void WallPlacementController::RemoveLastCorner() {
    if (!m_corners.empty()) {
        m_corners.pop_back();
        RegenerateSegments();
    }
}

void WallPlacementController::FinishCornerPlacement() {
    if (m_corners.size() >= m_config.minCorners) {
        m_mode = PlacementMode::EditingCurve;

        // Create final segment connecting last to first corner
        RegenerateSegments();
    }
}

void WallPlacementController::StartEditingSegmentCurve(const std::string& segmentId) {
    if (!m_config.allowCurvedWalls) return;

    m_mode = PlacementMode::EditingCurve;
    m_editingSegmentId = segmentId;
}

void WallPlacementController::SetSegmentCurvature(const std::string& segmentId, float curvature) {
    auto* segment = GetSegment(segmentId);
    if (!segment) return;

    segment->curvature = glm::clamp(curvature, -1.0f, 1.0f);

    // Generate control points based on curvature
    if (m_config.allowCurvedWalls && std::abs(curvature) > 0.01f) {
        // Find corner positions
        auto startCorner = std::find_if(m_corners.begin(), m_corners.end(),
            [&](const WallCorner& c) { return c.id == segment->startCornerId; });
        auto endCorner = std::find_if(m_corners.begin(), m_corners.end(),
            [&](const WallCorner& c) { return c.id == segment->endCornerId; });

        if (startCorner != m_corners.end() && endCorner != m_corners.end()) {
            glm::vec3 start = startCorner->position;
            glm::vec3 end = endCorner->position;
            glm::vec3 mid = (start + end) * 0.5f;

            // Calculate perpendicular offset
            glm::vec3 dir = glm::normalize(end - start);
            glm::vec3 perpendicular(-dir.z, 0, dir.x);
            float offset = glm::length(end - start) * 0.3f * curvature;

            // Create control points for Bezier curve
            segment->curveType = WallSegment::CurveType::Bezier;
            segment->controlPoints.clear();
            segment->controlPoints.push_back(start + dir * 0.33f + perpendicular * offset * 0.5f);
            segment->controlPoints.push_back(end - dir * 0.33f + perpendicular * offset * 0.5f);
        }
    } else {
        segment->curveType = WallSegment::CurveType::Straight;
        segment->controlPoints.clear();
    }
}

void WallPlacementController::AddControlPoint(const std::string& segmentId, const glm::vec3& point) {
    auto* segment = GetSegment(segmentId);
    if (!segment) return;

    if (static_cast<int>(segment->controlPoints.size()) < m_config.maxControlPointsPerSegment) {
        segment->controlPoints.push_back(point);
        segment->curveType = WallSegment::CurveType::Bezier;
    }
}

void WallPlacementController::FinishCurveEditing() {
    m_mode = PlacementMode::Finished;
    m_editingSegmentId.clear();
}

void WallPlacementController::SelectCorner(const std::string& cornerId) {
    m_selectedCornerId = cornerId;
    m_mode = PlacementMode::EditingCorner;
}

void WallPlacementController::MoveSelectedCorner(const glm::vec3& newPosition) {
    auto it = std::find_if(m_corners.begin(), m_corners.end(),
        [&](const WallCorner& c) { return c.id == m_selectedCornerId; });

    if (it != m_corners.end()) {
        it->position = ClampToBuildingBounds(newPosition);
        RegenerateSegments();
    }
}

void WallPlacementController::DeleteSelectedCorner() {
    if (m_corners.size() <= m_config.minCorners) {
        return; // Can't delete if at minimum
    }

    m_corners.erase(
        std::remove_if(m_corners.begin(), m_corners.end(),
            [&](const WallCorner& c) { return c.id == m_selectedCornerId; }),
        m_corners.end()
    );

    m_selectedCornerId.clear();
    RegenerateSegments();
}

WallSegment* WallPlacementController::GetSegment(const std::string& segmentId) {
    auto it = std::find_if(m_segments.begin(), m_segments.end(),
        [&](const WallSegment& s) { return s.id == segmentId; });
    return it != m_segments.end() ? &(*it) : nullptr;
}

bool WallPlacementController::ValidateWallSystem(std::vector<std::string>& errors) const {
    errors.clear();

    // Check minimum corners
    if (m_corners.size() < static_cast<size_t>(m_config.minCorners)) {
        errors.push_back("Need at least " + std::to_string(m_config.minCorners) + " corners");
        return false;
    }

    // Check area constraints
    if (!CheckAreaConstraints(errors)) {
        return false;
    }

    // Check for self-intersection
    if (PolygonAreaCalculator::HasSelfIntersection(m_corners)) {
        errors.push_back("Wall polygon self-intersects");
        return false;
    }

    return errors.empty();
}

float WallPlacementController::CalculateInternalArea() const {
    return PolygonAreaCalculator::CalculateArea(m_corners);
}

bool WallPlacementController::IsPointInsideWalls(const glm::vec3& point) const {
    return PolygonAreaCalculator::IsPointInside(point, m_corners);
}

void WallPlacementController::UpdatePreview(const glm::vec3& mousePosition) {
    m_preview.nextCornerPosition = ClampToBuildingBounds(mousePosition);
    m_preview.errors.clear();

    m_preview.valid = CheckCornerValidity(m_preview.nextCornerPosition, m_preview.errors);

    m_preview.glowColor = m_preview.valid ?
        glm::vec4(0.0f, 1.0f, 0.0f, 0.5f) :
        glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
}

void WallPlacementController::RegenerateSegments() {
    m_segments.clear();

    if (m_corners.size() < 2) return;

    // Create segments between consecutive corners
    for (size_t i = 0; i < m_corners.size(); ++i) {
        WallSegment segment;
        segment.id = "segment_" + std::to_string(s_nextSegmentId++);
        segment.startCornerId = m_corners[i].id;
        segment.endCornerId = m_corners[(i + 1) % m_corners.size()].id;
        segment.baseHeight = m_config.wallHeight;
        segment.baseThickness = m_config.wallThickness;
        m_segments.push_back(segment);
    }
}

void WallPlacementController::UpdateSegmentProperties() {
    for (auto& segment : m_segments) {
        segment.baseHeight = m_config.wallHeight;
        segment.baseThickness = m_config.wallThickness;
    }
}

bool WallPlacementController::CheckCornerValidity(const glm::vec3& position,
                                                  std::vector<std::string>& errors) const {
    // Check if within building bounds
    glm::vec3 clamped = ClampToBuildingBounds(position);
    if (glm::distance(position, clamped) > 0.01f) {
        errors.push_back("Corner outside building bounds");
        return false;
    }

    // Check minimum distance from other corners
    const float minDistance = 1.0f;
    for (const auto& corner : m_corners) {
        if (glm::distance(position, corner.position) < minDistance) {
            errors.push_back("Too close to existing corner");
            return false;
        }
    }

    return true;
}

bool WallPlacementController::CheckAreaConstraints(std::vector<std::string>& errors) const {
    float area = CalculateInternalArea();

    if (area < m_config.minInternalArea) {
        errors.push_back("Internal area too small: " + std::to_string(area) +
                        " < " + std::to_string(m_config.minInternalArea));
        return false;
    }

    if (area > m_config.maxInternalArea) {
        errors.push_back("Internal area too large: " + std::to_string(area) +
                        " > " + std::to_string(m_config.maxInternalArea));
        return false;
    }

    return true;
}

glm::vec3 WallPlacementController::ClampToBuildingBounds(const glm::vec3& position) const {
    // Get building footprint bounds
    glm::vec3 minBounds = m_building->GetTotalBoundsMin();
    glm::vec3 maxBounds = m_building->GetTotalBoundsMax();

    glm::vec3 clamped = position;
    clamped.x = glm::clamp(clamped.x, minBounds.x, maxBounds.x);
    clamped.y = 0.0f; // Walls on ground
    clamped.z = glm::clamp(clamped.z, minBounds.z, maxBounds.z);

    return clamped;
}

// =============================================================================
// PolygonAreaCalculator Implementation
// =============================================================================

float PolygonAreaCalculator::CalculateArea(const std::vector<WallCorner>& corners) {
    if (corners.size() < 3) return 0.0f;

    // Shoelace formula
    float area = 0.0f;
    for (size_t i = 0; i < corners.size(); ++i) {
        size_t j = (i + 1) % corners.size();
        area += corners[i].position.x * corners[j].position.z;
        area -= corners[j].position.x * corners[i].position.z;
    }

    return std::abs(area) * 0.5f;
}

bool PolygonAreaCalculator::IsPointInside(const glm::vec3& point,
                                         const std::vector<WallCorner>& corners) {
    if (corners.size() < 3) return false;

    // Ray casting algorithm
    int intersections = 0;
    for (size_t i = 0; i < corners.size(); ++i) {
        size_t j = (i + 1) % corners.size();

        glm::vec3 p1 = corners[i].position;
        glm::vec3 p2 = corners[j].position;

        if ((p1.z > point.z) != (p2.z > point.z)) {
            float xIntersect = (p2.x - p1.x) * (point.z - p1.z) / (p2.z - p1.z) + p1.x;
            if (point.x < xIntersect) {
                intersections++;
            }
        }
    }

    return (intersections % 2) == 1;
}

bool PolygonAreaCalculator::IsConvex(const std::vector<WallCorner>& corners) {
    if (corners.size() < 3) return false;

    bool hasPositive = false;
    bool hasNegative = false;

    for (size_t i = 0; i < corners.size(); ++i) {
        size_t j = (i + 1) % corners.size();
        size_t k = (i + 2) % corners.size();

        glm::vec3 v1 = corners[j].position - corners[i].position;
        glm::vec3 v2 = corners[k].position - corners[j].position;

        float cross = v1.x * v2.z - v1.z * v2.x;

        if (cross > 0) hasPositive = true;
        if (cross < 0) hasNegative = true;

        if (hasPositive && hasNegative) return false;
    }

    return true;
}

glm::vec3 PolygonAreaCalculator::GetCentroid(const std::vector<WallCorner>& corners) {
    if (corners.empty()) return glm::vec3(0, 0, 0);

    glm::vec3 centroid(0, 0, 0);
    for (const auto& corner : corners) {
        centroid += corner.position;
    }

    return centroid / static_cast<float>(corners.size());
}

bool PolygonAreaCalculator::HasSelfIntersection(const std::vector<WallCorner>& corners) {
    if (corners.size() < 4) return false;

    // Check each edge against all non-adjacent edges
    for (size_t i = 0; i < corners.size(); ++i) {
        size_t j = (i + 1) % corners.size();

        for (size_t k = i + 2; k < corners.size(); ++k) {
            if (k == corners.size() - 1 && i == 0) continue; // Skip adjacent edges

            size_t l = (k + 1) % corners.size();

            // Check if segments (i,j) and (k,l) intersect
            glm::vec2 p1(corners[i].position.x, corners[i].position.z);
            glm::vec2 p2(corners[j].position.x, corners[j].position.z);
            glm::vec2 p3(corners[k].position.x, corners[k].position.z);
            glm::vec2 p4(corners[l].position.x, corners[l].position.z);

            // Line intersection test (simplified)
            float d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
            if (std::abs(d) < 0.001f) continue; // Parallel

            float t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / d;
            float u = -((p1.x - p2.x) * (p1.y - p3.y) - (p1.y - p2.y) * (p1.x - p3.x)) / d;

            if (t > 0 && t < 1 && u > 0 && u < 1) {
                return true; // Intersection found
            }
        }
    }

    return false;
}

// =============================================================================
// WallGateComponent Implementation
// =============================================================================

WallGateComponent::WallGateComponent(const std::string& id, const std::string& name)
    : BuildingComponent(id, name) {
    SetCategory("Gate");
}

bool WallGateComponent::CanAttachToWall(const WallSegment& segment) const {
    // Check if the wall segment length can accommodate the gate width
    // This is a simplified check - actual implementation would need corner positions
    return segment.baseHeight >= m_height;
}

glm::vec3 WallGateComponent::CalculateAttachmentPosition(const WallSegment& segment,
                                                          float positionAlongWall,
                                                          const std::vector<WallCorner>& corners) const {
    // Find start and end corners
    glm::vec3 start(0, 0, 0), end(0, 0, 0);

    for (const auto& corner : corners) {
        if (corner.id == segment.startCornerId) {
            start = corner.position;
        }
        if (corner.id == segment.endCornerId) {
            end = corner.position;
        }
    }

    // Interpolate position along wall
    return glm::mix(start, end, positionAlongWall);
}

} // namespace Buildings
} // namespace Nova
