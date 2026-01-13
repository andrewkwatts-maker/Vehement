/**
 * @file SDFSculptTool.hpp
 * @brief SDF sculpting tool for the Nova3D/Vehement editor
 *
 * Provides comprehensive sculpting functionality for SDF models including:
 * - Multiple brush types (Add, Subtract, Smooth, Flatten, Pinch, Inflate, Grab, Clone)
 * - Configurable brush settings with pressure sensitivity and falloff
 * - Symmetry modes (None, X, Y, Z, Radial)
 * - Full undo/redo support via EditorCommand system
 * - Real-time brush preview and overlay visualization
 */

#pragma once

#include "EditorCommand.hpp"  // For ICommand base class

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <limits>

namespace Nova {

// Forward declarations
class SDFModel;
class CommandHistory;
class Camera;
class Shader;
class Mesh;

// =============================================================================
// Enumerations
// =============================================================================

/**
 * @brief Brush operation types for SDF sculpting
 */
enum class BrushType : uint8_t {
    Add,        ///< Add material (sphere addition to SDF)
    Subtract,   ///< Remove material (sphere subtraction from SDF)
    Smooth,     ///< Smooth surface by averaging nearby samples
    Flatten,    ///< Flatten surface to a reference plane
    Pinch,      ///< Pull surface inward toward stroke center
    Inflate,    ///< Push surface outward along normals
    Grab,       ///< Move surface region by displacement
    Clone       ///< Clone from a source point
};

/**
 * @brief Falloff curve types for brush influence
 */
enum class FalloffType : uint8_t {
    Linear,     ///< Linear falloff: 1 - d/r
    Smooth,     ///< Smooth falloff: smoothstep(1 - d/r)
    Sharp,      ///< Sharp falloff: (1 - d/r)^2
    Constant    ///< Constant: 1.0 if d < r, 0.0 otherwise
};

/**
 * @brief Symmetry modes for mirrored sculpting
 */
enum class SymmetryMode : uint8_t {
    None    = 0,        ///< No symmetry
    X       = 1 << 0,   ///< Mirror across YZ plane (X axis symmetry)
    Y       = 1 << 1,   ///< Mirror across XZ plane (Y axis symmetry)
    Z       = 1 << 2,   ///< Mirror across XY plane (Z axis symmetry)
    Radial  = 1 << 3    ///< Radial symmetry around Y axis
};

// Enable bitwise operations for SymmetryMode
inline SymmetryMode operator|(SymmetryMode a, SymmetryMode b) {
    return static_cast<SymmetryMode>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline SymmetryMode operator&(SymmetryMode a, SymmetryMode b) {
    return static_cast<SymmetryMode>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasSymmetry(SymmetryMode flags, SymmetryMode check) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(check)) != 0;
}

// =============================================================================
// Brush Settings
// =============================================================================

/**
 * @brief Configuration settings for sculpt brushes
 */
struct BrushSettings {
    // Core brush parameters
    float radius = 0.5f;            ///< Brush radius (0.01 - 10.0)
    float strength = 0.5f;          ///< Brush strength/intensity (0.0 - 1.0)
    FalloffType falloff = FalloffType::Smooth; ///< Falloff curve type

    // Pressure sensitivity
    bool pressureSensitivity = true; ///< Enable tablet pressure sensitivity
    float pressureRadiusScale = 0.5f; ///< How much pressure affects radius (0-1)
    float pressureStrengthScale = 1.0f; ///< How much pressure affects strength (0-1)

    // Stroke settings
    float spacing = 0.25f;          ///< Brush spacing as fraction of radius (0.1 - 1.0)
    bool lazyMouse = false;         ///< Enable lazy mouse smoothing
    float lazyRadius = 0.5f;        ///< Lazy mouse radius

    // Symmetry
    SymmetryMode symmetry = SymmetryMode::None;
    int radialCount = 8;            ///< Number of radial symmetry copies
    glm::vec3 symmetryOrigin{0.0f}; ///< Origin point for symmetry

    // Advanced settings
    bool autoSmooth = false;        ///< Automatically smooth after each stroke
    float autoSmoothStrength = 0.2f;
    bool invertBrush = false;       ///< Invert brush effect (e.g., subtract instead of add)

    // Clone brush specific
    glm::vec3 cloneSourceOffset{0.0f}; ///< Offset from stroke to clone source

    // Flatten brush specific
    bool useCustomPlane = false;    ///< Use custom flatten plane instead of auto
    glm::vec3 flattenPlaneNormal{0.0f, 1.0f, 0.0f};
    float flattenPlaneDistance = 0.0f;

