#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Character definition for dialog
 */
struct DialogCharacter {
    std::string id;
    std::string name;
    std::string title;                              ///< e.g., "Commander", "High Priest"
    std::map<std::string, std::string> portraits;   ///< Emotion -> portrait path
    std::string defaultPortrait;
    std::string voiceStyle;                         ///< Voice type for TTS/audio
    std::string faction;
    std::string textColor;                          ///< Color for character's text
};

/**
 * @brief Response choice in dialog
 */
struct DialogChoice {
    std::string id;
    std::string text;
    std::string tooltip;
    bool enabled = true;
    bool visited = false;
    std::string requiredFlag;                       ///< Flag required to show
    std::string setFlag;                            ///< Flag to set when selected
    std::string nextNodeId;                         ///< Node to go to
    std::function<void()> onSelect;
};

/**
 * @brief Single dialog node
 */
struct DialogNode {
    std::string id;
    std::string characterId;
    std::string text;
    std::string emotion;                            ///< Portrait emotion
    std::string voiceFile;
    float displayDuration = -1.0f;                  ///< -1 = wait for input
    std::vector<DialogChoice> choices;
    std::string nextNodeId;                         ///< Auto-advance (if no choices)
    bool autoAdvance = false;
    float autoAdvanceDelay = 3.0f;

    // Visual
    std::string portraitPosition;                   ///< left, right, center
    std::string backgroundEffect;
    std::string cameraTarget;                       ///< Unit to focus camera on

    // Audio
    std::string soundEffect;
    std::string ambientSound;

    // Conditions
    std::string condition;                          ///< Show only if condition met
    std::string onEnterScript;
    std::string onExitScript;
};

/**
 * @brief Dialog tree/conversation
 */
struct DialogTree {
    std::string id;
    std::string title;
    std::vector<DialogNode> nodes;
    std::string startNodeId;
    std::vector<std::string> participants;          ///< Character IDs
    bool canSkip = true;
    bool pauseGame = false;

    [[nodiscard]] const DialogNode* GetNode(const std::string& nodeId) const {
        for (const auto& node : nodes) {
            if (node.id == nodeId) return &node;
        }
        return nullptr;
    }
};

/**
 * @brief Dialog system state
 */
enum class DialogState : uint8_t {
    Inactive,
    Typing,                 ///< Text being typed out
    WaitingForInput,        ///< Waiting for player
    WaitingForChoice,       ///< Waiting for choice selection
    Transitioning,          ///< Transitioning between nodes
    Finished
};

/**
 * @brief Configuration for dialog display
 */
struct DialogConfig {
    float typingSpeed = 40.0f;                      ///< Characters per second
    bool enableTypewriter = true;
    bool enableVoice = true;
    bool enableSubtitles = true;
    float portraitAnimationSpeed = 0.3f;
    std::string defaultFont;
    float fontSize = 20.0f;
    std::string dialogBoxStyle;                     ///< UI style preset
    bool showCharacterName = true;
    bool showCharacterTitle = false;
    bool dimBackground = true;
    float dimAmount = 0.5f;
};

/**
 * @brief Dialog system for in-game conversations
 */
class DialogSystem {
public:
    DialogSystem();
    ~DialogSystem();

    // Singleton access
    [[nodiscard]] static DialogSystem& Instance();

    // Delete copy/move
    DialogSystem(const DialogSystem&) = delete;
    DialogSystem& operator=(const DialogSystem&) = delete;
    DialogSystem(DialogSystem&&) = delete;
    DialogSystem& operator=(DialogSystem&&) = delete;

    // Initialization
    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Configuration
    void SetConfig(const DialogConfig& config);
    [[nodiscard]] const DialogConfig& GetConfig() const { return m_config; }

    // Character management
    void RegisterCharacter(const DialogCharacter& character);
    void UnregisterCharacter(const std::string& characterId);
    [[nodiscard]] const DialogCharacter* GetCharacter(const std::string& characterId) const;
    void LoadCharactersFromFile(const std::string& jsonPath);

