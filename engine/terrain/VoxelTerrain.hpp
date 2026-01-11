#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <array>
#include <mutex>

namespace Nova {

/**
 * @brief Voxel material type
 */
enum class VoxelMaterial : uint8_t {
    Air = 0,
    Dirt,
    Stone,
    Sand,
    Grass,
    Water,
    Lava,
    Snow,
    Ice,
    Mud,
    Clay,
    Gravel,
    Ore,
    Crystal,
    Custom
};

/**
 * @brief Single voxel data
 */
struct Voxel {
    float density = 0.0f;           // SDF-style density (negative = inside, positive = outside)
    VoxelMaterial material = VoxelMaterial::Air;
    uint8_t flags = 0;              // Custom flags
    glm::vec3 color{0.5f, 0.5f, 0.5f};

    [[nodiscard]] bool IsSolid() const { return density < 0.0f; }
};

/**
 * @brief Chunk of voxel data
 */
class VoxelChunk {
public:
    static constexpr int SIZE = 32;
    static constexpr int TOTAL_VOXELS = SIZE * SIZE * SIZE;

    VoxelChunk(const glm::ivec3& position);

    /**
     * @brief Get voxel at local position
     */
    [[nodiscard]] Voxel& GetVoxel(int x, int y, int z);
    [[nodiscard]] const Voxel& GetVoxel(int x, int y, int z) const;

    /**
     * @brief Set voxel at local position
     */
    void SetVoxel(int x, int y, int z, const Voxel& voxel);

    /**
     * @brief Get chunk position (in chunk coordinates)
     */
    [[nodiscard]] const glm::ivec3& GetPosition() const { return m_position; }

    /**
     * @brief Check if chunk needs mesh rebuild
     */
    [[nodiscard]] bool NeedsMeshRebuild() const { return m_needsMeshRebuild; }
    void SetNeedsMeshRebuild(bool needs) { m_needsMeshRebuild = needs; }

    /**
     * @brief Check if chunk is empty (all air)
     */
    [[nodiscard]] bool IsEmpty() const;

    /**
     * @brief Check if chunk is solid (all solid)
     */
    [[nodiscard]] bool IsSolid() const;

    // Mesh data
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> colors;
    std::vector<uint32_t> indices;

private:
    glm::ivec3 m_position;
    std::array<Voxel, TOTAL_VOXELS> m_voxels;
    bool m_needsMeshRebuild = true;

    [[nodiscard]] int GetIndex(int x, int y, int z) const {
        return x + y * SIZE + z * SIZE * SIZE;
    }
};

/**
 * @brief Marching cubes triangle table entry
 */
struct MCTriangle {
    int vertices[3];
};

/**
 * @brief Marching Cubes mesh generator
 */
class MarchingCubes {
public:
    MarchingCubes();

    /**
     * @brief Generate mesh for a chunk
     * @param chunk The chunk to generate mesh for
     * @param isoLevel Surface threshold (0.0 for SDF)
     */
    void GenerateMesh(VoxelChunk& chunk, float isoLevel = 0.0f);

    /**
     * @brief Set interpolation enabled
     */
    void SetInterpolation(bool enabled) { m_useInterpolation = enabled; }

    /**
     * @brief Set smooth normals
     */
    void SetSmoothNormals(bool enabled) { m_smoothNormals = enabled; }

private:
    // Interpolate vertex position along edge
    glm::vec3 InterpolateVertex(const glm::vec3& p1, const glm::vec3& p2, float v1, float v2, float isoLevel);

    // Calculate gradient normal at position
    glm::vec3 CalculateNormal(VoxelChunk& chunk, const glm::vec3& pos);

    bool m_useInterpolation = true;
    bool m_smoothNormals = true;

