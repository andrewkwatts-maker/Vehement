#include "BriefingUI.hpp"
#include "engine/ui/runtime/UIBinding.hpp"

namespace Vehement {
namespace UI {
namespace Campaign {

BriefingUI::BriefingUI() = default;
BriefingUI::~BriefingUI() = default;

BriefingUI& BriefingUI::Instance() {
    static BriefingUI instance;
    return instance;
}

bool BriefingUI::Initialize() {
    if (m_initialized) return true;
    m_config = BriefingUIConfig();
    m_visible = false;
    m_initialized = true;
    return true;
}

void BriefingUI::Shutdown() {
    StopVoiceover();
    m_initialized = false;
}

void BriefingUI::SetConfig(const BriefingUIConfig& config) {
    m_config = config;
}

void BriefingUI::Show() {
    m_visible = true;
    m_textScrollPosition = 0.0f;
    SendDataToHTML();

    if (m_config.autoPlayVoiceover && !m_briefingData.voiceoverFile.empty()) {
        PlayVoiceover();
    }
}

void BriefingUI::Hide() {
    m_visible = false;
    StopVoiceover();
}

void BriefingUI::SetBriefingData(const BriefingData& data) {
    m_briefingData = data;
    m_textScrollPosition = 0.0f;
    SendDataToHTML();
}

void BriefingUI::ClearBriefing() {
    m_briefingData = BriefingData();
    SendDataToHTML();
}

void BriefingUI::ShowObjectivesPanel() {
    m_activePanel = "objectives";
    SendDataToHTML();
}

void BriefingUI::ShowTipsPanel() {
    m_activePanel = "tips";
    SendDataToHTML();
}

void BriefingUI::ShowIntelPanel() {
    m_activePanel = "intel";
    SendDataToHTML();
}

void BriefingUI::SetActivePanel(const std::string& panelId) {
    m_activePanel = panelId;
    SendDataToHTML();
}

void BriefingUI::SetDifficulty(int32_t difficulty) {
    m_selectedDifficulty = difficulty;
    if (m_onDifficultyChange) {
        m_onDifficultyChange(difficulty);
    }
    SendDataToHTML();
}

void BriefingUI::PlayVoiceover() {
    if (!m_config.enableVoiceover || m_briefingData.voiceoverFile.empty()) return;
    m_voiceoverPlaying = true;
    m_voiceoverPaused = false;
    m_voiceoverProgress = 0.0f;

    // Play audio file through audio system
    Engine::UI::UIBinding binding;
    nlohmann::json args;
    args["path"] = m_briefingData.voiceoverFile;
    args["channel"] = "voiceover";
    args["volume"] = 1.0f;
    binding.CallJS("Audio.play", args);
}

void BriefingUI::StopVoiceover() {
    m_voiceoverPlaying = false;
    m_voiceoverPaused = false;
    m_voiceoverProgress = 0.0f;

    // Stop audio through audio system
    Engine::UI::UIBinding binding;
    nlohmann::json args;
    args["channel"] = "voiceover";
    binding.CallJS("Audio.stop", args);
}

void BriefingUI::PauseVoiceover() {
    if (m_voiceoverPlaying && !m_voiceoverPaused) {
        m_voiceoverPaused = true;

        // Pause audio through audio system
        Engine::UI::UIBinding binding;
        nlohmann::json args;
        args["channel"] = "voiceover";
        binding.CallJS("Audio.pause", args);
    }
}

void BriefingUI::ResumeVoiceover() {
    if (m_voiceoverPlaying && m_voiceoverPaused) {
        m_voiceoverPaused = false;

        // Resume audio through audio system
        Engine::UI::UIBinding binding;
        nlohmann::json args;
        args["channel"] = "voiceover";
        binding.CallJS("Audio.resume", args);
    }
}

void BriefingUI::ScrollTextToTop() {
    m_textScrollPosition = 0.0f;
    SendDataToHTML();
}

void BriefingUI::ScrollTextToBottom() {
    m_textScrollPosition = 1.0f;
    SendDataToHTML();
}

void BriefingUI::SetTextScrollPosition(float position) {
    m_textScrollPosition = position;
    SendDataToHTML();
}

void BriefingUI::StartMission() {
    if (m_onStartMission) {
        m_onStartMission(m_selectedDifficulty);
    }
}

void BriefingUI::GoBack() {
    if (m_onBack) {
        m_onBack();
    }
}

void BriefingUI::Update(float deltaTime) {
    if (!m_visible) return;
    UpdateVoiceover(deltaTime);
}

void BriefingUI::Render() {
    // HTML-based rendering
}

void BriefingUI::BindToHTML() {
    // Register JavaScript callbacks for briefing UI interactions
    Engine::UI::UIBinding binding;

    binding.ExposeFunction("Briefing.startMission", [this](const nlohmann::json&) -> nlohmann::json {
        HandleHTMLEvent("startMission", "");
        return nullptr;
    });

    binding.ExposeFunction("Briefing.back", [this](const nlohmann::json&) -> nlohmann::json {
        HandleHTMLEvent("back", "");
        return nullptr;
    });

    binding.ExposeFunction("Briefing.setDifficulty", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("difficulty")) {
            HandleHTMLEvent("setDifficulty", std::to_string(args["difficulty"].get<int>()));
        }
        return nullptr;
    });

    binding.ExposeFunction("Briefing.setPanel", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("panel")) {
            HandleHTMLEvent("setPanel", args["panel"].get<std::string>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Briefing.playVoiceover", [this](const nlohmann::json&) -> nlohmann::json {
        HandleHTMLEvent("playVoiceover", "");
        return nullptr;
    });

