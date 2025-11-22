#include "CampaignManager.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace Game {
namespace UI {
namespace Menu {

CampaignManager::CampaignManager() = default;
CampaignManager::~CampaignManager() = default;

bool CampaignManager::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;

    m_uiManager = uiManager;
    SetupEventHandlers();

    return true;
}

void CampaignManager::Shutdown() {
    m_races.clear();
    m_progress.clear();
    m_uiManager = nullptr;
}

void CampaignManager::Update(float deltaTime) {
    // Update animations, etc.
}

void CampaignManager::SetupEventHandlers() {
    if (!m_uiManager) return;

    auto& binding = m_uiManager->GetBinding();

    binding.RegisterHandler("Campaign.onRaceSelect", [this](const nlohmann::json& data) {
        std::string raceId = data.value("raceId", "");
        SelectRace(raceId);
    });

    binding.RegisterHandler("Campaign.onChapterSelect", [this](const nlohmann::json& data) {
        int chapterId = data.value("chapterId", 0);
        SelectChapter(chapterId);
    });

    binding.RegisterHandler("Campaign.startMission", [this](const nlohmann::json& data) {
        std::string raceId = data.value("raceId", m_selectedRaceId);
        int chapterId = data.value("chapterId", m_selectedChapterId);
        std::string diffStr = data.value("difficulty", "normal");

        CampaignDifficulty diff = CampaignDifficulty::Normal;
        if (diffStr == "easy") diff = CampaignDifficulty::Easy;
        else if (diffStr == "hard") diff = CampaignDifficulty::Hard;
        else if (diffStr == "brutal") diff = CampaignDifficulty::Brutal;

        m_selectedDifficulty = diff;

        if (m_onStartMission) {
            m_onStartMission(raceId, chapterId, diff);
        }
    });

    binding.RegisterHandler("Campaign.playCinematic", [this](const nlohmann::json& data) {
        std::string raceId = data.value("raceId", m_selectedRaceId);
        int chapterId = data.value("chapterId", m_selectedChapterId);

        if (m_onPlayCinematic) {
            m_onPlayCinematic(raceId, chapterId);
        }
    });

    binding.RegisterHandler("Campaign.getCampaignProgress", [this](const nlohmann::json&) -> nlohmann::json {
        nlohmann::json result;
        for (const auto& [raceId, progress] : m_progress) {
            result[raceId] = {
                {"currentChapter", progress.currentChapter},
                {"totalPlayTime", progress.totalPlayTime},
                {"progress", GetRace(raceId) ? GetRace(raceId)->progressPercent : 0}
            };
        }
        return result;
    });
}

void CampaignManager::AddRaceCampaign(const RaceCampaign& race) {
    m_races.push_back(race);

    // Initialize progress if not exists
    if (m_progress.find(race.id) == m_progress.end()) {
        CampaignProgress progress;
        progress.raceId = race.id;
        progress.currentChapter = 0;

        // First chapter is always available
        if (!race.chapters.empty()) {
            progress.chapterStatus[race.chapters[0].id] = ChapterStatus::Available;
        }

        m_progress[race.id] = progress;
    }

    UpdateRaceProgress(race.id);
}

RaceCampaign* CampaignManager::GetRace(const std::string& raceId) {
    auto it = std::find_if(m_races.begin(), m_races.end(),
        [&raceId](const RaceCampaign& r) { return r.id == raceId; });

    return it != m_races.end() ? &(*it) : nullptr;
}

void CampaignManager::SelectRace(const std::string& raceId) {
    RaceCampaign* race = GetRace(raceId);
    if (!race || race->locked) return;

    m_selectedRaceId = raceId;
    m_selectedChapterId = -1;

    if (m_onRaceSelect) {
        m_onRaceSelect(raceId);
    }

    RefreshUI();
}

const RaceCampaign* CampaignManager::GetSelectedRace() const {
    auto it = std::find_if(m_races.begin(), m_races.end(),
        [this](const RaceCampaign& r) { return r.id == m_selectedRaceId; });

    return it != m_races.end() ? &(*it) : nullptr;
}

void CampaignManager::UnlockRace(const std::string& raceId) {
    RaceCampaign* race = GetRace(raceId);
    if (race) {
        race->locked = false;
        race->lockReason = "";
        RefreshUI();
    }
}

void CampaignManager::SelectChapter(int chapterId) {
    const RaceCampaign* race = GetSelectedRace();
    if (!race) return;

    auto it = std::find_if(race->chapters.begin(), race->chapters.end(),
        [chapterId](const CampaignChapter& c) { return c.id == chapterId; });

    if (it != race->chapters.end() && it->status != ChapterStatus::Locked) {
        m_selectedChapterId = chapterId;

        if (m_onChapterSelect) {
            m_onChapterSelect(chapterId);
        }
    }
}

const CampaignChapter* CampaignManager::GetSelectedChapter() const {
    const RaceCampaign* race = GetSelectedRace();
    if (!race || m_selectedChapterId < 0) return nullptr;

    auto it = std::find_if(race->chapters.begin(), race->chapters.end(),
        [this](const CampaignChapter& c) { return c.id == m_selectedChapterId; });

    return it != race->chapters.end() ? &(*it) : nullptr;
}

void CampaignManager::UnlockChapter(const std::string& raceId, int chapterId) {
    RaceCampaign* race = GetRace(raceId);
    if (!race) return;

    auto it = std::find_if(race->chapters.begin(), race->chapters.end(),
        [chapterId](CampaignChapter& c) { return c.id == chapterId; });

    if (it != race->chapters.end() && it->status == ChapterStatus::Locked) {
        it->status = ChapterStatus::Available;

        if (m_progress.count(raceId)) {
            m_progress[raceId].chapterStatus[chapterId] = ChapterStatus::Available;
        }

        UpdateRaceProgress(raceId);
        RefreshUI();
    }
}

void CampaignManager::CompleteChapter(const std::string& raceId, int chapterId, int score, int time) {
    RaceCampaign* race = GetRace(raceId);
    if (!race) return;

    auto it = std::find_if(race->chapters.begin(), race->chapters.end(),
        [chapterId](CampaignChapter& c) { return c.id == chapterId; });

    if (it != race->chapters.end()) {
        it->status = ChapterStatus::Completed;

        if (score > it->bestScore) it->bestScore = score;
        if (time < it->bestTime || it->bestTime == 0) it->bestTime = time;

        // Update progress
        if (m_progress.count(raceId)) {
            auto& progress = m_progress[raceId];
            progress.chapterStatus[chapterId] = ChapterStatus::Completed;
            progress.chapterScores[chapterId] = std::max(progress.chapterScores[chapterId], score);
            progress.chapterTimes[chapterId] = time;
        }

        // Unlock next chapter
        auto nextIt = it + 1;
        if (nextIt != race->chapters.end()) {
            nextIt->status = ChapterStatus::Available;
            if (m_progress.count(raceId)) {
                m_progress[raceId].chapterStatus[nextIt->id] = ChapterStatus::Available;
            }
        }

        UpdateRaceProgress(raceId);
        SaveProgress();
        RefreshUI();
    }
}

const CampaignChapter* CampaignManager::GetNextAvailableChapter(const std::string& raceId) const {
    auto it = std::find_if(m_races.begin(), m_races.end(),
        [&raceId](const RaceCampaign& r) { return r.id == raceId; });

    if (it == m_races.end()) return nullptr;

    for (const auto& chapter : it->chapters) {
        if (chapter.status == ChapterStatus::Available) {
            return &chapter;
        }
    }

    return nullptr;
}

void CampaignManager::SetDifficulty(CampaignDifficulty difficulty) {
    m_selectedDifficulty = difficulty;
}

bool CampaignManager::SaveProgress(const std::string& savePath) {
    std::string path = savePath.empty() ? "saves/campaign_progress.json" : savePath;

    nlohmann::json saveData;

    for (const auto& [raceId, progress] : m_progress) {
        nlohmann::json raceProgress;
        raceProgress["currentChapter"] = progress.currentChapter;
        raceProgress["totalPlayTime"] = progress.totalPlayTime;
        raceProgress["lastPlayed"] = progress.lastPlayed;

        nlohmann::json chapters;
        for (const auto& [chapterId, status] : progress.chapterStatus) {
            chapters[std::to_string(chapterId)] = {
                {"status", static_cast<int>(status)},
                {"score", progress.chapterScores.count(chapterId) ? progress.chapterScores.at(chapterId) : 0},
                {"time", progress.chapterTimes.count(chapterId) ? progress.chapterTimes.at(chapterId) : 0}
            };
        }
        raceProgress["chapters"] = chapters;

        saveData[raceId] = raceProgress;
    }

    std::ofstream file(path);
    if (file.is_open()) {
        file << saveData.dump(2);
        file.close();
        return true;
    }

    return false;
}

bool CampaignManager::LoadProgress(const std::string& savePath) {
    std::string path = savePath.empty() ? "saves/campaign_progress.json" : savePath;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        nlohmann::json saveData;
        file >> saveData;
        file.close();

        for (auto& [raceId, raceData] : saveData.items()) {
            CampaignProgress progress;
            progress.raceId = raceId;
            progress.currentChapter = raceData.value("currentChapter", 0);
            progress.totalPlayTime = raceData.value("totalPlayTime", 0);
            progress.lastPlayed = raceData.value("lastPlayed", "");

            if (raceData.contains("chapters")) {
                for (auto& [chapterIdStr, chapterData] : raceData["chapters"].items()) {
                    int chapterId = std::stoi(chapterIdStr);
                    progress.chapterStatus[chapterId] = static_cast<ChapterStatus>(chapterData.value("status", 0));
                    progress.chapterScores[chapterId] = chapterData.value("score", 0);
                    progress.chapterTimes[chapterId] = chapterData.value("time", 0);
                }
            }

            m_progress[raceId] = progress;
            UpdateRaceProgress(raceId);
        }

        return true;
    } catch (...) {
        return false;
    }
}

