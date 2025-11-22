#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <queue>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Colorblind Modes
// ============================================================================

/**
 * @brief Colorblind simulation/correction modes
 */
enum class ColorblindMode : uint8_t {
    None = 0,
    Protanopia,      ///< Red-blind
    Deuteranopia,    ///< Green-blind
    Tritanopia,      ///< Blue-blind
    Achromatopsia,   ///< Complete color blindness
    Custom           ///< User-defined color matrix
};

/**
 * @brief Get color transformation matrix for colorblind mode
 */
inline glm::mat3 GetColorblindMatrix(ColorblindMode mode) {
    switch (mode) {
        case ColorblindMode::Protanopia:
            return glm::mat3(
                0.567f, 0.433f, 0.0f,
                0.558f, 0.442f, 0.0f,
                0.0f,   0.242f, 0.758f
            );
        case ColorblindMode::Deuteranopia:
            return glm::mat3(
                0.625f, 0.375f, 0.0f,
                0.7f,   0.3f,   0.0f,
                0.0f,   0.3f,   0.7f
            );
        case ColorblindMode::Tritanopia:
            return glm::mat3(
                0.95f,  0.05f,  0.0f,
                0.0f,   0.433f, 0.567f,
                0.0f,   0.475f, 0.525f
            );
        case ColorblindMode::Achromatopsia:
            return glm::mat3(
                0.299f, 0.587f, 0.114f,
                0.299f, 0.587f, 0.114f,
                0.299f, 0.587f, 0.114f
            );
        default:
            return glm::mat3(1.0f);
    }
}

// ============================================================================
// High Contrast Theme
// ============================================================================

/**
 * @brief High contrast theme colors
 */
