#include "CampaignFile.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <ctime>

namespace Vehement {

CampaignFile::CampaignFile() {
    m_metadata.createdTime = static_cast<uint64_t>(std::time(nullptr));
    m_metadata.modifiedTime = m_metadata.createdTime;
    m_metadata.version = "1.0.0";
}

CampaignFile::~CampaignFile() = default;

bool CampaignFile::Save(const std::string& filepath) {
    return SaveJson(filepath);
}

bool CampaignFile::Load(const std::string& filepath) {
    return LoadJson(filepath);
}

bool CampaignFile::SaveJson(const std::string& filepath) {
    nlohmann::json j;

    // Metadata
    j["metadata"] = {
        {"id", m_metadata.id},
        {"name", m_metadata.name},
        {"description", m_metadata.description},
        {"author", m_metadata.author},
        {"authorId", m_metadata.authorId},
        {"version", m_metadata.version},
        {"created", m_metadata.createdTime},
        {"modified", m_metadata.modifiedTime},
        {"thumbnail", m_metadata.thumbnailPath},
        {"tags", m_metadata.tags},
        {"difficulty", m_metadata.difficulty},
        {"estimatedTime", m_metadata.estimatedTime},
        {"missionCount", GetTotalMissionCount()},
        {"hasMultiplayer", m_metadata.hasMultiplayer},
        {"requiredDLC", m_metadata.requiredDLC}
    };

    // Chapters
    nlohmann::json chaptersJson = nlohmann::json::array();
    for (const auto& chapter : m_chapters) {
        nlohmann::json chapterJson;
        chapterJson["id"] = chapter.id;
        chapterJson["name"] = chapter.name;
        chapterJson["description"] = chapter.description;
        chapterJson["introCinematic"] = chapter.introCinematic;
        chapterJson["interludeCinematic"] = chapter.interludeCinematic;
        chapterJson["outroCinematic"] = chapter.outroCinematic;
        chapterJson["orderIndex"] = chapter.orderIndex;
        chapterJson["unlockCondition"] = chapter.unlockCondition;

        // Missions
        nlohmann::json missionsJson = nlohmann::json::array();
        for (const auto& mission : chapter.missions) {
            nlohmann::json missionJson;
            missionJson["id"] = mission.id;
            missionJson["name"] = mission.name;
            missionJson["description"] = mission.description;
            missionJson["mapFile"] = mission.mapFile;
            missionJson["difficulty"] = mission.difficulty;
            missionJson["estimatedTime"] = mission.estimatedTime;
            missionJson["introCinematic"] = mission.introCinematic;
            missionJson["outroCinematic"] = mission.outroCinematic;
            missionJson["victoryTrigger"] = mission.victoryTrigger;
            missionJson["defeatTrigger"] = mission.defeatTrigger;
            missionJson["briefingText"] = mission.briefingText;
            missionJson["briefingVoice"] = mission.briefingVoice;
            missionJson["availableHeroes"] = mission.availableHeroes;
            missionJson["heroXPReward"] = mission.heroXPReward;
            missionJson["itemRewards"] = mission.itemRewards;
            missionJson["unlocks"] = mission.unlocks;

            // Objectives
            nlohmann::json objectivesJson = nlohmann::json::array();
            for (const auto& obj : mission.objectives) {
                objectivesJson.push_back({
                    {"id", obj.id},
                    {"text", obj.text},
                    {"description", obj.description},
                    {"isPrimary", obj.isPrimary},
                    {"isSecret", obj.isSecret},
                    {"requiredCount", obj.requiredCount},
                    {"iconPath", obj.iconPath}
                });
            }
            missionJson["objectives"] = objectivesJson;

            missionsJson.push_back(missionJson);
        }
        chapterJson["missions"] = missionsJson;

        chaptersJson.push_back(chapterJson);
    }
    j["chapters"] = chaptersJson;

    // Cinematics
    nlohmann::json cinematicsJson = nlohmann::json::array();
    for (const auto& cin : m_cinematics) {
        nlohmann::json cinJson;
        cinJson["id"] = cin.id;
        cinJson["name"] = cin.name;
        cinJson["duration"] = cin.duration;
        cinJson["musicTrack"] = cin.musicTrack;
        cinJson["letterbox"] = cin.letterbox;
        cinJson["skippable"] = cin.skippable;

        // Camera track
        nlohmann::json cameraJson = nlohmann::json::array();
        for (const auto& kf : cin.cameraTrack) {
            cameraJson.push_back({
                {"time", kf.time},
                {"position", {kf.position.x, kf.position.y, kf.position.z}},
                {"target", {kf.target.x, kf.target.y, kf.target.z}},
                {"fov", kf.fov},
                {"roll", kf.roll},
                {"easing", kf.easing}
            });
        }
        cinJson["cameraTrack"] = cameraJson;

        // Events
        nlohmann::json eventsJson = nlohmann::json::array();
        for (const auto& [time, event] : cin.events) {
            eventsJson.push_back({{"time", time}, {"event", event}});
        }
        cinJson["events"] = eventsJson;

        cinematicsJson.push_back(cinJson);
    }
    j["cinematics"] = cinematicsJson;

    // Dialogs
    nlohmann::json dialogsJson = nlohmann::json::array();
    for (const auto& dialog : m_dialogs) {
        nlohmann::json dialogJson;
        dialogJson["id"] = dialog.id;
        dialogJson["name"] = dialog.name;
        dialogJson["backgroundMusic"] = dialog.backgroundMusic;
        dialogJson["ambientSound"] = dialog.ambientSound;
        dialogJson["isSkippable"] = dialog.isSkippable;

        nlohmann::json linesJson = nlohmann::json::array();
        for (const auto& line : dialog.lines) {
            linesJson.push_back({
                {"speakerId", line.speakerId},
                {"speakerName", line.speakerName},
                {"portraitPath", line.portraitPath},
                {"text", line.text},
                {"voiceFile", line.voiceFile},
                {"duration", line.duration},
                {"emotion", line.emotion},
                {"animationTriggers", line.animationTriggers}
            });
        }
        dialogJson["lines"] = linesJson;

        dialogsJson.push_back(dialogJson);
    }
    j["dialogs"] = dialogsJson;

    // Global variables
    j["globalVariables"] = m_globalVariables;

    // Write file
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << j.dump(2);
    return true;
}

bool CampaignFile::LoadJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        // Metadata
        if (j.contains("metadata")) {
            auto& meta = j["metadata"];
            m_metadata.id = meta.value("id", "");
            m_metadata.name = meta.value("name", "");
            m_metadata.description = meta.value("description", "");
            m_metadata.author = meta.value("author", "");
            m_metadata.authorId = meta.value("authorId", "");
            m_metadata.version = meta.value("version", "1.0.0");
            m_metadata.createdTime = meta.value("created", 0ULL);
            m_metadata.modifiedTime = meta.value("modified", 0ULL);
            m_metadata.thumbnailPath = meta.value("thumbnail", "");
            if (meta.contains("tags")) {
                m_metadata.tags = meta["tags"].get<std::vector<std::string>>();
            }
            m_metadata.difficulty = meta.value("difficulty", "medium");
            m_metadata.estimatedTime = meta.value("estimatedTime", "");
            m_metadata.missionCount = meta.value("missionCount", 0);
            m_metadata.hasMultiplayer = meta.value("hasMultiplayer", false);
            m_metadata.requiredDLC = meta.value("requiredDLC", "");
        }