    /**
     * @brief Clamp settings to valid ranges
     */
    void Validate() {
        radius = glm::clamp(radius, 0.01f, 10.0f);
        strength = glm::clamp(strength, 0.0f, 1.0f);
        spacing = glm::clamp(spacing, 0.1f, 1.0f);
        pressureRadiusScale = glm::clamp(pressureRadiusScale, 0.0f, 1.0f);
        pressureStrengthScale = glm::clamp(pressureStrengthScale, 0.0f, 1.0f);
        radialCount = glm::clamp(radialCount, 2, 32);
    }
};

// =============================================================================
// SDF Grid for Sculpting
// =============================================================================

/**
 * @brief 3D grid of SDF values for sculpting operations
 *
 * Stores SDF values in a regular grid that can be modified by brushes
 * and converted to mesh via marching cubes.
 */
class SDFGrid {
public:
    SDFGrid();
    SDFGrid(const glm::ivec3& resolution, const glm::vec3& boundsMin, const glm::vec3& boundsMax);
    ~SDFGrid() = default;

    // Copy/move
    SDFGrid(const SDFGrid&) = default;
    SDFGrid& operator=(const SDFGrid&) = default;
    SDFGrid(SDFGrid&&) noexcept = default;
    SDFGrid& operator=(SDFGrid&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize grid with given resolution and bounds
     */
    void Initialize(const glm::ivec3& resolution, const glm::vec3& boundsMin, const glm::vec3& boundsMax);

    /**
     * @brief Initialize from an existing SDFModel
     */
    void InitializeFromModel(const SDFModel& model, int resolution = 64);

    /**
     * @brief Clear all values to specified distance (positive = outside)
     */
    void Clear(float value = 1.0f);

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Get grid resolution
     */
    [[nodiscard]] const glm::ivec3& GetResolution() const { return m_resolution; }

    /**
     * @brief Get bounds
     */
    [[nodiscard]] const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
    [[nodiscard]] const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

    /**
     * @brief Get voxel size
     */
    [[nodiscard]] const glm::vec3& GetVoxelSize() const { return m_voxelSize; }

    /**
     * @brief Get total number of voxels
     */
    [[nodiscard]] size_t GetVoxelCount() const { return m_data.size(); }

    /**
     * @brief Check if grid is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return !m_data.empty(); }

    // =========================================================================
    // SDF Access
    // =========================================================================

    /**
     * @brief Sample SDF at world position (trilinear interpolation)
     */
    [[nodiscard]] float Sample(const glm::vec3& worldPos) const;

    /**
     * @brief Sample SDF at grid index
     */
    [[nodiscard]] float SampleAt(int x, int y, int z) const;
    [[nodiscard]] float SampleAt(const glm::ivec3& index) const;

    /**
     * @brief Set SDF value at grid index
     */
    void SetAt(int x, int y, int z, float value);
    void SetAt(const glm::ivec3& index, float value);

    /**
     * @brief Calculate gradient/normal at world position
     */
    [[nodiscard]] glm::vec3 CalculateGradient(const glm::vec3& worldPos, float epsilon = 0.0f) const;

    /**
     * @brief Convert world position to grid index (may be out of bounds)
     */
    [[nodiscard]] glm::ivec3 WorldToGrid(const glm::vec3& worldPos) const;

    /**
     * @brief Convert grid index to world position
     */
    [[nodiscard]] glm::vec3 GridToWorld(const glm::ivec3& gridIndex) const;
    [[nodiscard]] glm::vec3 GridToWorld(int x, int y, int z) const;

    /**
     * @brief Check if grid index is valid
     */
    [[nodiscard]] bool IsValidIndex(int x, int y, int z) const;
    [[nodiscard]] bool IsValidIndex(const glm::ivec3& index) const;

    // =========================================================================
    // Modification Operations
    // =========================================================================

    /**
     * @brief Apply CSG union with a sphere
     */
    void UnionSphere(const glm::vec3& center, float radius, float smoothness = 0.0f);

    /**
     * @brief Apply CSG subtraction with a sphere
     */
    void SubtractSphere(const glm::vec3& center, float radius, float smoothness = 0.0f);

    /**
     * @brief Smooth values in a spherical region
     */
    void SmoothRegion(const glm::vec3& center, float radius, float strength);

    /**
     * @brief Flatten surface toward a plane
     */
    void FlattenToPlane(const glm::vec3& center, float radius,
                        const glm::vec3& planeNormal, float planeDistance, float strength);

    /**
     * @brief Pinch surface toward center
     */
    void PinchRegion(const glm::vec3& center, float radius, float strength);

    /**
     * @brief Inflate surface along normals
     */
    void InflateRegion(const glm::vec3& center, float radius, float strength);

    /**
     * @brief Displace a region of the surface
     */
    void DisplaceRegion(const glm::vec3& center, float radius, const glm::vec3& displacement, float strength);

    // =========================================================================
    // Snapshot for Undo
    // =========================================================================

    /**
     * @brief Capture values in a region for undo
     */
    struct RegionSnapshot {
        glm::ivec3 minIndex;
        glm::ivec3 maxIndex;
        std::vector<float> values;

        [[nodiscard]] bool IsEmpty() const { return values.empty(); }
    };

    /**
     * @brief Capture current values in a spherical region
     */
    [[nodiscard]] RegionSnapshot CaptureRegion(const glm::vec3& center, float radius) const;

    /**
     * @brief Restore values from a snapshot
     */
    void RestoreRegion(const RegionSnapshot& snapshot);

    /**
     * @brief Get raw data access (for mesh generation)
     */
    [[nodiscard]] const std::vector<float>& GetData() const { return m_data; }
    [[nodiscard]] std::vector<float>& GetData() { return m_data; }

private:
    /**
     * @brief Convert 3D index to linear index
     */
    [[nodiscard]] size_t GetLinearIndex(int x, int y, int z) const;

    glm::ivec3 m_resolution{0};
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};
    glm::vec3 m_voxelSize{0.0f};
    std::vector<float> m_data;
};

// =============================================================================
// Brush Stroke Recording
// =============================================================================

/**
 * @brief Records a single brush application point
 */
struct BrushDab {
    glm::vec3 position;         ///< World position of dab center
    glm::vec3 normal;           ///< Surface normal at position
    float pressure;             ///< Pressure value (0-1)
    float effectiveRadius;      ///< Actual radius after pressure scaling
    float effectiveStrength;    ///< Actual strength after pressure scaling
};

/**
 * @brief Records a complete sculpting stroke for undo/redo
 */
struct SDFBrushStroke {
    BrushType brushType;
    BrushSettings settings;
    std::vector<BrushDab> dabs;

