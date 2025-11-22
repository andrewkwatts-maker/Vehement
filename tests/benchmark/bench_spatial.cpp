/**
 * @file bench_spatial.cpp
 * @brief Performance benchmarks for spatial data structures
 */

#include <benchmark/benchmark.h>

#include "spatial/Octree.hpp"
#include "spatial/BVH.hpp"
#include "spatial/AABB.hpp"
#include "spatial/Frustum.hpp"

#include "utils/Generators.hpp"

#include <random>
#include <vector>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Benchmark Fixtures
// =============================================================================

class SpatialBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) {
        rng = RandomGenerator(42);
        posGen = Vec3Generator(-1000.0f, 1000.0f);
        sizeGen = FloatGenerator(0.5f, 5.0f);
    }

    RandomGenerator rng{42};
    Vec3Generator posGen{-1000.0f, 1000.0f};
    FloatGenerator sizeGen{0.5f, 5.0f};
};

// =============================================================================
// AABB Benchmarks
// =============================================================================

static void BM_AABB_Construction(benchmark::State& state) {
    for (auto _ : state) {
        AABB box(glm::vec3(-1.0f), glm::vec3(1.0f));
        benchmark::DoNotOptimize(box);
    }
}
BENCHMARK(BM_AABB_Construction);

static void BM_AABB_Intersection(benchmark::State& state) {
    AABB box1(glm::vec3(0.0f), glm::vec3(2.0f));
    AABB box2(glm::vec3(1.0f), glm::vec3(3.0f));

    for (auto _ : state) {
        bool result = box1.Intersects(box2);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_AABB_Intersection);

static void BM_AABB_Contains_Point(benchmark::State& state) {
    AABB box(glm::vec3(0.0f), glm::vec3(10.0f));
    glm::vec3 point(5.0f, 5.0f, 5.0f);

    for (auto _ : state) {
        bool result = box.Contains(point);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_AABB_Contains_Point);

static void BM_AABB_Contains_Box(benchmark::State& state) {
    AABB outer(glm::vec3(0.0f), glm::vec3(10.0f));
    AABB inner(glm::vec3(2.0f), glm::vec3(8.0f));

    for (auto _ : state) {
        bool result = outer.Contains(inner);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_AABB_Contains_Box);

static void BM_AABB_RayIntersection(benchmark::State& state) {
    AABB box(glm::vec3(0.0f), glm::vec3(10.0f));
    glm::vec3 rayOrigin(-5.0f, 5.0f, 5.0f);
    glm::vec3 rayDir = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    for (auto _ : state) {
        float t;
        bool result = box.IntersectsRay(rayOrigin, rayDir, t);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(t);
    }
}
BENCHMARK(BM_AABB_RayIntersection);

static void BM_AABB_Merge(benchmark::State& state) {
    AABB box1(glm::vec3(0.0f), glm::vec3(5.0f));
    AABB box2(glm::vec3(3.0f), glm::vec3(10.0f));

    for (auto _ : state) {
        AABB merged = AABB::Merge(box1, box2);
        benchmark::DoNotOptimize(merged);
    }
}
BENCHMARK(BM_AABB_Merge);

static void BM_AABB_SurfaceArea(benchmark::State& state) {
    AABB box(glm::vec3(0.0f), glm::vec3(10.0f, 20.0f, 30.0f));

    for (auto _ : state) {
        float area = box.SurfaceArea();
        benchmark::DoNotOptimize(area);
    }
}
BENCHMARK(BM_AABB_SurfaceArea);

// =============================================================================
// Octree Benchmarks
// =============================================================================

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_Insert)(benchmark::State& state) {
    int count = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
        state.ResumeTiming();

        for (int i = 0; i < count; ++i) {
            glm::vec3 pos = posGen.Generate(rng);
            float size = sizeGen.Generate(rng);
            AABB bounds(pos - glm::vec3(size), pos + glm::vec3(size));
            tree.Insert(i, bounds);
        }
    }

    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_Insert)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_Query_AABB)(benchmark::State& state) {
    int objectCount = state.range(0);

    // Build tree once
    Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
    for (int i = 0; i < objectCount; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        AABB bounds(pos - glm::vec3(size), pos + glm::vec3(size));
        tree.Insert(i, bounds);
    }

    AABB queryBox(glm::vec3(-50.0f), glm::vec3(50.0f));

    for (auto _ : state) {
        std::vector<int> results;
        tree.Query(queryBox, results);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_Query_AABB)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_Query_Sphere)(benchmark::State& state) {
    int objectCount = state.range(0);

    Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
    for (int i = 0; i < objectCount; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        AABB bounds(pos - glm::vec3(size), pos + glm::vec3(size));
        tree.Insert(i, bounds);
    }

    glm::vec3 center(0.0f);
    float radius = 100.0f;

    for (auto _ : state) {
        std::vector<int> results;
        tree.QuerySphere(center, radius, results);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_Query_Sphere)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_Query_Ray)(benchmark::State& state) {
    int objectCount = state.range(0);

    Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
    for (int i = 0; i < objectCount; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        AABB bounds(pos - glm::vec3(size), pos + glm::vec3(size));
        tree.Insert(i, bounds);
    }

    glm::vec3 origin(-500.0f, 0.0f, 0.0f);
    glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    for (auto _ : state) {
        std::vector<int> results;
        tree.QueryRay(origin, direction, 2000.0f, results);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_Query_Ray)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_Remove)(benchmark::State& state) {
    int objectCount = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
        std::vector<AABB> bounds;

        for (int i = 0; i < objectCount; ++i) {
            glm::vec3 pos = posGen.Generate(rng);
            float size = sizeGen.Generate(rng);
            AABB b(pos - glm::vec3(size), pos + glm::vec3(size));
            bounds.push_back(b);
            tree.Insert(i, b);
        }
        state.ResumeTiming();

        // Remove half the objects
        for (int i = 0; i < objectCount / 2; ++i) {
            tree.Remove(i, bounds[i]);
        }
    }

    state.SetItemsProcessed(state.iterations() * (objectCount / 2));
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_Remove)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