CampaignProgress* CampaignManager::GetProgress(const std::string& raceId) {
    return m_progress.count(raceId) ? &m_progress[raceId] : nullptr;
}

void CampaignManager::ResetProgress(const std::string& raceId) {
    RaceCampaign* race = GetRace(raceId);
    if (!race) return;

    CampaignProgress progress;
    progress.raceId = raceId;

    // Reset all chapters
    for (size_t i = 0; i < race->chapters.size(); i++) {
        race->chapters[i].status = (i == 0) ? ChapterStatus::Available : ChapterStatus::Locked;
        race->chapters[i].bestScore = 0;
        race->chapters[i].bestTime = 0;
        progress.chapterStatus[race->chapters[i].id] = race->chapters[i].status;
    }

    m_progress[raceId] = progress;
    UpdateRaceProgress(raceId);
    SaveProgress();
    RefreshUI();
}

void CampaignManager::ResetAllProgress() {
    for (auto& race : m_races) {
        ResetProgress(race.id);
    }
}

void CampaignManager::StartChapter() {
    if (m_selectedRaceId.empty() || m_selectedChapterId < 0) return;

    if (m_onStartMission) {
        m_onStartMission(m_selectedRaceId, m_selectedChapterId, m_selectedDifficulty);
    }
}

void CampaignManager::ContinueCampaign() {
    if (m_selectedRaceId.empty()) return;

    const CampaignChapter* next = GetNextAvailableChapter(m_selectedRaceId);
    if (next) {
        m_selectedChapterId = next->id;
        StartChapter();
    }
}