        // Chapters
        m_chapters.clear();
        if (j.contains("chapters")) {
            for (const auto& chapterJson : j["chapters"]) {
                ChapterData chapter;
                chapter.id = chapterJson.value("id", "");
                chapter.name = chapterJson.value("name", "");
                chapter.description = chapterJson.value("description", "");
                chapter.introCinematic = chapterJson.value("introCinematic", "");
                chapter.interludeCinematic = chapterJson.value("interludeCinematic", "");
                chapter.outroCinematic = chapterJson.value("outroCinematic", "");
                chapter.orderIndex = chapterJson.value("orderIndex", 0);
                chapter.unlockCondition = chapterJson.value("unlockCondition", "");
                chapter.isUnlocked = true;

                // Missions
                if (chapterJson.contains("missions")) {
                    for (const auto& missionJson : chapterJson["missions"]) {
                        MissionData mission;
                        mission.id = missionJson.value("id", "");
                        mission.name = missionJson.value("name", "");
                        mission.description = missionJson.value("description", "");
                        mission.mapFile = missionJson.value("mapFile", "");
                        mission.difficulty = missionJson.value("difficulty", "medium");
                        mission.estimatedTime = missionJson.value("estimatedTime", "");
                        mission.introCinematic = missionJson.value("introCinematic", "");
                        mission.outroCinematic = missionJson.value("outroCinematic", "");
                        mission.victoryTrigger = missionJson.value("victoryTrigger", "");
                        mission.defeatTrigger = missionJson.value("defeatTrigger", "");
                        mission.briefingText = missionJson.value("briefingText", "");
                        mission.briefingVoice = missionJson.value("briefingVoice", "");
                        mission.heroXPReward = missionJson.value("heroXPReward", 0);

                        if (missionJson.contains("availableHeroes")) {
                            mission.availableHeroes = missionJson["availableHeroes"].get<std::vector<std::string>>();
                        }
                        if (missionJson.contains("itemRewards")) {
                            mission.itemRewards = missionJson["itemRewards"].get<std::vector<std::string>>();
                        }
                        if (missionJson.contains("unlocks")) {
                            mission.unlocks = missionJson["unlocks"].get<std::vector<std::string>>();
                        }

                        // Objectives
                        if (missionJson.contains("objectives")) {
                            for (const auto& objJson : missionJson["objectives"]) {
                                MissionObjective obj;
                                obj.id = objJson.value("id", "");
                                obj.text = objJson.value("text", "");
                                obj.description = objJson.value("description", "");
                                obj.isPrimary = objJson.value("isPrimary", true);
                                obj.isSecret = objJson.value("isSecret", false);
                                obj.requiredCount = objJson.value("requiredCount", 1);
                                obj.iconPath = objJson.value("iconPath", "");
                                obj.isCompleted = false;
                                obj.isFailed = false;
                                obj.currentCount = 0;
                                mission.objectives.push_back(obj);
                            }
                        }

                        chapter.missions.push_back(mission);
                    }
                }

                m_chapters.push_back(chapter);
            }
        }