// =============================================================================
// BVH Benchmarks
// =============================================================================

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_BVH_Build)(benchmark::State& state) {
    int count = state.range(0);

    std::vector<AABB> boxes;
    boxes.reserve(count);
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        boxes.emplace_back(pos - glm::vec3(size), pos + glm::vec3(size));
    }

    for (auto _ : state) {
        BVH bvh;
        bvh.Build(boxes);
        benchmark::DoNotOptimize(bvh);
    }

    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_BVH_Build)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_BVH_Query_AABB)(benchmark::State& state) {
    int count = state.range(0);

    std::vector<AABB> boxes;
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        boxes.emplace_back(pos - glm::vec3(size), pos + glm::vec3(size));
    }

    BVH bvh;
    bvh.Build(boxes);

    AABB queryBox(glm::vec3(-100.0f), glm::vec3(100.0f));

    for (auto _ : state) {
        std::vector<int> results;
        bvh.Query(queryBox, results);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_BVH_Query_AABB)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_BVH_Query_Ray)(benchmark::State& state) {
    int count = state.range(0);

    std::vector<AABB> boxes;
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        boxes.emplace_back(pos - glm::vec3(size), pos + glm::vec3(size));
    }

    BVH bvh;
    bvh.Build(boxes);

    glm::vec3 origin(-500.0f, 0.0f, 0.0f);
    glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    for (auto _ : state) {
        std::vector<int> results;
        bvh.QueryRay(origin, direction, 2000.0f, results);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_BVH_Query_Ray)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_BVH_Refit)(benchmark::State& state) {
    int count = state.range(0);

    std::vector<AABB> boxes;
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        boxes.emplace_back(pos - glm::vec3(size), pos + glm::vec3(size));
    }

    BVH bvh;
    bvh.Build(boxes);

    for (auto _ : state) {
        // Move some boxes
        state.PauseTiming();
        for (int i = 0; i < count / 10; ++i) {
            glm::vec3 offset(0.1f, 0.0f, 0.0f);
            boxes[i].min += offset;
            boxes[i].max += offset;
        }
        state.ResumeTiming();

        bvh.Refit(boxes);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_BVH_Refit)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

// =============================================================================
// Frustum Culling Benchmarks
// =============================================================================

static void BM_Frustum_Construction(benchmark::State& state) {
    glm::mat4 viewProj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f) *
                         glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    for (auto _ : state) {
        Frustum frustum(viewProj);
        benchmark::DoNotOptimize(frustum);
    }
}
BENCHMARK(BM_Frustum_Construction);

