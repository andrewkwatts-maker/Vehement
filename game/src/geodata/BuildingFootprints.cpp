#include "BuildingFootprints.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Geo {

// =============================================================================
// ProcessedBuilding Implementation
// =============================================================================

bool ProcessedBuilding::Contains(const glm::vec2& point) const {
    return BuildingFootprints::PointInPolygon(point, outline);
}

float ProcessedBuilding::GetPerimeter() const {
    if (outline.size() < 2) return 0.0f;

    float perimeter = 0.0f;
    for (size_t i = 0; i < outline.size(); ++i) {
        size_t j = (i + 1) % outline.size();
        perimeter += glm::length(outline[j] - outline[i]);
    }
    return perimeter;
}

// =============================================================================
// BuildingHeightEstimator Implementation
// =============================================================================

float BuildingHeightEstimator::EstimateFromType(BuildingType type) {
    switch (type) {
        case BuildingType::House:
        case BuildingType::Detached:
        case BuildingType::Semidetached:
            return 8.0f;
        case BuildingType::Terrace:
            return 9.0f;
        case BuildingType::Apartments:
            return 15.0f;
        case BuildingType::Commercial:
        case BuildingType::Retail:
            return 12.0f;
        case BuildingType::Office:
            return 25.0f;
        case BuildingType::Industrial:
        case BuildingType::Warehouse:
            return 10.0f;
        case BuildingType::Hospital:
            return 20.0f;
        case BuildingType::School:
        case BuildingType::University:
            return 12.0f;
        case BuildingType::Church:
        case BuildingType::Mosque:
        case BuildingType::Temple:
            return 15.0f;
        case BuildingType::Shed:
        case BuildingType::Cabin:
            return 3.0f;
        case BuildingType::Garage:
            return 4.0f;
        default:
            return 10.0f;
    }
}

float BuildingHeightEstimator::EstimateFromArea(float area, BuildingType type) {
    float baseHeight = EstimateFromType(type);

    // Larger buildings tend to be taller (for commercial/industrial)
    if (type == BuildingType::Commercial || type == BuildingType::Office ||
        type == BuildingType::Apartments) {
        if (area > 5000.0f) {
            baseHeight *= 2.0f;
        } else if (area > 2000.0f) {
            baseHeight *= 1.5f;
        } else if (area > 1000.0f) {
            baseHeight *= 1.2f;
        }
    }

    return baseHeight;
}

float BuildingHeightEstimator::EstimateFromContext(
    const GeoBuilding& building,
    const std::vector<GeoBuilding>& neighbors) {

    if (neighbors.empty()) {
        return EstimateFromType(building.type);
    }

    // Average height of neighbors
    float avgHeight = 0.0f;
    int count = 0;

    for (const auto& neighbor : neighbors) {
        float h = neighbor.height > 0 ? neighbor.height : neighbor.GetEstimatedHeight();
        avgHeight += h;
        ++count;
    }

    avgHeight /= count;

    // Blend with type-based estimate
    float typeHeight = EstimateFromType(building.type);
    return typeHeight * 0.7f + avgHeight * 0.3f;
}

float BuildingHeightEstimator::GetFloorHeight(BuildingType type) {
    switch (type) {
        case BuildingType::Industrial:
        case BuildingType::Warehouse:
            return 5.0f;
        case BuildingType::Commercial:
        case BuildingType::Retail:
            return 4.0f;
        case BuildingType::Church:
        case BuildingType::Mosque:
        case BuildingType::Temple:
            return 6.0f;
        default:
            return 3.0f;
    }
}

int BuildingHeightEstimator::HeightToLevels(float height, BuildingType type) {
    float floorHeight = GetFloorHeight(type);
    return std::max(1, static_cast<int>(height / floorHeight));
}

// =============================================================================
// BuildingFootprints Implementation
// =============================================================================

BuildingFootprints::BuildingFootprints() {
    m_transform = [](const GeoCoordinate& coord) {
        return glm::vec2(static_cast<float>(coord.longitude),
                         static_cast<float>(coord.latitude));
    };
}