void CampaignManager::PlayCinematic(int chapterId) {
    if (m_selectedRaceId.empty()) return;

    if (m_onPlayCinematic) {
        m_onPlayCinematic(m_selectedRaceId, chapterId);
    }
}

void CampaignManager::UpdateRaceProgress(const std::string& raceId) {
    RaceCampaign* race = GetRace(raceId);
    if (!race) return;

    int completed = 0;
    for (const auto& chapter : race->chapters) {
        if (chapter.status == ChapterStatus::Completed) {
            completed++;
        }
    }

    race->completedChapters = completed;
    race->progressPercent = race->chapters.empty() ? 0 :
        (completed * 100) / race->chapters.size();
}

void CampaignManager::RefreshUI() {
    // Send updated data to UI
    if (!m_uiManager) return;

    nlohmann::json racesData = nlohmann::json::array();
    for (const auto& race : m_races) {
        racesData.push_back({
            {"id", race.id},
            {"name", race.name},
            {"progress", race.progressPercent},
            {"locked", race.locked}
        });
    }

    m_uiManager->ExecuteScript("campaign_menu",
        "if(CampaignMenu) CampaignMenu.updateRaces(" + racesData.dump() + ")");
}

std::string CampaignManager::DifficultyToString(CampaignDifficulty diff) const {
    switch (diff) {
        case CampaignDifficulty::Easy: return "easy";
        case CampaignDifficulty::Normal: return "normal";
        case CampaignDifficulty::Hard: return "hard";
        case CampaignDifficulty::Brutal: return "brutal";
        default: return "normal";
    }
}

void CampaignManager::SetOnRaceSelect(std::function<void(const std::string&)> callback) {
    m_onRaceSelect = std::move(callback);
}

void CampaignManager::SetOnChapterSelect(std::function<void(int)> callback) {
    m_onChapterSelect = std::move(callback);
}

void CampaignManager::SetOnStartMission(std::function<void(const std::string&, int, CampaignDifficulty)> callback) {
    m_onStartMission = std::move(callback);
}

void CampaignManager::SetOnPlayCinematic(std::function<void(const std::string&, int)> callback) {
    m_onPlayCinematic = std::move(callback);
}

} // namespace Menu
} // namespace UI
} // namespace Game