        // Cinematics
        m_cinematics.clear();
        if (j.contains("cinematics")) {
            for (const auto& cinJson : j["cinematics"]) {
                CinematicData cin;
                cin.id = cinJson.value("id", "");
                cin.name = cinJson.value("name", "");
                cin.duration = cinJson.value("duration", 0.0f);
                cin.musicTrack = cinJson.value("musicTrack", "");
                cin.letterbox = cinJson.value("letterbox", true);
                cin.skippable = cinJson.value("skippable", true);

                if (cinJson.contains("cameraTrack")) {
                    for (const auto& kfJson : cinJson["cameraTrack"]) {
                        CameraKeyframe kf;
                        kf.time = kfJson.value("time", 0.0f);
                        if (kfJson.contains("position") && kfJson["position"].is_array()) {
                            kf.position = glm::vec3(kfJson["position"][0], kfJson["position"][1], kfJson["position"][2]);
                        }
                        if (kfJson.contains("target") && kfJson["target"].is_array()) {
                            kf.target = glm::vec3(kfJson["target"][0], kfJson["target"][1], kfJson["target"][2]);
                        }
                        kf.fov = kfJson.value("fov", 60.0f);
                        kf.roll = kfJson.value("roll", 0.0f);
                        kf.easing = kfJson.value("easing", "linear");
                        cin.cameraTrack.push_back(kf);
                    }
                }

                if (cinJson.contains("events")) {
                    for (const auto& evtJson : cinJson["events"]) {
                        cin.events.push_back({evtJson["time"], evtJson["event"]});
                    }
                }

                m_cinematics.push_back(cin);
            }
        }

