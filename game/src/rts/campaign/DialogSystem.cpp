#include "DialogSystem.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include "engine/core/json_config.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/scene/Camera.hpp"

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

void DialogSystem::LoadCharactersFromFile(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        return;
    }

    try {
        json data;
        file >> data;

        if (!data.contains("characters") || !data["characters"].is_array()) {
            return;
        }

        for (const auto& charJson : data["characters"]) {
            DialogCharacter character;
            character.id = charJson.value("id", "");
            character.name = charJson.value("name", "");
            character.title = charJson.value("title", "");
            character.defaultPortrait = charJson.value("defaultPortrait", "");
            character.voiceStyle = charJson.value("voiceStyle", "");
            character.faction = charJson.value("faction", "");
            character.textColor = charJson.value("textColor", "#FFFFFF");

            if (charJson.contains("portraits") && charJson["portraits"].is_object()) {
                for (auto it = charJson["portraits"].begin(); it != charJson["portraits"].end(); ++it) {
                    character.portraits[it.key()] = it.value().get<std::string>();
                }
            }

            if (!character.id.empty()) {
                RegisterCharacter(character);
            }
        }
    } catch (const json::exception&) {
        // JSON parsing failed - silently ignore
    }
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

void DialogSystem::StartDialog(const std::string& treeId) {
    auto it = m_dialogTrees.find(treeId);
    if (it != m_dialogTrees.end() && it->second) {
        StartDialog(*it->second);
    }
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
            m_transitionTimer -= deltaTime;
            if (m_transitionTimer <= 0.0f) {
                // Transition complete - move to next appropriate state
                if (m_currentNode && !m_currentNode->choices.empty()) {
                    m_state = DialogState::WaitingForChoice;
                } else if (m_currentNode) {
                    m_state = DialogState::WaitingForInput;
                } else {
                    m_state = DialogState::Inactive;
                }
            }
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
    if (condition.empty()) {
        return true;
    }

    // Handle NOT operator: !flagName
    if (condition[0] == '!') {
        std::string flagName = condition.substr(1);
        return !GetFlag(flagName);
    }

    // Handle AND operator: flag1 && flag2
    size_t andPos = condition.find("&&");
    if (andPos != std::string::npos) {
        std::string left = condition.substr(0, andPos);
        std::string right = condition.substr(andPos + 2);
        // Trim whitespace
        while (!left.empty() && left.back() == ' ') left.pop_back();
        while (!right.empty() && right.front() == ' ') right.erase(0, 1);
        return CheckCondition(left) && CheckCondition(right);
    }

    // Handle OR operator: flag1 || flag2
    size_t orPos = condition.find("||");
    if (orPos != std::string::npos) {
        std::string left = condition.substr(0, orPos);
        std::string right = condition.substr(orPos + 2);
        // Trim whitespace
        while (!left.empty() && left.back() == ' ') left.pop_back();
        while (!right.empty() && right.front() == ' ') right.erase(0, 1);
        return CheckCondition(left) || CheckCondition(right);
    }

    // Handle equality check: flag == true/false
    size_t eqPos = condition.find("==");
    if (eqPos != std::string::npos) {
        std::string flagName = condition.substr(0, eqPos);
        std::string value = condition.substr(eqPos + 2);
        // Trim whitespace
        while (!flagName.empty() && flagName.back() == ' ') flagName.pop_back();
        while (!value.empty() && value.front() == ' ') value.erase(0, 1);
        bool expectedValue = (value == "true" || value == "1");
        return GetFlag(flagName) == expectedValue;
    }

    // Simple flag check - condition is flag name
    return GetFlag(condition);
}

void DialogSystem::ProcessNodeEnter() {
    if (!m_currentNode) return;

    // Play voiceover
    if (!m_currentNode->voiceFile.empty()) {
        PlayVoiceover(m_currentNode->voiceFile);
    }

    // Execute enter script (if script callback is registered)
    if (!m_currentNode->onEnterScript.empty()) {
        // Scripts are executed via the mission manager's script system
        // The dialog system exposes script names that the game layer handles
        SetFlag("_script_enter_" + m_currentNode->id, true);
    }

    // Play sound effects
    if (!m_currentNode->soundEffect.empty()) {
        auto& audio = Nova::AudioEngine::Instance();
        auto buffer = audio.LoadSound(m_currentNode->soundEffect);
        if (buffer) {
            audio.Play2D(buffer);
        }
    }

    // Play ambient sound (looping)
    if (!m_currentNode->ambientSound.empty()) {
        auto& audio = Nova::AudioEngine::Instance();
        auto buffer = audio.LoadSound(m_currentNode->ambientSound);
        if (buffer) {
            auto source = audio.Play2D(buffer, 0.5f);
            if (source) {
                source->SetLooping(true);
            }
        }
    }

    // Apply camera target - store target ID for game layer to handle
    if (!m_currentNode->cameraTarget.empty()) {
        SetFlag("_camera_target", true);
        // The camera target unit ID is stored as a flag key for lookup
        // Game layer should query GetCurrentNode()->cameraTarget to focus camera
    }
}

