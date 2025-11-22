# Spatial Systems API

The spatial systems provide efficient spatial indexing and queries for game objects, supporting frustum culling, collision detection, and proximity searches.

## ISpatialIndex Interface

**Header**: `engine/spatial/SpatialIndex.hpp`

Unified interface for all spatial acceleration structures.

### Object Management

#### Insert

```cpp
virtual void Insert(uint64_t id, const AABB& bounds, uint64_t layer = 0) = 0;
```

Insert an object into the index.

- `id`: Unique identifier for the object
- `bounds`: Axis-aligned bounding box
- `layer`: Layer for filtering (default 0)

#### Remove

```cpp
virtual bool Remove(uint64_t id) = 0;
```

Remove object by ID. Returns true if found and removed.

#### Update

```cpp
virtual bool Update(uint64_t id, const AABB& newBounds) = 0;
```

Update object bounds. Returns true if found and updated.

#### Clear

```cpp
virtual void Clear() = 0;
```

Remove all objects.

#### Rebuild

```cpp
virtual void Rebuild() {}
```

Rebuild index structure. Useful for BVH after many updates.

### Queries

#### QueryAABB

```cpp
[[nodiscard]] virtual std::vector<uint64_t> QueryAABB(
    const AABB& query,
    const SpatialQueryFilter& filter = {}) = 0;
```

Find objects intersecting an AABB.

#### QuerySphere

```cpp
[[nodiscard]] virtual std::vector<uint64_t> QuerySphere(
    const glm::vec3& center,
    float radius,
    const SpatialQueryFilter& filter = {}) = 0;
```

Find objects intersecting a sphere.

#### QueryFrustum

```cpp
[[nodiscard]] virtual std::vector<uint64_t> QueryFrustum(
    const Frustum& frustum,
    const SpatialQueryFilter& filter = {}) = 0;
```

Find objects inside a view frustum (frustum culling).

#### QueryRay

```cpp
[[nodiscard]] virtual std::vector<RayHit> QueryRay(
    const Ray& ray,
    float maxDist = std::numeric_limits<float>::max(),
    const SpatialQueryFilter& filter = {}) = 0;
```

Cast a ray and find intersecting objects. Results sorted by distance.

#### QueryNearest

```cpp
[[nodiscard]] virtual uint64_t QueryNearest(
    const glm::vec3& point,
    float maxDist = std::numeric_limits<float>::max(),
    const SpatialQueryFilter& filter = {}) = 0;
```

Find the nearest object to a point. Returns 0 if none found.

#### QueryKNearest

```cpp
[[nodiscard]] virtual std::vector<uint64_t> QueryKNearest(
    const glm::vec3& point,
    size_t k,
    float maxDist = std::numeric_limits<float>::max(),
    const SpatialQueryFilter& filter = {}) = 0;
```

Find K nearest objects to a point.

### Callback Queries

```cpp
using VisitorCallback = std::function<bool(uint64_t id, const AABB& bounds)>;

virtual void QueryAABB(
    const AABB& query,
    const VisitorCallback& callback,
    const SpatialQueryFilter& filter = {}) = 0;

virtual void QuerySphere(
    const glm::vec3& center,
    float radius,
    const VisitorCallback& callback,
    const SpatialQueryFilter& filter = {}) = 0;
```

Query with callback instead of returning vector. Return false from callback to stop iteration.

### Information

```cpp
[[nodiscard]] virtual size_t GetObjectCount() const noexcept = 0;
[[nodiscard]] virtual AABB GetBounds() const noexcept = 0;
[[nodiscard]] virtual size_t GetMemoryUsage() const noexcept = 0;
[[nodiscard]] virtual std::string_view GetTypeName() const noexcept = 0;
[[nodiscard]] virtual const SpatialQueryStats& GetLastQueryStats() const noexcept = 0;
[[nodiscard]] virtual bool SupportsMovingObjects() const noexcept { return false; }
[[nodiscard]] virtual AABB GetObjectBounds(uint64_t id) const noexcept = 0;
[[nodiscard]] virtual bool Contains(uint64_t id) const noexcept = 0;
```

---

## SpatialQueryFilter Struct

Filter for spatial queries.

```cpp
struct SpatialQueryFilter {
    uint64_t layerMask = ~0ULL;   // Bitmask of layers to include
    uint64_t excludeId = 0;       // ID to exclude from results
    bool sortByDistance = false;  // Sort results by distance

    [[nodiscard]] bool PassesFilter(uint64_t id, uint64_t layer) const noexcept;
};
```

---

## SpatialQueryStats Struct

Performance statistics from queries.

```cpp
struct SpatialQueryStats {
    size_t nodesVisited = 0;
    size_t objectsTested = 0;
    size_t objectsReturned = 0;
    float queryTimeMs = 0.0f;

    void Reset() noexcept;
};
```

---

## SpatialIndexType Enum