        // Dialogs
        m_dialogs.clear();
        if (j.contains("dialogs")) {
            for (const auto& dialogJson : j["dialogs"]) {
                DialogScene dialog;
                dialog.id = dialogJson.value("id", "");
                dialog.name = dialogJson.value("name", "");
                dialog.backgroundMusic = dialogJson.value("backgroundMusic", "");
                dialog.ambientSound = dialogJson.value("ambientSound", "");
                dialog.isSkippable = dialogJson.value("isSkippable", true);

                if (dialogJson.contains("lines")) {
                    for (const auto& lineJson : dialogJson["lines"]) {
                        DialogLine line;
                        line.speakerId = lineJson.value("speakerId", "");
                        line.speakerName = lineJson.value("speakerName", "");
                        line.portraitPath = lineJson.value("portraitPath", "");
                        line.text = lineJson.value("text", "");
                        line.voiceFile = lineJson.value("voiceFile", "");
                        line.duration = lineJson.value("duration", 0.0f);
                        line.emotion = lineJson.value("emotion", "neutral");
                        if (lineJson.contains("animationTriggers")) {
                            line.animationTriggers = lineJson["animationTriggers"].get<std::vector<std::string>>();
                        }
                        dialog.lines.push_back(line);
                    }
                }

                m_dialogs.push_back(dialog);
            }
        }

        // Global variables
        if (j.contains("globalVariables")) {
            m_globalVariables = j["globalVariables"].get<std::unordered_map<std::string, std::string>>();
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool CampaignFile::SaveProgress(const std::string& filepath, const CampaignSaveData& data) {
    nlohmann::json j;

    j["campaignId"] = data.campaignId;
    j["saveSlotName"] = data.saveSlotName;
    j["saveTime"] = data.saveTime;
    j["currentMission"] = data.currentMission;
    j["completedMissions"] = data.completedMissions;
    j["variables"] = data.variables;
    j["heroLevels"] = data.heroLevels;
    j["heroXP"] = data.heroXP;
    j["unlockedItems"] = data.unlockedItems;
    j["unlockedUnits"] = data.unlockedUnits;
    j["achievements"] = data.achievements;
    j["totalPlayTime"] = data.totalPlayTime;
    j["deathCount"] = data.deathCount;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << j.dump(2);
    return true;
}

bool CampaignFile::LoadProgress(const std::string& filepath, CampaignSaveData& data) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        data.campaignId = j.value("campaignId", "");
        data.saveSlotName = j.value("saveSlotName", "");
        data.saveTime = j.value("saveTime", 0ULL);
        data.currentMission = j.value("currentMission", "");
        if (j.contains("completedMissions")) {
            data.completedMissions = j["completedMissions"].get<std::vector<std::string>>();
        }
        if (j.contains("variables")) {
            data.variables = j["variables"].get<std::unordered_map<std::string, std::string>>();
        }
        if (j.contains("heroLevels")) {
            data.heroLevels = j["heroLevels"].get<std::unordered_map<std::string, int>>();
        }
        if (j.contains("heroXP")) {
            data.heroXP = j["heroXP"].get<std::unordered_map<std::string, int>>();
        }
        if (j.contains("unlockedItems")) {
            data.unlockedItems = j["unlockedItems"].get<std::vector<std::string>>();
        }
        if (j.contains("unlockedUnits")) {
            data.unlockedUnits = j["unlockedUnits"].get<std::vector<std::string>>();
        }
        if (j.contains("achievements")) {
            data.achievements = j["achievements"].get<std::vector<std::string>>();
        }
        data.totalPlayTime = j.value("totalPlayTime", 0.0f);
        data.deathCount = j.value("deathCount", 0);

        return true;
    } catch (...) {
        return false;
    }
}

void CampaignFile::AddChapter(const ChapterData& chapter) {
    m_chapters.push_back(chapter);
}

void CampaignFile::RemoveChapter(const std::string& id) {
    m_chapters.erase(
        std::remove_if(m_chapters.begin(), m_chapters.end(),
            [&id](const ChapterData& c) { return c.id == id; }),
        m_chapters.end());
}

ChapterData* CampaignFile::GetChapter(const std::string& id) {
    for (auto& chapter : m_chapters) {
        if (chapter.id == id) {
            return &chapter;
        }
    }
    return nullptr;
}

MissionData* CampaignFile::GetMission(const std::string& missionId) {
    for (auto& chapter : m_chapters) {
        for (auto& mission : chapter.missions) {
            if (mission.id == missionId) {
                return &mission;
            }
        }
    }
    return nullptr;
}

