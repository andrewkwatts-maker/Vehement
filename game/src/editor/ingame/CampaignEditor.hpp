#pragma once

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>

namespace Vehement {

class InGameEditor;
class CampaignFile;

/**
 * @brief Mission briefing entry
 */
struct BriefingEntry {
    float timestamp = 0.0f;
    std::string speakerName;
    std::string speakerPortrait;
    std::string text;
    std::string voiceoverPath;
};

/**
 * @brief Dialog line for story sequences
 */
struct DialogLine {
    std::string speaker;
    std::string portrait;
    std::string text;
    std::string emotion;
    float duration = 3.0f;
    std::string voiceoverPath;
};

/**
 * @brief Cinematic keyframe
 */
struct CinematicKeyframe {
    float time = 0.0f;
    std::string cameraTarget;
    float cameraZoom = 1.0f;
    float cameraPan = 0.0f;
    float cameraTilt = 0.0f;
    bool letterbox = true;
    std::vector<DialogLine> dialogLines;
    std::string triggerAction;
};

/**
 * @brief Cinematic sequence
 */
struct Cinematic {
    std::string id;
    std::string name;
    float duration = 0.0f;
    bool skippable = true;
    std::vector<CinematicKeyframe> keyframes;
    std::string musicTrack;
};

/**
 * @brief Victory/defeat condition
 */
struct GameCondition {
    enum class Type : uint8_t {
        DestroyAllEnemies,
        DestroyBuilding,
        ProtectUnit,
        ProtectBuilding,
        ReachLocation,
        SurviveTime,
        CollectResources,
        ResearchTech,
        Custom
    };

    Type type = Type::DestroyAllEnemies;
    std::string targetId;
    int targetAmount = 0;
    float timeLimit = 0.0f;
    std::string description;
    bool showInUI = true;
    bool required = true;
    std::string customScript;
};

/**
 * @brief Mission objective
 */
struct Objective {
    std::string id;
    std::string title;
    std::string description;
    bool required = true;
    bool hidden = false;
    bool completed = false;
    std::vector<GameCondition> conditions;
};

/**
 * @brief Campaign mission
 */
struct Mission {
    std::string id;
    std::string name;
    std::string description;
    std::string mapPath;
    int difficulty = 1;

    // Briefing
    std::vector<BriefingEntry> briefing;
    std::string briefingMusic;
    std::string briefingBackground;

    // Objectives
    std::vector<Objective> objectives;
    std::vector<GameCondition> victoryConditions;
    std::vector<GameCondition> defeatConditions;

    // Cinematics
    std::string introCinematicId;
    std::string outroCinematicId;

    // Rewards
    std::vector<std::string> unlockedUnits;
    std::vector<std::string> unlockedTechs;
    int experienceReward = 0;

    // Branching
    std::vector<std::string> nextMissionIds;
    std::string branchCondition;

    // Position on campaign map
    float mapX = 0.0f;
    float mapY = 0.0f;
};

/**
 * @brief Campaign chapter
 */
struct Chapter {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> missionIds;
    std::string chapterImage;
};

/**
 * @brief Complete campaign definition
 */
struct Campaign {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::string backgroundImage;
    std::string mainMenuMusic;

    std::vector<Chapter> chapters;
    std::vector<Mission> missions;
    std::vector<Cinematic> cinematics;

    // Global campaign data
    std::unordered_map<std::string, std::string> persistentVariables;
};

/**
 * @brief Campaign editor command for undo/redo
 */
class CampaignEditorCommand {
public:
    virtual ~CampaignEditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
};

/**
 * @brief Campaign Editor - Create campaign missions
 *
 * Features:
 * - Mission sequence with branching paths
 * - Story/dialog editor with portraits
 * - Cinematic editor with timeline
 * - Victory/defeat conditions
 * - Mission briefings with voiceover
 */
class CampaignEditor {
public:
    CampaignEditor();
    ~CampaignEditor();

    CampaignEditor(const CampaignEditor&) = delete;
    CampaignEditor& operator=(const CampaignEditor&) = delete;

    bool Initialize(InGameEditor& parent);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // Campaign management
    bool CreateNew(const std::string& name);
    bool LoadFromFile(const CampaignFile& file);
    void SaveToFile(CampaignFile& file) const;

    // Update/Render
    void Update(float deltaTime);
    void Render();
    void ProcessInput();

    // Campaign properties
    void SetCampaignName(const std::string& name);
    void SetCampaignDescription(const std::string& desc);
    [[nodiscard]] Campaign& GetCampaign() { return m_campaign; }
    [[nodiscard]] const Campaign& GetCampaign() const { return m_campaign; }

