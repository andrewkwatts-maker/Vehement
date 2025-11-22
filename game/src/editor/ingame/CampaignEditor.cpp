#include "CampaignEditor.hpp"
#include "InGameEditor.hpp"
#include "CampaignFile.hpp"

#include <imgui.h>
#include <algorithm>

namespace Vehement {

CampaignEditor::CampaignEditor() = default;
CampaignEditor::~CampaignEditor() = default;

bool CampaignEditor::Initialize(InGameEditor& parent) {
    if (m_initialized) return true;
    m_parent = &parent;
    m_initialized = true;
    return true;
}

void CampaignEditor::Shutdown() {
    m_initialized = false;
    m_parent = nullptr;
    m_campaign = Campaign{};
    ClearHistory();
}

bool CampaignEditor::CreateNew(const std::string& name) {
    m_campaign = Campaign{};
    m_campaign.id = "campaign_" + std::to_string(m_nextChapterId++);
    m_campaign.name = name;
    m_campaign.version = "1.0.0";

    // Create default chapter
    Chapter defaultChapter;
    defaultChapter.id = GenerateChapterId();
    defaultChapter.name = "Chapter 1";
    m_campaign.chapters.push_back(defaultChapter);

    m_selectedChapterId = defaultChapter.id;
    m_selectedMissionId.clear();

    ClearHistory();
    return true;
}

bool CampaignEditor::LoadFromFile(const CampaignFile& file) {
    m_campaign = file.GetCampaign();

    m_nextMissionId = 1;
    for (const auto& mission : m_campaign.missions) {
        // Extract number from ID
    }
    m_nextChapterId = static_cast<int>(m_campaign.chapters.size()) + 1;
    m_nextCinematicId = static_cast<int>(m_campaign.cinematics.size()) + 1;

    ClearHistory();
    return true;
}

void CampaignEditor::SaveToFile(CampaignFile& file) const {
    file.SetCampaign(m_campaign);
}

void CampaignEditor::Update(float deltaTime) {
    if (!m_initialized) return;

    if (m_isPlaying) {
        m_previewTime += deltaTime;

        // Check if preview should stop
        if (Cinematic* cinematic = GetCinematic(m_selectedCinematicId)) {
            if (m_previewTime >= cinematic->duration) {
                m_isPlaying = false;
                m_previewTime = 0.0f;
            }
        }
    }
}

void CampaignEditor::Render() {
    if (!m_initialized) return;

    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Campaign Editor")) {
        // Toolbar
        if (ImGui::Button("Overview")) m_mode = EditorMode::Overview;
        ImGui::SameLine();
        if (ImGui::Button("Missions")) m_mode = EditorMode::Mission;
        ImGui::SameLine();
        if (ImGui::Button("Briefings")) m_mode = EditorMode::Briefing;
        ImGui::SameLine();
        if (ImGui::Button("Objectives")) m_mode = EditorMode::Objectives;
        ImGui::SameLine();
        if (ImGui::Button("Cinematics")) m_mode = EditorMode::Cinematic;
        ImGui::SameLine();
        if (ImGui::Button("Flow")) m_mode = EditorMode::Flow;

        ImGui::Separator();

        switch (m_mode) {
            case EditorMode::Overview:
                RenderCampaignOverview();
                break;
            case EditorMode::Mission:
                ImGui::BeginChild("MissionList", ImVec2(250, 0), true);
                RenderMissionList();
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("MissionDetails", ImVec2(0, 0), true);
                RenderMissionEditor();
                ImGui::EndChild();
                break;
            case EditorMode::Briefing:
                RenderBriefingEditor();
                break;
            case EditorMode::Objectives:
                RenderObjectiveEditor();
                break;
            case EditorMode::Cinematic:
                RenderCinematicEditor();
                break;
            case EditorMode::Flow:
                RenderMissionFlowDiagram();
                break;
        }
    }
    ImGui::End();
}

void CampaignEditor::ProcessInput() {
    if (!m_initialized) return;
}

