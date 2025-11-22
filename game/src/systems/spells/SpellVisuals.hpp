#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>

namespace Vehement {
namespace Spells {

// Forward declarations
class SpellInstance;

// ============================================================================
// Visual Effect Types
// ============================================================================

/**
 * @brief Types of visual effects
 */
enum class VisualEffectType : uint8_t {
    Particle,           // Particle system
    Model,              // 3D model
    Decal,              // Ground decal
    Light,              // Dynamic light
    ScreenEffect,       // Screen-space effect (shake, flash)
    Trail,              // Trail effect
    Beam,               // Beam between points
    Sprite,             // Billboard sprite
    Animation           // Skeletal animation
};

/**
 * @brief Attachment point for effects
 */
enum class AttachPoint : uint8_t {
    Origin,             // World origin
    Caster,             // Caster entity
    CasterHand,         // Caster's casting hand
    CasterChest,        // Caster's chest
    CasterHead,         // Caster's head
    CasterFeet,         // Caster's feet
    Target,             // Target entity
    TargetCenter,       // Target's center
    TargetGround,       // Ground at target
    TargetPoint,        // Targeted point
    Projectile,         // Attached to projectile
    Impact              // Impact location
};

// ============================================================================
// Particle System Configuration
// ============================================================================

/**
 * @brief Configuration for a particle effect
 */
struct ParticleConfig {
    std::string systemPath;         // Path to particle system file
    float duration = 0.0f;          // Override duration (0 = use default)
    float scale = 1.0f;             // Scale multiplier
    glm::vec3 offset{0.0f};         // Position offset from attach point
    glm::vec3 rotation{0.0f};       // Rotation offset (euler angles)
    bool inheritRotation = false;   // Inherit parent rotation
    bool loop = false;              // Loop the effect
    glm::vec4 colorTint{1.0f};      // Color tint (RGBA)

    // Emission overrides
    std::optional<float> emissionRateOverride;
    std::optional<float> lifetimeOverride;
    std::optional<float> speedOverride;
};

// ============================================================================
// Model Configuration
// ============================================================================

/**
 * @brief Configuration for a 3D model effect
 */
struct ModelConfig {
    std::string modelPath;          // Path to model file
    std::string texturePath;        // Optional texture override
    std::string materialPath;       // Optional material override
    glm::vec3 scale{1.0f};          // Scale
    glm::vec3 offset{0.0f};         // Position offset
    glm::vec3 rotation{0.0f};       // Rotation offset
    float duration = 0.0f;          // How long to display (0 = until spell ends)

    // Animation
    std::string animationName;      // Animation to play
    float animationSpeed = 1.0f;
    bool loopAnimation = false;

    // Rendering
    bool castShadows = false;
    bool receiveShadows = false;
    float opacity = 1.0f;
    glm::vec4 colorTint{1.0f};

    // Fade
    float fadeInDuration = 0.0f;
    float fadeOutDuration = 0.0f;
};

// ============================================================================
// Light Configuration
// ============================================================================

/**
 * @brief Configuration for a dynamic light effect
 */
struct LightConfig {
    enum class LightType : uint8_t { Point, Spot, Directional };

    LightType type = LightType::Point;
    glm::vec3 color{1.0f};          // Light color (RGB)
    float intensity = 1.0f;         // Light intensity
    float range = 10.0f;            // Light range (point/spot)
    float spotAngle = 45.0f;        // Spot light angle

    glm::vec3 offset{0.0f};         // Position offset
    float duration = 0.0f;          // How long light persists

    // Animation
    bool flicker = false;
    float flickerFrequency = 10.0f;
    float flickerAmplitude = 0.2f;

    // Fade
    float fadeInDuration = 0.1f;
    float fadeOutDuration = 0.2f;

    bool castShadows = false;
};

// ============================================================================
// Screen Effect Configuration
// ============================================================================

/**
 * @brief Configuration for screen-space effects
 */
struct ScreenEffectConfig {
    enum class EffectType : uint8_t {
        Shake,              // Camera shake
        Flash,              // Screen flash
        Blur,               // Motion/radial blur
        Distortion,         // Screen distortion
        ColorGrade,         // Color grading
        Vignette            // Vignette effect
    };

    EffectType type = EffectType::Flash;
    float duration = 0.5f;
    float intensity = 1.0f;

    // Type-specific parameters
    // Shake
    glm::vec3 shakeDirection{1.0f, 1.0f, 0.0f};
    float shakeFrequency = 20.0f;

    // Flash
    glm::vec4 flashColor{1.0f};

    // Blur
    float blurRadius = 5.0f;

    // Only apply to self (caster) or everyone in range
    bool selfOnly = false;
    float affectRange = 0.0f;       // 0 = self only
};

// ============================================================================
// Trail Configuration
// ============================================================================

/**
 * @brief Configuration for trail effects
 */
struct TrailConfig {
    std::string texturePath;        // Trail texture
    float width = 0.5f;             // Trail width
    float duration = 1.0f;          // Trail lifetime
    float fadeTime = 0.5f;          // Time to fade out