BuildingFootprints::~BuildingFootprints() = default;

void BuildingFootprints::SetDefaultTransform(const GeoCoordinate& origin, float scale) {
    m_origin = origin;
    m_scale = scale;

    m_transform = [this](const GeoCoordinate& coord) {
        double dx = (coord.longitude - m_origin.longitude) *
                    std::cos(DegToRad(m_origin.latitude)) * 111320.0;
        double dy = (coord.latitude - m_origin.latitude) * 110540.0;

        return glm::vec2(static_cast<float>(dx * m_scale),
                         static_cast<float>(dy * m_scale));
    };
}

int BuildingFootprints::ProcessBuildings(const std::vector<GeoBuilding>& buildings) {
    int count = 0;
    for (const auto& building : buildings) {
        ProcessBuilding(building);
        ++count;
    }
    return count;
}

void BuildingFootprints::ProcessBuilding(const GeoBuilding& building) {
    if (building.outline.size() < 3) return;

    ProcessedBuilding processed;
    processed.id = building.id;
    processed.name = building.name;
    processed.type = building.type;

    // Transform outline
    processed.outline.reserve(building.outline.size());
    for (const auto& geoPoint : building.outline) {
        processed.outline.push_back(TransformCoord(geoPoint));
    }

    // Transform holes
    for (const auto& hole : building.holes) {
        std::vector<glm::vec2> transformedHole;
        for (const auto& geoPoint : hole) {
            transformedHole.push_back(TransformCoord(geoPoint));
        }
        processed.holes.push_back(std::move(transformedHole));
    }

    // Height
    processed.height = building.height > 0 ? building.height * m_scale :
                       BuildingHeightEstimator::EstimateFromType(building.type) * m_scale;
    processed.minHeight = building.minHeight * m_scale;
    processed.levels = building.levels > 0 ? building.levels :
                       BuildingHeightEstimator::HeightToLevels(processed.height / m_scale, building.type);
    processed.minLevel = building.minLevel;

    // Materials and colors
    processed.material = building.material;
    processed.roofType = building.roofType;
    processed.roofHeight = building.roofHeight * m_scale;
    processed.wallColor = building.wallColor;
    processed.roofColor = building.roofColor;

    // Calculate derived properties
    processed.centroid = CalculateCentroid(processed.outline);
    processed.area = CalculatePolygonArea(processed.outline);

    processed.boundsMin = processed.outline[0];
    processed.boundsMax = processed.outline[0];
    for (const auto& p : processed.outline) {
        processed.boundsMin = glm::min(processed.boundsMin, p);
        processed.boundsMax = glm::max(processed.boundsMax, p);
    }

    // Store
    m_buildingIndex[processed.id] = m_buildings.size();
    m_buildings.push_back(std::move(processed));
}

void BuildingFootprints::EstimateHeights() {
    for (auto& building : m_buildings) {
        if (building.height <= 0.01f) {
            building.height = BuildingHeightEstimator::EstimateFromArea(
                building.area / (m_scale * m_scale), building.type) * m_scale;
            building.levels = BuildingHeightEstimator::HeightToLevels(
                building.height / m_scale, building.type);
        }
    }
}

void BuildingFootprints::ProcessAll(const std::vector<GeoBuilding>& buildings) {
    Clear();
    ProcessBuildings(buildings);
    EstimateHeights();
}

void BuildingFootprints::Clear() {
    m_buildings.clear();
    m_buildingIndex.clear();
}

const ProcessedBuilding* BuildingFootprints::GetBuilding(int64_t id) const {
    auto it = m_buildingIndex.find(id);
    if (it != m_buildingIndex.end() && it->second < m_buildings.size()) {
        return &m_buildings[it->second];
    }
    return nullptr;
}

std::vector<int64_t> BuildingFootprints::GetBuildingsInBounds(
    const glm::vec2& min, const glm::vec2& max) const {

    std::vector<int64_t> result;

    for (const auto& building : m_buildings) {
        if (building.boundsMax.x >= min.x && building.boundsMin.x <= max.x &&
            building.boundsMax.y >= min.y && building.boundsMin.y <= max.y) {
            result.push_back(building.id);
        }
    }

    return result;
}

