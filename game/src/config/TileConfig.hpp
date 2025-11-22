#pragma once

#include "EntityConfig.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Vehement {
namespace Config {

// ============================================================================
// Tile Visual Variant
// ============================================================================

/**
 * @brief A visual variant of a tile for variety
 */
struct TileVariant {
    std::string id;
    std::string modelPath;           // Override model
    std::string texturePath;         // Override texture
    float weight = 1.0f;             // Selection weight for random placement
    glm::vec4 tintColor{1.0f};       // Color tint
    float rotationVariance = 0.0f;   // Random rotation in degrees
};

// ============================================================================
// Tile Transition Rule
// ============================================================================

/**
 * @brief Rule for transitioning between tile types
 */
struct TileTransitionRule {
    std::string adjacentTileType;    // Type of adjacent tile
    std::string transitionModel;     // Model to use for transition
    std::string transitionTexture;   // Texture for transition
    int priority = 0;                // Higher priority wins conflicts

    // Direction flags for which edges this applies to
    bool applyNorth = true;
    bool applySouth = true;
    bool applyEast = true;
    bool applyWest = true;
    bool applyNorthEast = true;
    bool applyNorthWest = true;
    bool applySouthEast = true;
    bool applySouthWest = true;
};

// ============================================================================
// Tile Resource Yield
// ============================================================================

/**
 * @brief Resource yield for harvestable tiles
 */
struct TileResourceYield {
    ResourceType resourceType = ResourceType::None;
    int baseAmount = 0;              // Base yield per harvest
    float regenRate = 0.0f;          // Units per second regeneration
    int maxAmount = 0;               // Maximum harvestable amount
    bool depletes = false;           // Does harvesting deplete the tile?
    std::string depletedModelPath;   // Model when depleted
};

// ============================================================================
// Tile Animation
// ============================================================================

/**
 * @brief Animation configuration for tiles
 */
struct TileAnimationConfig {
    enum class AnimationType {
        None,
        UVScroll,           // Scrolling texture (water, lava)
        SpriteSheet,        // Animated sprite sheet
        VertexWave,         // Vertex displacement (grass, water)
        ColorCycle          // Color cycling
    };

    AnimationType type = AnimationType::None;
    float speed = 1.0f;
    glm::vec2 scrollDirection{1.0f, 0.0f};
    int frameCount = 1;
    float frameDuration = 0.1f;
    float waveAmplitude = 0.1f;
    float waveFrequency = 1.0f;
};

// ============================================================================
// Tile Configuration
// ============================================================================

/**
 * @brief Complete configuration for a terrain tile type
 *
 * Supports:
 * - Tile type ID and display name
 * - Model path (or procedural type)
 * - Walkability and buildability flags
 * - Movement cost modifiers
 * - Resource yield (if harvestable)
 * - Visual variants list
 * - Transition rules to adjacent tile types
 * - Python hooks: on_enter, on_exit, on_interact
 */
class TileConfig : public EntityConfig {
public:
    TileConfig() = default;
    ~TileConfig() override = default;

    [[nodiscard]] std::string GetConfigType() const override { return "tile"; }

    ValidationResult Validate() const override;
    void ApplyBaseConfig(const EntityConfig& baseConfig) override;

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] int GetTileTypeId() const { return m_tileTypeId; }
    void SetTileTypeId(int id) { m_tileTypeId = id; }

    [[nodiscard]] const std::string& GetDisplayName() const { return m_displayName; }
    void SetDisplayName(const std::string& name) { m_displayName = name; }

    [[nodiscard]] const std::string& GetCategory() const { return m_category; }
    void SetCategory(const std::string& category) { m_category = category; }

    // =========================================================================
    // Model and Rendering
    // =========================================================================

    [[nodiscard]] bool IsProcedural() const { return m_isProcedural; }
    void SetIsProcedural(bool procedural) { m_isProcedural = procedural; }

    [[nodiscard]] const std::string& GetProceduralType() const { return m_proceduralType; }
    void SetProceduralType(const std::string& type) { m_proceduralType = type; }