    glm::vec4 startColor{1.0f};     // Color at start
    glm::vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f}; // Color at end (faded)

    int segments = 20;              // Trail segments
    float minVertexDistance = 0.1f; // Minimum distance between trail points
};

// ============================================================================
// Beam Configuration
// ============================================================================

/**
 * @brief Configuration for beam effects (lightning, laser, etc.)
 */
struct BeamConfig {
    std::string texturePath;        // Beam texture
    float width = 0.5f;             // Beam width
    float duration = 0.0f;          // How long beam persists (0 = instant)

    glm::vec4 color{1.0f};          // Beam color
    float intensity = 1.0f;         // Brightness

    // Animation
    bool animate = false;
    float scrollSpeed = 1.0f;       // Texture scroll speed
    float waveAmplitude = 0.0f;     // Wave distortion amplitude
    float waveFrequency = 1.0f;     // Wave frequency

    // Branching (for lightning)
    bool branching = false;
    int branchCount = 2;
    float branchChance = 0.3f;
    float branchScale = 0.5f;

    AttachPoint startPoint = AttachPoint::CasterHand;
    AttachPoint endPoint = AttachPoint::Target;
};

// ============================================================================
// Decal Configuration
// ============================================================================

/**
 * @brief Configuration for ground decals
 */
struct DecalConfig {
    std::string texturePath;        // Decal texture
    float size = 2.0f;              // Decal size
    float duration = 5.0f;          // How long decal persists
    float fadeInTime = 0.2f;
    float fadeOutTime = 1.0f;

    glm::vec4 color{1.0f};          // Color tint

    // Animation
    bool rotate = false;
    float rotationSpeed = 0.0f;     // Degrees per second
    bool pulse = false;
    float pulseSpeed = 1.0f;
    float pulseAmplitude = 0.1f;
};

// ============================================================================
// Sound Configuration
// ============================================================================

/**
 * @brief Configuration for sound effects
 */
struct SoundConfig {
    std::string soundPath;          // Path to sound file
    float volume = 1.0f;            // Volume (0-1)
    float pitch = 1.0f;             // Pitch multiplier
    float pitchVariation = 0.0f;    // Random pitch variation

    bool positional = true;         // 3D positional audio
    float minDistance = 1.0f;       // Full volume distance
    float maxDistance = 50.0f;      // Cutoff distance

    float delay = 0.0f;             // Delay before playing
    bool loop = false;              // Loop the sound

    AttachPoint attachTo = AttachPoint::Caster;
};

// ============================================================================
// Visual Effect Entry
// ============================================================================

/**
 * @brief A single visual effect entry
 */
struct VisualEffectEntry {
    std::string id;                 // Unique ID for this effect
    VisualEffectType type;
    AttachPoint attachPoint = AttachPoint::Caster;

    // Timing
    float delay = 0.0f;             // Delay before effect starts
    float duration = 0.0f;          // Duration (0 = default/until spell ends)

    // Trigger condition
    enum class Trigger : uint8_t {
        OnCastStart,
        OnCastComplete,
        OnChannelTick,
        OnProjectileLaunch,
        OnProjectileTravel,
        OnHit,
        OnCrit,
        OnKill,
        OnMiss,
        OnExpire,
        Continuous               // Always active while spell is
    };
    Trigger trigger = Trigger::OnCastStart;

    // Type-specific config (only one is used based on type)
    std::optional<ParticleConfig> particle;
    std::optional<ModelConfig> model;
    std::optional<LightConfig> light;
    std::optional<ScreenEffectConfig> screenEffect;
    std::optional<TrailConfig> trail;
    std::optional<BeamConfig> beam;
    std::optional<DecalConfig> decal;
};

// ============================================================================
// Sound Effect Entry
// ============================================================================

/**
 * @brief A single sound effect entry
 */
struct SoundEffectEntry {
    std::string id;
    SoundConfig config;

    // Trigger (same as visual effects)
    VisualEffectEntry::Trigger trigger = VisualEffectEntry::Trigger::OnCastStart;
};

// ============================================================================
// Animation Entry
// ============================================================================

/**
 * @brief Animation to play on caster/target
 */
struct AnimationEntry {
    std::string id;
    std::string animationName;      // Animation clip name
    float speed = 1.0f;
    float blendTime = 0.2f;         // Blend in time
    bool loop = false;

    // Which entity plays the animation
    enum class Target : uint8_t { Caster, SpellTarget, Both };
    Target target = Target::Caster;