static void BM_Frustum_Contains_Point(benchmark::State& state) {
    glm::mat4 viewProj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f) *
                         glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum(viewProj);

    glm::vec3 point(0.0f, 0.0f, -50.0f);

    for (auto _ : state) {
        bool result = frustum.ContainsPoint(point);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Frustum_Contains_Point);

static void BM_Frustum_Contains_Sphere(benchmark::State& state) {
    glm::mat4 viewProj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f) *
                         glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum(viewProj);

    glm::vec3 center(0.0f, 0.0f, -50.0f);
    float radius = 5.0f;

    for (auto _ : state) {
        FrustumResult result = frustum.ContainsSphere(center, radius);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Frustum_Contains_Sphere);

static void BM_Frustum_Contains_AABB(benchmark::State& state) {
    glm::mat4 viewProj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f) *
                         glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum(viewProj);

    AABB box(glm::vec3(-5.0f, -5.0f, -55.0f), glm::vec3(5.0f, 5.0f, -45.0f));

    for (auto _ : state) {
        FrustumResult result = frustum.ContainsAABB(box);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Frustum_Contains_AABB);

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Frustum_CullMany)(benchmark::State& state) {
    int count = state.range(0);

    glm::mat4 viewProj = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 1000.0f) *
                         glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum(viewProj);

    std::vector<AABB> boxes;
    Vec3Generator bigPosGen(-500.0f, 500.0f);
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = bigPosGen.Generate(rng);
        pos.z = -pos.z - 50.0f;  // In front of camera
        float size = sizeGen.Generate(rng);
        boxes.emplace_back(pos - glm::vec3(size), pos + glm::vec3(size));
    }

    for (auto _ : state) {
        int visibleCount = 0;
        for (const auto& box : boxes) {
            if (frustum.ContainsAABB(box) != FrustumResult::Outside) {
                visibleCount++;
            }
        }
        benchmark::DoNotOptimize(visibleCount);
    }

    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Frustum_CullMany)
    ->RangeMultiplier(10)
    ->Range(100, 100000);

// =============================================================================
// Comparison Benchmarks: Octree vs BVH
// =============================================================================

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_vs_BVH_Query)(benchmark::State& state) {
    int count = state.range(0);
    bool useBVH = state.range(1);

    std::vector<AABB> boxes;
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        float size = sizeGen.Generate(rng);
        boxes.emplace_back(pos - glm::vec3(size), pos + glm::vec3(size));
    }

    AABB queryBox(glm::vec3(-100.0f), glm::vec3(100.0f));

    if (useBVH) {
        BVH bvh;
        bvh.Build(boxes);

        for (auto _ : state) {
            std::vector<int> results;
            bvh.Query(queryBox, results);
            benchmark::DoNotOptimize(results);
        }
    } else {
        Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
        for (int i = 0; i < count; ++i) {
            tree.Insert(i, boxes[i]);
        }

        for (auto _ : state) {
            std::vector<int> results;
            tree.Query(queryBox, results);
            benchmark::DoNotOptimize(results);
        }
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_vs_BVH_Query)
    ->Args({1000, 0})   // Octree, 1000 objects
    ->Args({1000, 1})   // BVH, 1000 objects
    ->Args({10000, 0})  // Octree, 10000 objects
    ->Args({10000, 1}); // BVH, 10000 objects

// =============================================================================
// Nearest Neighbor Query Benchmark
// =============================================================================

BENCHMARK_DEFINE_F(SpatialBenchmark, BM_Octree_NearestNeighbor)(benchmark::State& state) {
    int count = state.range(0);

    Octree<int> tree(AABB(glm::vec3(-1000.0f), glm::vec3(1000.0f)), 10);
    std::vector<glm::vec3> positions;

    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        positions.push_back(pos);
        float size = 1.0f;
        tree.Insert(i, AABB(pos - glm::vec3(size), pos + glm::vec3(size)));
    }

    glm::vec3 queryPoint(0.0f);

    for (auto _ : state) {
        auto result = tree.FindNearest(queryPoint);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(SpatialBenchmark, BM_Octree_NearestNeighbor)
    ->RangeMultiplier(10)
    ->Range(100, 10000);

// Main function for benchmark
BENCHMARK_MAIN();