    // Region affected by this stroke (for efficient undo)
    glm::vec3 boundsMin{std::numeric_limits<float>::max()};
    glm::vec3 boundsMax{std::numeric_limits<float>::lowest()};

    // Snapshot of affected voxels before stroke
    SDFGrid::RegionSnapshot beforeSnapshot;

    /**
     * @brief Expand bounds to include a dab
     */
    void ExpandBounds(const glm::vec3& center, float radius) {
        boundsMin = glm::min(boundsMin, center - glm::vec3(radius));
        boundsMax = glm::max(boundsMax, center + glm::vec3(radius));
    }

    /**
     * @brief Check if stroke has any dabs
     */
    [[nodiscard]] bool IsEmpty() const { return dabs.empty(); }
};

// =============================================================================
// Sculpt Command for Undo/Redo
// =============================================================================

/**
 * @brief Command for undoing/redoing a sculpt stroke
 */
class SDFSculptCommand : public ICommand {
public:
    /**
     * @brief Create a sculpt command
     * @param grid The SDF grid being modified
     * @param stroke The recorded stroke data
     */
    SDFSculptCommand(SDFGrid* grid, SDFBrushStroke stroke);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    SDFGrid* m_grid;
    SDFBrushStroke m_stroke;
    SDFGrid::RegionSnapshot m_afterSnapshot;
    bool m_executed = false;
};

// =============================================================================
// SDF Sculpt Tool
// =============================================================================

/**
 * @brief Main sculpting tool for SDF models
 *
 * Provides comprehensive sculpting functionality with multiple brush types,
 * symmetry support, and undo/redo integration.
 *
 * Usage:
 *   SDFSculptTool tool;
 *   tool.Initialize();
 *   tool.SetTarget(sdfGrid);
 *   tool.SetBrushType(BrushType::Add);
 *   tool.GetSettings().radius = 0.5f;
 *
 *   // In input handler:
 *   if (mouseDown) {
 *       auto hit = RayCast(ray);  // Your ray casting
 *       tool.BeginStroke(hit.position, hit.normal);
 *   }
 *   if (dragging) {
 *       tool.UpdateStroke(currentHit.position, currentHit.normal, pressure);
 *   }
 *   if (mouseUp) {
 *       tool.EndStroke(commandHistory);
 *   }
 */
class SDFSculptTool {
public:
    SDFSculptTool();
    ~SDFSculptTool();