    [[nodiscard]] float GetTileHeight() const { return m_tileHeight; }
    void SetTileHeight(float height) { m_tileHeight = height; }

    [[nodiscard]] bool IsWall() const { return m_isWall; }
    void SetIsWall(bool wall) { m_isWall = wall; }

    [[nodiscard]] float GetWallHeight() const { return m_wallHeight; }
    void SetWallHeight(float height) { m_wallHeight = height; }

    // =========================================================================
    // Walkability and Movement
    // =========================================================================

    [[nodiscard]] bool IsWalkable() const { return m_isWalkable; }
    void SetIsWalkable(bool walkable) { m_isWalkable = walkable; }

    [[nodiscard]] bool IsBuildable() const { return m_isBuildable; }
    void SetIsBuildable(bool buildable) { m_isBuildable = buildable; }

    [[nodiscard]] bool BlocksSight() const { return m_blocksSight; }
    void SetBlocksSight(bool blocks) { m_blocksSight = blocks; }

    [[nodiscard]] bool BlocksProjectiles() const { return m_blocksProjectiles; }
    void SetBlocksProjectiles(bool blocks) { m_blocksProjectiles = blocks; }

    [[nodiscard]] float GetMovementCost() const { return m_movementCost; }
    void SetMovementCost(float cost) { m_movementCost = cost; }

    // Per-unit-class movement costs
    [[nodiscard]] float GetMovementCostFor(const std::string& unitClass) const;
    void SetMovementCostFor(const std::string& unitClass, float cost);

    // =========================================================================
    // Environment Effects
    // =========================================================================

    [[nodiscard]] bool IsDamaging() const { return m_damagePerSecond > 0; }
    [[nodiscard]] float GetDamagePerSecond() const { return m_damagePerSecond; }
    void SetDamagePerSecond(float damage) { m_damagePerSecond = damage; }

    [[nodiscard]] const std::string& GetDamageType() const { return m_damageType; }
    void SetDamageType(const std::string& type) { m_damageType = type; }

    [[nodiscard]] float GetSpeedModifier() const { return m_speedModifier; }
    void SetSpeedModifier(float modifier) { m_speedModifier = modifier; }

    [[nodiscard]] bool ProvidesConcealment() const { return m_providesConcealment; }
    void SetProvidesConcealment(bool concealment) { m_providesConcealment = concealment; }

    [[nodiscard]] float GetConcealmentBonus() const { return m_concealmentBonus; }
    void SetConcealmentBonus(float bonus) { m_concealmentBonus = bonus; }

    // =========================================================================
    // Resources
    // =========================================================================

    [[nodiscard]] bool IsHarvestable() const { return m_resourceYield.resourceType != ResourceType::None; }
    [[nodiscard]] const TileResourceYield& GetResourceYield() const { return m_resourceYield; }
    void SetResourceYield(const TileResourceYield& yield) { m_resourceYield = yield; }

    // =========================================================================
    // Visual Variants
    // =========================================================================

    [[nodiscard]] const std::vector<TileVariant>& GetVariants() const { return m_variants; }
    void SetVariants(const std::vector<TileVariant>& variants) { m_variants = variants; }
    void AddVariant(const TileVariant& variant) { m_variants.push_back(variant); }

    [[nodiscard]] const TileVariant* GetVariant(const std::string& id) const;
    [[nodiscard]] const TileVariant* GetRandomVariant() const;

    // =========================================================================
    // Transitions
    // =========================================================================

    [[nodiscard]] const std::vector<TileTransitionRule>& GetTransitionRules() const {
        return m_transitionRules;
    }
    void SetTransitionRules(const std::vector<TileTransitionRule>& rules) {
        m_transitionRules = rules;
    }
    void AddTransitionRule(const TileTransitionRule& rule) {
        m_transitionRules.push_back(rule);
    }

    [[nodiscard]] const TileTransitionRule* GetTransitionTo(const std::string& adjacentType) const;

