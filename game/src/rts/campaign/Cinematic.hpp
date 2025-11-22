#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Camera movement types for cinematic scenes
 */
enum class CameraMovementType : uint8_t {
    Static,         ///< No movement
    Pan,            ///< Linear movement
    Zoom,           ///< Zoom in/out
    Orbit,          ///< Circle around target
    Shake,          ///< Camera shake effect
    Follow,         ///< Follow a unit
    Bezier,         ///< Smooth curve path
    Custom          ///< Script-defined movement
};

/**
 * @brief Scene transition types
 */
enum class TransitionType : uint8_t {
    None,           ///< Instant cut
    Fade,           ///< Fade to black
    CrossFade,      ///< Dissolve to next scene
    Wipe,           ///< Wipe transition
    Zoom,           ///< Zoom transition
    Custom
};

/**
 * @brief Position in 3D space
 */
struct CinematicPosition {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float pitch = 0.0f;     ///< Look angle up/down
    float yaw = 0.0f;       ///< Look angle left/right
    float roll = 0.0f;      ///< Rotation
    float fov = 60.0f;      ///< Field of view
};

/**
 * @brief Camera path keyframe
 */
struct CameraKeyframe {
    float time = 0.0f;              ///< Time in seconds
    CinematicPosition position;
    CinematicPosition target;       ///< Look-at target
    float easing = 1.0f;            ///< Easing factor
    std::string easingType;         ///< linear, ease-in, ease-out, ease-in-out
};

/**
 * @brief Camera movement configuration
 */
struct CameraMovement {
    CameraMovementType type = CameraMovementType::Static;
    std::vector<CameraKeyframe> keyframes;
    float duration = 0.0f;
    std::string followTargetId;     ///< Unit ID to follow
    float followDistance = 10.0f;
    float shakeIntensity = 0.0f;
    float shakeDuration = 0.0f;
};

/**
 * @brief Dialog line in a cinematic
 */
struct CinematicDialog {
    std::string characterId;
    std::string characterName;
    std::string text;
    std::string voiceoverFile;
    float startTime = 0.0f;
    float duration = 0.0f;
    std::string portraitImage;
    std::string emotion;            ///< happy, sad, angry, neutral
    bool showSubtitle = true;
    std::string position;           ///< left, right, center
};

/**
 * @brief Sound/music cue
 */
struct AudioCue {
    std::string audioFile;
    float startTime = 0.0f;
    float volume = 1.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    bool loop = false;
    bool isMusic = false;           ///< Music or SFX
    std::string channel;            ///< Audio channel name
};

/**
 * @brief Unit animation command
 */
struct UnitAnimation {
    std::string unitId;
    std::string animation;          ///< Animation name
    float startTime = 0.0f;
    float duration = 0.0f;
    CinematicPosition moveTarget;   ///< Target position for movement
    float moveSpeed = 1.0f;
    bool loop = false;
};

/**
 * @brief Visual effect spawn
 */
struct VisualEffect {
    std::string effectId;
    CinematicPosition position;
    float startTime = 0.0f;
    float duration = 0.0f;
    float scale = 1.0f;
    std::string attachTo;           ///< Unit ID to attach to
};

/**
 * @brief Scene in a cinematic
 */
struct CinematicScene {
    std::string id;
    std::string name;
    float startTime = 0.0f;
    float duration = 0.0f;

    // Camera
    CameraMovement camera;

    // Content
    std::vector<CinematicDialog> dialogs;
    std::vector<AudioCue> audioCues;
    std::vector<UnitAnimation> unitAnimations;
    std::vector<VisualEffect> effects;

    // Transitions
    TransitionType transitionIn = TransitionType::None;
    TransitionType transitionOut = TransitionType::None;
    float transitionDuration = 0.5f;
    std::string transitionColor;    ///< Fade color

    // Environment
    std::string skybox;
    std::string lighting;
    float timeOfDay = 12.0f;        ///< 0-24
    std::string weather;

    // Script
    std::string initScript;
    std::string updateScript;
    std::string endScript;
};

/**
 * @brief Chapter title card
 */
struct TitleCard {
    std::string title;
    std::string subtitle;
    std::string backgroundImage;
    float duration = 5.0f;
    TransitionType transitionIn = TransitionType::Fade;
    TransitionType transitionOut = TransitionType::Fade;
    std::string font;
    std::string textColor;
    std::string animationType;      ///< fade-in, slide-in, typewriter
};

/**
 * @brief Full cinematic sequence
 */
class Cinematic {
public:
    Cinematic() = default;
    explicit Cinematic(const std::string& cinematicId);
    ~Cinematic() = default;

    // Identification
    std::string id;
    std::string title;
    std::string description;

    // Content
    std::vector<CinematicScene> scenes;
    TitleCard titleCard;
    bool hasTitleCard = false;

    // Timing
    float totalDuration = 0.0f;
    bool canSkip = true;
    float skipDelay = 2.0f;         ///< Seconds before skip allowed

    // Audio
    std::string backgroundMusic;
    float musicVolume = 0.8f;
    bool fadeInMusic = true;
    bool fadeOutMusic = true;

    // State
    bool isPlaying = false;
    bool isPaused = false;
    bool isSkipped = false;
    float currentTime = 0.0f;
    int32_t currentSceneIndex = 0;

    // Map/level
    std::string mapFile;            ///< Optional map to load
    bool useGameWorld = true;       ///< Use current game state
    std::vector<std::string> requiredUnits; ///< Units that must exist

    // Callbacks
    std::function<void(Cinematic&)> onStart;
    std::function<void(Cinematic&)> onEnd;
    std::function<void(Cinematic&)> onSkip;
    std::function<void(Cinematic&, int)> onSceneChange;
    std::function<void(Cinematic&, const CinematicDialog&)> onDialog;

    // Methods
    void Initialize();
    void Start();
    void Update(float deltaTime);
    void Pause();
    void Resume();
    void Skip();
    void Stop();
    void Reset();

    [[nodiscard]] CinematicScene* GetCurrentScene();
    [[nodiscard]] const CinematicScene* GetCurrentScene() const;
    [[nodiscard]] CinematicScene* GetScene(const std::string& sceneId);
    void AdvanceToNextScene();
    void GoToScene(const std::string& sceneId);
    void GoToScene(int32_t index);

    void AddScene(const CinematicScene& scene);
    void RemoveScene(const std::string& sceneId);

    [[nodiscard]] float GetProgress() const;
    [[nodiscard]] bool IsComplete() const;
    [[nodiscard]] bool CanSkipNow() const;

    void CalculateDuration();

    // Serialization
    [[nodiscard]] std::string Serialize() const;
    bool Deserialize(const std::string& json);

private:
    void UpdateCurrentScene(float deltaTime);
    void ProcessSceneTransition();
    void TriggerSceneEvents(const CinematicScene& scene);
};

/**
 * @brief Factory for creating cinematics
 */
class CinematicFactory {
public:
    [[nodiscard]] static std::unique_ptr<Cinematic> CreateFromJson(const std::string& jsonPath);
    [[nodiscard]] static std::unique_ptr<Cinematic> CreateSimple(
        const std::string& id,
        const std::string& title,
        float duration);
    [[nodiscard]] static CinematicScene CreateDialogScene(
        const std::string& characterId,
        const std::string& text,
        float duration);
    [[nodiscard]] static CinematicScene CreateCameraPanScene(
        const CinematicPosition& start,
        const CinematicPosition& end,
        float duration);
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