    // Chapter management
    std::string CreateChapter(const std::string& name);
    void DeleteChapter(const std::string& id);
    void RenameChapter(const std::string& id, const std::string& name);
    void ReorderChapter(const std::string& id, int newIndex);
    [[nodiscard]] Chapter* GetChapter(const std::string& id);

    // Mission management
    std::string CreateMission(const std::string& chapterId, const std::string& name);
    void DeleteMission(const std::string& id);
    void UpdateMission(const std::string& id, const Mission& mission);
    void MoveMissionToChapter(const std::string& missionId, const std::string& chapterId);
    [[nodiscard]] Mission* GetMission(const std::string& id);

    void SelectMission(const std::string& id);
    [[nodiscard]] const std::string& GetSelectedMissionId() const { return m_selectedMissionId; }
    [[nodiscard]] Mission* GetSelectedMission();

    // Briefing editing
    void AddBriefingEntry(const std::string& missionId, const BriefingEntry& entry);
    void UpdateBriefingEntry(const std::string& missionId, int index, const BriefingEntry& entry);
    void RemoveBriefingEntry(const std::string& missionId, int index);

    // Objective editing
    void AddObjective(const std::string& missionId, const Objective& objective);
    void UpdateObjective(const std::string& missionId, const std::string& objectiveId, const Objective& objective);
    void RemoveObjective(const std::string& missionId, const std::string& objectiveId);

    // Cinematic management
    std::string CreateCinematic(const std::string& name);
    void DeleteCinematic(const std::string& id);
    void UpdateCinematic(const std::string& id, const Cinematic& cinematic);
    [[nodiscard]] Cinematic* GetCinematic(const std::string& id);

    void SelectCinematic(const std::string& id);
    [[nodiscard]] const std::string& GetSelectedCinematicId() const { return m_selectedCinematicId; }

    // Keyframe editing
    void AddKeyframe(const std::string& cinematicId, const CinematicKeyframe& keyframe);
    void UpdateKeyframe(const std::string& cinematicId, int index, const CinematicKeyframe& keyframe);
    void RemoveKeyframe(const std::string& cinematicId, int index);

    // Mission flow
    void SetNextMission(const std::string& missionId, const std::string& nextMissionId);
    void AddBranch(const std::string& missionId, const std::string& branchMissionId, const std::string& condition);
    void RemoveBranch(const std::string& missionId, const std::string& branchMissionId);

    // Preview
    void PreviewBriefing(const std::string& missionId);
    void PreviewCinematic(const std::string& cinematicId);

    // Undo/Redo
    void ExecuteCommand(std::unique_ptr<CampaignEditorCommand> command);
    void Undo();
    void Redo();
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    void ClearHistory();

    // Callbacks
    std::function<void(const std::string&)> OnMissionSelected;
    std::function<void(const std::string&)> OnCinematicSelected;
    std::function<void()> OnCampaignModified;

private:
    void RenderCampaignOverview();
    void RenderMissionList();
    void RenderMissionEditor();
    void RenderBriefingEditor();
    void RenderObjectiveEditor();
    void RenderCinematicEditor();
    void RenderCinematicTimeline();
    void RenderMissionFlowDiagram();
    void RenderDialogEditor();

    std::string GenerateMissionId();
    std::string GenerateChapterId();
    std::string GenerateCinematicId();
    std::string GenerateObjectiveId();

    bool m_initialized = false;
    InGameEditor* m_parent = nullptr;

    Campaign m_campaign;
    std::string m_selectedMissionId;
    std::string m_selectedChapterId;
    std::string m_selectedCinematicId;
    int m_selectedKeyframeIndex = -1;

    // Editor mode
    enum class EditorMode {
        Overview,
        Mission,
        Briefing,
        Objectives,
        Cinematic,
        Flow
    };
    EditorMode m_mode = EditorMode::Overview;

    // Timeline state
    float m_timelineZoom = 1.0f;
    float m_timelineScroll = 0.0f;
    float m_previewTime = 0.0f;
    bool m_isPlaying = false;

    // Undo/Redo
    std::deque<std::unique_ptr<CampaignEditorCommand>> m_undoStack;
    std::deque<std::unique_ptr<CampaignEditorCommand>> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    int m_nextMissionId = 1;
    int m_nextChapterId = 1;
    int m_nextCinematicId = 1;
    int m_nextObjectiveId = 1;
};

} // namespace Vehement