    // Marching cubes lookup tables
    static const int edgeTable[256];
    static const int triTable[256][16];
};

/**
 * @brief SDF operation type
 */
enum class SDFOperation {
    Union,
    Subtract,
    Intersect,
    SmoothUnion,
    SmoothSubtract,
    SmoothIntersect
};

/**
 * @brief SDF brush shape
 */
enum class SDFBrushShape {
    Sphere,
    Box,
    Cylinder,
    Capsule,
    Cone,
    Torus,
    Custom
};

/**
 * @brief SDF brush for terrain modification
 */
struct SDFBrush {
    SDFBrushShape shape = SDFBrushShape::Sphere;
    SDFOperation operation = SDFOperation::Union;
    glm::vec3 position{0.0f};
    glm::vec3 size{1.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float smoothness = 0.5f;
    VoxelMaterial material = VoxelMaterial::Dirt;
    glm::vec3 color{0.5f, 0.4f, 0.3f};

    // Custom SDF function
    std::function<float(const glm::vec3&)> customSDF;

    /**
     * @brief Evaluate SDF at position
     */
    [[nodiscard]] float Evaluate(const glm::vec3& worldPos) const;
};

/**
 * @brief Terrain modification record for undo/redo
 */
struct TerrainModification {
    glm::ivec3 chunkPos;
    std::vector<std::pair<glm::ivec3, Voxel>> originalVoxels;
    std::vector<std::pair<glm::ivec3, Voxel>> newVoxels;
};

/**
 * @brief Voxel terrain system with marching cubes and SDF support
 *
 * Features:
 * - Voxel-based terrain with SDF density values
 * - Marching cubes mesh generation
 * - SDF boolean operations (union, subtract, intersect)
 * - Smooth blending between operations
 * - LOD support
 * - Spatial partitioning with octree
 * - Cave/tunnel/overhang support
 * - Material support per voxel
 */
class VoxelTerrain {
public:
    struct Config {
        float voxelSize = 1.0f;
        int chunkSize = VoxelChunk::SIZE;
        int viewDistance = 8;  // In chunks
        int maxLODLevels = 4;
        bool useOctree = true;
        bool asyncMeshGeneration = true;
        int maxMeshesPerFrame = 4;
    };

    VoxelTerrain();
    ~VoxelTerrain();

    /**
     * @brief Initialize terrain
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Update terrain (mesh generation, LOD, etc.)
     */
    void Update(const glm::vec3& cameraPosition, float deltaTime);

    // =========================================================================
    // Voxel Access
    // =========================================================================

    /**
     * @brief Get voxel at world position
     */
    [[nodiscard]] Voxel GetVoxel(const glm::vec3& worldPos) const;
    [[nodiscard]] Voxel GetVoxel(int x, int y, int z) const;

    /**
     * @brief Set voxel at world position
     */
    void SetVoxel(const glm::vec3& worldPos, const Voxel& voxel);
    void SetVoxel(int x, int y, int z, const Voxel& voxel);

    /**
     * @brief Sample terrain density at world position (interpolated)
     */
    [[nodiscard]] float SampleDensity(const glm::vec3& worldPos) const;

    /**
     * @brief Get terrain height at XZ position (raycast down)
     */
    [[nodiscard]] float GetHeightAt(float x, float z) const;

    /**
     * @brief Get terrain normal at world position
     */
    [[nodiscard]] glm::vec3 GetNormalAt(const glm::vec3& worldPos) const;

    // =========================================================================
    // SDF Modifications
    // =========================================================================

    /**
     * @brief Apply SDF brush to terrain
     */
    TerrainModification ApplyBrush(const SDFBrush& brush);

    /**
     * @brief Apply SDF sphere (convenience)
     */
    TerrainModification ApplySphere(const glm::vec3& center, float radius, SDFOperation op, VoxelMaterial material = VoxelMaterial::Dirt);

    /**
     * @brief Apply SDF box (convenience)
     */
    TerrainModification ApplyBox(const glm::vec3& center, const glm::vec3& size, const glm::quat& rotation, SDFOperation op, VoxelMaterial material = VoxelMaterial::Dirt);

    /**
     * @brief Apply SDF cylinder (convenience)
     */
    TerrainModification ApplyCylinder(const glm::vec3& base, float height, float radius, SDFOperation op, VoxelMaterial material = VoxelMaterial::Dirt);

    /**
     * @brief Dig tunnel between two points
     */
    TerrainModification DigTunnel(const glm::vec3& start, const glm::vec3& end, float radius, float smoothness = 0.5f);

    /**
     * @brief Create cave at position
     */
    TerrainModification CreateCave(const glm::vec3& center, const glm::vec3& size, float noiseScale = 1.0f, int seed = 0);

    /**
     * @brief Smooth terrain at position
     */
    void SmoothTerrain(const glm::vec3& center, float radius, float strength = 0.5f);

    /**
     * @brief Flatten terrain at position
     */
    void FlattenTerrain(const glm::vec3& center, float radius, float targetHeight, float strength = 0.5f);

    /**
     * @brief Paint material at position
     */
    void PaintMaterial(const glm::vec3& center, float radius, VoxelMaterial material, const glm::vec3& color);

    // =========================================================================
    // Chunk Management
    // =========================================================================

    /**
     * @brief Get chunk at chunk coordinates
     */
    [[nodiscard]] VoxelChunk* GetChunk(const glm::ivec3& chunkPos);
    [[nodiscard]] const VoxelChunk* GetChunk(const glm::ivec3& chunkPos) const;

