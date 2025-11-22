#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace Vehement {

/**
 * @brief Campaign metadata
 */
struct CampaignMetadata {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string authorId;
    std::string version;
    uint64_t createdTime;
    uint64_t modifiedTime;
    std::string thumbnailPath;
    std::vector<std::string> tags;
    std::string difficulty;  // easy, medium, hard
    std::string estimatedTime;  // e.g., "4-6 hours"
    int missionCount;
    bool hasMultiplayer;
    std::string requiredDLC;
};

/**
 * @brief Dialog line data
 */
struct DialogLine {
    std::string speakerId;
    std::string speakerName;
    std::string portraitPath;
    std::string text;
    std::string voiceFile;
    float duration;  // Auto-calculated if 0
    std::string emotion;  // neutral, happy, angry, sad
    std::vector<std::string> animationTriggers;
};

/**
 * @brief Dialog scene
 */
struct DialogScene {
    std::string id;
    std::string name;
    std::vector<DialogLine> lines;
    std::string backgroundMusic;
    std::string ambientSound;
    bool isSkippable;
};

/**
 * @brief Cinematic camera keyframe
 */
struct CameraKeyframe {
    float time;
    glm::vec3 position;
    glm::vec3 target;
    float fov;
    float roll;
    std::string easing;  // linear, ease-in, ease-out, ease-in-out
};

/**
 * @brief Cinematic data
 */
struct CinematicData {
    std::string id;
    std::string name;
    float duration;
    std::vector<CameraKeyframe> cameraTrack;
    std::vector<DialogScene> dialogs;
    std::string musicTrack;
    bool letterbox;
    bool skippable;
    std::vector<std::pair<float, std::string>> events;  // time -> event name
};

/**
 * @brief Mission objective
 */
struct MissionObjective {
    std::string id;
    std::string text;
    std::string description;
    bool isPrimary;
    bool isSecret;
    bool isCompleted;
    bool isFailed;
    int requiredCount;  // For "kill X units" objectives
    int currentCount;
    std::string iconPath;
};

/**
 * @brief Mission data
 */
struct MissionData {
    std::string id;
    std::string name;
    std::string description;
    std::string mapFile;
    std::string difficulty;
    std::string estimatedTime;
    std::vector<MissionObjective> objectives;
    std::string introCinematic;
    std::string outroCinematic;
    std::string victoryTrigger;
    std::string defeatTrigger;
    std::string briefingText;
    std::string briefingVoice;
    std::vector<std::string> availableHeroes;
    std::unordered_map<std::string, std::string> variables;  // Persistent variables
    int heroXPReward;
    std::vector<std::string> itemRewards;
    std::vector<std::string> unlocks;  // Units, abilities, etc.
};

/**
 * @brief Chapter data
 */
struct ChapterData {
    std::string id;
    std::string name;
    std::string description;
    std::vector<MissionData> missions;
    std::string introCinematic;
    std::string interludeCinematic;
    std::string outroCinematic;
    int orderIndex;
    bool isUnlocked;
    std::string unlockCondition;
};

/**
 * @brief Campaign save data (player progress)
 */
struct CampaignSaveData {
    std::string campaignId;
    std::string saveSlotName;
    uint64_t saveTime;
    std::string currentMission;
    std::vector<std::string> completedMissions;
    std::unordered_map<std::string, std::string> variables;
    std::unordered_map<std::string, int> heroLevels;
    std::unordered_map<std::string, int> heroXP;
    std::vector<std::string> unlockedItems;
    std::vector<std::string> unlockedUnits;
    std::vector<std::string> achievements;
    float totalPlayTime;
    int deathCount;
};

/**
 * @brief Campaign File - Save/load campaigns
 *
 * Features:
 * - Multi-mission campaigns
 * - Story/dialog system
 * - Cinematics
 * - Progress tracking
 * - Variable persistence
 */
class CampaignFile {
public:
    CampaignFile();
    ~CampaignFile();

    // File operations
    bool Save(const std::string& filepath);
    bool Load(const std::string& filepath);
    bool SaveProgress(const std::string& filepath, const CampaignSaveData& data);
    bool LoadProgress(const std::string& filepath, CampaignSaveData& data);

    // Metadata
    CampaignMetadata& GetMetadata() { return m_metadata; }
    const CampaignMetadata& GetMetadata() const { return m_metadata; }
    void SetMetadata(const CampaignMetadata& metadata) { m_metadata = metadata; }

    // Chapters
    const std::vector<ChapterData>& GetChapters() const { return m_chapters; }
    void AddChapter(const ChapterData& chapter);
    void RemoveChapter(const std::string& id);
    ChapterData* GetChapter(const std::string& id);
    void ReorderChapters(const std::vector<std::string>& order);

    // Missions
    MissionData* GetMission(const std::string& missionId);
    MissionData* GetMissionByIndex(int chapterIndex, int missionIndex);
    std::vector<MissionData*> GetAllMissions();
    void AddMissionToChapter(const std::string& chapterId, const MissionData& mission);

    // Cinematics
    const std::vector<CinematicData>& GetCinematics() const { return m_cinematics; }
    void AddCinematic(const CinematicData& cinematic);
    void RemoveCinematic(const std::string& id);
    CinematicData* GetCinematic(const std::string& id);

    // Dialogs
    const std::vector<DialogScene>& GetDialogs() const { return m_dialogs; }
    void AddDialog(const DialogScene& dialog);
    void RemoveDialog(const std::string& id);
    DialogScene* GetDialog(const std::string& id);

    // Global variables (persist across missions)
    void SetGlobalVariable(const std::string& name, const std::string& value);
    std::string GetGlobalVariable(const std::string& name) const;
    const std::unordered_map<std::string, std::string>& GetAllVariables() const { return m_globalVariables; }

    // Validation
    bool Validate(std::vector<std::string>& errors) const;
    bool ValidateChapter(const ChapterData& chapter, std::vector<std::string>& errors) const;
    bool ValidateMission(const MissionData& mission, std::vector<std::string>& errors) const;

    // Utilities
    int GetTotalMissionCount() const;
    float GetEstimatedPlayTime() const;
    std::vector<std::string> GetRequiredMapFiles() const;
    std::vector<std::string> GetRequiredAssets() const;

    // Export
    bool ExportToDirectory(const std::string& directory);
    bool PackageForDistribution(const std::string& outputPath);

private:
    bool SaveJson(const std::string& filepath);
    bool LoadJson(const std::string& filepath);

    CampaignMetadata m_metadata;
    std::vector<ChapterData> m_chapters;
    std::vector<CinematicData> m_cinematics;
    std::vector<DialogScene> m_dialogs;
    std::unordered_map<std::string, std::string> m_globalVariables;
};

} // namespace Vehement