    // Dialog control
    void StartDialog(const DialogTree& tree);
    void StartDialog(const std::string& treeId);
    void StartSimpleDialog(const std::string& characterId, const std::string& text);
    void AdvanceDialog();
    void SelectChoice(int32_t choiceIndex);
    void SelectChoice(const std::string& choiceId);
    void SkipDialog();
    void EndDialog();

    // Update
    void Update(float deltaTime);

    // State queries
    [[nodiscard]] DialogState GetState() const { return m_state; }
    [[nodiscard]] bool IsActive() const { return m_state != DialogState::Inactive; }
    [[nodiscard]] bool IsWaitingForInput() const {
        return m_state == DialogState::WaitingForInput ||
               m_state == DialogState::WaitingForChoice;
    }
    [[nodiscard]] bool HasChoices() const;

    // Current dialog info
    [[nodiscard]] const DialogTree* GetCurrentTree() const { return m_currentTree; }
    [[nodiscard]] const DialogNode* GetCurrentNode() const { return m_currentNode; }
    [[nodiscard]] const DialogCharacter* GetCurrentCharacter() const;
    [[nodiscard]] std::string GetDisplayedText() const;
    [[nodiscard]] float GetTextProgress() const;
    [[nodiscard]] const std::vector<DialogChoice>& GetCurrentChoices() const;
    [[nodiscard]] std::string GetCurrentPortrait() const;
    [[nodiscard]] std::string GetCurrentCharacterName() const;

    // History
    [[nodiscard]] const std::vector<std::pair<std::string, std::string>>& GetHistory() const { return m_history; }
    void ClearHistory();

    // Events
    void SetOnDialogStart(std::function<void(const DialogTree&)> callback) { m_onDialogStart = callback; }
    void SetOnDialogEnd(std::function<void()> callback) { m_onDialogEnd = callback; }
    void SetOnNodeChange(std::function<void(const DialogNode&)> callback) { m_onNodeChange = callback; }
    void SetOnChoiceSelected(std::function<void(const DialogChoice&)> callback) { m_onChoiceSelected = callback; }
    void SetOnTextComplete(std::function<void()> callback) { m_onTextComplete = callback; }

    // Flag management (for dialog conditions)
    void SetFlag(const std::string& flag, bool value);
    [[nodiscard]] bool GetFlag(const std::string& flag) const;

private:
    bool m_initialized = false;
    DialogConfig m_config;
    DialogState m_state = DialogState::Inactive;

    // Characters
    std::map<std::string, DialogCharacter> m_characters;

    // Current dialog
    const DialogTree* m_currentTree = nullptr;
    const DialogNode* m_currentNode = nullptr;
    size_t m_displayedCharCount = 0;
    float m_typewriterTime = 0.0f;
    float m_autoAdvanceTimer = 0.0f;

    // History
    std::vector<std::pair<std::string, std::string>> m_history; // character, text

    // Flags
    std::map<std::string, bool> m_flags;

    // Callbacks
    std::function<void(const DialogTree&)> m_onDialogStart;
    std::function<void()> m_onDialogEnd;
    std::function<void(const DialogNode&)> m_onNodeChange;
    std::function<void(const DialogChoice&)> m_onChoiceSelected;
    std::function<void()> m_onTextComplete;

    // Internal methods
    void GoToNode(const std::string& nodeId);
    void UpdateTypewriter(float deltaTime);
    void CompleteTypewriter();
    void AddToHistory(const std::string& character, const std::string& text);
    bool CheckCondition(const std::string& condition) const;
    void ProcessNodeEnter();
    void ProcessNodeExit();
    void PlayVoiceover(const std::string& voiceFile);
    void StopVoiceover();
};

/**
 * @brief Factory for creating dialog trees
 */
class DialogFactory {
public:
    [[nodiscard]] static std::unique_ptr<DialogTree> CreateFromJson(const std::string& jsonPath);
    [[nodiscard]] static DialogTree CreateSimple(
        const std::string& characterId,
        const std::string& text);
    [[nodiscard]] static DialogTree CreateWithChoices(
        const std::string& characterId,
        const std::string& text,
        const std::vector<std::pair<std::string, std::string>>& choices);
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