void CampaignEditor::SetCampaignName(const std::string& name) {
    m_campaign.name = name;
    if (OnCampaignModified) OnCampaignModified();
}

void CampaignEditor::SetCampaignDescription(const std::string& desc) {
    m_campaign.description = desc;
    if (OnCampaignModified) OnCampaignModified();
}

std::string CampaignEditor::CreateChapter(const std::string& name) {
    Chapter chapter;
    chapter.id = GenerateChapterId();
    chapter.name = name;
    m_campaign.chapters.push_back(chapter);
    if (OnCampaignModified) OnCampaignModified();
    return chapter.id;
}

void CampaignEditor::DeleteChapter(const std::string& id) {
    auto it = std::find_if(m_campaign.chapters.begin(), m_campaign.chapters.end(),
                          [&id](const Chapter& c) { return c.id == id; });
    if (it != m_campaign.chapters.end()) {
        m_campaign.chapters.erase(it);
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::RenameChapter(const std::string& id, const std::string& name) {
    if (Chapter* chapter = GetChapter(id)) {
        chapter->name = name;
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::ReorderChapter(const std::string& id, int newIndex) {
    auto it = std::find_if(m_campaign.chapters.begin(), m_campaign.chapters.end(),
                          [&id](const Chapter& c) { return c.id == id; });
    if (it != m_campaign.chapters.end()) {
        Chapter chapter = std::move(*it);
        m_campaign.chapters.erase(it);
        newIndex = std::clamp(newIndex, 0, static_cast<int>(m_campaign.chapters.size()));
        m_campaign.chapters.insert(m_campaign.chapters.begin() + newIndex, std::move(chapter));
        if (OnCampaignModified) OnCampaignModified();
    }
}

Chapter* CampaignEditor::GetChapter(const std::string& id) {
    auto it = std::find_if(m_campaign.chapters.begin(), m_campaign.chapters.end(),
                          [&id](const Chapter& c) { return c.id == id; });
    return it != m_campaign.chapters.end() ? &(*it) : nullptr;
}

std::string CampaignEditor::CreateMission(const std::string& chapterId, const std::string& name) {
    Mission mission;
    mission.id = GenerateMissionId();
    mission.name = name;
    mission.difficulty = 1;

    m_campaign.missions.push_back(mission);

    if (Chapter* chapter = GetChapter(chapterId)) {
        chapter->missionIds.push_back(mission.id);
    }

    if (OnCampaignModified) OnCampaignModified();
    return mission.id;
}

void CampaignEditor::DeleteMission(const std::string& id) {
    // Remove from chapter
    for (auto& chapter : m_campaign.chapters) {
        chapter.missionIds.erase(
            std::remove(chapter.missionIds.begin(), chapter.missionIds.end(), id),
            chapter.missionIds.end());
    }

    // Remove mission
    m_campaign.missions.erase(
        std::remove_if(m_campaign.missions.begin(), m_campaign.missions.end(),
                      [&id](const Mission& m) { return m.id == id; }),
        m_campaign.missions.end());

    if (m_selectedMissionId == id) {
        m_selectedMissionId.clear();
    }

    if (OnCampaignModified) OnCampaignModified();
}

void CampaignEditor::UpdateMission(const std::string& id, const Mission& mission) {
    auto it = std::find_if(m_campaign.missions.begin(), m_campaign.missions.end(),
                          [&id](const Mission& m) { return m.id == id; });
    if (it != m_campaign.missions.end()) {
        std::string oldId = it->id;
        *it = mission;
        it->id = oldId;
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::MoveMissionToChapter(const std::string& missionId, const std::string& chapterId) {
    // Remove from all chapters
    for (auto& chapter : m_campaign.chapters) {
        chapter.missionIds.erase(
            std::remove(chapter.missionIds.begin(), chapter.missionIds.end(), missionId),
            chapter.missionIds.end());
    }

    // Add to target chapter
    if (Chapter* chapter = GetChapter(chapterId)) {
        chapter->missionIds.push_back(missionId);
    }

    if (OnCampaignModified) OnCampaignModified();
}

Mission* CampaignEditor::GetMission(const std::string& id) {
    auto it = std::find_if(m_campaign.missions.begin(), m_campaign.missions.end(),
                          [&id](const Mission& m) { return m.id == id; });
    return it != m_campaign.missions.end() ? &(*it) : nullptr;
}

void CampaignEditor::SelectMission(const std::string& id) {
    m_selectedMissionId = id;
    if (OnMissionSelected) OnMissionSelected(id);
}

Mission* CampaignEditor::GetSelectedMission() {
    return GetMission(m_selectedMissionId);
}

void CampaignEditor::AddBriefingEntry(const std::string& missionId, const BriefingEntry& entry) {
    if (Mission* mission = GetMission(missionId)) {
        mission->briefing.push_back(entry);
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::UpdateBriefingEntry(const std::string& missionId, int index, const BriefingEntry& entry) {
    if (Mission* mission = GetMission(missionId)) {
        if (index >= 0 && index < static_cast<int>(mission->briefing.size())) {
            mission->briefing[index] = entry;
            if (OnCampaignModified) OnCampaignModified();
        }
    }
}

void CampaignEditor::RemoveBriefingEntry(const std::string& missionId, int index) {
    if (Mission* mission = GetMission(missionId)) {
        if (index >= 0 && index < static_cast<int>(mission->briefing.size())) {
            mission->briefing.erase(mission->briefing.begin() + index);
            if (OnCampaignModified) OnCampaignModified();
        }
    }
}

void CampaignEditor::AddObjective(const std::string& missionId, const Objective& objective) {
    if (Mission* mission = GetMission(missionId)) {
        Objective obj = objective;
        if (obj.id.empty()) {
            obj.id = GenerateObjectiveId();
        }
        mission->objectives.push_back(obj);
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::UpdateObjective(const std::string& missionId, const std::string& objectiveId, const Objective& objective) {
    if (Mission* mission = GetMission(missionId)) {
        auto it = std::find_if(mission->objectives.begin(), mission->objectives.end(),
                              [&objectiveId](const Objective& o) { return o.id == objectiveId; });
        if (it != mission->objectives.end()) {
            *it = objective;
            if (OnCampaignModified) OnCampaignModified();
        }
    }
}

void CampaignEditor::RemoveObjective(const std::string& missionId, const std::string& objectiveId) {
    if (Mission* mission = GetMission(missionId)) {
        mission->objectives.erase(
            std::remove_if(mission->objectives.begin(), mission->objectives.end(),
                          [&objectiveId](const Objective& o) { return o.id == objectiveId; }),
            mission->objectives.end());
        if (OnCampaignModified) OnCampaignModified();
    }
}

std::string CampaignEditor::CreateCinematic(const std::string& name) {
    Cinematic cinematic;
    cinematic.id = GenerateCinematicId();
    cinematic.name = name;
    cinematic.duration = 10.0f;
    cinematic.skippable = true;

    m_campaign.cinematics.push_back(cinematic);
    if (OnCampaignModified) OnCampaignModified();
    return cinematic.id;
}

void CampaignEditor::DeleteCinematic(const std::string& id) {
    m_campaign.cinematics.erase(
        std::remove_if(m_campaign.cinematics.begin(), m_campaign.cinematics.end(),
                      [&id](const Cinematic& c) { return c.id == id; }),
        m_campaign.cinematics.end());

    if (m_selectedCinematicId == id) {
        m_selectedCinematicId.clear();
    }

    if (OnCampaignModified) OnCampaignModified();
}

void CampaignEditor::UpdateCinematic(const std::string& id, const Cinematic& cinematic) {
    auto it = std::find_if(m_campaign.cinematics.begin(), m_campaign.cinematics.end(),
                          [&id](const Cinematic& c) { return c.id == id; });
    if (it != m_campaign.cinematics.end()) {
        std::string oldId = it->id;
        *it = cinematic;
        it->id = oldId;
        if (OnCampaignModified) OnCampaignModified();
    }
}

Cinematic* CampaignEditor::GetCinematic(const std::string& id) {
    auto it = std::find_if(m_campaign.cinematics.begin(), m_campaign.cinematics.end(),
                          [&id](const Cinematic& c) { return c.id == id; });
    return it != m_campaign.cinematics.end() ? &(*it) : nullptr;
}

void CampaignEditor::SelectCinematic(const std::string& id) {
    m_selectedCinematicId = id;
    if (OnCinematicSelected) OnCinematicSelected(id);
}

void CampaignEditor::AddKeyframe(const std::string& cinematicId, const CinematicKeyframe& keyframe) {
    if (Cinematic* cinematic = GetCinematic(cinematicId)) {
        cinematic->keyframes.push_back(keyframe);
        std::sort(cinematic->keyframes.begin(), cinematic->keyframes.end(),
                  [](const CinematicKeyframe& a, const CinematicKeyframe& b) {
                      return a.time < b.time;
                  });
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::UpdateKeyframe(const std::string& cinematicId, int index, const CinematicKeyframe& keyframe) {
    if (Cinematic* cinematic = GetCinematic(cinematicId)) {
        if (index >= 0 && index < static_cast<int>(cinematic->keyframes.size())) {
            cinematic->keyframes[index] = keyframe;
            if (OnCampaignModified) OnCampaignModified();
        }
    }
}

void CampaignEditor::RemoveKeyframe(const std::string& cinematicId, int index) {
    if (Cinematic* cinematic = GetCinematic(cinematicId)) {
        if (index >= 0 && index < static_cast<int>(cinematic->keyframes.size())) {
            cinematic->keyframes.erase(cinematic->keyframes.begin() + index);
            if (OnCampaignModified) OnCampaignModified();
        }
    }
}

void CampaignEditor::SetNextMission(const std::string& missionId, const std::string& nextMissionId) {
    if (Mission* mission = GetMission(missionId)) {
        mission->nextMissionIds.clear();
        if (!nextMissionId.empty()) {
            mission->nextMissionIds.push_back(nextMissionId);
        }
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::AddBranch(const std::string& missionId, const std::string& branchMissionId, const std::string& condition) {
    if (Mission* mission = GetMission(missionId)) {
        if (std::find(mission->nextMissionIds.begin(), mission->nextMissionIds.end(), branchMissionId) == mission->nextMissionIds.end()) {
            mission->nextMissionIds.push_back(branchMissionId);
            mission->branchCondition = condition;
            if (OnCampaignModified) OnCampaignModified();
        }
    }
}

void CampaignEditor::RemoveBranch(const std::string& missionId, const std::string& branchMissionId) {
    if (Mission* mission = GetMission(missionId)) {
        mission->nextMissionIds.erase(
            std::remove(mission->nextMissionIds.begin(), mission->nextMissionIds.end(), branchMissionId),
            mission->nextMissionIds.end());
        if (OnCampaignModified) OnCampaignModified();
    }
}

void CampaignEditor::PreviewBriefing(const std::string& missionId) {
    // Start briefing preview
}

void CampaignEditor::PreviewCinematic(const std::string& cinematicId) {
    m_selectedCinematicId = cinematicId;
    m_previewTime = 0.0f;
    m_isPlaying = true;
}

void CampaignEditor::ExecuteCommand(std::unique_ptr<CampaignEditorCommand> command) {
    command->Execute();
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();
    if (m_undoStack.size() > MAX_UNDO_HISTORY) {
        m_undoStack.pop_front();
    }
}

void CampaignEditor::Undo() {
    if (m_undoStack.empty()) return;
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->Undo();
    m_redoStack.push_back(std::move(cmd));
}

void CampaignEditor::Redo() {
    if (m_redoStack.empty()) return;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->Execute();
    m_undoStack.push_back(std::move(cmd));
}

void CampaignEditor::ClearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void CampaignEditor::RenderCampaignOverview() {
    ImGui::Text("Campaign Overview");
    ImGui::Separator();

    char nameBuffer[256];
    strncpy(nameBuffer, m_campaign.name.c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        m_campaign.name = nameBuffer;
    }

    char descBuffer[1024];
    strncpy(descBuffer, m_campaign.description.c_str(), sizeof(descBuffer) - 1);
    if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(0, 100))) {
        m_campaign.description = descBuffer;
    }

    char authorBuffer[128];
    strncpy(authorBuffer, m_campaign.author.c_str(), sizeof(authorBuffer) - 1);
    if (ImGui::InputText("Author", authorBuffer, sizeof(authorBuffer))) {
        m_campaign.author = authorBuffer;
    }

    ImGui::Separator();
    ImGui::Text("Statistics:");
    ImGui::BulletText("Chapters: %zu", m_campaign.chapters.size());
    ImGui::BulletText("Missions: %zu", m_campaign.missions.size());
    ImGui::BulletText("Cinematics: %zu", m_campaign.cinematics.size());
}

void CampaignEditor::RenderMissionList() {
    ImGui::Text("Missions");
    ImGui::Separator();

    for (auto& chapter : m_campaign.chapters) {
        bool chapterOpen = ImGui::TreeNodeEx(chapter.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Add Mission")) {
                CreateMission(chapter.id, "New Mission");
            }
            if (ImGui::MenuItem("Rename Chapter")) {
                // Show rename dialog
            }
            if (ImGui::MenuItem("Delete Chapter")) {
                DeleteChapter(chapter.id);
            }
            ImGui::EndPopup();
        }

        if (chapterOpen) {
            for (const auto& missionId : chapter.missionIds) {
                if (Mission* mission = GetMission(missionId)) {
                    bool isSelected = (m_selectedMissionId == missionId);
                    if (ImGui::Selectable(mission->name.c_str(), isSelected)) {
                        SelectMission(missionId);
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Delete Mission")) {
                            DeleteMission(missionId);
                        }
                        ImGui::EndPopup();
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    if (ImGui::Button("+ Add Chapter")) {
        CreateChapter("New Chapter");
    }
}

void CampaignEditor::RenderMissionEditor() {
    Mission* mission = GetSelectedMission();
    if (!mission) {
        ImGui::Text("Select a mission to edit");
        return;
    }

    ImGui::Text("Mission: %s", mission->name.c_str());
    ImGui::Separator();

    char nameBuffer[256];
    strncpy(nameBuffer, mission->name.c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        mission->name = nameBuffer;
    }

    char descBuffer[1024];
    strncpy(descBuffer, mission->description.c_str(), sizeof(descBuffer) - 1);
    if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(0, 60))) {
        mission->description = descBuffer;
    }

    char mapBuffer[256];
    strncpy(mapBuffer, mission->mapPath.c_str(), sizeof(mapBuffer) - 1);
    if (ImGui::InputText("Map Path", mapBuffer, sizeof(mapBuffer))) {
        mission->mapPath = mapBuffer;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        // Open file dialog
    }

    ImGui::SliderInt("Difficulty", &mission->difficulty, 1, 5);

    ImGui::Separator();
    ImGui::Text("Cinematics");

    // Intro cinematic
    if (ImGui::BeginCombo("Intro Cinematic", mission->introCinematicId.empty() ? "(None)" : mission->introCinematicId.c_str())) {
        if (ImGui::Selectable("(None)", mission->introCinematicId.empty())) {
            mission->introCinematicId.clear();
        }
        for (const auto& cinematic : m_campaign.cinematics) {
            if (ImGui::Selectable(cinematic.name.c_str(), mission->introCinematicId == cinematic.id)) {
                mission->introCinematicId = cinematic.id;
            }
        }
        ImGui::EndCombo();
    }

    // Outro cinematic
    if (ImGui::BeginCombo("Outro Cinematic", mission->outroCinematicId.empty() ? "(None)" : mission->outroCinematicId.c_str())) {
        if (ImGui::Selectable("(None)", mission->outroCinematicId.empty())) {
            mission->outroCinematicId.clear();
        }
        for (const auto& cinematic : m_campaign.cinematics) {
            if (ImGui::Selectable(cinematic.name.c_str(), mission->outroCinematicId == cinematic.id)) {
                mission->outroCinematicId = cinematic.id;
            }
        }
        ImGui::EndCombo();
    }
}

void CampaignEditor::RenderBriefingEditor() {
    Mission* mission = GetSelectedMission();
    if (!mission) {
        ImGui::Text("Select a mission first");
        return;
    }

    ImGui::Text("Briefing Editor - %s", mission->name.c_str());
    ImGui::Separator();

    if (ImGui::Button("+ Add Entry")) {
        BriefingEntry entry;
        entry.speakerName = "Commander";
        entry.text = "New briefing text...";
        AddBriefingEntry(mission->id, entry);
    }
    ImGui::SameLine();
    if (ImGui::Button("Preview")) {
        PreviewBriefing(mission->id);
    }

    ImGui::Separator();

    for (size_t i = 0; i < mission->briefing.size(); ++i) {
        BriefingEntry& entry = mission->briefing[i];
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::CollapsingHeader(("Entry " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            char speakerBuffer[128];
            strncpy(speakerBuffer, entry.speakerName.c_str(), sizeof(speakerBuffer) - 1);
            if (ImGui::InputText("Speaker", speakerBuffer, sizeof(speakerBuffer))) {
                entry.speakerName = speakerBuffer;
            }

            char textBuffer[1024];
            strncpy(textBuffer, entry.text.c_str(), sizeof(textBuffer) - 1);
            if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(0, 60))) {
                entry.text = textBuffer;
            }

            char voiceBuffer[256];
            strncpy(voiceBuffer, entry.voiceoverPath.c_str(), sizeof(voiceBuffer) - 1);
            if (ImGui::InputText("Voiceover", voiceBuffer, sizeof(voiceBuffer))) {
                entry.voiceoverPath = voiceBuffer;
            }

            if (ImGui::Button("Remove")) {
                RemoveBriefingEntry(mission->id, static_cast<int>(i));
            }
        }

        ImGui::PopID();
    }
}

void CampaignEditor::RenderObjectiveEditor() {
    Mission* mission = GetSelectedMission();
    if (!mission) {
        ImGui::Text("Select a mission first");
        return;
    }

    ImGui::Text("Objectives - %s", mission->name.c_str());
    ImGui::Separator();

    if (ImGui::Button("+ Add Objective")) {
        Objective obj;
        obj.id = GenerateObjectiveId();
        obj.title = "New Objective";
        obj.description = "Complete this objective";
        AddObjective(mission->id, obj);
    }

    ImGui::Separator();

    for (auto& objective : mission->objectives) {
        ImGui::PushID(objective.id.c_str());

        if (ImGui::CollapsingHeader(objective.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            char titleBuffer[256];
            strncpy(titleBuffer, objective.title.c_str(), sizeof(titleBuffer) - 1);
            if (ImGui::InputText("Title", titleBuffer, sizeof(titleBuffer))) {
                objective.title = titleBuffer;
            }

            char descBuffer[512];
            strncpy(descBuffer, objective.description.c_str(), sizeof(descBuffer) - 1);
            if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(0, 40))) {
                objective.description = descBuffer;
            }

            ImGui::Checkbox("Required", &objective.required);
            ImGui::SameLine();
            ImGui::Checkbox("Hidden", &objective.hidden);

            if (ImGui::Button("Remove")) {
                RemoveObjective(mission->id, objective.id);
            }
        }

        ImGui::PopID();
    }
}

void CampaignEditor::RenderCinematicEditor() {
    // Left panel - cinematic list
    ImGui::BeginChild("CinematicList", ImVec2(200, 0), true);

    ImGui::Text("Cinematics");
    ImGui::Separator();

    if (ImGui::Button("+ New Cinematic")) {
        std::string id = CreateCinematic("New Cinematic");
        SelectCinematic(id);
    }

    for (const auto& cinematic : m_campaign.cinematics) {
        bool isSelected = (m_selectedCinematicId == cinematic.id);
        if (ImGui::Selectable(cinematic.name.c_str(), isSelected)) {
            SelectCinematic(cinematic.id);
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel - timeline and details
    ImGui::BeginChild("CinematicDetails", ImVec2(0, 0), true);

    Cinematic* cinematic = GetCinematic(m_selectedCinematicId);
    if (cinematic) {
        char nameBuffer[256];
        strncpy(nameBuffer, cinematic->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            cinematic->name = nameBuffer;
        }

        ImGui::InputFloat("Duration", &cinematic->duration, 1.0f, 10.0f);
        ImGui::Checkbox("Skippable", &cinematic->skippable);

        ImGui::Separator();

        // Timeline controls
        if (m_isPlaying) {
            if (ImGui::Button("Stop")) {
                m_isPlaying = false;
            }
        } else {
            if (ImGui::Button("Play")) {
                m_isPlaying = true;
            }
        }
        ImGui::SameLine();
        ImGui::SliderFloat("Time", &m_previewTime, 0.0f, cinematic->duration);

        ImGui::Separator();

        // Render timeline
        RenderCinematicTimeline();
    } else {
        ImGui::Text("Select a cinematic to edit");
    }

    ImGui::EndChild();
}

void CampaignEditor::RenderCinematicTimeline() {
    Cinematic* cinematic = GetCinematic(m_selectedCinematicId);
    if (!cinematic) return;

    ImGui::Text("Timeline");

    if (ImGui::Button("+ Add Keyframe")) {
        CinematicKeyframe kf;
        kf.time = m_previewTime;
        AddKeyframe(cinematic->id, kf);
    }

    // Simple timeline visualization
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, 60.0f);

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 40, 40, 255));

    // Timeline markers
    float pixelsPerSecond = size.x / cinematic->duration;
    for (float t = 0.0f; t <= cinematic->duration; t += 1.0f) {
        float x = pos.x + t * pixelsPerSecond;
        drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + size.y), IM_COL32(80, 80, 80, 255));
    }

    // Keyframes
    for (size_t i = 0; i < cinematic->keyframes.size(); ++i) {
        const CinematicKeyframe& kf = cinematic->keyframes[i];
        float x = pos.x + kf.time * pixelsPerSecond;

        ImU32 color = (m_selectedKeyframeIndex == static_cast<int>(i))
            ? IM_COL32(255, 200, 0, 255)
            : IM_COL32(0, 200, 255, 255);

        drawList->AddCircleFilled(ImVec2(x, pos.y + size.y / 2), 6.0f, color);
    }

    // Current time indicator
    float currentX = pos.x + m_previewTime * pixelsPerSecond;
    drawList->AddLine(ImVec2(currentX, pos.y), ImVec2(currentX, pos.y + size.y), IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::Dummy(size);

    // Keyframe list
    ImGui::Separator();
    ImGui::Text("Keyframes");

    for (size_t i = 0; i < cinematic->keyframes.size(); ++i) {
        CinematicKeyframe& kf = cinematic->keyframes[i];
        ImGui::PushID(static_cast<int>(i));

        bool isSelected = (m_selectedKeyframeIndex == static_cast<int>(i));
        if (ImGui::Selectable(("Keyframe at " + std::to_string(kf.time) + "s").c_str(), isSelected)) {
            m_selectedKeyframeIndex = static_cast<int>(i);
            m_previewTime = kf.time;
        }

        if (isSelected) {
            ImGui::Indent();
            ImGui::InputFloat("Time", &kf.time, 0.1f, 1.0f);
            ImGui::InputFloat("Zoom", &kf.cameraZoom, 0.1f, 0.5f);
            ImGui::Checkbox("Letterbox", &kf.letterbox);

            if (ImGui::Button("Remove")) {
                RemoveKeyframe(cinematic->id, static_cast<int>(i));
                m_selectedKeyframeIndex = -1;
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
}

void CampaignEditor::RenderMissionFlowDiagram() {
    ImGui::Text("Mission Flow");
    ImGui::Separator();

    // Simple node-based view of mission connections
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(30, 30, 30, 255));

    // Draw missions as nodes
    std::unordered_map<std::string, ImVec2> nodePositions;
    float nodeWidth = 120.0f;
    float nodeHeight = 40.0f;
    float startX = canvasPos.x + 50.0f;
    float startY = canvasPos.y + 50.0f;

    int missionIndex = 0;
    for (const auto& mission : m_campaign.missions) {
        float x = startX + (missionIndex % 4) * (nodeWidth + 50);
        float y = startY + (missionIndex / 4) * (nodeHeight + 80);
        nodePositions[mission.id] = ImVec2(x, y);

        // Draw node
        ImU32 nodeColor = (m_selectedMissionId == mission.id)
            ? IM_COL32(100, 150, 200, 255)
            : IM_COL32(70, 70, 70, 255);

        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + nodeWidth, y + nodeHeight), nodeColor, 4.0f);
        drawList->AddRect(ImVec2(x, y), ImVec2(x + nodeWidth, y + nodeHeight), IM_COL32(150, 150, 150, 255), 4.0f);

        // Draw name
        ImVec2 textSize = ImGui::CalcTextSize(mission.name.c_str());
        drawList->AddText(ImVec2(x + (nodeWidth - textSize.x) / 2, y + (nodeHeight - textSize.y) / 2),
                          IM_COL32(255, 255, 255, 255), mission.name.c_str());

        missionIndex++;
    }

    // Draw connections
    for (const auto& mission : m_campaign.missions) {
        auto fromIt = nodePositions.find(mission.id);
        if (fromIt == nodePositions.end()) continue;

        ImVec2 fromPos(fromIt->second.x + nodeWidth, fromIt->second.y + nodeHeight / 2);

        for (const auto& nextId : mission.nextMissionIds) {
            auto toIt = nodePositions.find(nextId);
            if (toIt == nodePositions.end()) continue;

            ImVec2 toPos(toIt->second.x, toIt->second.y + nodeHeight / 2);

            // Draw arrow
            drawList->AddLine(fromPos, toPos, IM_COL32(200, 200, 200, 255), 2.0f);

            // Arrowhead
            ImVec2 dir = ImVec2(toPos.x - fromPos.x, toPos.y - fromPos.y);
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            dir.x /= len;
            dir.y /= len;

            ImVec2 arrowP1(toPos.x - dir.x * 10 - dir.y * 5, toPos.y - dir.y * 10 + dir.x * 5);
            ImVec2 arrowP2(toPos.x - dir.x * 10 + dir.y * 5, toPos.y - dir.y * 10 - dir.x * 5);
            drawList->AddTriangleFilled(toPos, arrowP1, arrowP2, IM_COL32(200, 200, 200, 255));
        }
    }

    // Handle clicking on nodes
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        for (const auto& [id, pos] : nodePositions) {
            if (mousePos.x >= pos.x && mousePos.x <= pos.x + nodeWidth &&
                mousePos.y >= pos.y && mousePos.y <= pos.y + nodeHeight) {
                SelectMission(id);
                break;
            }
        }
    }

    ImGui::Dummy(canvasSize);
}

void CampaignEditor::RenderDialogEditor() {
    // Dialog editing for briefings and cinematics
}

std::string CampaignEditor::GenerateMissionId() {
    return "mission_" + std::to_string(m_nextMissionId++);
}

std::string CampaignEditor::GenerateChapterId() {
    return "chapter_" + std::to_string(m_nextChapterId++);
}

std::string CampaignEditor::GenerateCinematicId() {
    return "cinematic_" + std::to_string(m_nextCinematicId++);
}

std::string CampaignEditor::GenerateObjectiveId() {
    return "objective_" + std::to_string(m_nextObjectiveId++);
}

} // namespace Vehement