    binding.ExposeFunction("Briefing.stopVoiceover", [this](const nlohmann::json&) -> nlohmann::json {
        HandleHTMLEvent("stopVoiceover", "");
        return nullptr;
    });

    binding.ExposeFunction("Briefing.pauseVoiceover", [this](const nlohmann::json&) -> nlohmann::json {
        PauseVoiceover();
        return nullptr;
    });

    binding.ExposeFunction("Briefing.resumeVoiceover", [this](const nlohmann::json&) -> nlohmann::json {
        ResumeVoiceover();
        return nullptr;
    });
}

void BriefingUI::HandleHTMLEvent(const std::string& eventName, const std::string& data) {
    if (eventName == "startMission") {
        StartMission();
    } else if (eventName == "back") {
        GoBack();
    } else if (eventName == "setDifficulty") {
        SetDifficulty(std::stoi(data));
    } else if (eventName == "setPanel") {
        SetActivePanel(data);
    } else if (eventName == "playVoiceover") {
        PlayVoiceover();
    } else if (eventName == "stopVoiceover") {
        StopVoiceover();
    }
}

void BriefingUI::SendDataToHTML() {
    // Send briefing data to HTML via JavaScript bindings
    Engine::UI::UIBinding binding;

    nlohmann::json data;
    data["visible"] = m_visible;
    data["activePanel"] = m_activePanel;
    data["selectedDifficulty"] = m_selectedDifficulty;
    data["textScrollPosition"] = m_textScrollPosition;
    data["voiceoverPlaying"] = m_voiceoverPlaying;

    // Mission info
    nlohmann::json missionInfo;
    missionInfo["id"] = m_briefingData.missionId;
    missionInfo["title"] = m_briefingData.missionTitle;
    missionInfo["subtitle"] = m_briefingData.missionSubtitle;
    missionInfo["storyText"] = m_briefingData.storyText;
    missionInfo["mapPreviewImage"] = m_briefingData.mapPreviewImage;
    missionInfo["mapName"] = m_briefingData.mapName;
    missionInfo["voiceoverFile"] = m_briefingData.voiceoverFile;
    missionInfo["backgroundMusic"] = m_briefingData.backgroundMusic;
    missionInfo["estimatedTime"] = m_briefingData.estimatedTime;
    missionInfo["parTime"] = m_briefingData.parTime;
    missionInfo["difficultyDescription"] = m_briefingData.difficultyDescription;
    data["mission"] = missionInfo;

    // Objectives
    nlohmann::json objectivesArray = nlohmann::json::array();
    for (const auto& obj : m_briefingData.objectives) {
        nlohmann::json objJson;
        objJson["title"] = obj.title;
        objJson["description"] = obj.description;
        objJson["isPrimary"] = obj.isPrimary;
        objJson["icon"] = obj.icon;
        objectivesArray.push_back(objJson);
    }
    data["objectives"] = objectivesArray;

    // Tips
    nlohmann::json tipsArray = nlohmann::json::array();
    for (const auto& tip : m_briefingData.tips) {
        nlohmann::json tipJson;
        tipJson["text"] = tip.text;
        tipJson["icon"] = tip.icon;
        tipJson["category"] = tip.category;
        tipsArray.push_back(tipJson);
    }
    data["tips"] = tipsArray;

    // Intel reports
    nlohmann::json intelArray = nlohmann::json::array();
    for (const auto& intel : m_briefingData.intelReports) {
        nlohmann::json intelJson;
        intelJson["title"] = intel.title;
        intelJson["text"] = intel.text;
        intelJson["image"] = intel.image;
        intelJson["isNew"] = intel.isNew;
        intelArray.push_back(intelJson);
    }
    data["intelReports"] = intelArray;

    // Config flags
    nlohmann::json configJson;
    configJson["enableVoiceover"] = m_config.enableVoiceover;
    configJson["showObjectives"] = m_config.showObjectives;
    configJson["showTips"] = m_config.showTips;
    configJson["showIntel"] = m_config.showIntel;
    configJson["showDifficultySelect"] = m_config.showDifficultySelect;
    configJson["showEstimatedTime"] = m_config.showEstimatedTime;
    data["config"] = configJson;

    binding.CallJS("Briefing.updateData", data);
}

void BriefingUI::UpdateVoiceover(float deltaTime) {
    // Update voiceover playback state and sync with UI
    if (!m_voiceoverPlaying || m_voiceoverPaused) return;

    // Query current playback state from audio system
    Engine::UI::UIBinding binding;
    nlohmann::json queryArgs;
    queryArgs["channel"] = "voiceover";

    // Update progress based on elapsed time
    if (m_voiceoverDuration > 0.0f) {
        m_voiceoverProgress += deltaTime / m_voiceoverDuration;

        // Check if voiceover has finished
        if (m_voiceoverProgress >= 1.0f) {
            m_voiceoverProgress = 1.0f;
            m_voiceoverPlaying = false;
            SendDataToHTML();
        }
    }

    // Emit playback progress event for UI synchronization
    nlohmann::json progressData;
    progressData["progress"] = m_voiceoverProgress;
    progressData["playing"] = m_voiceoverPlaying;
    progressData["paused"] = m_voiceoverPaused;
    binding.CallJS("Briefing.onVoiceoverProgress", progressData);
}

} // namespace Campaign
} // namespace UI
} // namespace Vehement