MissionData* CampaignFile::GetMissionByIndex(int chapterIndex, int missionIndex) {
    if (chapterIndex < 0 || chapterIndex >= static_cast<int>(m_chapters.size())) {
        return nullptr;
    }

    auto& chapter = m_chapters[chapterIndex];
    if (missionIndex < 0 || missionIndex >= static_cast<int>(chapter.missions.size())) {
        return nullptr;
    }

    return &chapter.missions[missionIndex];
}

std::vector<MissionData*> CampaignFile::GetAllMissions() {
    std::vector<MissionData*> missions;
    for (auto& chapter : m_chapters) {
        for (auto& mission : chapter.missions) {
            missions.push_back(&mission);
        }
    }
    return missions;
}

void CampaignFile::AddMissionToChapter(const std::string& chapterId, const MissionData& mission) {
    if (auto* chapter = GetChapter(chapterId)) {
        chapter->missions.push_back(mission);
    }
}

void CampaignFile::AddCinematic(const CinematicData& cinematic) {
    m_cinematics.push_back(cinematic);
}

void CampaignFile::RemoveCinematic(const std::string& id) {
    m_cinematics.erase(
        std::remove_if(m_cinematics.begin(), m_cinematics.end(),
            [&id](const CinematicData& c) { return c.id == id; }),
        m_cinematics.end());
}

CinematicData* CampaignFile::GetCinematic(const std::string& id) {
    for (auto& cin : m_cinematics) {
        if (cin.id == id) {
            return &cin;
        }
    }
    return nullptr;
}

void CampaignFile::AddDialog(const DialogScene& dialog) {
    m_dialogs.push_back(dialog);
}

void CampaignFile::RemoveDialog(const std::string& id) {
    m_dialogs.erase(
        std::remove_if(m_dialogs.begin(), m_dialogs.end(),
            [&id](const DialogScene& d) { return d.id == id; }),
        m_dialogs.end());
}

DialogScene* CampaignFile::GetDialog(const std::string& id) {
    for (auto& dialog : m_dialogs) {
        if (dialog.id == id) {
            return &dialog;
        }
    }
    return nullptr;
}

void CampaignFile::SetGlobalVariable(const std::string& name, const std::string& value) {
    m_globalVariables[name] = value;
}

std::string CampaignFile::GetGlobalVariable(const std::string& name) const {
    auto it = m_globalVariables.find(name);
    return it != m_globalVariables.end() ? it->second : "";
}

bool CampaignFile::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    if (m_metadata.name.empty()) {
        errors.push_back("Campaign name is required");
        valid = false;
    }

    if (m_chapters.empty()) {
        errors.push_back("Campaign must have at least one chapter");
        valid = false;
    }

    for (const auto& chapter : m_chapters) {
        valid &= ValidateChapter(chapter, errors);
    }

    return valid;
}

bool CampaignFile::ValidateChapter(const ChapterData& chapter, std::vector<std::string>& errors) const {
    bool valid = true;

    if (chapter.id.empty()) {
        errors.push_back("Chapter ID is required");
        valid = false;
    }

    if (chapter.missions.empty()) {
        errors.push_back("Chapter '" + chapter.name + "' has no missions");
        valid = false;
    }

    for (const auto& mission : chapter.missions) {
        valid &= ValidateMission(mission, errors);
    }

    return valid;
}

bool CampaignFile::ValidateMission(const MissionData& mission, std::vector<std::string>& errors) const {
    bool valid = true;

    if (mission.id.empty()) {
        errors.push_back("Mission ID is required");
        valid = false;
    }

    if (mission.mapFile.empty()) {
        errors.push_back("Mission '" + mission.name + "' has no map file");
        valid = false;
    }

    return valid;
}

int CampaignFile::GetTotalMissionCount() const {
    int count = 0;
    for (const auto& chapter : m_chapters) {
        count += static_cast<int>(chapter.missions.size());
    }
    return count;
}

std::vector<std::string> CampaignFile::GetRequiredMapFiles() const {
    std::vector<std::string> maps;
    for (const auto& chapter : m_chapters) {
        for (const auto& mission : chapter.missions) {
            if (!mission.mapFile.empty()) {
                maps.push_back(mission.mapFile);
            }
        }
    }
    return maps;
}

} // namespace Vehement