std::vector<int64_t> BuildingFootprints::GetBuildingsByType(BuildingType type) const {
    std::vector<int64_t> result;

    for (const auto& building : m_buildings) {
        if (building.type == type) {
            result.push_back(building.id);
        }
    }

    return result;
}

int64_t BuildingFootprints::FindBuildingAt(const glm::vec2& point) const {
    for (const auto& building : m_buildings) {
        if (point.x >= building.boundsMin.x && point.x <= building.boundsMax.x &&
            point.y >= building.boundsMin.y && point.y <= building.boundsMax.y) {
            if (building.Contains(point)) {
                return building.id;
            }
        }
    }
    return -1;
}

std::pair<int64_t, float> BuildingFootprints::FindNearestBuilding(const glm::vec2& point) const {
    int64_t nearestId = -1;
    float nearestDist = std::numeric_limits<float>::max();

    for (const auto& building : m_buildings) {
        float dist = glm::length(building.centroid - point);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestId = building.id;
        }
    }

    return {nearestId, nearestDist};
}

float BuildingFootprints::GetBuildingCoverage(const glm::vec2& min, const glm::vec2& max) const {
    float totalArea = (max.x - min.x) * (max.y - min.y);
    if (totalArea <= 0) return 0.0f;

    float buildingArea = 0.0f;
    for (const auto& building : m_buildings) {
        if (building.boundsMax.x >= min.x && building.boundsMin.x <= max.x &&
            building.boundsMax.y >= min.y && building.boundsMin.y <= max.y) {
            buildingArea += building.area;
        }
    }

    return std::min(1.0f, buildingArea / totalArea);
}

float BuildingFootprints::GetAverageHeight(const glm::vec2& min, const glm::vec2& max) const {
    float totalHeight = 0.0f;
    int count = 0;

    for (const auto& building : m_buildings) {
        if (building.boundsMax.x >= min.x && building.boundsMin.x <= max.x &&
            building.boundsMax.y >= min.y && building.boundsMin.y <= max.y) {
            totalHeight += building.height;
            ++count;
        }
    }

    return count > 0 ? totalHeight / count : 0.0f;
}

BuildingFootprints::BuildingMesh BuildingFootprints::GenerateBuildingMesh(int64_t buildingId) const {
    const ProcessedBuilding* building = GetBuilding(buildingId);
    if (!building) return BuildingMesh{};
    return GenerateBuildingMesh(*building);
}

BuildingFootprints::BuildingMesh BuildingFootprints::GenerateBuildingMesh(
    const ProcessedBuilding& building) const {

    BuildingMesh mesh;
    mesh.buildingId = building.id;

    GenerateWalls(building, mesh.vertices, mesh.indices);
    GenerateRoof(building, mesh.vertices, mesh.indices);

    return mesh;
}