    /**
     * @brief Create chunk at chunk coordinates
     */
    VoxelChunk* CreateChunk(const glm::ivec3& chunkPos);

    /**
     * @brief Remove chunk at chunk coordinates
     */
    void RemoveChunk(const glm::ivec3& chunkPos);

    /**
     * @brief Get all loaded chunks
     */
    [[nodiscard]] const std::unordered_map<uint64_t, std::unique_ptr<VoxelChunk>>& GetChunks() const { return m_chunks; }

    /**
     * @brief Force mesh rebuild for chunks in region
     */
    void RebuildMeshes(const glm::vec3& minWorld, const glm::vec3& maxWorld);

    /**
     * @brief Rebuild all meshes
     */
    void RebuildAllMeshes();

    // =========================================================================
    // Terrain Generation
    // =========================================================================

    /**
     * @brief Generate terrain using noise
     */
    void GenerateTerrain(int seed, float scale, int octaves, float persistence, float lacunarity);

    /**
     * @brief Generate flat terrain at height
     */
    void GenerateFlatTerrain(float height);

    /**
     * @brief Set custom terrain generator
     */
    void SetTerrainGenerator(std::function<float(const glm::vec3&)> generator) { m_terrainGenerator = std::move(generator); }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Save terrain to file
     */
    bool SaveTerrain(const std::string& path) const;

    /**
     * @brief Load terrain from file
     */
    bool LoadTerrain(const std::string& path);

    /**
     * @brief Save chunk to file
     */
    bool SaveChunk(const glm::ivec3& chunkPos, const std::string& path) const;

    /**
     * @brief Load chunk from file
     */
    bool LoadChunk(const std::string& path);

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last modification
     */
    void Undo();

    /**
     * @brief Redo last undone modification
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    // =========================================================================
    // Raycasting
    // =========================================================================

    /**
     * @brief Raycast against terrain
     */
    bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

    /**
     * @brief Get all chunks intersected by ray
     */
    std::vector<glm::ivec3> GetChunksAlongRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Convert world position to chunk position
     */
    [[nodiscard]] glm::ivec3 WorldToChunk(const glm::vec3& worldPos) const;

    /**
     * @brief Convert world position to local voxel position
     */
    [[nodiscard]] glm::ivec3 WorldToLocal(const glm::vec3& worldPos, const glm::ivec3& chunkPos) const;

    /**
     * @brief Convert local voxel position to world position
     */
    [[nodiscard]] glm::vec3 LocalToWorld(const glm::ivec3& localPos, const glm::ivec3& chunkPos) const;

    /**
     * @brief Get chunk hash key
     */
    [[nodiscard]] uint64_t GetChunkKey(const glm::ivec3& pos) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(VoxelChunk*)> OnChunkCreated;
    std::function<void(const glm::ivec3&)> OnChunkRemoved;
    std::function<void(VoxelChunk*)> OnChunkMeshUpdated;
    std::function<void(const TerrainModification&)> OnTerrainModified;

private:
    // SDF functions
    float SDFSphere(const glm::vec3& p, const glm::vec3& center, float radius) const;
    float SDFBox(const glm::vec3& p, const glm::vec3& center, const glm::vec3& size, const glm::quat& rotation) const;
    float SDFCylinder(const glm::vec3& p, const glm::vec3& base, float height, float radius) const;
    float SDFCapsule(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, float radius) const;

    // SDF operations
    float SDFUnion(float d1, float d2) const;
    float SDFSubtract(float d1, float d2) const;
    float SDFIntersect(float d1, float d2) const;
    float SDFSmoothUnion(float d1, float d2, float k) const;
    float SDFSmoothSubtract(float d1, float d2, float k) const;
    float SDFSmoothIntersect(float d1, float d2, float k) const;

    // Chunk management
    void UpdateLoadedChunks(const glm::vec3& cameraPosition);
    void ProcessMeshQueue();

    // Noise generation
    float PerlinNoise3D(float x, float y, float z, int seed) const;
    float FBM(float x, float y, float z, int octaves, float persistence, float lacunarity, int seed) const;

    Config m_config;
    MarchingCubes m_marchingCubes;

    // Chunks
    std::unordered_map<uint64_t, std::unique_ptr<VoxelChunk>> m_chunks;
    mutable std::mutex m_chunkMutex;

    // Mesh generation queue
    std::vector<glm::ivec3> m_meshQueue;

    // Undo/redo
    std::vector<TerrainModification> m_undoStack;
    std::vector<TerrainModification> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 50;

    // Terrain generator
    std::function<float(const glm::vec3&)> m_terrainGenerator;

    bool m_initialized = false;
};

} // namespace Nova