    // Non-copyable
    SDFSculptTool(const SDFSculptTool&) = delete;
    SDFSculptTool& operator=(const SDFSculptTool&) = delete;
    SDFSculptTool(SDFSculptTool&&) noexcept;
    SDFSculptTool& operator=(SDFSculptTool&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize rendering resources
     */
    bool Initialize();

    /**
     * @brief Cleanup resources
     */
    void Shutdown();

    /**
     * @brief Check if tool is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Target Management
    // =========================================================================

    /**
     * @brief Set the SDF grid to sculpt on
     */
    void SetTarget(SDFGrid* grid);

    /**
     * @brief Get the current target grid
     */
    [[nodiscard]] SDFGrid* GetTarget() const { return m_targetGrid; }

    /**
     * @brief Check if a target is set
     */
    [[nodiscard]] bool HasTarget() const { return m_targetGrid != nullptr; }

    // =========================================================================
    // Brush Configuration
    // =========================================================================

    /**
     * @brief Set the active brush type
     */
    void SetBrushType(BrushType type) { m_brushType = type; }

    /**
     * @brief Get the active brush type
     */
    [[nodiscard]] BrushType GetBrushType() const { return m_brushType; }

    /**
     * @brief Get brush settings (mutable)
     */
    [[nodiscard]] BrushSettings& GetSettings() { return m_settings; }

    /**
     * @brief Get brush settings (const)
     */
    [[nodiscard]] const BrushSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Set brush settings
     */
    void SetSettings(const BrushSettings& settings) { m_settings = settings; }

    // =========================================================================
    // Clone Source (for Clone brush)
    // =========================================================================

    /**
     * @brief Set clone source point
     */
    void SetCloneSource(const glm::vec3& position, const glm::vec3& normal);

    /**
     * @brief Check if clone source is set
     */
    [[nodiscard]] bool HasCloneSource() const { return m_hasCloneSource; }

    /**
     * @brief Get clone source position
     */
    [[nodiscard]] const glm::vec3& GetCloneSource() const { return m_cloneSource; }

    // =========================================================================
    // Stroke Operations
    // =========================================================================

    /**
     * @brief Begin a new sculpting stroke
     * @param hitPos World position where brush contacts surface
     * @param normal Surface normal at contact point
     * @return true if stroke started successfully
     */
    bool BeginStroke(const glm::vec3& hitPos, const glm::vec3& normal);

    /**
     * @brief Update the current stroke with new position
     * @param hitPos Current world position
     * @param normal Current surface normal
     * @param pressure Tablet pressure (0-1), use 1.0 for mouse
     */
    void UpdateStroke(const glm::vec3& hitPos, const glm::vec3& normal, float pressure = 1.0f);

    /**
     * @brief End the current stroke and optionally record to history
     * @param history Command history for undo/redo (optional)
     */
    void EndStroke(CommandHistory* history = nullptr);

    /**
     * @brief Cancel the current stroke without applying
     */
    void CancelStroke();

    /**
     * @brief Check if a stroke is currently active
     */
    [[nodiscard]] bool IsStrokeActive() const { return m_strokeActive; }

    // =========================================================================
    // Preview and Visualization
    // =========================================================================

    /**
     * @brief Update brush preview position (call when hovering)
     * @param hitPos World position under cursor
     * @param normal Surface normal
     */
    void UpdatePreview(const glm::vec3& hitPos, const glm::vec3& normal);

    /**
     * @brief Clear brush preview
     */
    void ClearPreview();

    /**
     * @brief Render brush overlay/preview
     * @param camera Active camera
     */
    void RenderOverlay(const Camera& camera);

    /**
     * @brief Render brush overlay with explicit matrices
     */
    void RenderOverlay(const glm::mat4& view, const glm::mat4& projection);

    /**
     * @brief Enable/disable preview rendering
     */
    void SetPreviewEnabled(bool enabled) { m_showPreview = enabled; }

    /**
     * @brief Check if preview is enabled
     */
    [[nodiscard]] bool IsPreviewEnabled() const { return m_showPreview; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using StrokeCallback = std::function<void()>;

    /**
     * @brief Set callback for stroke begin
     */
    void SetOnStrokeBegin(StrokeCallback callback) { m_onStrokeBegin = std::move(callback); }

    /**
     * @brief Set callback for stroke end
     */
    void SetOnStrokeEnd(StrokeCallback callback) { m_onStrokeEnd = std::move(callback); }

    /**
     * @brief Set callback for grid modification (for mesh regeneration)
     */
    void SetOnGridModified(StrokeCallback callback) { m_onGridModified = std::move(callback); }

    // =========================================================================
    // Falloff Calculation (exposed for custom brushes)
    // =========================================================================

    /**
     * @brief Calculate falloff value for a distance
     * @param distance Distance from brush center
     * @param radius Brush radius
     * @param falloffType Type of falloff curve
     * @return Falloff value (0-1)
     */
    [[nodiscard]] static float CalculateFalloff(float distance, float radius, FalloffType falloffType);

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Apply a single brush dab to the grid
     */
    void ApplyDab(const BrushDab& dab);

    /**
     * @brief Apply brush at multiple positions for symmetry
     */
    void ApplyWithSymmetry(const BrushDab& dab);

    /**
     * @brief Get symmetry-mirrored positions
     */
    [[nodiscard]] std::vector<glm::vec3> GetSymmetryPositions(const glm::vec3& position) const;

    /**
     * @brief Calculate effective brush parameters with pressure
     */
    void CalculateEffectiveParams(float pressure, float& outRadius, float& outStrength) const;

    /**
     * @brief Check if new position is far enough from last dab (spacing)
     */
    [[nodiscard]] bool ShouldApplyDab(const glm::vec3& position) const;

    /**
     * @brief Create brush overlay mesh
     */
    void CreateOverlayMesh();

    /**
     * @brief Update overlay mesh for current preview state
     */
    void UpdateOverlayMesh();

    /**
     * @brief Initialize shader for overlay rendering
     */
    bool CreateOverlayShader();

    // Brush operations
    void ApplyAddBrush(const glm::vec3& center, float radius, float strength);
    void ApplySubtractBrush(const glm::vec3& center, float radius, float strength);
    void ApplySmoothBrush(const glm::vec3& center, float radius, float strength);
    void ApplyFlattenBrush(const glm::vec3& center, float radius, float strength, const glm::vec3& normal);
    void ApplyPinchBrush(const glm::vec3& center, float radius, float strength);
    void ApplyInflateBrush(const glm::vec3& center, float radius, float strength);
    void ApplyGrabBrush(const glm::vec3& center, float radius, float strength, const glm::vec3& delta);
    void ApplyCloneBrush(const glm::vec3& center, float radius, float strength);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // State
    bool m_initialized = false;
    bool m_strokeActive = false;
    bool m_showPreview = true;
    bool m_previewValid = false;

    // Target
    SDFGrid* m_targetGrid = nullptr;

    // Brush configuration
    BrushType m_brushType = BrushType::Add;
    BrushSettings m_settings;

    // Clone source
    bool m_hasCloneSource = false;
    glm::vec3 m_cloneSource{0.0f};
    glm::vec3 m_cloneSourceNormal{0.0f, 1.0f, 0.0f};

    // Current stroke
    SDFBrushStroke m_currentStroke;
    glm::vec3 m_lastDabPosition{0.0f};
    glm::vec3 m_strokeStartPosition{0.0f};
    float m_strokeDistance = 0.0f;

    // Grab brush state
    glm::vec3 m_grabStartPosition{0.0f};
    glm::vec3 m_grabLastPosition{0.0f};

    // Flatten plane (auto-calculated)
    glm::vec3 m_flattenPlaneNormal{0.0f, 1.0f, 0.0f};
    float m_flattenPlaneDistance = 0.0f;

    // Preview state
    glm::vec3 m_previewPosition{0.0f};
    glm::vec3 m_previewNormal{0.0f, 1.0f, 0.0f};

    // Lazy mouse
    glm::vec3 m_lazyPosition{0.0f};

    // Rendering resources
    std::unique_ptr<Shader> m_overlayShader;
    std::unique_ptr<Mesh> m_brushCircleMesh;
    std::unique_ptr<Mesh> m_brushSphereMesh;
    uint32_t m_circleVAO = 0;
    uint32_t m_circleVBO = 0;
    static constexpr int CIRCLE_SEGMENTS = 64;

    // Callbacks
    StrokeCallback m_onStrokeBegin;
    StrokeCallback m_onStrokeEnd;
    StrokeCallback m_onGridModified;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get display name for brush type
 */
[[nodiscard]] const char* GetBrushTypeName(BrushType type);

/**
 * @brief Get display name for falloff type
 */
[[nodiscard]] const char* GetFalloffTypeName(FalloffType type);

/**
 * @brief Get display name for symmetry mode
 */
[[nodiscard]] const char* GetSymmetryModeName(SymmetryMode mode);

} // namespace Nova