BuildingFootprints::BuildingMesh BuildingFootprints::GenerateLODMesh(
    const ProcessedBuilding& building, int lodLevel) const {

    BuildingMesh mesh;
    mesh.buildingId = building.id;

    if (lodLevel >= 2) {
        // Very simple box
        glm::vec2 center = building.centroid;
        glm::vec2 halfSize = (building.boundsMax - building.boundsMin) * 0.5f;

        float baseZ = building.minHeight;
        float topZ = building.minHeight + building.height;

        // 8 vertices for box
        mesh.vertices = {
            {{center.x - halfSize.x, center.y - halfSize.y, baseZ}, {0, 0, -1}, {0, 0}, building.wallColor},
            {{center.x + halfSize.x, center.y - halfSize.y, baseZ}, {0, 0, -1}, {1, 0}, building.wallColor},
            {{center.x + halfSize.x, center.y + halfSize.y, baseZ}, {0, 0, -1}, {1, 1}, building.wallColor},
            {{center.x - halfSize.x, center.y + halfSize.y, baseZ}, {0, 0, -1}, {0, 1}, building.wallColor},
            {{center.x - halfSize.x, center.y - halfSize.y, topZ}, {0, 0, 1}, {0, 0}, building.roofColor},
            {{center.x + halfSize.x, center.y - halfSize.y, topZ}, {0, 0, 1}, {1, 0}, building.roofColor},
            {{center.x + halfSize.x, center.y + halfSize.y, topZ}, {0, 0, 1}, {1, 1}, building.roofColor},
            {{center.x - halfSize.x, center.y + halfSize.y, topZ}, {0, 0, 1}, {0, 1}, building.roofColor},
        };

        // Box indices (6 faces)
        mesh.indices = {
            0, 2, 1, 0, 3, 2,  // Bottom
            4, 5, 6, 4, 6, 7,  // Top
            0, 1, 5, 0, 5, 4,  // Front
            2, 3, 7, 2, 7, 6,  // Back
            1, 2, 6, 1, 6, 5,  // Right
            3, 0, 4, 3, 4, 7   // Left
        };
    } else {
        // Use simplified outline
        float tolerance = lodLevel == 1 ? 2.0f : 0.5f;
        ProcessedBuilding simplified = building;
        simplified.outline = SimplifyPolygon(building.outline, tolerance);

        GenerateWalls(simplified, mesh.vertices, mesh.indices);
        GenerateRoof(simplified, mesh.vertices, mesh.indices);
    }

    return mesh;
}

std::vector<BuildingFootprints::BuildingMesh> BuildingFootprints::GenerateMeshesInBounds(
    const glm::vec2& min, const glm::vec2& max) const {

    std::vector<BuildingMesh> meshes;
    auto ids = GetBuildingsInBounds(min, max);

    for (int64_t id : ids) {
        const ProcessedBuilding* building = GetBuilding(id);
        if (building) {
            meshes.push_back(GenerateBuildingMesh(*building));
        }
    }

    return meshes;
}

BuildingFootprints::BuildingMesh BuildingFootprints::GenerateCombinedMesh(
    const glm::vec2& min, const glm::vec2& max) const {

    BuildingMesh combined;
    auto ids = GetBuildingsInBounds(min, max);

    for (int64_t id : ids) {
        const ProcessedBuilding* building = GetBuilding(id);
        if (!building) continue;

        BuildingMesh mesh = GenerateBuildingMesh(*building);

        uint32_t baseIndex = static_cast<uint32_t>(combined.vertices.size());
        combined.vertices.insert(combined.vertices.end(),
                                  mesh.vertices.begin(), mesh.vertices.end());

        for (uint32_t idx : mesh.indices) {
            combined.indices.push_back(baseIndex + idx);
        }
    }

    return combined;
}

std::vector<BuildingFootprints::PlacementData> BuildingFootprints::GetPlacementData(
    const glm::vec2& min, const glm::vec2& max) const {

    std::vector<PlacementData> result;
    auto ids = GetBuildingsInBounds(min, max);

    for (int64_t id : ids) {
        const ProcessedBuilding* building = GetBuilding(id);
        if (!building) continue;

        PlacementData data;
        data.position = building->centroid;
        data.footprintSize = building->boundsMax - building->boundsMin;
        data.height = building->height;
        data.suggestedType = building->type;

        // Calculate rotation from oriented bounding box
        glm::vec2 center, halfExtents;
        GetOrientedBoundingBox(building->outline, center, halfExtents, data.rotation);

        result.push_back(data);
    }

    return result;
}

float BuildingFootprints::CalculatePolygonArea(const std::vector<glm::vec2>& polygon) {
    if (polygon.size() < 3) return 0.0f;

    float area = 0.0f;
    int n = static_cast<int>(polygon.size());

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += polygon[i].x * polygon[j].y;
        area -= polygon[j].x * polygon[i].y;
    }

    return std::abs(area) * 0.5f;
}

