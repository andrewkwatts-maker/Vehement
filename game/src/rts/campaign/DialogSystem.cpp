#include "DialogSystem.hpp"
#include <algorithm>

namespace Vehement {
namespace RTS {
namespace Campaign {

DialogSystem::DialogSystem() = default;
DialogSystem::~DialogSystem() = default;

DialogSystem& DialogSystem::Instance() {
    static DialogSystem instance;
    return instance;
}

bool DialogSystem::Initialize() {
    if (m_initialized) return true;

    m_config = DialogConfig();
    m_state = DialogState::Inactive;
    m_characters.clear();
    m_history.clear();
    m_flags.clear();
    m_initialized = true;
    return true;
}

void DialogSystem::Shutdown() {
    EndDialog();
    m_characters.clear();
    m_history.clear();
    m_flags.clear();
    m_initialized = false;
}

void DialogSystem::SetConfig(const DialogConfig& config) {
    m_config = config;
}

void DialogSystem::RegisterCharacter(const DialogCharacter& character) {
    m_characters[character.id] = character;
}

void DialogSystem::UnregisterCharacter(const std::string& characterId) {
    m_characters.erase(characterId);
}

const DialogCharacter* DialogSystem::GetCharacter(const std::string& characterId) const {
    auto it = m_characters.find(characterId);
    return (it != m_characters.end()) ? &it->second : nullptr;
}

void DialogSystem::LoadCharactersFromFile(const std::string& /*jsonPath*/) {
    // TODO: Load characters from JSON file
}

void DialogSystem::StartDialog(const DialogTree& tree) {
    m_currentTree = &tree;
    m_state = DialogState::Typing;
    m_history.clear();

    if (m_onDialogStart) {
        m_onDialogStart(tree);
    }

    GoToNode(tree.startNodeId);
}

void DialogSystem::StartDialog(const std::string& /*treeId*/) {
    // TODO: Look up dialog tree by ID
}

void DialogSystem::StartSimpleDialog(const std::string& characterId, const std::string& text) {
    DialogTree tree;
    tree.id = "simple_dialog";

    DialogNode node;
    node.id = "single";
    node.characterId = characterId;
    node.text = text;
    tree.nodes.push_back(node);
    tree.startNodeId = "single";

    StartDialog(tree);
}

void DialogSystem::AdvanceDialog() {
    if (m_state == DialogState::Typing) {
        // Complete typewriter effect
        CompleteTypewriter();
    } else if (m_state == DialogState::WaitingForInput) {
        // Go to next node
        if (m_currentNode && !m_currentNode->nextNodeId.empty()) {
            GoToNode(m_currentNode->nextNodeId);
        } else {
            EndDialog();
        }
    }
}

void DialogSystem::SelectChoice(int32_t choiceIndex) {
    if (m_state != DialogState::WaitingForChoice) return;
    if (!m_currentNode) return;

    if (choiceIndex >= 0 && choiceIndex < static_cast<int32_t>(m_currentNode->choices.size())) {
        const auto& choice = m_currentNode->choices[choiceIndex];

        if (!choice.enabled) return;

        if (m_onChoiceSelected) {
            m_onChoiceSelected(choice);
        }

        // Set flag if specified
        if (!choice.setFlag.empty()) {
            SetFlag(choice.setFlag, true);
        }

        // Execute callback if specified
        if (choice.onSelect) {
            choice.onSelect();
        }

        // Go to next node
        if (!choice.nextNodeId.empty()) {
            GoToNode(choice.nextNodeId);
        } else {
            EndDialog();
        }
    }
}

void DialogSystem::SelectChoice(const std::string& choiceId) {
    if (!m_currentNode) return;

    for (size_t i = 0; i < m_currentNode->choices.size(); ++i) {
        if (m_currentNode->choices[i].id == choiceId) {
            SelectChoice(static_cast<int32_t>(i));
            return;
        }
    }
}

void DialogSystem::SkipDialog() {
    if (!m_currentTree || !m_currentTree->canSkip) return;
    EndDialog();
}

void DialogSystem::EndDialog() {
    if (m_currentNode) {
        ProcessNodeExit();
    }

    StopVoiceover();

    m_currentTree = nullptr;
    m_currentNode = nullptr;
    m_state = DialogState::Inactive;
    m_displayedCharCount = 0;
    m_typewriterTime = 0.0f;

    if (m_onDialogEnd) {
        m_onDialogEnd();
    }
}

void DialogSystem::Update(float deltaTime) {
    if (!m_initialized || m_state == DialogState::Inactive) return;

    switch (m_state) {
        case DialogState::Typing:
            UpdateTypewriter(deltaTime);
            break;

        case DialogState::WaitingForInput:
            if (m_currentNode && m_currentNode->autoAdvance) {
                m_autoAdvanceTimer -= deltaTime;
                if (m_autoAdvanceTimer <= 0.0f) {
                    AdvanceDialog();
                }
            }
            break;

        case DialogState::Transitioning:
            // TODO: Handle transition animation
            break;

        default:
            break;
    }
}

bool DialogSystem::HasChoices() const {
    return m_currentNode && !m_currentNode->choices.empty();
}

const DialogCharacter* DialogSystem::GetCurrentCharacter() const {
    if (!m_currentNode) return nullptr;
    return GetCharacter(m_currentNode->characterId);
}

std::string DialogSystem::GetDisplayedText() const {
    if (!m_currentNode) return "";

    if (m_config.enableTypewriter && m_state == DialogState::Typing) {
        return m_currentNode->text.substr(0, m_displayedCharCount);
    }
    return m_currentNode->text;
}

float DialogSystem::GetTextProgress() const {
    if (!m_currentNode || m_currentNode->text.empty()) return 1.0f;
    return static_cast<float>(m_displayedCharCount) / static_cast<float>(m_currentNode->text.length());
}

const std::vector<DialogChoice>& DialogSystem::GetCurrentChoices() const {
    static std::vector<DialogChoice> empty;
    if (!m_currentNode) return empty;
    return m_currentNode->choices;
}

std::string DialogSystem::GetCurrentPortrait() const {
    const auto* character = GetCurrentCharacter();
    if (!character) return "";

    if (m_currentNode && !m_currentNode->emotion.empty()) {
        auto it = character->portraits.find(m_currentNode->emotion);
        if (it != character->portraits.end()) {
            return it->second;
        }
    }
    return character->defaultPortrait;
}

std::string DialogSystem::GetCurrentCharacterName() const {
    const auto* character = GetCurrentCharacter();
    if (!character) return "";
    return character->name;
}

void DialogSystem::ClearHistory() {
    m_history.clear();
}

void DialogSystem::SetFlag(const std::string& flag, bool value) {
    m_flags[flag] = value;
}

bool DialogSystem::GetFlag(const std::string& flag) const {
    auto it = m_flags.find(flag);
    return (it != m_flags.end()) ? it->second : false;
}

void DialogSystem::GoToNode(const std::string& nodeId) {
    if (!m_currentTree) return;

    // Exit current node
    if (m_currentNode) {
        ProcessNodeExit();
    }

    // Find next node
    m_currentNode = m_currentTree->GetNode(nodeId);
    if (!m_currentNode) {
        EndDialog();
        return;
    }

    // Check condition
    if (!m_currentNode->condition.empty() && !CheckCondition(m_currentNode->condition)) {
        // Skip to next node
        if (!m_currentNode->nextNodeId.empty()) {
            GoToNode(m_currentNode->nextNodeId);
        } else {
            EndDialog();
        }
        return;
    }

    // Reset typewriter
    m_displayedCharCount = 0;
    m_typewriterTime = 0.0f;
    m_autoAdvanceTimer = m_currentNode->autoAdvanceDelay;

    if (m_config.enableTypewriter) {
        m_state = DialogState::Typing;
    } else {
        CompleteTypewriter();
    }

    ProcessNodeEnter();

    if (m_onNodeChange) {
        m_onNodeChange(*m_currentNode);
    }
}

void DialogSystem::UpdateTypewriter(float deltaTime) {
    if (!m_currentNode) return;

    m_typewriterTime += deltaTime;
    size_t targetChars = static_cast<size_t>(m_typewriterTime * m_config.typingSpeed);

    if (targetChars >= m_currentNode->text.length()) {
        CompleteTypewriter();
    } else {
        m_displayedCharCount = targetChars;
    }
}

void DialogSystem::CompleteTypewriter() {
    if (!m_currentNode) return;

    m_displayedCharCount = m_currentNode->text.length();

    // Add to history
    AddToHistory(GetCurrentCharacterName(), m_currentNode->text);

    if (m_onTextComplete) {
        m_onTextComplete();
    }

    // Determine next state
    if (!m_currentNode->choices.empty()) {
        m_state = DialogState::WaitingForChoice;
    } else {
        m_state = DialogState::WaitingForInput;
    }
}

void DialogSystem::AddToHistory(const std::string& character, const std::string& text) {
    m_history.emplace_back(character, text);
}

bool DialogSystem::CheckCondition(const std::string& condition) const {
    // Simple flag check - condition is flag name
    // TODO: Implement more complex condition parsing
    return GetFlag(condition);
}

void DialogSystem::ProcessNodeEnter() {
    if (!m_currentNode) return;

    // Play voiceover
    if (!m_currentNode->voiceFile.empty()) {
        PlayVoiceover(m_currentNode->voiceFile);
    }

    // TODO: Execute enter script
    // TODO: Play sound effects
    // TODO: Apply camera target
}

void DialogSystem::ProcessNodeExit() {
    // TODO: Execute exit script
    StopVoiceover();
}

void DialogSystem::PlayVoiceover(const std::string& /*voiceFile*/) {
    // TODO: Play voice audio
}

void DialogSystem::StopVoiceover() {
    // TODO: Stop voice audio
}

// DialogFactory implementations

std::unique_ptr<DialogTree> DialogFactory::CreateFromJson(const std::string& /*jsonPath*/) {
    // TODO: Load and parse JSON file
    return std::make_unique<DialogTree>();
}

DialogTree DialogFactory::CreateSimple(const std::string& characterId, const std::string& text) {
    DialogTree tree;
    tree.id = "simple";

    DialogNode node;
    node.id = "node1";
    node.characterId = characterId;
    node.text = text;
    tree.nodes.push_back(node);
    tree.startNodeId = "node1";

    return tree;
}

DialogTree DialogFactory::CreateWithChoices(
    const std::string& characterId,
    const std::string& text,
    const std::vector<std::pair<std::string, std::string>>& choices) {

    DialogTree tree;
    tree.id = "choice_dialog";

    DialogNode node;
    node.id = "question";
    node.characterId = characterId;
    node.text = text;

    for (size_t i = 0; i < choices.size(); ++i) {
        DialogChoice choice;
        choice.id = "choice_" + std::to_string(i);
        choice.text = choices[i].first;
        choice.nextNodeId = choices[i].second;
        node.choices.push_back(choice);
    }

    tree.nodes.push_back(node);
    tree.startNodeId = "question";

    return tree;
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