struct HighContrastTheme {
    glm::vec4 background{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 foreground{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 accent{1.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 highlight{0.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 error{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 success{0.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 warning{1.0f, 0.5f, 0.0f, 1.0f};
    glm::vec4 disabled{0.5f, 0.5f, 0.5f, 1.0f};

    static HighContrastTheme WhiteOnBlack() {
        return HighContrastTheme{};  // Default is white on black
    }

    static HighContrastTheme BlackOnWhite() {
        HighContrastTheme theme;
        std::swap(theme.background, theme.foreground);
        theme.accent = {0.0f, 0.0f, 1.0f, 1.0f};
        theme.highlight = {1.0f, 0.0f, 1.0f, 1.0f};
        return theme;
    }

    static HighContrastTheme YellowOnBlack() {
        HighContrastTheme theme;
        theme.foreground = {1.0f, 1.0f, 0.0f, 1.0f};
        theme.accent = {0.0f, 1.0f, 1.0f, 1.0f};
        return theme;
    }
};

// ============================================================================
// Screen Reader Interface
// ============================================================================

/**
 * @brief Priority levels for screen reader announcements
 */
enum class AnnouncementPriority : uint8_t {
    Low,       ///< General information
    Normal,    ///< Standard announcements
    High,      ///< Important information
    Critical   ///< Interrupts current speech
};

/**
 * @brief Screen reader announcement
 */
struct Announcement {
    std::string text;
    AnnouncementPriority priority = AnnouncementPriority::Normal;
    float delay = 0.0f;           ///< Delay before speaking
    bool interruptible = true;    ///< Can be interrupted by higher priority
};

/**
 * @brief Interface for screen reader implementations
 */
class IScreenReader {
public:
    virtual ~IScreenReader() = default;

    /**
     * @brief Check if screen reader is available
     */
    [[nodiscard]] virtual bool IsAvailable() const = 0;

    /**
     * @brief Speak text immediately
     */
    virtual void Speak(const std::string& text, bool interrupt = false) = 0;

    /**
     * @brief Stop current speech
     */
    virtual void Stop() = 0;

    /**
     * @brief Check if currently speaking
     */
    [[nodiscard]] virtual bool IsSpeaking() const = 0;

    /**
     * @brief Set speech rate (0.5 to 2.0)
     */
    virtual void SetRate(float rate) = 0;

    /**
     * @brief Set speech volume (0.0 to 1.0)
     */
    virtual void SetVolume(float volume) = 0;
};

// ============================================================================
// Subtitle/Caption System
// ============================================================================

/**
 * @brief Caption type
 */
enum class CaptionType : uint8_t {
    Dialogue,        ///< Character speech
    SoundEffect,     ///< [Sound effect description]
    Music,           ///< Musical cue description
    Narrator,        ///< Narration
    SystemMessage    ///< Game system messages
};

/**
 * @brief A single caption/subtitle entry
 */
struct Caption {
    std::string text;
    std::string speaker;         ///< Speaker name (for dialogue)
    CaptionType type = CaptionType::Dialogue;
    float duration = 0.0f;       ///< Display duration (0 = auto)
    glm::vec4 color{1.0f};       ///< Text color
    bool background = true;      ///< Show background box
    float fontSize = 1.0f;       ///< Relative font size

    // Timing
    float startTime = 0.0f;
    float endTime = 0.0f;
};

/**
 * @brief Caption display settings
 */
struct CaptionSettings {
    bool enabled = true;
    float fontScale = 1.0f;              ///< Font size multiplier
    float backgroundOpacity = 0.75f;     ///< Background box opacity
    glm::vec4 textColor{1.0f};           ///< Override text color
    glm::vec4 backgroundColor{0.0f, 0.0f, 0.0f, 0.75f};
    bool showSpeakerNames = true;        ///< Show who is speaking
    bool showSoundEffects = true;        ///< Show [sound effect] captions
    bool showMusicCues = false;          ///< Show music descriptions
    int maxLines = 3;                    ///< Max visible caption lines
    float edgeMargin = 0.05f;            ///< Screen edge margin
    enum class Position : uint8_t {
        Bottom,
        Top,
        Custom
    } position = Position::Bottom;
    glm::vec2 customPosition{0.5f, 0.9f};
};

// ============================================================================
// Motor Accessibility
// ============================================================================

/**
 * @brief Motor accessibility settings
 */
struct MotorAccessibilitySettings {
    // Input timing
    float holdTimeRequired = 0.0f;       ///< Disable button holds (auto-repeat)
    float doubleTapWindow = 0.5f;        ///< Time window for double-tap
    bool stickyKeys = false;             ///< Modifier keys stay active
    bool slowKeys = false;               ///< Delay before key registers
    float slowKeyDelay = 0.3f;           ///< Slow key delay in seconds

    // One-handed mode
    bool oneHandedMode = false;
    bool leftHanded = false;             ///< Swap left/right controls

    // Auto-aim assistance
    float aimAssistStrength = 0.0f;      ///< 0 to 1
    float aimAssistRadius = 50.0f;       ///< Screen pixels

    // Interaction
    bool autoInteract = false;           ///< Auto-interact with nearby objects
    float interactRadius = 2.0f;         ///< Auto-interact distance

    // Camera
    float cameraShakeReduction = 0.0f;   ///< Reduce camera shake (0 to 1)
    bool reducedMotion = false;          ///< Reduce all motion effects
};

// ============================================================================
// Cognitive Accessibility
// ============================================================================

/**
 * @brief Cognitive accessibility settings
 */
struct CognitiveAccessibilitySettings {
    // Reading aids
    bool dyslexiaFont = false;           ///< Use dyslexia-friendly font
    float lineSpacing = 1.0f;            ///< Line height multiplier
    float wordSpacing = 1.0f;            ///< Word spacing multiplier

    // Visual aids
    bool highlightInteractables = true;  ///< Highlight interactive objects
    bool objectiveMarkers = true;        ///< Always show objective markers
    bool simplifiedUI = false;           ///< Reduce UI complexity
    bool tutorialHints = true;           ///< Show contextual hints

    // Time aids
    bool pauseInMenus = true;            ///< Pause game when in menus
    bool extendedTimers = false;         ///< Give more time for timed events
    float timerExtension = 1.5f;         ///< Timer multiplier

    // Memory aids
    bool questReminders = true;          ///< Periodic quest reminders
    bool autoSaveReminders = true;       ///< Notify when auto-saving
    bool skipCutscenes = true;           ///< Allow skipping all cutscenes
};

// ============================================================================
// Accessibility Manager
// ============================================================================

/**
 * @brief Complete accessibility management system
 *
 * Features:
 * - Screen reader support (platform-specific)
 * - High contrast mode with customizable themes
 * - Colorblind modes (protanopia, deuteranopia, tritanopia)
 * - Scalable fonts and UI
 * - Subtitle/caption system
 * - Motor accessibility options
 * - Cognitive accessibility aids
 *
 * Usage:
 * @code
 * auto& access = Accessibility::Instance();
 * access.Initialize();
 *
 * // Enable high contrast
 * access.SetHighContrastEnabled(true);
 *
 * // Set colorblind mode
 * access.SetColorblindMode(ColorblindMode::Deuteranopia);
 *
 * // Scale UI
 * access.SetUIScale(1.5f);
 *
 * // Announce to screen reader
 * access.Announce("Game started");
 *
 * // Show caption
 * access.ShowCaption({
 *     .text = "Hello, adventurer!",
 *     .speaker = "NPC",
 *     .type = CaptionType::Dialogue
 * });
 * @endcode
 */
class Accessibility {
public:
    /**
     * @brief Get singleton instance
     */
    static Accessibility& Instance();

    // Non-copyable
    Accessibility(const Accessibility&) = delete;
    Accessibility& operator=(const Accessibility&) = delete;

    /**
     * @brief Initialize accessibility system
     */
    bool Initialize();

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update (call each frame)
     */
    void Update(float deltaTime);

    // =========== Screen Reader ===========

    /**
     * @brief Set screen reader implementation
     */
    void SetScreenReader(std::shared_ptr<IScreenReader> reader);

    /**
     * @brief Check if screen reader is available
     */
    [[nodiscard]] bool IsScreenReaderAvailable() const;

    /**
     * @brief Announce text to screen reader
     */
    void Announce(const std::string& text,
                  AnnouncementPriority priority = AnnouncementPriority::Normal);

    /**
     * @brief Queue an announcement
     */
    void QueueAnnouncement(const Announcement& announcement);

    /**
     * @brief Stop current announcement
     */
    void StopAnnouncement();

    /**
     * @brief Enable/disable screen reader support
     */
    void SetScreenReaderEnabled(bool enabled) { m_screenReaderEnabled = enabled; }
    [[nodiscard]] bool IsScreenReaderEnabled() const { return m_screenReaderEnabled; }

    // =========== High Contrast ===========

    /**
     * @brief Enable/disable high contrast mode
     */
    void SetHighContrastEnabled(bool enabled);
    [[nodiscard]] bool IsHighContrastEnabled() const { return m_highContrastEnabled; }

    /**
     * @brief Set high contrast theme
     */
    void SetHighContrastTheme(const HighContrastTheme& theme);
    [[nodiscard]] const HighContrastTheme& GetHighContrastTheme() const { return m_highContrastTheme; }

    /**
     * @brief Get color adjusted for high contrast mode
     */
    [[nodiscard]] glm::vec4 GetAccessibleColor(const glm::vec4& original,
                                                bool isBackground = false) const;

    // =========== Colorblind Mode ===========

    /**
     * @brief Set colorblind mode
     */
    void SetColorblindMode(ColorblindMode mode);
    [[nodiscard]] ColorblindMode GetColorblindMode() const { return m_colorblindMode; }

    /**
     * @brief Get colorblind transformation matrix
     */
    [[nodiscard]] glm::mat3 GetColorblindMatrix() const;

    /**
     * @brief Set custom colorblind matrix
     */
    void SetCustomColorMatrix(const glm::mat3& matrix);

    /**
     * @brief Apply colorblind transformation to a color
     */
    [[nodiscard]] glm::vec3 ApplyColorblindTransform(const glm::vec3& color) const;

    // =========== Font Scaling ===========

    /**
     * @brief Set global font scale
     */
    void SetFontScale(float scale);
    [[nodiscard]] float GetFontScale() const { return m_fontScale; }

    /**
     * @brief Set UI scale
     */
    void SetUIScale(float scale);
    [[nodiscard]] float GetUIScale() const { return m_uiScale; }

    /**
     * @brief Add a font fallback
     */
    void AddFontFallback(const std::string& script, const std::string& fontPath);

    /**
     * @brief Get font path for fallback
     */
    [[nodiscard]] std::string GetFontFallback(const std::string& script) const;

    // =========== Captions/Subtitles ===========

    /**
     * @brief Get caption settings
     */
    CaptionSettings& GetCaptionSettings() { return m_captionSettings; }
    [[nodiscard]] const CaptionSettings& GetCaptionSettings() const { return m_captionSettings; }

    /**
     * @brief Show a caption
     */
    void ShowCaption(const Caption& caption);

    /**
     * @brief Clear all captions
     */
    void ClearCaptions();

    /**
     * @brief Get active captions for rendering
     */
    [[nodiscard]] std::vector<Caption> GetActiveCaptions() const;

    /**
     * @brief Set caption font
     */
    void SetCaptionFont(const std::string& fontPath);

    // =========== Motor Accessibility ===========

    /**
     * @brief Get motor accessibility settings
     */
    MotorAccessibilitySettings& GetMotorSettings() { return m_motorSettings; }
    [[nodiscard]] const MotorAccessibilitySettings& GetMotorSettings() const { return m_motorSettings; }

    /**
     * @brief Check if button hold should be converted to toggle
     */
    [[nodiscard]] bool ShouldConvertHoldToToggle() const {
        return m_motorSettings.holdTimeRequired > 0.0f;
    }

    /**
     * @brief Get aim assist target offset
     */
    [[nodiscard]] glm::vec2 GetAimAssistOffset(const glm::vec2& aimPos,
                                                const std::vector<glm::vec2>& targets) const;

    // =========== Cognitive Accessibility ===========

    /**
     * @brief Get cognitive accessibility settings
     */
    CognitiveAccessibilitySettings& GetCognitiveSettings() { return m_cognitiveSettings; }
    [[nodiscard]] const CognitiveAccessibilitySettings& GetCognitiveSettings() const { return m_cognitiveSettings; }

    /**
     * @brief Get timer multiplier for extended timers
     */
    [[nodiscard]] float GetTimerMultiplier() const {
        return m_cognitiveSettings.extendedTimers ? m_cognitiveSettings.timerExtension : 1.0f;
    }

    // =========== Presets ===========

    /**
     * @brief Apply low vision preset
     */
    void ApplyLowVisionPreset();

    /**
     * @brief Apply motor impairment preset
     */
    void ApplyMotorPreset();

    /**
     * @brief Apply cognitive accessibility preset
     */
    void ApplyCognitivePreset();

    /**
     * @brief Reset all accessibility settings to defaults
     */
    void ResetToDefaults();

    // =========== Persistence ===========

    /**
     * @brief Save settings to JSON
     */
    [[nodiscard]] nlohmann::json SaveSettings() const;

    /**
     * @brief Load settings from JSON
     */
    void LoadSettings(const nlohmann::json& json);

    // =========== Callbacks ===========

    using SettingsChangedCallback = std::function<void()>;

    /**
     * @brief Register callback for settings changes
     */
    uint32_t OnSettingsChanged(SettingsChangedCallback callback);

    /**
     * @brief Remove callback
     */
    void RemoveSettingsCallback(uint32_t id);

private:
    Accessibility() = default;
    ~Accessibility() = default;

    void NotifySettingsChanged();
    void ProcessAnnouncementQueue();

    // Screen reader
    std::shared_ptr<IScreenReader> m_screenReader;
    std::queue<Announcement> m_announcementQueue;
    bool m_screenReaderEnabled = true;
    float m_announcementDelay = 0.0f;

    // Visual
    bool m_highContrastEnabled = false;
    HighContrastTheme m_highContrastTheme;
    ColorblindMode m_colorblindMode = ColorblindMode::None;
    glm::mat3 m_customColorMatrix{1.0f};

    // Scaling
    float m_fontScale = 1.0f;
    float m_uiScale = 1.0f;

    // Font fallbacks
    std::unordered_map<std::string, std::string> m_fontFallbacks;

    // Captions
    CaptionSettings m_captionSettings;
    std::vector<Caption> m_activeCaptions;
    std::string m_captionFont;
    float m_captionTimer = 0.0f;

    // Motor
    MotorAccessibilitySettings m_motorSettings;

    // Cognitive
    CognitiveAccessibilitySettings m_cognitiveSettings;

    // Callbacks
    std::unordered_map<uint32_t, SettingsChangedCallback> m_callbacks;
    uint32_t m_nextCallbackId = 1;

    bool m_initialized = false;
};

// ============================================================================
// Accessibility Implementation
// ============================================================================

inline Accessibility& Accessibility::Instance() {
    static Accessibility instance;
    return instance;
}

inline bool Accessibility::Initialize() {
    m_initialized = true;
    return true;
}

inline void Accessibility::Shutdown() {
    m_screenReader.reset();
    m_activeCaptions.clear();
    m_initialized = false;
}

inline void Accessibility::Update(float deltaTime) {
    // Process announcement queue
    ProcessAnnouncementQueue();

    // Update captions
    auto now = m_captionTimer;
    m_captionTimer += deltaTime;

    m_activeCaptions.erase(
        std::remove_if(m_activeCaptions.begin(), m_activeCaptions.end(),
            [now](const Caption& c) { return c.endTime > 0 && now >= c.endTime; }),
        m_activeCaptions.end());
}

inline void Accessibility::SetScreenReader(std::shared_ptr<IScreenReader> reader) {
    m_screenReader = std::move(reader);
}

inline bool Accessibility::IsScreenReaderAvailable() const {
    return m_screenReader && m_screenReader->IsAvailable();
}

inline void Accessibility::Announce(const std::string& text, AnnouncementPriority priority) {
    QueueAnnouncement({text, priority});
}

inline void Accessibility::QueueAnnouncement(const Announcement& announcement) {
    if (!m_screenReaderEnabled) return;
    m_announcementQueue.push(announcement);
}

inline void Accessibility::StopAnnouncement() {
    if (m_screenReader) {
        m_screenReader->Stop();
    }
}

inline void Accessibility::ProcessAnnouncementQueue() {
    if (!m_screenReader || !m_screenReaderEnabled) return;

    if (!m_announcementQueue.empty() && !m_screenReader->IsSpeaking()) {
        const auto& ann = m_announcementQueue.front();
        m_screenReader->Speak(ann.text, ann.priority == AnnouncementPriority::Critical);
        m_announcementQueue.pop();
    }
}

inline void Accessibility::SetHighContrastEnabled(bool enabled) {
    if (m_highContrastEnabled != enabled) {
        m_highContrastEnabled = enabled;
        NotifySettingsChanged();
    }
}

inline void Accessibility::SetHighContrastTheme(const HighContrastTheme& theme) {
    m_highContrastTheme = theme;
    NotifySettingsChanged();
}

inline glm::vec4 Accessibility::GetAccessibleColor(const glm::vec4& original, bool isBackground) const {
    if (!m_highContrastEnabled) {
        if (m_colorblindMode != ColorblindMode::None) {
            glm::vec3 transformed = ApplyColorblindTransform(glm::vec3(original));
            return glm::vec4(transformed, original.a);
        }
        return original;
    }

    return isBackground ? m_highContrastTheme.background : m_highContrastTheme.foreground;
}

inline void Accessibility::SetColorblindMode(ColorblindMode mode) {
    if (m_colorblindMode != mode) {
        m_colorblindMode = mode;
        NotifySettingsChanged();
    }
}

inline glm::mat3 Accessibility::GetColorblindMatrix() const {
    if (m_colorblindMode == ColorblindMode::Custom) {
        return m_customColorMatrix;
    }
    return Nova::GetColorblindMatrix(m_colorblindMode);
}

inline void Accessibility::SetCustomColorMatrix(const glm::mat3& matrix) {
    m_customColorMatrix = matrix;
    m_colorblindMode = ColorblindMode::Custom;
    NotifySettingsChanged();
}

inline glm::vec3 Accessibility::ApplyColorblindTransform(const glm::vec3& color) const {
    if (m_colorblindMode == ColorblindMode::None) return color;
    return GetColorblindMatrix() * color;
}

inline void Accessibility::SetFontScale(float scale) {
    m_fontScale = std::clamp(scale, 0.5f, 3.0f);
    NotifySettingsChanged();
}

inline void Accessibility::SetUIScale(float scale) {
    m_uiScale = std::clamp(scale, 0.5f, 3.0f);
    NotifySettingsChanged();
}

inline void Accessibility::AddFontFallback(const std::string& script, const std::string& fontPath) {
    m_fontFallbacks[script] = fontPath;
}

inline std::string Accessibility::GetFontFallback(const std::string& script) const {
    auto it = m_fontFallbacks.find(script);
    return (it != m_fontFallbacks.end()) ? it->second : "";
}

inline void Accessibility::ShowCaption(const Caption& caption) {
    if (!m_captionSettings.enabled) return;

    // Filter by type
    if (caption.type == CaptionType::SoundEffect && !m_captionSettings.showSoundEffects) return;
    if (caption.type == CaptionType::Music && !m_captionSettings.showMusicCues) return;

    Caption newCaption = caption;
    newCaption.startTime = m_captionTimer;
    newCaption.endTime = (caption.duration > 0) ?
        m_captionTimer + caption.duration :
        m_captionTimer + std::max(2.0f, caption.text.length() * 0.05f);

    m_activeCaptions.push_back(newCaption);

    // Limit to max lines
    while (m_activeCaptions.size() > static_cast<size_t>(m_captionSettings.maxLines)) {
        m_activeCaptions.erase(m_activeCaptions.begin());
    }

    // Also announce to screen reader
    if (m_screenReaderEnabled && caption.type == CaptionType::Dialogue) {
        std::string announcement = caption.speaker.empty() ?
            caption.text : caption.speaker + ": " + caption.text;
        Announce(announcement, AnnouncementPriority::Normal);
    }
}

inline void Accessibility::ClearCaptions() {
    m_activeCaptions.clear();
}

inline std::vector<Caption> Accessibility::GetActiveCaptions() const {
    return m_activeCaptions;
}

inline void Accessibility::SetCaptionFont(const std::string& fontPath) {
    m_captionFont = fontPath;
}

inline glm::vec2 Accessibility::GetAimAssistOffset(const glm::vec2& aimPos,
                                                    const std::vector<glm::vec2>& targets) const {
    if (m_motorSettings.aimAssistStrength <= 0.0f || targets.empty()) {
        return glm::vec2(0.0f);
    }

    // Find closest target within radius
    float closestDist = m_motorSettings.aimAssistRadius;
    glm::vec2 closestTarget = aimPos;

    for (const auto& target : targets) {
        float dist = glm::length(target - aimPos);
        if (dist < closestDist) {
            closestDist = dist;
            closestTarget = target;
        }
    }

    if (closestDist >= m_motorSettings.aimAssistRadius) {
        return glm::vec2(0.0f);
    }

    // Calculate assist offset
    glm::vec2 offset = closestTarget - aimPos;
    float strength = m_motorSettings.aimAssistStrength *
                     (1.0f - closestDist / m_motorSettings.aimAssistRadius);
    return offset * strength;
}

inline void Accessibility::ApplyLowVisionPreset() {
    SetHighContrastEnabled(true);
    SetHighContrastTheme(HighContrastTheme::YellowOnBlack());
    SetFontScale(1.5f);
    SetUIScale(1.5f);
    m_captionSettings.enabled = true;
    m_captionSettings.fontScale = 1.5f;
    m_captionSettings.backgroundOpacity = 0.9f;
    NotifySettingsChanged();
}

inline void Accessibility::ApplyMotorPreset() {
    m_motorSettings.holdTimeRequired = 0.5f;
    m_motorSettings.stickyKeys = true;
    m_motorSettings.slowKeys = true;
    m_motorSettings.aimAssistStrength = 0.75f;
    m_motorSettings.autoInteract = true;
    m_motorSettings.reducedMotion = true;
    NotifySettingsChanged();
}

inline void Accessibility::ApplyCognitivePreset() {
    m_cognitiveSettings.dyslexiaFont = true;
    m_cognitiveSettings.lineSpacing = 1.5f;
    m_cognitiveSettings.highlightInteractables = true;
    m_cognitiveSettings.simplifiedUI = true;
    m_cognitiveSettings.extendedTimers = true;
    m_cognitiveSettings.questReminders = true;
    NotifySettingsChanged();
}

inline void Accessibility::ResetToDefaults() {
    m_highContrastEnabled = false;
    m_highContrastTheme = HighContrastTheme{};
    m_colorblindMode = ColorblindMode::None;
    m_fontScale = 1.0f;
    m_uiScale = 1.0f;
    m_captionSettings = CaptionSettings{};
    m_motorSettings = MotorAccessibilitySettings{};
    m_cognitiveSettings = CognitiveAccessibilitySettings{};
    NotifySettingsChanged();
}

inline nlohmann::json Accessibility::SaveSettings() const {
    nlohmann::json json;

    json["highContrast"]["enabled"] = m_highContrastEnabled;
    json["colorblindMode"] = static_cast<int>(m_colorblindMode);
    json["fontScale"] = m_fontScale;
    json["uiScale"] = m_uiScale;
    json["screenReaderEnabled"] = m_screenReaderEnabled;

    // Captions
    json["captions"]["enabled"] = m_captionSettings.enabled;
    json["captions"]["fontScale"] = m_captionSettings.fontScale;
    json["captions"]["showSoundEffects"] = m_captionSettings.showSoundEffects;
    json["captions"]["showMusicCues"] = m_captionSettings.showMusicCues;

    // Motor
    json["motor"]["holdTimeRequired"] = m_motorSettings.holdTimeRequired;
    json["motor"]["stickyKeys"] = m_motorSettings.stickyKeys;
    json["motor"]["aimAssistStrength"] = m_motorSettings.aimAssistStrength;
    json["motor"]["reducedMotion"] = m_motorSettings.reducedMotion;

    // Cognitive
    json["cognitive"]["dyslexiaFont"] = m_cognitiveSettings.dyslexiaFont;
    json["cognitive"]["extendedTimers"] = m_cognitiveSettings.extendedTimers;
    json["cognitive"]["simplifiedUI"] = m_cognitiveSettings.simplifiedUI;

    return json;
}

inline void Accessibility::LoadSettings(const nlohmann::json& json) {
    if (json.contains("highContrast")) {
        m_highContrastEnabled = json["highContrast"].value("enabled", false);
    }
    m_colorblindMode = static_cast<ColorblindMode>(json.value("colorblindMode", 0));
    m_fontScale = json.value("fontScale", 1.0f);
    m_uiScale = json.value("uiScale", 1.0f);
    m_screenReaderEnabled = json.value("screenReaderEnabled", true);

    if (json.contains("captions")) {
        m_captionSettings.enabled = json["captions"].value("enabled", true);
        m_captionSettings.fontScale = json["captions"].value("fontScale", 1.0f);
        m_captionSettings.showSoundEffects = json["captions"].value("showSoundEffects", true);
        m_captionSettings.showMusicCues = json["captions"].value("showMusicCues", false);
    }

    if (json.contains("motor")) {
        m_motorSettings.holdTimeRequired = json["motor"].value("holdTimeRequired", 0.0f);
        m_motorSettings.stickyKeys = json["motor"].value("stickyKeys", false);
        m_motorSettings.aimAssistStrength = json["motor"].value("aimAssistStrength", 0.0f);
        m_motorSettings.reducedMotion = json["motor"].value("reducedMotion", false);
    }

    if (json.contains("cognitive")) {
        m_cognitiveSettings.dyslexiaFont = json["cognitive"].value("dyslexiaFont", false);
        m_cognitiveSettings.extendedTimers = json["cognitive"].value("extendedTimers", false);
        m_cognitiveSettings.simplifiedUI = json["cognitive"].value("simplifiedUI", false);
    }

    NotifySettingsChanged();
}

inline uint32_t Accessibility::OnSettingsChanged(SettingsChangedCallback callback) {
    uint32_t id = m_nextCallbackId++;
    m_callbacks[id] = std::move(callback);
    return id;
}

inline void Accessibility::RemoveSettingsCallback(uint32_t id) {
    m_callbacks.erase(id);
}

inline void Accessibility::NotifySettingsChanged() {
    for (const auto& [id, callback] : m_callbacks) {
        callback();
    }
}

} // namespace Nova
