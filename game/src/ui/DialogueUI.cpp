#include "DialogueUI.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIWindow.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include "engine/ui/runtime/UIAnimation.hpp"

namespace Game {
namespace UI {

DialogueUI::DialogueUI() = default;
DialogueUI::~DialogueUI() { Shutdown(); }

bool DialogueUI::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;
    m_uiManager = uiManager;

    m_window = m_uiManager->CreateWindow("dialogue", "game/assets/ui/html/dialogue.html",
                                         Engine::UI::UILayer::Popups);
    if (!m_window) return false;

    m_window->SetTitleBarVisible(false);
    m_window->SetResizable(false);
    m_window->SetDraggable(false);
    m_window->Hide();

    SetupEventHandlers();
    return true;
}

void DialogueUI::Shutdown() {
    if (m_uiManager && m_window) {
        m_uiManager->CloseWindow("dialogue");
        m_window = nullptr;
    }
}

void DialogueUI::Update(float deltaTime) {
    if (!m_visible || !m_active) return;

    UpdateTypewriter(deltaTime);

    // Auto-advance handling
    if (m_typewriterComplete && m_currentNode.autoAdvanceDelay > 0) {
        m_autoAdvanceTimer += deltaTime;
        if (m_autoAdvanceTimer >= m_currentNode.autoAdvanceDelay) {
            AdvanceDialogue();
        }
    }
}

void DialogueUI::Show() {
    m_visible = true;
    if (m_window) {
        m_window->Show();
        m_uiManager->GetAnimation().Play("slideInLeft", "dialogue");
    }
}

void DialogueUI::Hide() {
    m_visible = false;
    m_active = false;
    if (m_window) {
        m_window->Hide();
    }
}

void DialogueUI::SetConfig(const DialogueConfig& config) {
    m_config = config;
}

void DialogueUI::StartDialogue(const std::string& dialogueId) {
    m_active = true;
    Show();
    if (m_nodeCallback) {
        m_nodeCallback(dialogueId);
    }
}

void DialogueUI::ShowNode(const DialogueNode& node) {
    m_currentNode = node;
    m_autoAdvanceTimer = 0;

    SetSpeaker(node.speakerName, node.speakerPortrait);
    SetText(node.text);
    SetChoices(node.choices);

    if (m_config.enableHistory) {
        AddToHistory(node.speakerName, node.text);
    }
}

void DialogueUI::AdvanceDialogue() {
    if (!m_typewriterComplete) {
        SkipTypewriter();
        return;
    }

    if (m_currentNode.isEnd) {
        EndDialogue();
        return;
    }

    if (!m_currentNode.nextNode.empty() && m_currentNode.choices.empty()) {
        if (m_nodeCallback) {
            m_nodeCallback(m_currentNode.nextNode);
        }
    }
}

void DialogueUI::SelectChoice(int choiceIndex) {
    if (choiceIndex < 0 || choiceIndex >= static_cast<int>(m_currentNode.choices.size())) return;
    SelectChoice(m_currentNode.choices[choiceIndex].id);
}

void DialogueUI::SelectChoice(const std::string& choiceId) {
    for (const auto& choice : m_currentNode.choices) {
        if (choice.id == choiceId && choice.enabled) {
            if (m_choiceCallback) {
                m_choiceCallback(choiceId);
            }
            if (!choice.resultNode.empty() && m_nodeCallback) {
                m_nodeCallback(choice.resultNode);
            }
            break;
        }
    }
}

void DialogueUI::EndDialogue() {
    m_active = false;
    Hide();
    if (m_dialogueEndCallback) {
        m_dialogueEndCallback();
    }
}

void DialogueUI::SetText(const std::string& text) {
    m_fullText = text;
    m_displayedText = "";
    m_typewriterProgress = 0;
    m_typewriterComplete = false;

    if (m_config.typewriterSpeed <= 0) {
        SkipTypewriter();
    }
}

void DialogueUI::SetSpeaker(const std::string& name, const std::string& portraitPath) {
    auto& binding = m_uiManager->GetBinding();
    binding.CallJS("Dialogue.setSpeaker", {{"name", name}, {"portrait", portraitPath}});
}

void DialogueUI::SetTypewriterSpeed(float charsPerSecond) {
    m_config.typewriterSpeed = charsPerSecond;
}

void DialogueUI::SkipTypewriter() {
    m_displayedText = m_fullText;
    m_typewriterProgress = static_cast<float>(m_fullText.length());
    m_typewriterComplete = true;
    DisplayCurrentText();
}

bool DialogueUI::IsTypewriterComplete() const {
    return m_typewriterComplete;
}

void DialogueUI::SetChoices(const std::vector<DialogueChoice>& choices) {
    nlohmann::json choicesJson = nlohmann::json::array();
    for (size_t i = 0; i < choices.size(); ++i) {
        nlohmann::json c;
        c["id"] = choices[i].id;
        c["text"] = choices[i].text;
        c["enabled"] = choices[i].enabled;
        c["visited"] = choices[i].visited;
        c["index"] = i;
        choicesJson.push_back(c);
    }
    m_uiManager->GetBinding().CallJS("Dialogue.setChoices", choicesJson);
}

void DialogueUI::ClearChoices() {
    m_uiManager->GetBinding().CallJS("Dialogue.clearChoices", {});
}

void DialogueUI::SetChoiceEnabled(const std::string& choiceId, bool enabled) {
    for (auto& choice : m_currentNode.choices) {
        if (choice.id == choiceId) {
            choice.enabled = enabled;
            break;
        }
    }
    SetChoices(m_currentNode.choices);
}

void DialogueUI::AddToHistory(const std::string& speaker, const std::string& text) {
    m_history.push_back({speaker, text});

    nlohmann::json entry;
    entry["speaker"] = speaker;
    entry["text"] = text;
    m_uiManager->GetBinding().CallJS("Dialogue.addHistory", entry);
}

void DialogueUI::ClearHistory() {
    m_history.clear();
    m_uiManager->GetBinding().CallJS("Dialogue.clearHistory", {});
}

void DialogueUI::ShowHistory() {
    m_historyVisible = true;
    m_uiManager->GetBinding().CallJS("Dialogue.showHistory", {});
}

void DialogueUI::HideHistory() {
    m_historyVisible = false;
    m_uiManager->GetBinding().CallJS("Dialogue.hideHistory", {});
}

void DialogueUI::SetLeftPortrait(const std::string& path, bool active) {
    m_uiManager->GetBinding().CallJS("Dialogue.setLeftPortrait", {{"path", path}, {"active", active}});
}

void DialogueUI::SetRightPortrait(const std::string& path, bool active) {
    m_uiManager->GetBinding().CallJS("Dialogue.setRightPortrait", {{"path", path}, {"active", active}});
}

void DialogueUI::ClearPortraits() {
    SetLeftPortrait("", false);
    SetRightPortrait("", false);
}

void DialogueUI::SetChoiceCallback(std::function<void(const std::string&)> callback) {
    m_choiceCallback = callback;
}

void DialogueUI::SetDialogueEndCallback(std::function<void()> callback) {
    m_dialogueEndCallback = callback;
}

void DialogueUI::SetNodeCallback(std::function<void(const std::string&)> callback) {
    m_nodeCallback = callback;
}

void DialogueUI::SetPosition(const std::string& position) {
    m_uiManager->GetBinding().CallJS("Dialogue.setPosition", {{"position", position}});
}

void DialogueUI::SetSize(int width, int height) {
    if (m_window) {
        m_window->Resize(width, height);
    }
}

void DialogueUI::UpdateTypewriter(float deltaTime) {
    if (m_typewriterComplete) return;

    m_typewriterProgress += m_config.typewriterSpeed * deltaTime;
    int charCount = static_cast<int>(m_typewriterProgress);

    if (charCount >= static_cast<int>(m_fullText.length())) {
        m_displayedText = m_fullText;
        m_typewriterComplete = true;
    } else {
        m_displayedText = m_fullText.substr(0, charCount);
    }

    DisplayCurrentText();
}

void DialogueUI::DisplayCurrentText() {
    m_uiManager->GetBinding().CallJS("Dialogue.setText", {{"text", m_displayedText}});
}

void DialogueUI::SetupEventHandlers() {
    auto& binding = m_uiManager->GetBinding();

    binding.ExposeFunction("Dialogue.onChoiceSelect", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            SelectChoice(args["id"].get<std::string>());
        } else if (args.contains("index")) {
            SelectChoice(args["index"].get<int>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Dialogue.onContinue", [this](const nlohmann::json&) -> nlohmann::json {
        AdvanceDialogue();
        return nullptr;
    });

    binding.ExposeFunction("Dialogue.onSkip", [this](const nlohmann::json&) -> nlohmann::json {
        SkipTypewriter();
        return nullptr;
    });
}

} // namespace UI
} // namespace Game
