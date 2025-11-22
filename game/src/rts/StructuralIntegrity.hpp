#pragma once

/**
 * @file StructuralIntegrity.hpp
 * @brief Building physics and structural integrity system
 *
 * Simulates realistic building mechanics:
 * - Support requirements (pillars, walls)
 * - Maximum spans for different materials
 * - Collapse simulation when support removed
 * - Damage propagation through structures
 */

#include "../world/Tile.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace Vehement {
namespace RTS {

// Forward declarations
struct Voxel;
class Voxel3DMap;

// ============================================================================
// Material Properties
// ============================================================================

/**
 * @brief Structural properties of materials
 */
struct MaterialProperties {
    TileType type;

    int maxUnsupportedSpan = 3;     ///< Max tiles without support
    int maxStackHeight = 10;        ///< Max height when stacked
    float loadCapacity = 100.0f;    ///< Weight it can support
    float weight = 10.0f;           ///< Weight of one voxel
    float tensileStrength = 50.0f;  ///< Resistance to pulling apart
    float compressionStrength = 100.0f; ///< Resistance to crushing
    float brittleness = 0.5f;       ///< How likely to shatter (0-1)

    bool canBeSupport = true;       ///< Can act as support column
    bool requiresFoundation = true; ///< Must be on solid ground
    bool isFlexible = false;        ///< Bends instead of breaks
};

/**
 * @brief Get material properties for tile type
 */
MaterialProperties GetMaterialProperties(TileType type);

// ============================================================================
// Support Types
// ============================================================================

/**
 * @brief Types of structural support
 */
enum class SupportType : uint8_t {
    None,           ///< No support
    Ground,         ///< Resting on terrain
    Pillar,         ///< Vertical column support
    Wall,           ///< Wall providing support
    Cantilever,     ///< Extended from supported structure
    Beam,           ///< Horizontal load-bearing
    Arch,           ///< Curved support structure
    Cable           ///< Suspension support
};

/**
 * @brief Support information for a position
 */
struct SupportInfo {
    SupportType type = SupportType::None;
    glm::ivec3 supportSource{0, 0, 0};  ///< Position of supporting element
    float supportStrength = 0.0f;        ///< How much load it can handle
    int chainLength = 0;                 ///< Distance from ground
    bool isStable = false;
};

// ============================================================================
// Collapse Event
// ============================================================================

/**
 * @brief Collapse simulation result
 */
struct CollapseEvent {
    std::vector<glm::ivec3> collapsedPositions;
    std::vector<glm::ivec3> damagedPositions;
    glm::ivec3 originPoint;
    float totalMassCollapsed;
    float damageRadius;
};

// ============================================================================
// Structural Integrity System
// ============================================================================

/**
 * @brief Manages building physics and collapse simulation
 */
class StructuralIntegrity {
public:
    StructuralIntegrity();
    ~StructuralIntegrity();

    // Non-copyable
    StructuralIntegrity(const StructuralIntegrity&) = delete;
    StructuralIntegrity& operator=(const StructuralIntegrity&) = delete;

    /**
     * @brief Initialize with voxel map reference
     */
    bool Initialize(Voxel3DMap* voxelMap);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update structural calculations (call periodically)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Stability Queries
    // =========================================================================

    /**
     * @brief Check if structure at position is stable
     */
    [[nodiscard]] bool IsStable(const glm::ivec3& pos) const;

    /**
     * @brief Check if entire structure is stable
     */
    [[nodiscard]] bool IsStructureStable(const glm::ivec3& min, const glm::ivec3& max) const;

    /**
     * @brief Check if position has adequate support
     */
    [[nodiscard]] bool HasSupport(const glm::ivec3& pos) const;

    /**
     * @brief Get support information for position
     */
    [[nodiscard]] SupportInfo GetSupportInfo(const glm::ivec3& pos) const;

    /**
     * @brief Get all voxels supporting a position
     */
    [[nodiscard]] std::vector<glm::ivec3> GetSupportingVoxels(const glm::ivec3& pos) const;

    /**
     * @brief Get all voxels supported by a position
     */
    [[nodiscard]] std::vector<glm::ivec3> GetSupportedVoxels(const glm::ivec3& pos) const;

    /**
     * @brief Check if removal would cause collapse
     */
    [[nodiscard]] bool CanSafelyRemove(const glm::ivec3& pos) const;

    /**
     * @brief Get voxels that would collapse if position removed
     */
    [[nodiscard]] std::vector<glm::ivec3> GetCollapsePreview(const glm::ivec3& pos) const;

    // =========================================================================
    // Material Properties
    // =========================================================================

    /**
     * @brief Get maximum unsupported span for material
     */
    [[nodiscard]] int GetMaxUnsupportedSpan() const { return m_defaultMaxSpan; }

    /**
     * @brief Get maximum span for specific material
     */
    [[nodiscard]] int GetMaxUnsupportedSpan(TileType type) const;

    /**
     * @brief Get maximum stack height for material
     */
    [[nodiscard]] int GetMaxHeight(TileType type) const;

    /**
     * @brief Get load capacity at position
     */
    [[nodiscard]] float GetLoadCapacity(const glm::ivec3& pos) const;

    /**
     * @brief Get current load at position
     */
    [[nodiscard]] float GetCurrentLoad(const glm::ivec3& pos) const;

