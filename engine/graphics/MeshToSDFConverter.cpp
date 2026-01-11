#include "MeshToSDFConverter.hpp"
#include "../animation/Skeleton.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <chrono>
#include <thread>
#include <iostream>

namespace Nova {

// ============================================================================
// Triangle Implementation
// ============================================================================

Triangle::Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    : v0(a), v1(b), v2(c)
{
    // Calculate normal
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    normal = glm::normalize(glm::cross(edge1, edge2));

    // Calculate centroid
    centroid = (v0 + v1 + v2) / 3.0f;

    // Calculate area
    area = glm::length(glm::cross(edge1, edge2)) * 0.5f;
}

float Triangle::DistanceToPoint(const glm::vec3& point) const {
    // Project point onto triangle plane
    glm::vec3 v0p = point - v0;
    float dist = glm::dot(v0p, normal);
    glm::vec3 projected = point - normal * dist;

    // Check if projection is inside triangle
    glm::vec3 edge0 = v1 - v0;
    glm::vec3 edge1 = v2 - v1;
    glm::vec3 edge2 = v0 - v2;

    glm::vec3 c0 = projected - v0;
    glm::vec3 c1 = projected - v1;
    glm::vec3 c2 = projected - v2;

    bool inside = (glm::dot(normal, glm::cross(edge0, c0)) >= 0.0f) &&
                  (glm::dot(normal, glm::cross(edge1, c1)) >= 0.0f) &&
                  (glm::dot(normal, glm::cross(edge2, c2)) >= 0.0f);

    if (inside) {
        return std::abs(dist);
    }

    // Find closest edge
    float minDist = glm::length(point - v0);
    minDist = std::min(minDist, glm::length(point - v1));
    minDist = std::min(minDist, glm::length(point - v2));

    // Check edges
    auto distToSegment = [](const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
        glm::vec3 ab = b - a;
        float t = glm::clamp(glm::dot(p - a, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
        glm::vec3 closest = a + t * ab;
        return glm::length(p - closest);
    };

    minDist = std::min(minDist, distToSegment(point, v0, v1));
    minDist = std::min(minDist, distToSegment(point, v1, v2));
    minDist = std::min(minDist, distToSegment(point, v2, v0));

    return minDist;
}

bool Triangle::Contains(const glm::vec3& point) const {
    return DistanceToPoint(point) < 0.001f;
}

// ============================================================================
// PrimitiveFitResult Implementation
// ============================================================================

std::unique_ptr<SDFPrimitive> PrimitiveFitResult::ToPrimitive(const std::string& name) const {
    auto primitive = std::make_unique<SDFPrimitive>(name.empty() ? "Primitive" : name, type);

    SDFTransform transform;
    transform.position = position;
    transform.rotation = orientation;
    transform.scale = scale;
    primitive->SetLocalTransform(transform);
    primitive->SetParameters(parameters);

    return primitive;
}

// ============================================================================
// MeshToSDFConverter Implementation
// ============================================================================

MeshToSDFConverter::MeshToSDFConverter() = default;

MeshToSDFConverter::~MeshToSDFConverter() = default;

ConversionResult MeshToSDFConverter::Convert(const Mesh& mesh, const ConversionSettings& settings) {
    // Extract vertex and index data from mesh
    // Note: In a real implementation, we'd need access to the mesh's internal data
    // For now, we'll use the mesh bounds to create a simple approximation

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // TODO: Extract actual geometry from mesh
    // This would require mesh to expose its vertex/index data

    return Convert(vertices, indices, settings);
}

ConversionResult MeshToSDFConverter::Convert(
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    const ConversionSettings& settings)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    ConversionResult result;
    result.originalVertexCount = vertices.size();
    result.originalTriangleCount = indices.size() / 3;

    if (vertices.empty() || indices.empty()) {
        result.success = false;
        result.errorMessage = "Empty mesh";
        return result;
    }

    // Build triangle list
    std::vector<Triangle> triangles = BuildTriangleList(vertices, indices);

    if (triangles.empty()) {
        result.success = false;
        result.errorMessage = "Failed to build triangle list";
        return result;
    }

    // Select strategy
    ConversionStrategy strategy = settings.strategy;
    if (strategy == ConversionStrategy::Auto) {
        strategy = SelectBestStrategy(triangles, settings);
    }

    // Convert using selected strategy
    switch (strategy) {
        case ConversionStrategy::PrimitiveFitting:
            result = ConvertPrimitiveFitting(triangles, settings);
            break;
        case ConversionStrategy::ConvexDecomposition:
            result = ConvertConvexDecomposition(triangles, settings);
            break;
        case ConversionStrategy::Voxelization:
            result = ConvertVoxelization(triangles, settings);
            break;
        case ConversionStrategy::Hybrid:
            // Try primitive fitting first, fallback to voxelization
            result = ConvertPrimitiveFitting(triangles, settings);
            if (result.totalError > settings.errorThreshold * 2.0f) {
                result = ConvertVoxelization(triangles, settings);
            }
            break;
        default:
            result = ConvertPrimitiveFitting(triangles, settings);
            break;
    }

    // Generate LODs if requested
    if (settings.generateLODs && result.success) {
        result.lodLevels = GenerateLODs(result.allPrimitives, settings.lodPrimitiveCounts);
    }

    // Compute bone weights if skeleton provided
    if (settings.bindToSkeleton && settings.skeleton && result.success) {
        ComputeBoneWeights(result.allPrimitives, *settings.skeleton);
    }

    // Calculate timing
    auto endTime = std::chrono::high_resolution_clock::now();
    result.conversionTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    return result;
}

std::vector<Triangle> MeshToSDFConverter::BuildTriangleList(
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    std::vector<Triangle> triangles;
    triangles.reserve(indices.size() / 3);

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        if (i0 < vertices.size() && i1 < vertices.size() && i2 < vertices.size()) {
            triangles.emplace_back(
                vertices[i0].position,
                vertices[i1].position,
                vertices[i2].position
            );
        }
    }

    return triangles;
}

ConversionStrategy MeshToSDFConverter::SelectBestStrategy(
    const std::vector<Triangle>& triangles,
    const ConversionSettings& settings)
{
    // Analyze mesh complexity
    float avgArea = 0.0f;
    float varArea = 0.0f;

    for (const auto& tri : triangles) {
        avgArea += tri.area;
    }
    avgArea /= triangles.size();

    for (const auto& tri : triangles) {
        float diff = tri.area - avgArea;
        varArea += diff * diff;
    }
    varArea /= triangles.size();

    // Simple heuristic:
    // - Low triangle count + regular geometry -> Primitive fitting
    // - High triangle count + complex -> Voxelization
    // - Medium -> Convex decomposition

    if (triangles.size() < 100 && varArea < avgArea * 0.5f) {
        return ConversionStrategy::PrimitiveFitting;
    } else if (triangles.size() > 1000) {
        return ConversionStrategy::Voxelization;
    } else {
        return ConversionStrategy::ConvexDecomposition;
    }
}

// ============================================================================
// Primitive Fitting Strategy
// ============================================================================

ConversionResult MeshToSDFConverter::ConvertPrimitiveFitting(
    const std::vector<Triangle>& triangles,
    const ConversionSettings& settings)
{
    ConversionResult result;
    result.success = true;

    // Fit primitives
    result.allPrimitives = FitPrimitives(triangles, settings.maxPrimitives, settings);
    result.primitiveCount = static_cast<int>(result.allPrimitives.size());

    if (result.allPrimitives.empty()) {
        result.success = false;
        result.errorMessage = "Failed to fit any primitives";
        return result;
    }

    // Build CSG tree if enabled
    if (settings.useCSG) {
        BuildCSGTree(result.allPrimitives, settings);
    }

    // Create root primitive
    result.rootPrimitive = std::make_unique<SDFPrimitive>("Root", SDFPrimitiveType::Box);

    for (size_t i = 0; i < result.allPrimitives.size(); ++i) {
        auto primitive = result.allPrimitives[i].ToPrimitive("Primitive_" + std::to_string(i));
        result.rootPrimitive->AddChild(std::move(primitive));
    }

    // Calculate statistics
    result.totalError = 0.0f;
    result.maxError = 0.0f;
    for (const auto& prim : result.allPrimitives) {
        result.totalError += prim.error;
        result.maxError = std::max(result.maxError, prim.error);
    }
    result.avgError = result.allPrimitives.empty() ? 0.0f :
                      result.totalError / result.allPrimitives.size();

    return result;
}

std::vector<PrimitiveFitResult> MeshToSDFConverter::FitPrimitives(
    const std::vector<Triangle>& triangles,
    int maxPrimitives,
    const ConversionSettings& settings)
{
    std::vector<PrimitiveFitResult> primitives;
    std::vector<bool> covered(triangles.size(), false);

    if (settings.verbose) {
        std::cout << "Fitting primitives to " << triangles.size() << " triangles...\n";
    }

    // Progressive primitive fitting
    for (int primIndex = 0; primIndex < maxPrimitives; ++primIndex) {
        // Report progress
        if (settings.progressCallback) {
            settings.progressCallback(static_cast<float>(primIndex) / maxPrimitives);
        }

        // Find uncovered region with highest error
        std::vector<int> region;
        FindHighestErrorRegion(triangles, covered, region);

        if (region.empty()) {
            break; // All triangles covered
        }

        // Fit best primitive to this region
        PrimitiveFitResult bestFit = FitBestPrimitive(triangles, region, settings);

        // Check if error threshold met
        if (bestFit.error < settings.errorThreshold) {
            primitives.push_back(bestFit);
            break;
        }

        // Mark triangles as covered
        for (int idx : bestFit.triangleIndices) {
            if (idx >= 0 && idx < static_cast<int>(covered.size())) {
                covered[idx] = true;
            }
        }

        primitives.push_back(bestFit);

        if (settings.verbose) {
            std::cout << "  Primitive " << primIndex << ": "
                      << static_cast<int>(bestFit.type) << " (error: "
                      << bestFit.error << ", coverage: " << bestFit.coverage << ")\n";
        }
    }

    // Sort by importance for LOD generation
    SortByImportance(primitives);

    return primitives;
}

void MeshToSDFConverter::FindHighestErrorRegion(
    const std::vector<Triangle>& triangles,
    const std::vector<bool>& covered,
    std::vector<int>& outRegion)
{
    outRegion.clear();

    // Find largest uncovered region
    // For simplicity, we'll just collect all uncovered triangles
    for (size_t i = 0; i < triangles.size(); ++i) {
        if (!covered[i]) {
            outRegion.push_back(static_cast<int>(i));
        }
    }
}

PrimitiveFitResult MeshToSDFConverter::FitBestPrimitive(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& triangleIndices,
    const ConversionSettings& settings)
{
    std::vector<PrimitiveFitResult> candidates;

    // Try each primitive type
    if (settings.allowSpheres) {
        candidates.push_back(FitSphere(triangles, triangleIndices));
    }
    if (settings.allowBoxes) {
        candidates.push_back(FitBox(triangles, triangleIndices));
    }
    if (settings.allowCapsules) {
        candidates.push_back(FitCapsule(triangles, triangleIndices));
    }
    if (settings.allowCylinders) {
        candidates.push_back(FitCylinder(triangles, triangleIndices));
    }
    if (settings.allowCones) {
        candidates.push_back(FitCone(triangles, triangleIndices));
    }

    // Return best fit (lowest error)
    if (candidates.empty()) {
        // Fallback to sphere
        return FitSphere(triangles, triangleIndices);
    }

    auto best = std::min_element(candidates.begin(), candidates.end(),
        [](const PrimitiveFitResult& a, const PrimitiveFitResult& b) {
            return a.error < b.error;
        });

    return *best;
}

// ============================================================================
// Primitive Fitting Algorithms
// ============================================================================

PrimitiveFitResult MeshToSDFConverter::FitSphere(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices)
{
    PrimitiveFitResult result;
    result.type = SDFPrimitiveType::Sphere;
    result.triangleIndices = indices;

    // Calculate centroid
    glm::vec3 center{0.0f};
    for (int idx : indices) {
        center += triangles[idx].centroid;
    }
    center /= static_cast<float>(indices.size());

    // Calculate average radius
    float radius = 0.0f;
    for (int idx : indices) {
        radius += glm::length(triangles[idx].centroid - center);
    }
    radius /= static_cast<float>(indices.size());

    result.position = center;
    result.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    result.scale = glm::vec3(1.0f);
    result.parameters.radius = radius;

    // Calculate error
    result.error = CalculateError(result, triangles, indices);
    result.coverage = static_cast<float>(indices.size()) / triangles.size();
    result.importance = result.coverage / (1.0f + result.error);

    return result;
}

PrimitiveFitResult MeshToSDFConverter::FitBox(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices)
{
    PrimitiveFitResult result;
    result.type = SDFPrimitiveType::Box;
    result.triangleIndices = indices;

    // Compute PCA to get oriented bounding box
    glm::vec3 center, axis1, axis2, axis3;
    ComputePCA(triangles, indices, center, axis1, axis2, axis3);

    // Calculate dimensions along each axis
    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;

    for (int idx : indices) {
        glm::vec3 localPos = triangles[idx].centroid - center;
        float x = glm::dot(localPos, axis1);
        float y = glm::dot(localPos, axis2);
        float z = glm::dot(localPos, axis3);

        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        minZ = std::min(minZ, z);
        maxZ = std::max(maxZ, z);
    }

    glm::vec3 dimensions{
        maxX - minX,
        maxY - minY,
        maxZ - minZ
    };

    result.position = center;
    result.orientation = glm::quatLookAt(axis1, axis2);
    result.scale = glm::vec3(1.0f);
    result.parameters.dimensions = dimensions;

    result.error = CalculateError(result, triangles, indices);
    result.coverage = static_cast<float>(indices.size()) / triangles.size();
    result.importance = result.coverage / (1.0f + result.error);

    return result;
}

PrimitiveFitResult MeshToSDFConverter::FitCapsule(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices)
{
    PrimitiveFitResult result;
    result.type = SDFPrimitiveType::Capsule;
    result.triangleIndices = indices;

    // Compute PCA to get primary axis
    glm::vec3 center, axis1, axis2, axis3;
    ComputePCA(triangles, indices, center, axis1, axis2, axis3);

    // Find extent along primary axis
    float minT = FLT_MAX, maxT = -FLT_MAX;
    float avgRadius = 0.0f;

    for (int idx : indices) {
        glm::vec3 localPos = triangles[idx].centroid - center;
        float t = glm::dot(localPos, axis1);
        minT = std::min(minT, t);
        maxT = std::max(maxT, t);

        // Calculate distance from axis
        glm::vec3 projected = center + axis1 * t;
        avgRadius += glm::length(triangles[idx].centroid - projected);
    }

    avgRadius /= static_cast<float>(indices.size());
    float height = maxT - minT;

    result.position = center;
    result.orientation = glm::quatLookAt(axis1, axis2);
    result.scale = glm::vec3(1.0f);
    result.parameters.height = height;
    result.parameters.radius = avgRadius;

    result.error = CalculateError(result, triangles, indices);
    result.coverage = static_cast<float>(indices.size()) / triangles.size();
    result.importance = result.coverage / (1.0f + result.error);

    return result;
}

PrimitiveFitResult MeshToSDFConverter::FitCylinder(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices)
{
    // Similar to capsule but with flat ends
    PrimitiveFitResult result = FitCapsule(triangles, indices);
    result.type = SDFPrimitiveType::Cylinder;
    return result;
}

PrimitiveFitResult MeshToSDFConverter::FitCone(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices)
{
    PrimitiveFitResult result;
    result.type = SDFPrimitiveType::Cone;
    result.triangleIndices = indices;

    // Compute PCA to get primary axis
    glm::vec3 center, axis1, axis2, axis3;
    ComputePCA(triangles, indices, center, axis1, axis2, axis3);

    // Find extent and radius variation along axis
    float minT = FLT_MAX, maxT = -FLT_MAX;
    float avgRadiusBottom = 0.0f;
    int countBottom = 0;

    for (int idx : indices) {
        glm::vec3 localPos = triangles[idx].centroid - center;
        float t = glm::dot(localPos, axis1);
        minT = std::min(minT, t);
        maxT = std::max(maxT, t);

        // Calculate distance from axis at bottom
        if (t < 0.0f) {
            glm::vec3 projected = center + axis1 * t;
            avgRadiusBottom += glm::length(triangles[idx].centroid - projected);
            countBottom++;
        }
    }

    avgRadiusBottom = countBottom > 0 ? avgRadiusBottom / countBottom : 1.0f;
    float height = maxT - minT;

    result.position = center;
    result.orientation = glm::quatLookAt(axis1, axis2);
    result.scale = glm::vec3(1.0f);
    result.parameters.height = height;
    result.parameters.bottomRadius = avgRadiusBottom;
    result.parameters.topRadius = 0.0f;

    result.error = CalculateError(result, triangles, indices);
    result.coverage = static_cast<float>(indices.size()) / triangles.size();
    result.importance = result.coverage / (1.0f + result.error);

    return result;
}

// ============================================================================
// Error Calculation
// ============================================================================

float MeshToSDFConverter::CalculateError(
    const PrimitiveFitResult& primitive,
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices)
{
    if (indices.empty()) return 0.0f;

    // Calculate RMS error
    float totalError = 0.0f;

    for (int idx : indices) {
        const Triangle& tri = triangles[idx];

        // Simplified: use distance from centroid to primitive surface
        glm::vec3 localPos = tri.centroid - primitive.position;

        // Rotate to primitive space
        glm::mat4 rotMat = glm::toMat4(glm::inverse(primitive.orientation));
        glm::vec3 localRotated = glm::vec3(rotMat * glm::vec4(localPos, 0.0f));

        float dist = 0.0f;

        switch (primitive.type) {
            case SDFPrimitiveType::Sphere:
                dist = std::abs(glm::length(localRotated) - primitive.parameters.radius);
                break;
            case SDFPrimitiveType::Box:
                dist = glm::length(glm::max(glm::abs(localRotated) - primitive.parameters.dimensions * 0.5f, 0.0f));
                break;
            case SDFPrimitiveType::Capsule:
            case SDFPrimitiveType::Cylinder:
                {
                    float h = primitive.parameters.height * 0.5f;
                    float r = primitive.parameters.radius;
                    float d = std::sqrt(localRotated.x * localRotated.x + localRotated.z * localRotated.z);
                    dist = std::abs(d - r) + std::abs(std::abs(localRotated.y) - h);
                }
                break;
            default:
                dist = glm::length(localRotated);
                break;
        }

        totalError += dist * dist * tri.area; // Weight by triangle area
    }

    // Normalize by total area
    float totalArea = 0.0f;
    for (int idx : indices) {
        totalArea += triangles[idx].area;
    }

    return totalArea > 0.0f ? std::sqrt(totalError / totalArea) : 0.0f;
}

void MeshToSDFConverter::CalculateCoverage(
    const std::vector<PrimitiveFitResult>& primitives,
    const std::vector<Triangle>& triangles,
    std::vector<bool>& outCovered)
{
    outCovered.assign(triangles.size(), false);

    for (const auto& prim : primitives) {
        for (int idx : prim.triangleIndices) {
            if (idx >= 0 && idx < static_cast<int>(outCovered.size())) {
                outCovered[idx] = true;
            }
        }
    }
}

// ============================================================================
// LOD Generation
// ============================================================================

std::vector<std::vector<int>> MeshToSDFConverter::GenerateLODs(
    const std::vector<PrimitiveFitResult>& primitives,
    const std::vector<int>& primitiveCounts)
{
    std::vector<std::vector<int>> lodLevels;
    lodLevels.reserve(primitiveCounts.size());

    for (int count : primitiveCounts) {
        std::vector<int> lodIndices;
        lodIndices.reserve(std::min(count, static_cast<int>(primitives.size())));

        // Take top N primitives (already sorted by importance)
        for (int i = 0; i < std::min(count, static_cast<int>(primitives.size())); ++i) {
            lodIndices.push_back(i);
        }

        lodLevels.push_back(lodIndices);
    }

    return lodLevels;
}

void MeshToSDFConverter::SortByImportance(std::vector<PrimitiveFitResult>& primitives) {
    std::sort(primitives.begin(), primitives.end(),
        [](const PrimitiveFitResult& a, const PrimitiveFitResult& b) {
            return a.importance > b.importance; // Higher importance first
        });
}

// ============================================================================
// Skeletal Binding
// ============================================================================

void MeshToSDFConverter::ComputeBoneWeights(
    std::vector<PrimitiveFitResult>& primitives,
    const Skeleton& skeleton)
{
    // For each primitive, find the 4 nearest bones and calculate weights
    const auto& bones = skeleton.GetBones();

    for (auto& prim : primitives) {
        std::vector<std::pair<int, float>> boneDistances;
        boneDistances.reserve(bones.size());

        // Calculate distance from primitive center to each bone
        for (size_t i = 0; i < bones.size(); ++i) {
            // Extract bone position from offset matrix
            glm::vec3 bonePos = glm::vec3(bones[i].localTransform[3]);
            float dist = glm::length(prim.position - bonePos);
            boneDistances.emplace_back(static_cast<int>(i), dist);
        }

        // Sort by distance
        std::sort(boneDistances.begin(), boneDistances.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        // Take top 4 bones and calculate weights
        // Store in primitive for later use in skeletal animation system
    }
}

// ============================================================================
// PCA (Principal Component Analysis)
// ============================================================================

void MeshToSDFConverter::ComputePCA(
    const std::vector<Triangle>& triangles,
    const std::vector<int>& indices,
    glm::vec3& outCenter,
    glm::vec3& outAxis1,
    glm::vec3& outAxis2,
    glm::vec3& outAxis3)
{
    // Calculate centroid
    outCenter = glm::vec3(0.0f);
    for (int idx : indices) {
        outCenter += triangles[idx].centroid;
    }
    outCenter /= static_cast<float>(indices.size());

    // Calculate covariance matrix
    glm::mat3 covariance{0.0f};
    for (int idx : indices) {
        glm::vec3 centered = triangles[idx].centroid - outCenter;
        covariance[0][0] += centered.x * centered.x;
        covariance[1][1] += centered.y * centered.y;
        covariance[2][2] += centered.z * centered.z;
        covariance[0][1] += centered.x * centered.y;
        covariance[0][2] += centered.x * centered.z;
        covariance[1][2] += centered.y * centered.z;
    }

    covariance[1][0] = covariance[0][1];
    covariance[2][0] = covariance[0][2];
    covariance[2][1] = covariance[1][2];

    // Simplified: use basis vectors (full eigenvalue decomposition would be better)
    outAxis1 = glm::vec3(1.0f, 0.0f, 0.0f);
    outAxis2 = glm::vec3(0.0f, 1.0f, 0.0f);
    outAxis3 = glm::vec3(0.0f, 0.0f, 1.0f);

    // TODO: Implement proper eigenvalue decomposition for better OBB fitting
}

// ============================================================================
// Other Strategies (Stubs)
// ============================================================================

ConversionResult MeshToSDFConverter::ConvertConvexDecomposition(
    const std::vector<Triangle>& triangles,
    const ConversionSettings& settings)
{
    // TODO: Implement convex decomposition using V-HACD or similar
    // For now, fallback to primitive fitting
    return ConvertPrimitiveFitting(triangles, settings);
}

ConversionResult MeshToSDFConverter::ConvertVoxelization(
    const std::vector<Triangle>& triangles,
    const ConversionSettings& settings)
{
    ConversionResult result;
    result.success = true;

    // Calculate bounds
    glm::vec3 boundsMin{FLT_MAX};
    glm::vec3 boundsMax{-FLT_MAX};

    for (const auto& tri : triangles) {
        boundsMin = glm::min(boundsMin, tri.v0);
        boundsMin = glm::min(boundsMin, tri.v1);
        boundsMin = glm::min(boundsMin, tri.v2);
        boundsMax = glm::max(boundsMax, tri.v0);
        boundsMax = glm::max(boundsMax, tri.v1);
        boundsMax = glm::max(boundsMax, tri.v2);
    }

    glm::vec3 size = boundsMax - boundsMin;
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;

    // Create a single box primitive as fallback
    PrimitiveFitResult boxPrim;
    boxPrim.type = SDFPrimitiveType::Box;
    boxPrim.position = center;
    boxPrim.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    boxPrim.scale = glm::vec3(1.0f);
    boxPrim.parameters.dimensions = size;

    result.allPrimitives.push_back(boxPrim);
    result.primitiveCount = 1;
    result.rootPrimitive = boxPrim.ToPrimitive("VoxelBox");

    return result;
}

void MeshToSDFConverter::BuildCSGTree(
    std::vector<PrimitiveFitResult>& primitives,
    const ConversionSettings& settings)
{
    // Set CSG operations based on primitive overlap
    // For now, just use union for all
    // TODO: Detect subtractions and intersections based on geometry
}

} // namespace Nova