void DialogSystem::ProcessNodeExit() {
    if (m_currentNode) {
        // Execute exit script (if script callback is registered)
        if (!m_currentNode->onExitScript.empty()) {
            // Scripts are executed via the mission manager's script system
            // The dialog system exposes script names that the game layer handles
            SetFlag("_script_exit_" + m_currentNode->id, true);
        }

        // Clear camera target flag
        if (!m_currentNode->cameraTarget.empty()) {
            SetFlag("_camera_target", false);
        }
    }

    StopVoiceover();
}

void DialogSystem::PlayVoiceover(const std::string& voiceFile) {
    if (!m_config.enableVoice) {
        return;
    }

    // Stop any existing voiceover first
    StopVoiceover();

    auto& audio = Nova::AudioEngine::Instance();
    auto buffer = audio.LoadSound(voiceFile);
    if (buffer) {
        m_voiceoverSource = audio.Play2D(buffer, 1.0f);
    }
}

void DialogSystem::StopVoiceover() {
    if (m_voiceoverSource) {
        m_voiceoverSource->Stop();
        m_voiceoverSource.reset();
    }
}

// DialogFactory implementations

std::unique_ptr<DialogTree> DialogFactory::CreateFromJson(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        return nullptr;
    }

    try {
        json data;
        file >> data;

        auto tree = std::make_unique<DialogTree>();
        tree->id = data.value("id", "");
        tree->title = data.value("title", "");
        tree->startNodeId = data.value("startNodeId", "");
        tree->canSkip = data.value("canSkip", true);
        tree->pauseGame = data.value("pauseGame", false);

        // Parse participants
        if (data.contains("participants") && data["participants"].is_array()) {
            for (const auto& participant : data["participants"]) {
                tree->participants.push_back(participant.get<std::string>());
            }
        }

        // Parse nodes
        if (data.contains("nodes") && data["nodes"].is_array()) {
            for (const auto& nodeJson : data["nodes"]) {
                DialogNode node;
                node.id = nodeJson.value("id", "");
                node.characterId = nodeJson.value("characterId", "");
                node.text = nodeJson.value("text", "");
                node.emotion = nodeJson.value("emotion", "");
                node.voiceFile = nodeJson.value("voiceFile", "");
                node.displayDuration = nodeJson.value("displayDuration", -1.0f);
                node.nextNodeId = nodeJson.value("nextNodeId", "");
                node.autoAdvance = nodeJson.value("autoAdvance", false);
                node.autoAdvanceDelay = nodeJson.value("autoAdvanceDelay", 3.0f);

                // Visual properties
                node.portraitPosition = nodeJson.value("portraitPosition", "left");
                node.backgroundEffect = nodeJson.value("backgroundEffect", "");
                node.cameraTarget = nodeJson.value("cameraTarget", "");

                // Audio properties
                node.soundEffect = nodeJson.value("soundEffect", "");
                node.ambientSound = nodeJson.value("ambientSound", "");

                // Conditions and scripts
                node.condition = nodeJson.value("condition", "");
                node.onEnterScript = nodeJson.value("onEnterScript", "");
                node.onExitScript = nodeJson.value("onExitScript", "");

                // Parse choices
                if (nodeJson.contains("choices") && nodeJson["choices"].is_array()) {
                    for (const auto& choiceJson : nodeJson["choices"]) {
                        DialogChoice choice;
                        choice.id = choiceJson.value("id", "");
                        choice.text = choiceJson.value("text", "");
                        choice.tooltip = choiceJson.value("tooltip", "");
                        choice.enabled = choiceJson.value("enabled", true);
                        choice.visited = choiceJson.value("visited", false);
                        choice.requiredFlag = choiceJson.value("requiredFlag", "");
                        choice.setFlag = choiceJson.value("setFlag", "");
                        choice.nextNodeId = choiceJson.value("nextNodeId", "");
                        node.choices.push_back(choice);
                    }
                }

                tree->nodes.push_back(node);
            }
        }

        return tree;
    } catch (const json::exception&) {
        return nullptr;
    }
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
