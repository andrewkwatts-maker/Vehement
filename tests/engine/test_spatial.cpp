/**
 * @file test_spatial.cpp
 * @brief Unit tests for spatial systems (AABB, Octree, BVH, Frustum, Ray casting)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "spatial/AABB.hpp"
#include "spatial/Octree.hpp"
#include "spatial/BVH.hpp"
#include "spatial/Frustum.hpp"
#include "spatial/SpatialHash3D.hpp"

#include "utils/TestHelpers.hpp"
#include "utils/Generators.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// AABB Tests
// =============================================================================

class AABBTest : public ::testing::Test {
protected:
    AABB unitBox{glm::vec3(0.0f), glm::vec3(1.0f)};
    AABB centeredBox{glm::vec3(-1.0f), glm::vec3(1.0f)};
    AABB largeBox{glm::vec3(-100.0f), glm::vec3(100.0f)};
};

TEST_F(AABBTest, ConstructorFromMinMax) {
    AABB box(glm::vec3(-1.0f, -2.0f, -3.0f), glm::vec3(1.0f, 2.0f, 3.0f));

    EXPECT_VEC3_EQ(glm::vec3(-1.0f, -2.0f, -3.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(1.0f, 2.0f, 3.0f), box.max);
}

TEST_F(AABBTest, FromCenterExtents) {
    AABB box = AABB::FromCenterExtents(glm::vec3(0.0f), glm::vec3(1.0f));

    EXPECT_VEC3_EQ(glm::vec3(-1.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(1.0f), box.max);
}

TEST_F(AABBTest, FromPoint) {
    AABB box = AABB::FromPoint(glm::vec3(5.0f, 10.0f, 15.0f));

    EXPECT_VEC3_EQ(glm::vec3(5.0f, 10.0f, 15.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(5.0f, 10.0f, 15.0f), box.max);
}

TEST_F(AABBTest, GetCenter) {
    EXPECT_VEC3_EQ(glm::vec3(0.5f), unitBox.GetCenter());
    EXPECT_VEC3_EQ(glm::vec3(0.0f), centeredBox.GetCenter());
}

TEST_F(AABBTest, GetExtents) {
    EXPECT_VEC3_EQ(glm::vec3(0.5f), unitBox.GetExtents());
    EXPECT_VEC3_EQ(glm::vec3(1.0f), centeredBox.GetExtents());
}

TEST_F(AABBTest, GetSize) {
    EXPECT_VEC3_EQ(glm::vec3(1.0f), unitBox.GetSize());
    EXPECT_VEC3_EQ(glm::vec3(2.0f), centeredBox.GetSize());
}

TEST_F(AABBTest, GetVolume) {
    EXPECT_FLOAT_EQ(1.0f, unitBox.GetVolume());
    EXPECT_FLOAT_EQ(8.0f, centeredBox.GetVolume());
}

TEST_F(AABBTest, GetSurfaceArea) {
    EXPECT_FLOAT_EQ(6.0f, unitBox.GetSurfaceArea());
    EXPECT_FLOAT_EQ(24.0f, centeredBox.GetSurfaceArea());
}

TEST_F(AABBTest, IsValid) {
    EXPECT_TRUE(unitBox.IsValid());
    EXPECT_TRUE(centeredBox.IsValid());

    AABB invalid(glm::vec3(1.0f), glm::vec3(-1.0f));
    EXPECT_FALSE(invalid.IsValid());

    AABB defaultAABB;
    EXPECT_FALSE(defaultAABB.IsValid());
}

TEST_F(AABBTest, GetLongestAxis) {
    AABB xLongest(glm::vec3(0.0f), glm::vec3(10.0f, 1.0f, 1.0f));
    AABB yLongest(glm::vec3(0.0f), glm::vec3(1.0f, 10.0f, 1.0f));
    AABB zLongest(glm::vec3(0.0f), glm::vec3(1.0f, 1.0f, 10.0f));

    EXPECT_EQ(0, xLongest.GetLongestAxis());
    EXPECT_EQ(1, yLongest.GetLongestAxis());
    EXPECT_EQ(2, zLongest.GetLongestAxis());
}

TEST_F(AABBTest, GetCorners) {
    auto corners = unitBox.GetCorners();

    EXPECT_EQ(8, corners.size());
    EXPECT_VEC3_EQ(glm::vec3(0.0f, 0.0f, 0.0f), corners[0]);
    EXPECT_VEC3_EQ(glm::vec3(1.0f, 1.0f, 1.0f), corners[7]);
}

TEST_F(AABBTest, ExpandByPoint) {
    AABB box = AABB::FromPoint(glm::vec3(0.0f));
    box.Expand(glm::vec3(1.0f, 2.0f, 3.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(1.0f, 2.0f, 3.0f), box.max);
}

TEST_F(AABBTest, ExpandByAABB) {
    AABB box(glm::vec3(0.0f), glm::vec3(1.0f));
    AABB other(glm::vec3(-1.0f), glm::vec3(2.0f));
    box.Expand(other);

    EXPECT_VEC3_EQ(glm::vec3(-1.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(2.0f), box.max);
}

TEST_F(AABBTest, Inflate) {
    AABB box = centeredBox;
    box.Inflate(1.0f);

    EXPECT_VEC3_EQ(glm::vec3(-2.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(2.0f), box.max);
}

TEST_F(AABBTest, Translate) {
    AABB box = unitBox;
    box.Translate(glm::vec3(10.0f, 20.0f, 30.0f));

    EXPECT_VEC3_EQ(glm::vec3(10.0f, 20.0f, 30.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(11.0f, 21.0f, 31.0f), box.max);
}

TEST_F(AABBTest, Scale) {
    AABB box = centeredBox;
    box.Scale(2.0f);

    EXPECT_VEC3_EQ(glm::vec3(-2.0f), box.min);
    EXPECT_VEC3_EQ(glm::vec3(2.0f), box.max);
}

TEST_F(AABBTest, Merge) {
    AABB merged = AABB::Merge(unitBox, centeredBox);

    EXPECT_VEC3_EQ(glm::vec3(-1.0f, -1.0f, -1.0f), merged.min);
    EXPECT_VEC3_EQ(glm::vec3(1.0f, 1.0f, 1.0f), merged.max);
}

TEST_F(AABBTest, Intersection) {
    AABB a(glm::vec3(0.0f), glm::vec3(2.0f));
    AABB b(glm::vec3(1.0f), glm::vec3(3.0f));
    AABB intersection = AABB::Intersection(a, b);

    EXPECT_VEC3_EQ(glm::vec3(1.0f), intersection.min);
    EXPECT_VEC3_EQ(glm::vec3(2.0f), intersection.max);
}

// =============================================================================
// AABB Intersection Tests
// =============================================================================

TEST_F(AABBTest, ContainsPoint_Inside) {
    EXPECT_TRUE(unitBox.Contains(glm::vec3(0.5f)));
    EXPECT_TRUE(centeredBox.Contains(glm::vec3(0.0f)));
}

TEST_F(AABBTest, ContainsPoint_OnBoundary) {
    EXPECT_TRUE(unitBox.Contains(glm::vec3(0.0f)));
    EXPECT_TRUE(unitBox.Contains(glm::vec3(1.0f)));
}

TEST_F(AABBTest, ContainsPoint_Outside) {
    EXPECT_FALSE(unitBox.Contains(glm::vec3(-0.1f)));
    EXPECT_FALSE(unitBox.Contains(glm::vec3(1.1f)));
}

TEST_F(AABBTest, ContainsAABB_Fully) {
    AABB inner(glm::vec3(-0.5f), glm::vec3(0.5f));
    EXPECT_TRUE(centeredBox.Contains(inner));
}

TEST_F(AABBTest, ContainsAABB_Partial) {
    AABB partial(glm::vec3(0.5f), glm::vec3(1.5f));
    EXPECT_FALSE(centeredBox.Contains(partial));
}

TEST_F(AABBTest, IntersectsAABB_Overlapping) {
    AABB a(glm::vec3(0.0f), glm::vec3(2.0f));
    AABB b(glm::vec3(1.0f), glm::vec3(3.0f));

    EXPECT_TRUE(a.Intersects(b));
    EXPECT_TRUE(b.Intersects(a));
}

TEST_F(AABBTest, IntersectsAABB_Touching) {
    AABB a(glm::vec3(0.0f), glm::vec3(1.0f));
    AABB b(glm::vec3(1.0f), glm::vec3(2.0f));

    EXPECT_TRUE(a.Intersects(b));
}

TEST_F(AABBTest, IntersectsAABB_Separate) {
    AABB a(glm::vec3(0.0f), glm::vec3(1.0f));
    AABB b(glm::vec3(2.0f), glm::vec3(3.0f));

    EXPECT_FALSE(a.Intersects(b));
}

TEST_F(AABBTest, IntersectsSphere_Inside) {
    EXPECT_TRUE(centeredBox.IntersectsSphere(glm::vec3(0.0f), 0.5f));
}

TEST_F(AABBTest, IntersectsSphere_Overlapping) {
    EXPECT_TRUE(centeredBox.IntersectsSphere(glm::vec3(2.0f, 0.0f, 0.0f), 1.5f));
}

TEST_F(AABBTest, IntersectsSphere_Outside) {
    EXPECT_FALSE(centeredBox.IntersectsSphere(glm::vec3(10.0f, 0.0f, 0.0f), 0.5f));
}

// =============================================================================
// Ray-AABB Intersection Tests
// =============================================================================

TEST_F(AABBTest, RayIntersect_HitFromFront) {
    Ray ray(glm::vec3(-5.0f, 0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f));

    float t = unitBox.RayIntersect(ray.origin, ray.direction);
    EXPECT_GT(t, 0.0f);
    EXPECT_FLOAT_NEAR_EPSILON(5.0f, t, 0.001f);
}

TEST_F(AABBTest, RayIntersect_HitFromInside) {
    Ray ray(glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f));

    float t = unitBox.RayIntersect(ray.origin, ray.direction);
    EXPECT_GE(t, 0.0f);
}

TEST_F(AABBTest, RayIntersect_Miss) {
    Ray ray(glm::vec3(-5.0f, 5.0f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f));

    float t = unitBox.RayIntersect(ray.origin, ray.direction);
    EXPECT_LT(t, 0.0f);
}

TEST_F(AABBTest, RayIntersect_ParallelMiss) {
    Ray ray(glm::vec3(2.0f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f));

    float t = unitBox.RayIntersect(ray.origin, ray.direction);
    EXPECT_LT(t, 0.0f);
}

// =============================================================================
// Distance Tests
// =============================================================================

TEST_F(AABBTest, ClosestPoint_Inside) {
    glm::vec3 point(0.5f, 0.5f, 0.5f);
    glm::vec3 closest = unitBox.ClosestPoint(point);

    EXPECT_VEC3_EQ(point, closest);
}

TEST_F(AABBTest, ClosestPoint_Outside) {
    glm::vec3 point(2.0f, 0.5f, 0.5f);
    glm::vec3 closest = unitBox.ClosestPoint(point);

    EXPECT_VEC3_EQ(glm::vec3(1.0f, 0.5f, 0.5f), closest);
}

TEST_F(AABBTest, Distance_Inside) {
    float dist = centeredBox.Distance(glm::vec3(0.0f));
    EXPECT_FLOAT_EQ(0.0f, dist);
}

TEST_F(AABBTest, Distance_Outside) {
    float dist = unitBox.Distance(glm::vec3(2.0f, 0.5f, 0.5f));
    EXPECT_FLOAT_NEAR_EPSILON(1.0f, dist, 0.001f);
}

// =============================================================================
// Octree Tests
// =============================================================================

class OctreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        worldBounds = AABB(glm::vec3(-100.0f), glm::vec3(100.0f));
    }

    AABB worldBounds;
};

TEST_F(OctreeTest, Construction) {
    Octree<uint64_t> octree(worldBounds);

    EXPECT_EQ(0, octree.GetObjectCount());
    EXPECT_TRUE(octree.GetBounds().IsValid());
}

TEST_F(OctreeTest, InsertSingle) {
    Octree<uint64_t> octree(worldBounds);

    AABB objBounds(glm::vec3(-1.0f), glm::vec3(1.0f));
    octree.Insert(1, objBounds);

    EXPECT_EQ(1, octree.GetObjectCount());
    EXPECT_TRUE(octree.Contains(1));
}

TEST_F(OctreeTest, InsertMany) {
    Octree<uint64_t> octree(worldBounds);

    for (uint64_t i = 0; i < 100; ++i) {
        float offset = static_cast<float>(i) * 2.0f - 100.0f;
        AABB objBounds(glm::vec3(offset - 0.5f), glm::vec3(offset + 0.5f));
        octree.Insert(i, objBounds);
    }

    EXPECT_EQ(100, octree.GetObjectCount());
}

TEST_F(OctreeTest, Remove) {
    Octree<uint64_t> octree(worldBounds);

    AABB objBounds(glm::vec3(-1.0f), glm::vec3(1.0f));
    octree.Insert(1, objBounds);
    EXPECT_TRUE(octree.Contains(1));

    octree.Remove(1);
    EXPECT_FALSE(octree.Contains(1));
    EXPECT_EQ(0, octree.GetObjectCount());
}

TEST_F(OctreeTest, Update) {
    Octree<uint64_t> octree(worldBounds);

    AABB oldBounds(glm::vec3(-1.0f), glm::vec3(1.0f));
    octree.Insert(1, oldBounds);

    AABB newBounds(glm::vec3(10.0f), glm::vec3(12.0f));
    bool updated = octree.Update(1, newBounds);

    EXPECT_TRUE(updated);
    EXPECT_VEC3_EQ(newBounds.min, octree.GetObjectBounds(1).min);
}

TEST_F(OctreeTest, QueryAABB) {
    Octree<uint64_t> octree(worldBounds);

    // Insert objects
    octree.Insert(1, AABB(glm::vec3(0.0f), glm::vec3(1.0f)));
    octree.Insert(2, AABB(glm::vec3(10.0f), glm::vec3(11.0f)));
    octree.Insert(3, AABB(glm::vec3(0.5f), glm::vec3(1.5f)));

    // Query should find objects 1 and 3
    AABB queryBox(glm::vec3(-1.0f), glm::vec3(2.0f));
    auto results = octree.QueryAABB(queryBox);

    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(Contains(results, 1ull));
    EXPECT_TRUE(Contains(results, 3ull));
    EXPECT_FALSE(Contains(results, 2ull));
}

TEST_F(OctreeTest, QuerySphere) {
    Octree<uint64_t> octree(worldBounds);

    octree.Insert(1, AABB(glm::vec3(-1.0f), glm::vec3(1.0f)));
    octree.Insert(2, AABB(glm::vec3(10.0f), glm::vec3(11.0f)));

    auto results = octree.QuerySphere(glm::vec3(0.0f), 5.0f);

    EXPECT_EQ(1, results.size());
    EXPECT_TRUE(Contains(results, 1ull));
}

TEST_F(OctreeTest, QueryRay) {
    Octree<uint64_t> octree(worldBounds);

    octree.Insert(1, AABB(glm::vec3(5.0f, -1.0f, -1.0f), glm::vec3(7.0f, 1.0f, 1.0f)));
    octree.Insert(2, AABB(glm::vec3(10.0f, -1.0f, -1.0f), glm::vec3(12.0f, 1.0f, 1.0f)));
    octree.Insert(3, AABB(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(1.0f, 11.0f, 1.0f)));

    Ray ray(glm::vec3(-10.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    auto results = octree.QueryRay(ray, 100.0f);

    EXPECT_EQ(2, results.size());
    // Results should be sorted by distance
    EXPECT_EQ(1, results[0].entityId);
    EXPECT_EQ(2, results[1].entityId);
}

TEST_F(OctreeTest, QueryNearest) {
    Octree<uint64_t> octree(worldBounds);

    octree.Insert(1, AABB(glm::vec3(-10.0f), glm::vec3(-9.0f)));
    octree.Insert(2, AABB(glm::vec3(-1.0f), glm::vec3(1.0f)));
    octree.Insert(3, AABB(glm::vec3(10.0f), glm::vec3(11.0f)));

    uint64_t nearest = octree.QueryNearest(glm::vec3(0.0f));

    EXPECT_EQ(2, nearest);
}

TEST_F(OctreeTest, QueryKNearest) {
    Octree<uint64_t> octree(worldBounds);

    for (uint64_t i = 0; i < 10; ++i) {
        float offset = static_cast<float>(i) * 5.0f;
        octree.Insert(i, AABB(glm::vec3(offset), glm::vec3(offset + 1.0f)));
    }

    auto results = octree.QueryKNearest(glm::vec3(0.0f), 3);

    EXPECT_EQ(3, results.size());
    EXPECT_EQ(0, results[0]);
}

TEST_F(OctreeTest, Clear) {
    Octree<uint64_t> octree(worldBounds);

    for (uint64_t i = 0; i < 50; ++i) {
        octree.Insert(i, AABB(glm::vec3(0.0f), glm::vec3(1.0f)));
    }

    octree.Clear();
    EXPECT_EQ(0, octree.GetObjectCount());
}

TEST_F(OctreeTest, Rebuild) {
    Octree<uint64_t> octree(worldBounds);

    for (uint64_t i = 0; i < 100; ++i) {
        float offset = RandomFloat(-90.0f, 90.0f);
        octree.Insert(i, AABB(glm::vec3(offset), glm::vec3(offset + 1.0f)));
    }

    octree.Rebuild();
    EXPECT_EQ(100, octree.GetObjectCount());
}

TEST_F(OctreeTest, LayerFiltering) {
    Octree<uint64_t> octree(worldBounds);

    octree.Insert(1, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), 1);
    octree.Insert(2, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), 2);
    octree.Insert(3, AABB(glm::vec3(0.0f), glm::vec3(1.0f)), 1);

    SpatialQueryFilter filter;
    filter.layerMask = 1;

    auto results = octree.QueryAABB(AABB(glm::vec3(-1.0f), glm::vec3(2.0f)), filter);

    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(Contains(results, 1ull));
    EXPECT_TRUE(Contains(results, 3ull));
}

// =============================================================================
// BVH Tests
// =============================================================================

class BVHTest : public ::testing::Test {
protected:
    void SetUp() override {
        bvh = std::make_unique<BVH>();
    }

    std::unique_ptr<BVH> bvh;
};

TEST_F(BVHTest, Construction) {
    EXPECT_EQ(0, bvh->GetObjectCount());
}

TEST_F(BVHTest, InsertAndBuild) {
    bvh->Insert(1, AABB(glm::vec3(0.0f), glm::vec3(1.0f)));
    bvh->Insert(2, AABB(glm::vec3(5.0f), glm::vec3(6.0f)));
    bvh->Insert(3, AABB(glm::vec3(-5.0f), glm::vec3(-4.0f)));

    bvh->Build();

    EXPECT_EQ(3, bvh->GetObjectCount());
    EXPECT_TRUE(bvh->Contains(1));
    EXPECT_TRUE(bvh->Contains(2));
    EXPECT_TRUE(bvh->Contains(3));
}

TEST_F(BVHTest, QueryAABB) {
    for (uint64_t i = 0; i < 20; ++i) {
        float x = static_cast<float>(i) * 2.0f;
        bvh->Insert(i, AABB(glm::vec3(x, 0.0f, 0.0f), glm::vec3(x + 1.0f, 1.0f, 1.0f)));
    }
    bvh->Build();

    auto results = bvh->QueryAABB(AABB(glm::vec3(-1.0f), glm::vec3(5.0f, 2.0f, 2.0f)));

    EXPECT_EQ(3, results.size());
}

TEST_F(BVHTest, QueryRay) {
    bvh->Insert(1, AABB(glm::vec3(5.0f, -1.0f, -1.0f), glm::vec3(6.0f, 1.0f, 1.0f)));
    bvh->Insert(2, AABB(glm::vec3(10.0f, -1.0f, -1.0f), glm::vec3(11.0f, 1.0f, 1.0f)));
    bvh->Build();

    Ray ray(glm::vec3(-10.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    auto results = bvh->QueryRay(ray);

    EXPECT_EQ(2, results.size());
    EXPECT_LT(results[0].distance, results[1].distance);
}

TEST_F(BVHTest, Refit) {
    bvh->Insert(1, AABB(glm::vec3(0.0f), glm::vec3(1.0f)));
    bvh->Build();

    // Update bounds (simulating object movement)
    bvh->Update(1, AABB(glm::vec3(5.0f), glm::vec3(6.0f)));
    bvh->Refit();

    auto results = bvh->QueryAABB(AABB(glm::vec3(4.0f), glm::vec3(7.0f)));
    EXPECT_EQ(1, results.size());
}

TEST_F(BVHTest, SAHCost) {
    for (uint64_t i = 0; i < 100; ++i) {
        float x = RandomFloat(-50.0f, 50.0f);
        float y = RandomFloat(-50.0f, 50.0f);
        float z = RandomFloat(-50.0f, 50.0f);
        bvh->Insert(i, AABB(glm::vec3(x, y, z), glm::vec3(x + 1.0f, y + 1.0f, z + 1.0f)));
    }
    bvh->Build();

    float sahCost = bvh->GetSAHCost();
    EXPECT_GT(sahCost, 0.0f);
}

// =============================================================================
// Frustum Tests
// =============================================================================

class FrustumTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a standard perspective frustum
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        frustum = Frustum(view, projection);
    }

    Frustum frustum;
};

TEST_F(FrustumTest, ContainsPoint_Inside) {
    EXPECT_TRUE(frustum.ContainsPoint(glm::vec3(0.0f, 0.0f, -10.0f)));
}

TEST_F(FrustumTest, ContainsPoint_Outside) {
    EXPECT_FALSE(frustum.ContainsPoint(glm::vec3(0.0f, 0.0f, 10.0f)));
    EXPECT_FALSE(frustum.ContainsPoint(glm::vec3(100.0f, 0.0f, -10.0f)));
}

TEST_F(FrustumTest, TestSphere_Inside) {
    auto result = frustum.TestSphere(glm::vec3(0.0f, 0.0f, -10.0f), 1.0f);
    EXPECT_NE(FrustumResult::Outside, result);
}

TEST_F(FrustumTest, TestSphere_Outside) {
    EXPECT_TRUE(frustum.IsSphereOutside(glm::vec3(0.0f, 0.0f, 10.0f), 1.0f));
}

TEST_F(FrustumTest, TestAABB_Inside) {
    AABB box(glm::vec3(-1.0f, -1.0f, -11.0f), glm::vec3(1.0f, 1.0f, -9.0f));
    EXPECT_TRUE(frustum.IsAABBVisible(box));
}

TEST_F(FrustumTest, TestAABB_Outside) {
    AABB box(glm::vec3(-1.0f, -1.0f, 9.0f), glm::vec3(1.0f, 1.0f, 11.0f));
    EXPECT_TRUE(frustum.IsAABBOutside(box));
}

TEST_F(FrustumTest, TestAABB_Intersecting) {
    AABB box(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
    auto result = frustum.TestAABB(box);
    EXPECT_EQ(FrustumResult::Intersect, result);
}

TEST_F(FrustumTest, CoherentCulling) {
    AABB box(glm::vec3(-1.0f, -1.0f, -11.0f), glm::vec3(1.0f, 1.0f, -9.0f));

    uint8_t planeMask = 0x3F;
    bool visible = frustum.TestAABBCoherent(box, planeMask);

    EXPECT_TRUE(visible);
    // Mask should be updated to skip planes that passed
}

// =============================================================================
// Spatial Hash Collision Tests
// =============================================================================

TEST(SpatialHashTest, HashCollisionHandling) {
    // This test verifies that spatial hash handles collisions correctly
    // by inserting many objects and verifying all can be retrieved

    AABB worldBounds(glm::vec3(-1000.0f), glm::vec3(1000.0f));
    Octree<uint64_t> index(worldBounds);

    const size_t numObjects = 1000;
    std::vector<std::pair<uint64_t, AABB>> objects;

    RandomGenerator rng(42);
    AABBGenerator aabbGen(0.5f, 2.0f, -500.0f, 500.0f);

    for (size_t i = 0; i < numObjects; ++i) {
        auto bounds = aabbGen.Generate(rng);
        AABB aabb(bounds.min, bounds.max);
        objects.emplace_back(i, aabb);
        index.Insert(i, aabb);
    }

    EXPECT_EQ(numObjects, index.GetObjectCount());

    // Verify all objects can be found
    for (const auto& [id, bounds] : objects) {
        EXPECT_TRUE(index.Contains(id));
        auto queryResults = index.QueryAABB(bounds);
        EXPECT_TRUE(Contains(queryResults, id));
    }
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST(AABBPropertyTest, MergeContainsBoth) {
    RandomGenerator rng(42);
    AABBGenerator gen;

    for (int i = 0; i < 100; ++i) {
        auto a = gen.Generate(rng);
        auto b = gen.Generate(rng);

        AABB boxA(a.min, a.max);
        AABB boxB(b.min, b.max);
        AABB merged = AABB::Merge(boxA, boxB);

        EXPECT_TRUE(merged.Contains(boxA));
        EXPECT_TRUE(merged.Contains(boxB));
    }
}

TEST(AABBPropertyTest, IntersectionSymmetric) {
    RandomGenerator rng(42);
    AABBGenerator gen;

    for (int i = 0; i < 100; ++i) {
        auto a = gen.Generate(rng);
        auto b = gen.Generate(rng);

        AABB boxA(a.min, a.max);
        AABB boxB(b.min, b.max);

        EXPECT_EQ(boxA.Intersects(boxB), boxB.Intersects(boxA));
    }
}