    /**
     * @brief Check if position is overloaded
     */
    [[nodiscard]] bool IsOverloaded(const glm::ivec3& pos) const;

    // =========================================================================
    // Collapse Simulation
    // =========================================================================

    /**
     * @brief Check for collapse starting from damaged position
     */
    void CheckCollapse(const glm::ivec3& damagedPos);

    /**
     * @brief Simulate collapse of unsupported voxels
     */
    CollapseEvent SimulateCollapse(const std::vector<glm::ivec3>& unsupportedVoxels);

    /**
     * @brief Apply damage to structure
     */
    void ApplyDamage(const glm::ivec3& pos, float damage);

    /**
     * @brief Apply area damage (explosion, etc)
     */
    void ApplyAreaDamage(const glm::ivec3& center, float radius, float damage);

    /**
     * @brief Propagate damage through connected structure
     */
    void PropagateDamage(const glm::ivec3& origin, float damage, float falloff);

    // =========================================================================
    // Structural Analysis
    // =========================================================================

    /**
     * @brief Analyze structural integrity of region
     */
    struct StructuralAnalysis {
        bool isStable;
        int totalVoxels;
        int supportedVoxels;
        int unsupportedVoxels;
        int overloadedVoxels;
        float averageStress;
        float maxStress;
        glm::ivec3 weakestPoint;
        std::vector<glm::ivec3> criticalPoints; ///< Removal would cause collapse
    };

    [[nodiscard]] StructuralAnalysis AnalyzeStructure(const glm::ivec3& min,
                                                       const glm::ivec3& max) const;

    /**
     * @brief Find the weakest point in a structure
     */
    [[nodiscard]] glm::ivec3 FindWeakestPoint(const glm::ivec3& min, const glm::ivec3& max) const;

    /**
     * @brief Get all critical support points
     */
    [[nodiscard]] std::vector<glm::ivec3> GetCriticalSupports(const glm::ivec3& min,
                                                               const glm::ivec3& max) const;

    /**
     * @brief Suggest support placements for unstable structure
     */
    [[nodiscard]] std::vector<glm::ivec3> SuggestSupports(const glm::ivec3& min,
                                                          const glm::ivec3& max) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Enable/disable realistic physics
     */
    void SetRealisticPhysics(bool enabled) { m_realisticPhysics = enabled; }

    /**
     * @brief Check if realistic physics enabled
     */
    [[nodiscard]] bool IsRealisticPhysics() const { return m_realisticPhysics; }

    /**
     * @brief Set gravity strength
     */
    void SetGravity(float gravity) { m_gravity = gravity; }

    /**
     * @brief Get gravity strength
     */
    [[nodiscard]] float GetGravity() const { return m_gravity; }

    /**
     * @brief Set default max span
     */
    void SetDefaultMaxSpan(int span) { m_defaultMaxSpan = span; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using CollapseCallback = std::function<void(const CollapseEvent& event)>;
    using DamageCallback = std::function<void(const glm::ivec3& pos, float damage)>;
    using WarningCallback = std::function<void(const glm::ivec3& pos, const std::string& message)>;

    void SetOnCollapse(CollapseCallback cb) { m_onCollapse = std::move(cb); }
    void SetOnDamage(DamageCallback cb) { m_onDamage = std::move(cb); }
    void SetOnStructuralWarning(WarningCallback cb) { m_onWarning = std::move(cb); }

private:
    void RecalculateSupport(const glm::ivec3& pos);
    void PropagateSupport(const glm::ivec3& from);
    bool CheckGroundSupport(const glm::ivec3& pos) const;
    bool CheckAdjacentSupport(const glm::ivec3& pos) const;
    float CalculateStress(const glm::ivec3& pos) const;
    void RemoveVoxelInternal(const glm::ivec3& pos);

    int64_t GetKey(const glm::ivec3& pos) const {
        return (static_cast<int64_t>(pos.x) << 40) |
               (static_cast<int64_t>(pos.y) << 20) |
               static_cast<int64_t>(pos.z);
    }

    Voxel3DMap* m_voxelMap = nullptr;

    // Cached support information
    std::unordered_map<int64_t, SupportInfo> m_supportCache;
    bool m_supportCacheDirty = true;

    // Configuration
    bool m_realisticPhysics = true;
    float m_gravity = 9.81f;
    int m_defaultMaxSpan = 4;

    // Pending collapses
    std::vector<glm::ivec3> m_pendingCollapseChecks;
    float m_collapseCheckTimer = 0.0f;
    float m_collapseCheckInterval = 0.1f;

    // Callbacks
    CollapseCallback m_onCollapse;
    DamageCallback m_onDamage;
    WarningCallback m_onWarning;
};

// ============================================================================
// Structural Helpers
// ============================================================================

/**
 * @brief Check if a shape forms a valid arch
 */
bool IsValidArch(const std::vector<glm::ivec3>& positions);

/**
 * @brief Calculate center of mass for structure
 */
glm::vec3 CalculateCenterOfMass(const std::vector<Voxel>& voxels);

/**
 * @brief Check if structure is top-heavy
 */
bool IsTopHeavy(const std::vector<Voxel>& voxels);

/**
 * @brief Get optimal pillar positions for area
 */
std::vector<glm::ivec3> GetOptimalPillarPositions(const glm::ivec3& min,
                                                   const glm::ivec3& max,
                                                   int maxSpan);

} // namespace RTS
} // namespace Vehement