```cpp
enum class SpatialIndexType {
    Octree,       // Standard octree - good for static scenes
    LooseOctree,  // Loose octree - better for dynamic objects
    BVH,          // Bounding Volume Hierarchy - optimal for ray queries
    SpatialHash,  // Spatial hashing - best for uniform distributions
    Auto          // Automatically choose based on object distribution
};
```

---

## Factory Function

```cpp
[[nodiscard]] std::unique_ptr<ISpatialIndex> CreateSpatialIndex(
    SpatialIndexType type,
    const AABB& worldBounds = AABB(glm::vec3(-1000), glm::vec3(1000)),
    float cellSize = 10.0f);
```

Create a spatial index of the specified type.

---

## AABB Class

Axis-aligned bounding box.

### Constructor

```cpp
AABB();
AABB(const glm::vec3& min, const glm::vec3& max);
AABB(const glm::vec3& center, float halfExtent);
```

### Methods

```cpp
[[nodiscard]] glm::vec3 GetCenter() const;
[[nodiscard]] glm::vec3 GetSize() const;
[[nodiscard]] glm::vec3 GetHalfExtents() const;
[[nodiscard]] float GetSurfaceArea() const;
[[nodiscard]] float GetVolume() const;
[[nodiscard]] bool Contains(const glm::vec3& point) const;
[[nodiscard]] bool Contains(const AABB& other) const;
[[nodiscard]] bool Intersects(const AABB& other) const;
[[nodiscard]] bool Intersects(const Sphere& sphere) const;
[[nodiscard]] AABB Merge(const AABB& other) const;
void Expand(const glm::vec3& point);
void Expand(const AABB& other);
[[nodiscard]] bool IsValid() const;
```

---

## Ray Struct

```cpp
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;

    [[nodiscard]] glm::vec3 GetPoint(float t) const;
};
```

---

## RayHit Struct

```cpp
struct RayHit {
    uint64_t objectId;
    float distance;
    glm::vec3 point;
    glm::vec3 normal;
};
```

---

## Frustum Class

View frustum for culling.

### Methods

```cpp
void Update(const glm::mat4& viewProjection);
[[nodiscard]] bool Contains(const glm::vec3& point) const;
[[nodiscard]] bool Intersects(const AABB& aabb) const;
[[nodiscard]] bool Intersects(const Sphere& sphere) const;
[[nodiscard]] const std::array<Plane, 6>& GetPlanes() const;
```

---

## Usage Examples

### Basic Spatial Index

```cpp
#include <engine/spatial/SpatialIndex.hpp>

// Create octree for a 2000x2000x2000 world
auto spatial = Nova::CreateSpatialIndex(
    Nova::SpatialIndexType::Octree,
    Nova::AABB(glm::vec3(-1000), glm::vec3(1000))
);

// Insert objects
for (auto& entity : entities) {
    spatial->Insert(entity.id, entity.GetBounds(), entity.layer);
}

// Query
auto nearby = spatial->QuerySphere(playerPos, 50.0f);
for (uint64_t id : nearby) {
    // Process nearby object
}
```

### Frustum Culling

```cpp
// Update frustum from camera
Frustum frustum;
frustum.Update(camera.GetViewProjectionMatrix());

// Query visible objects
auto visible = spatial->QueryFrustum(frustum);

// Render only visible objects
for (uint64_t id : visible) {
    RenderObject(id);
}
```

### Filtered Query

```cpp
// Only query layer 1 and 2, exclude player
SpatialQueryFilter filter;
filter.layerMask = (1ULL << 1) | (1ULL << 2);
filter.excludeId = playerId;

auto enemies = spatial->QuerySphere(playerPos, 100.0f, filter);
```

### Ray Casting

```cpp
// Cast ray from camera
Ray ray;
ray.origin = camera.GetPosition();
ray.direction = camera.GetForward();

auto hits = spatial->QueryRay(ray, 1000.0f);
if (!hits.empty()) {
    // Process first hit
    auto& hit = hits[0];
    HighlightObject(hit.objectId);
}
```

### Callback Query (No Allocation)

```cpp
int enemyCount = 0;
spatial->QuerySphere(position, radius, [&](uint64_t id, const AABB& bounds) {
    if (IsEnemy(id)) {
        enemyCount++;
    }
    return true;  // Continue searching
});
```

### Dynamic Objects

```cpp
// Use loose octree for frequently moving objects
auto dynamicIndex = CreateSpatialIndex(SpatialIndexType::LooseOctree);

// Update positions each frame
for (auto& entity : movingEntities) {
    dynamicIndex->Update(entity.id, entity.GetBounds());
}
```

### Choosing Index Type

| Use Case | Recommended Type |
|----------|------------------|
| Static level geometry | Octree or BVH |
| Many moving objects | LooseOctree |
| Ray tracing / picking | BVH |
| Uniform object distribution | SpatialHash |
| Mixed workload | Auto |

### Performance Tips

1. **Choose appropriate cell size** for SpatialHash based on average object size
2. **Rebuild BVH periodically** after many updates
3. **Use callback queries** to avoid allocations in hot paths
4. **Use layer masks** to reduce query scope
5. **Profile with GetLastQueryStats()** to identify bottlenecks
