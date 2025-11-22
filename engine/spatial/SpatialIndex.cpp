#include "SpatialIndex.hpp"
#include "Octree.hpp"
#include "BVH.hpp"
#include "SpatialHash3D.hpp"

namespace Nova {

std::unique_ptr<ISpatialIndex> CreateSpatialIndex(
    SpatialIndexType type,
    const AABB& worldBounds,
    float cellSize) {

    switch (type) {
        case SpatialIndexType::Octree:
            return std::make_unique<Octree<uint64_t>>(worldBounds);

        case SpatialIndexType::LooseOctree:
            return std::make_unique<Octree<uint64_t>>(worldBounds, 2.0f); // Loose factor of 2

        case SpatialIndexType::BVH:
            return std::make_unique<BVH>();

        case SpatialIndexType::SpatialHash:
            return std::make_unique<SpatialHash3D>(cellSize);

        case SpatialIndexType::Auto:
        default:
            // Default to BVH as it's generally good for dynamic scenes
            return std::make_unique<BVH>();
    }
}

} // namespace Nova