    // =========================================================================
    // Animation
    // =========================================================================

    [[nodiscard]] const TileAnimationConfig& GetAnimation() const { return m_animation; }
    void SetAnimation(const TileAnimationConfig& anim) { m_animation = anim; }

    [[nodiscard]] bool IsAnimated() const {
        return m_animation.type != TileAnimationConfig::AnimationType::None;
    }

    // =========================================================================
    // Lighting
    // =========================================================================

    [[nodiscard]] float GetLightEmission() const { return m_lightEmission; }
    void SetLightEmission(float emission) { m_lightEmission = emission; }

    [[nodiscard]] const glm::vec3& GetLightColor() const { return m_lightColor; }
    void SetLightColor(const glm::vec3& color) { m_lightColor = color; }

    // =========================================================================
    // Audio
    // =========================================================================

    [[nodiscard]] const std::string& GetFootstepSound() const { return m_footstepSound; }
    void SetFootstepSound(const std::string& sound) { m_footstepSound = sound; }

    [[nodiscard]] const std::string& GetAmbientSound() const { return m_ambientSound; }
    void SetAmbientSound(const std::string& sound) { m_ambientSound = sound; }

    [[nodiscard]] float GetAmbientVolume() const { return m_ambientVolume; }
    void SetAmbientVolume(float volume) { m_ambientVolume = volume; }

    // =========================================================================
    // Python Script Hooks
    // =========================================================================

    [[nodiscard]] std::string GetOnEnterScript() const { return GetScriptHook("on_enter"); }
    [[nodiscard]] std::string GetOnExitScript() const { return GetScriptHook("on_exit"); }
    [[nodiscard]] std::string GetOnInteractScript() const { return GetScriptHook("on_interact"); }

    void SetOnEnterScript(const std::string& path) { SetScriptHook("on_enter", path); }
    void SetOnExitScript(const std::string& path) { SetScriptHook("on_exit", path); }
    void SetOnInteractScript(const std::string& path) { SetScriptHook("on_interact", path); }

protected:
    void ParseTypeSpecificFields(const std::string& jsonContent) override;
    std::string SerializeTypeSpecificFields() const override;

private:
    std::string GetScriptHook(const std::string& hookName) const;
    void SetScriptHook(const std::string& hookName, const std::string& path);

    // Identity
    int m_tileTypeId = 0;
    std::string m_displayName;
    std::string m_category;          // e.g., "terrain", "water", "road"

    // Rendering
    bool m_isProcedural = false;
    std::string m_proceduralType;    // e.g., "flat", "hex", "slopes"
    float m_tileHeight = 0.0f;
    bool m_isWall = false;
    float m_wallHeight = 2.0f;

    // Movement
    bool m_isWalkable = true;
    bool m_isBuildable = true;
    bool m_blocksSight = false;
    bool m_blocksProjectiles = false;
    float m_movementCost = 1.0f;
    std::unordered_map<std::string, float> m_unitClassMovementCosts;

    // Environment
    float m_damagePerSecond = 0.0f;
    std::string m_damageType;        // e.g., "fire", "poison", "cold"
    float m_speedModifier = 1.0f;
    bool m_providesConcealment = false;
    float m_concealmentBonus = 0.0f;

    // Resources
    TileResourceYield m_resourceYield;

    // Variants
    std::vector<TileVariant> m_variants;

    // Transitions
    std::vector<TileTransitionRule> m_transitionRules;

    // Animation
    TileAnimationConfig m_animation;

    // Lighting
    float m_lightEmission = 0.0f;
    glm::vec3 m_lightColor{1.0f};

    // Audio
    std::string m_footstepSound;
    std::string m_ambientSound;
    float m_ambientVolume = 1.0f;

    // Script hooks
    std::unordered_map<std::string, std::string> m_scriptHooks;
};

// Register the tile config type
REGISTER_CONFIG_TYPE("tile", TileConfig)

} // namespace Config
} // namespace Vehement