glm::vec2 BuildingFootprints::CalculateCentroid(const std::vector<glm::vec2>& polygon) {
    if (polygon.empty()) return glm::vec2(0);
    if (polygon.size() == 1) return polygon[0];

    glm::vec2 centroid(0);
    for (const auto& p : polygon) {
        centroid += p;
    }
    return centroid / static_cast<float>(polygon.size());
}

bool BuildingFootprints::PointInPolygon(const glm::vec2& point,
                                          const std::vector<glm::vec2>& polygon) {
    if (polygon.size() < 3) return false;

    bool inside = false;
    int n = static_cast<int>(polygon.size());

    for (int i = 0, j = n - 1; i < n; j = i++) {
        const auto& pi = polygon[i];
        const auto& pj = polygon[j];

        if (((pi.y > point.y) != (pj.y > point.y)) &&
            (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }

    return inside;
}

std::vector<glm::vec2> BuildingFootprints::SimplifyPolygon(
    const std::vector<glm::vec2>& polygon, float tolerance) {

    if (polygon.size() < 4) return polygon;

    // Simple vertex reduction
    std::vector<glm::vec2> result;
    result.push_back(polygon[0]);

    for (size_t i = 1; i < polygon.size() - 1; ++i) {
        glm::vec2 prev = result.back();
        glm::vec2 curr = polygon[i];
        glm::vec2 next = polygon[i + 1];

        // Keep if angle is significant
        glm::vec2 v1 = glm::normalize(curr - prev);
        glm::vec2 v2 = glm::normalize(next - curr);
        float dot = glm::dot(v1, v2);

        if (dot < 0.99f || glm::length(curr - prev) > tolerance * 5.0f) {
            result.push_back(curr);
        }
    }

    result.push_back(polygon.back());

    // Ensure minimum vertices
    if (result.size() < 3) return polygon;

    return result;
}

void BuildingFootprints::GetOrientedBoundingBox(
    const std::vector<glm::vec2>& polygon,
    glm::vec2& center, glm::vec2& halfExtents, float& rotation) {

    center = CalculateCentroid(polygon);

    // Find principal axis using edge directions
    glm::vec2 principalAxis(1, 0);
    float maxLength = 0.0f;

    for (size_t i = 0; i < polygon.size(); ++i) {
        size_t j = (i + 1) % polygon.size();
        glm::vec2 edge = polygon[j] - polygon[i];
        float len = glm::length(edge);
        if (len > maxLength) {
            maxLength = len;
            principalAxis = glm::normalize(edge);
        }
    }

    rotation = std::atan2(principalAxis.y, principalAxis.x);

    // Calculate extents along principal axes
    glm::vec2 perpAxis(-principalAxis.y, principalAxis.x);

    float minProj1 = std::numeric_limits<float>::max();
    float maxProj1 = std::numeric_limits<float>::lowest();
    float minProj2 = std::numeric_limits<float>::max();
    float maxProj2 = std::numeric_limits<float>::lowest();

    for (const auto& p : polygon) {
        glm::vec2 centered = p - center;
        float proj1 = glm::dot(centered, principalAxis);
        float proj2 = glm::dot(centered, perpAxis);

        minProj1 = std::min(minProj1, proj1);
        maxProj1 = std::max(maxProj1, proj1);
        minProj2 = std::min(minProj2, proj2);
        maxProj2 = std::max(maxProj2, proj2);
    }

    halfExtents.x = (maxProj1 - minProj1) * 0.5f;
    halfExtents.y = (maxProj2 - minProj2) * 0.5f;
}

glm::vec2 BuildingFootprints::TransformCoord(const GeoCoordinate& coord) const {
    return m_transform(coord);
}

void BuildingFootprints::GenerateWalls(const ProcessedBuilding& building,
                                         std::vector<BuildingVertex>& vertices,
                                         std::vector<uint32_t>& indices) const {

    float baseZ = building.minHeight;
    float topZ = building.minHeight + building.height;

    const auto& outline = building.outline;
    int n = static_cast<int>(outline.size());

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;

        glm::vec2 p0 = outline[i];
        glm::vec2 p1 = outline[j];

        glm::vec2 edge = p1 - p0;
        float edgeLen = glm::length(edge);
        glm::vec3 normal = glm::normalize(glm::vec3(-edge.y, edge.x, 0.0f));

        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        // Four vertices per wall segment
        BuildingVertex v0, v1, v2, v3;

        v0.position = glm::vec3(p0.x, p0.y, baseZ);
        v0.normal = normal;
        v0.texCoord = glm::vec2(0, 0);
        v0.color = building.wallColor;

        v1.position = glm::vec3(p1.x, p1.y, baseZ);
        v1.normal = normal;
        v1.texCoord = glm::vec2(edgeLen / 5.0f, 0);
        v1.color = building.wallColor;

        v2.position = glm::vec3(p1.x, p1.y, topZ);
        v2.normal = normal;
        v2.texCoord = glm::vec2(edgeLen / 5.0f, (topZ - baseZ) / 3.0f);
        v2.color = building.wallColor;

        v3.position = glm::vec3(p0.x, p0.y, topZ);
        v3.normal = normal;
        v3.texCoord = glm::vec2(0, (topZ - baseZ) / 3.0f);
        v3.color = building.wallColor;

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 2);

        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 3);
    }
}