    // Trigger
    VisualEffectEntry::Trigger trigger = VisualEffectEntry::Trigger::OnCastStart;
};

// ============================================================================
// Spell Visuals
// ============================================================================

/**
 * @brief Complete visual configuration for a spell
 */
class SpellVisuals {
public:
    SpellVisuals() = default;
    ~SpellVisuals() = default;

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * @brief Load visuals from JSON string
     */
    bool LoadFromJson(const std::string& jsonString);

    /**
     * @brief Serialize visuals to JSON string
     */
    std::string ToJsonString() const;

    /**
     * @brief Validate visual configuration
     */
    bool Validate(std::vector<std::string>& errors) const;

    // =========================================================================
    // Effect Queries
    // =========================================================================

    /**
     * @brief Get visual effects for a trigger
     */
    std::vector<const VisualEffectEntry*> GetEffectsForTrigger(
        VisualEffectEntry::Trigger trigger
    ) const;

    /**
     * @brief Get sounds for a trigger
     */
    std::vector<const SoundEffectEntry*> GetSoundsForTrigger(
        VisualEffectEntry::Trigger trigger
    ) const;

    /**
     * @brief Get animations for a trigger
     */
    std::vector<const AnimationEntry*> GetAnimationsForTrigger(
        VisualEffectEntry::Trigger trigger
    ) const;

    /**
     * @brief Get all visual effects
     */
    const std::vector<VisualEffectEntry>& GetVisualEffects() const { return m_visualEffects; }

    /**
     * @brief Get all sound effects
     */
    const std::vector<SoundEffectEntry>& GetSoundEffects() const { return m_soundEffects; }

    /**
     * @brief Get all animations
     */
    const std::vector<AnimationEntry>& GetAnimations() const { return m_animations; }

    // =========================================================================
    // Projectile Visuals
    // =========================================================================

    /**
     * @brief Get projectile model path
     */
    [[nodiscard]] const std::string& GetProjectileModel() const { return m_projectileModel; }

    /**
     * @brief Get projectile trail effect
     */
    [[nodiscard]] const std::string& GetProjectileTrail() const { return m_projectileTrail; }

    /**
     * @brief Get projectile scale
     */
    [[nodiscard]] float GetProjectileScale() const { return m_projectileScale; }

    /**
     * @brief Get projectile rotation speed
     */
    [[nodiscard]] const glm::vec3& GetProjectileRotation() const { return m_projectileRotation; }

    // =========================================================================
    // Icon
    // =========================================================================

    /**
     * @brief Get spell icon path
     */
    [[nodiscard]] const std::string& GetIconPath() const { return m_iconPath; }

    /**
     * @brief Get spell icon path when on cooldown
     */
    [[nodiscard]] const std::string& GetCooldownIconPath() const { return m_cooldownIconPath; }

    // =========================================================================
    // Mutators
    // =========================================================================

    void AddVisualEffect(const VisualEffectEntry& effect) { m_visualEffects.push_back(effect); }
    void AddSoundEffect(const SoundEffectEntry& sound) { m_soundEffects.push_back(sound); }
    void AddAnimation(const AnimationEntry& animation) { m_animations.push_back(animation); }

    void SetProjectileModel(const std::string& path) { m_projectileModel = path; }
    void SetProjectileTrail(const std::string& path) { m_projectileTrail = path; }
    void SetProjectileScale(float scale) { m_projectileScale = scale; }
    void SetProjectileRotation(const glm::vec3& rotation) { m_projectileRotation = rotation; }

    void SetIconPath(const std::string& path) { m_iconPath = path; }
    void SetCooldownIconPath(const std::string& path) { m_cooldownIconPath = path; }

private:
    // Visual effects
    std::vector<VisualEffectEntry> m_visualEffects;

    // Sound effects
    std::vector<SoundEffectEntry> m_soundEffects;

    // Animations
    std::vector<AnimationEntry> m_animations;

    // Projectile specifics
    std::string m_projectileModel;
    std::string m_projectileTrail;
    float m_projectileScale = 1.0f;
    glm::vec3 m_projectileRotation{0.0f};   // Rotation speed (degrees/sec)

    // Icons
    std::string m_iconPath;
    std::string m_cooldownIconPath;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert VisualEffectType to string
 */
const char* VisualEffectTypeToString(VisualEffectType type);

/**
 * @brief Parse VisualEffectType from string
 */
VisualEffectType StringToVisualEffectType(const std::string& str);

/**
 * @brief Convert AttachPoint to string
 */
const char* AttachPointToString(AttachPoint point);

/**
 * @brief Parse AttachPoint from string
 */
AttachPoint StringToAttachPoint(const std::string& str);

/**
 * @brief Convert Trigger to string
 */
const char* TriggerToString(VisualEffectEntry::Trigger trigger);

/**
 * @brief Parse Trigger from string
 */
VisualEffectEntry::Trigger StringToTrigger(const std::string& str);

} // namespace Spells
} // namespace Vehement
