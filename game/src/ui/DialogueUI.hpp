#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; } }

namespace Game {
namespace UI {

/**
 * @brief Dialogue choice option
 */
struct DialogueChoice {
    std::string id;
    std::string text;
    bool enabled = true;
    bool visited = false;
    std::string condition; // Optional condition expression
    std::string resultNode; // Node to go to when selected
};

/**
 * @brief Dialogue node
 */
struct DialogueNode {
    std::string id;
    std::string speakerName;
    std::string speakerPortrait;
    std::string text;
    std::vector<DialogueChoice> choices;
    std::string nextNode; // For linear progression
    bool isEnd = false;
    float autoAdvanceDelay = 0; // 0 = wait for input
};

/**
 * @brief Dialogue configuration
 */
struct DialogueConfig {
    float typewriterSpeed = 30.0f; // Characters per second
    bool allowSkip = true;
    bool showSpeakerName = true;
    bool showPortrait = true;
    bool enableHistory = true;
    std::string defaultPortrait;
};

/**
 * @brief Dialogue UI System
 *
 * Text display with typewriter effect, choice buttons,
 * character portraits, and history log.
 */
class DialogueUI {
public:
    DialogueUI();
    ~DialogueUI();

    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);
    void Shutdown();
    void Update(float deltaTime);

    // Visibility
    void Show();
    void Hide();
    bool IsVisible() const { return m_visible; }

    // Configuration
    void SetConfig(const DialogueConfig& config);
    const DialogueConfig& GetConfig() const { return m_config; }

    // Dialogue Control
    void StartDialogue(const std::string& dialogueId);
    void ShowNode(const DialogueNode& node);
    void AdvanceDialogue();
    void SelectChoice(int choiceIndex);
    void SelectChoice(const std::string& choiceId);
    void EndDialogue();
    bool IsDialogueActive() const { return m_active; }

    // Text Display
    void SetText(const std::string& text);
    void SetSpeaker(const std::string& name, const std::string& portraitPath = "");
    void SetTypewriterSpeed(float charsPerSecond);
    void SkipTypewriter();
    bool IsTypewriterComplete() const;

    // Choices
    void SetChoices(const std::vector<DialogueChoice>& choices);
    void ClearChoices();
    void SetChoiceEnabled(const std::string& choiceId, bool enabled);

    // History
    void AddToHistory(const std::string& speaker, const std::string& text);
    void ClearHistory();
    void ShowHistory();
    void HideHistory();

    // Portraits
    void SetLeftPortrait(const std::string& path, bool active = true);
    void SetRightPortrait(const std::string& path, bool active = true);
    void ClearPortraits();

    // Callbacks
    void SetChoiceCallback(std::function<void(const std::string&)> callback);
    void SetDialogueEndCallback(std::function<void()> callback);
    void SetNodeCallback(std::function<void(const std::string&)> callback);

    // Utility
    void SetPosition(const std::string& position); // "bottom", "top", "center"
    void SetSize(int width, int height);

private:
    void UpdateTypewriter(float deltaTime);
    void DisplayCurrentText();
    void SetupEventHandlers();

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    Engine::UI::UIWindow* m_window = nullptr;

    bool m_visible = false;
    bool m_active = false;
    DialogueConfig m_config;

    // Current state
    DialogueNode m_currentNode;
    std::string m_fullText;
    std::string m_displayedText;
    float m_typewriterProgress = 0;
    bool m_typewriterComplete = false;
    float m_autoAdvanceTimer = 0;

    // History
    struct HistoryEntry {
        std::string speaker;
        std::string text;
    };
    std::vector<HistoryEntry> m_history;
    bool m_historyVisible = false;

    // Callbacks
    std::function<void(const std::string&)> m_choiceCallback;
    std::function<void()> m_dialogueEndCallback;
    std::function<void(const std::string&)> m_nodeCallback;
};

} // namespace UI
} // namespace Game