void BuildingFootprints::GenerateRoof(const ProcessedBuilding& building,
                                        std::vector<BuildingVertex>& vertices,
                                        std::vector<uint32_t>& indices) const {

    float topZ = building.minHeight + building.height;

    // Simple flat roof using triangulation
    auto roofIndices = TriangulatePolygon(building.outline);

    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    // Add vertices
    for (const auto& p : building.outline) {
        BuildingVertex v;
        v.position = glm::vec3(p.x, p.y, topZ);
        v.normal = glm::vec3(0, 0, 1);
        v.texCoord = glm::vec2(p.x / 10.0f, p.y / 10.0f);
        v.color = building.roofColor;
        vertices.push_back(v);
    }

    // Add indices
    for (uint32_t idx : roofIndices) {
        indices.push_back(baseIndex + idx);
    }
}

std::vector<uint32_t> BuildingFootprints::TriangulatePolygon(
    const std::vector<glm::vec2>& polygon) const {

    std::vector<uint32_t> result;

    if (polygon.size() < 3) return result;

    // Simple ear clipping triangulation
    std::vector<int> remaining;
    for (int i = 0; i < static_cast<int>(polygon.size()); ++i) {
        remaining.push_back(i);
    }

    while (remaining.size() > 3) {
        bool earFound = false;

        for (size_t i = 0; i < remaining.size(); ++i) {
            size_t prev = (i + remaining.size() - 1) % remaining.size();
            size_t next = (i + 1) % remaining.size();

            int iPrev = remaining[prev];
            int iCurr = remaining[i];
            int iNext = remaining[next];

            const glm::vec2& p0 = polygon[iPrev];
            const glm::vec2& p1 = polygon[iCurr];
            const glm::vec2& p2 = polygon[iNext];

            // Check if convex
            float cross = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
            if (cross <= 0) continue;

            // Check if ear (no other vertices inside)
            bool isEar = true;
            for (size_t j = 0; j < remaining.size(); ++j) {
                if (j == prev || j == i || j == next) continue;

                const glm::vec2& pt = polygon[remaining[j]];

                // Point in triangle test
                auto sign = [](const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
                    return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
                };

                float d1 = sign(pt, p0, p1);
                float d2 = sign(pt, p1, p2);
                float d3 = sign(pt, p2, p0);

                bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

                if (!(hasNeg && hasPos)) {
                    isEar = false;
                    break;
                }
            }

            if (isEar) {
                result.push_back(iPrev);
                result.push_back(iCurr);
                result.push_back(iNext);

                remaining.erase(remaining.begin() + i);
                earFound = true;
                break;
            }
        }

        if (!earFound) break;  // Degenerate polygon
    }

    // Last triangle
    if (remaining.size() == 3) {
        result.push_back(remaining[0]);
        result.push_back(remaining[1]);
        result.push_back(remaining[2]);
    }

    return result;
}

} // namespace Geo
} // namespace Vehement
