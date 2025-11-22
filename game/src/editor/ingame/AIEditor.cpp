#include "AIEditor.hpp"
#include "InGameEditor.hpp"

#include <imgui.h>
#include <algorithm>

namespace Vehement {

AIEditor::AIEditor() = default;
AIEditor::~AIEditor() = default;

bool AIEditor::Initialize(InGameEditor& parent) {
    if (m_initialized) return true;
    m_parent = &parent;
    InitializeDefaults();
    m_initialized = true;
    return true;
}

void AIEditor::Shutdown() {
    m_initialized = false;
    m_parent = nullptr;
    m_difficulties.clear();
    m_strategies.clear();
    m_playerConfigs.clear();
}

void AIEditor::InitializeDefaults() {
    // Default difficulties
    {
        AIDifficulty easy;
        easy.id = "easy";
        easy.name = "Easy";
        easy.gatherRateBonus = 0.7f;
        easy.buildSpeedBonus = 0.8f;
        easy.damageBonus = 0.8f;
        easy.healthBonus = 0.9f;
        easy.reactionTime = 2.0f;
        easy.decisionAccuracy = 0.5f;
        easy.microLevel = 0.2f;
        easy.maxWorkers = 15;
        m_difficulties.push_back(easy);
    }
    {
        AIDifficulty normal;
        normal.id = "normal";
        normal.name = "Normal";
        normal.gatherRateBonus = 1.0f;
        normal.buildSpeedBonus = 1.0f;
        normal.damageBonus = 1.0f;
        normal.healthBonus = 1.0f;
        normal.reactionTime = 1.0f;
        normal.decisionAccuracy = 0.7f;
        normal.microLevel = 0.5f;
        normal.maxWorkers = 25;
        m_difficulties.push_back(normal);
    }
    {
        AIDifficulty hard;
        hard.id = "hard";
        hard.name = "Hard";
        hard.gatherRateBonus = 1.2f;
        hard.buildSpeedBonus = 1.2f;
        hard.damageBonus = 1.1f;
        hard.healthBonus = 1.1f;
        hard.reactionTime = 0.5f;
        hard.decisionAccuracy = 0.9f;
        hard.microLevel = 0.8f;
        hard.maxWorkers = 35;
        m_difficulties.push_back(hard);
    }
    {
        AIDifficulty insane;
        insane.id = "insane";
        insane.name = "Insane";
        insane.gatherRateBonus = 1.5f;
        insane.buildSpeedBonus = 1.5f;
        insane.damageBonus = 1.2f;
        insane.healthBonus = 1.2f;
        insane.reactionTime = 0.2f;
        insane.decisionAccuracy = 1.0f;
        insane.microLevel = 1.0f;
        insane.maxWorkers = 50;
        insane.cheats = true;
        m_difficulties.push_back(insane);
    }

    // Default strategies
    {
        AIStrategy balanced;
        balanced.id = "balanced";
        balanced.name = "Balanced";
        balanced.description = "A well-rounded strategy with mixed units";
        balanced.targetWorkers = 20;
        balanced.workerRatio = 0.3f;
        balanced.aggressiveness = 0.5f;
        balanced.armyRatio = 0.5f;

        BuildOrderEntry bo1{"building_barracks", 1, 1, ""};
        BuildOrderEntry bo2{"building_farm", 2, 3, ""};
        BuildOrderEntry bo3{"building_tower", 3, 2, "has_barracks"};
        balanced.buildOrder = {bo1, bo2, bo3};

        TrainOrderEntry to1{"unit_soldier", 1, 0, 0.5f, ""};
        TrainOrderEntry to2{"unit_archer", 2, 0, 0.3f, ""};
        TrainOrderEntry to3{"unit_worker", 3, 20, 0.2f, ""};
        balanced.trainOrder = {to1, to2, to3};

        AttackWave wave1;
        wave1.id = "first_attack";
        wave1.name = "First Attack";
        wave1.minArmySize = 10;
        wave1.minTimer = 300.0f;
        wave1.targetPriority = "base";
        balanced.attackWaves.push_back(wave1);

        m_strategies.push_back(balanced);
    }
    {
        AIStrategy rush;
        rush.id = "rush";
        rush.name = "Rush";
        rush.description = "Early aggression with fast units";
        rush.targetWorkers = 12;
        rush.workerRatio = 0.2f;
        rush.aggressiveness = 0.9f;
        rush.armyRatio = 0.7f;
        rush.expandAggressively = false;

        BuildOrderEntry bo1{"building_barracks", 1, 2, ""};
        rush.buildOrder = {bo1};

        TrainOrderEntry to1{"unit_soldier", 1, 0, 0.8f, ""};
        TrainOrderEntry to2{"unit_worker", 2, 12, 0.2f, ""};
        rush.trainOrder = {to1, to2};

        AttackWave wave1;
        wave1.id = "early_rush";
        wave1.name = "Early Rush";
        wave1.minArmySize = 5;
        wave1.minTimer = 120.0f;
        wave1.repeatInterval = 90.0f;
        wave1.targetPriority = "workers";
        wave1.waitForAll = false;
        rush.attackWaves.push_back(wave1);

        m_strategies.push_back(rush);
    }
    {
        AIStrategy turtle;
        turtle.id = "turtle";
        turtle.name = "Turtle";
        turtle.description = "Defensive play with heavy fortifications";
        turtle.targetWorkers = 30;
        turtle.workerRatio = 0.4f;
        turtle.aggressiveness = 0.2f;
        turtle.armyRatio = 0.3f;
        turtle.expandAggressively = true;

        BuildOrderEntry bo1{"building_farm", 1, 4, ""};
        BuildOrderEntry bo2{"building_tower", 2, 6, ""};
        BuildOrderEntry bo3{"building_wall", 3, 20, ""};
        BuildOrderEntry bo4{"building_barracks", 4, 1, "army_size > 10"};
        turtle.buildOrder = {bo1, bo2, bo3, bo4};

        TrainOrderEntry to1{"unit_worker", 1, 30, 0.5f, ""};
        TrainOrderEntry to2{"unit_archer", 2, 0, 0.4f, ""};
        TrainOrderEntry to3{"unit_soldier", 3, 0, 0.1f, ""};
        turtle.trainOrder = {to1, to2, to3};

        AttackWave wave1;
        wave1.id = "counter_attack";
        wave1.name = "Counter Attack";
        wave1.minArmySize = 30;
        wave1.minTimer = 600.0f;
        wave1.targetPriority = "army";
        turtle.attackWaves.push_back(wave1);

        m_strategies.push_back(turtle);
    }

    m_selectedDifficultyId = "normal";
    m_selectedStrategyId = "balanced";
}

void AIEditor::Update(float deltaTime) {
    if (!m_initialized) return;
}

void AIEditor::Render() {
    if (!m_initialized) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("AI Editor")) {
        if (ImGui::BeginTabBar("AIEditorTabs")) {
            if (ImGui::BeginTabItem("Difficulty")) {
                RenderDifficultyEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Strategies")) {
                RenderStrategyEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Build Orders")) {
                RenderBuildOrderEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Train Orders")) {
                RenderTrainOrderEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Attack Waves")) {
                RenderAttackWaveEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Assignment")) {
                RenderAIAssignment();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Test")) {
                RenderTestPanel();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void AIEditor::ProcessInput() {
    if (!m_initialized) return;
}

AIDifficulty* AIEditor::GetDifficulty(const std::string& id) {
    auto it = std::find_if(m_difficulties.begin(), m_difficulties.end(),
                          [&id](const AIDifficulty& d) { return d.id == id; });
    return it != m_difficulties.end() ? &(*it) : nullptr;
}

void AIEditor::CreateDifficulty(const AIDifficulty& difficulty) {
    m_difficulties.push_back(difficulty);
    if (OnAIConfigChanged) OnAIConfigChanged();
}

void AIEditor::UpdateDifficulty(const std::string& id, const AIDifficulty& difficulty) {
    auto it = std::find_if(m_difficulties.begin(), m_difficulties.end(),
                          [&id](const AIDifficulty& d) { return d.id == id; });
    if (it != m_difficulties.end()) {
        *it = difficulty;
        if (OnAIConfigChanged) OnAIConfigChanged();
    }
}

void AIEditor::DeleteDifficulty(const std::string& id) {
    m_difficulties.erase(
        std::remove_if(m_difficulties.begin(), m_difficulties.end(),
                      [&id](const AIDifficulty& d) { return d.id == id; }),
        m_difficulties.end());
    if (OnAIConfigChanged) OnAIConfigChanged();
}

AIStrategy* AIEditor::GetStrategy(const std::string& id) {
    auto it = std::find_if(m_strategies.begin(), m_strategies.end(),
                          [&id](const AIStrategy& s) { return s.id == id; });
    return it != m_strategies.end() ? &(*it) : nullptr;
}

void AIEditor::CreateStrategy(const AIStrategy& strategy) {
    m_strategies.push_back(strategy);
    if (OnAIConfigChanged) OnAIConfigChanged();
}

void AIEditor::UpdateStrategy(const std::string& id, const AIStrategy& strategy) {
    auto it = std::find_if(m_strategies.begin(), m_strategies.end(),
                          [&id](const AIStrategy& s) { return s.id == id; });
    if (it != m_strategies.end()) {
        *it = strategy;
        if (OnAIConfigChanged) OnAIConfigChanged();
    }
}

void AIEditor::DeleteStrategy(const std::string& id) {
    m_strategies.erase(
        std::remove_if(m_strategies.begin(), m_strategies.end(),
                      [&id](const AIStrategy& s) { return s.id == id; }),
        m_strategies.end());
    if (OnAIConfigChanged) OnAIConfigChanged();
}

void AIEditor::SetAIConfig(const std::string& playerId, const AIConfig& config) {
    auto it = std::find_if(m_playerConfigs.begin(), m_playerConfigs.end(),
                          [&playerId](const AIConfig& c) { return c.playerId == playerId; });
    if (it != m_playerConfigs.end()) {
        *it = config;
    } else {
        m_playerConfigs.push_back(config);
    }
    if (OnAIConfigChanged) OnAIConfigChanged();
}

AIConfig* AIEditor::GetAIConfig(const std::string& playerId) {
    auto it = std::find_if(m_playerConfigs.begin(), m_playerConfigs.end(),
                          [&playerId](const AIConfig& c) { return c.playerId == playerId; });
    return it != m_playerConfigs.end() ? &(*it) : nullptr;
}

void AIEditor::SelectDifficulty(const std::string& id) {
    m_selectedDifficultyId = id;
}

void AIEditor::SelectStrategy(const std::string& id) {
    m_selectedStrategyId = id;
}

void AIEditor::TestAI(const std::string& strategyId) {
    m_testLog.clear();
    m_testLog.push_back("Testing AI strategy: " + strategyId);

    AIStrategy* strategy = GetStrategy(strategyId);
    if (!strategy) {
        m_testLog.push_back("ERROR: Strategy not found");
        return;
    }

    m_testLog.push_back("Strategy: " + strategy->name);
    m_testLog.push_back("Target Workers: " + std::to_string(strategy->targetWorkers));
    m_testLog.push_back("Aggressiveness: " + std::to_string(strategy->aggressiveness));
    m_testLog.push_back("Build Order:");
    for (const auto& bo : strategy->buildOrder) {
        m_testLog.push_back("  - " + bo.buildingId + " x" + std::to_string(bo.targetCount));
    }
    m_testLog.push_back("Train Order:");
    for (const auto& to : strategy->trainOrder) {
        m_testLog.push_back("  - " + to.unitId + " (ratio: " + std::to_string(to.ratio) + ")");
    }
    m_testLog.push_back("Attack Waves: " + std::to_string(strategy->attackWaves.size()));

    m_showTestPanel = true;
}

void AIEditor::SimulateDecision(const std::string& scenario) {
    m_testLog.push_back("Simulating decision for: " + scenario);
    // Simulation logic would go here
}

void AIEditor::RenderDifficultyEditor() {
    ImGui::BeginChild("DifficultyList", ImVec2(200, 0), true);
    ImGui::Text("Difficulties");
    ImGui::Separator();

    for (const auto& diff : m_difficulties) {
        bool isSelected = (m_selectedDifficultyId == diff.id);
        if (ImGui::Selectable(diff.name.c_str(), isSelected)) {
            SelectDifficulty(diff.id);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("+ New Difficulty")) {
        AIDifficulty newDiff;
        newDiff.id = "custom_" + std::to_string(m_difficulties.size());
        newDiff.name = "Custom";
        CreateDifficulty(newDiff);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("DifficultyDetails", ImVec2(0, 0), true);
    AIDifficulty* selected = GetDifficulty(m_selectedDifficultyId);
    if (selected) {
        char nameBuffer[128];
        strncpy(nameBuffer, selected->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            selected->name = nameBuffer;
        }

        ImGui::Separator();
        ImGui::Text("Bonuses");
        ImGui::SliderFloat("Gather Rate", &selected->gatherRateBonus, 0.5f, 2.0f);
        ImGui::SliderFloat("Build Speed", &selected->buildSpeedBonus, 0.5f, 2.0f);
        ImGui::SliderFloat("Damage", &selected->damageBonus, 0.5f, 2.0f);
        ImGui::SliderFloat("Health", &selected->healthBonus, 0.5f, 2.0f);

        ImGui::Separator();
        ImGui::Text("Behavior");
        ImGui::SliderFloat("Reaction Time (s)", &selected->reactionTime, 0.1f, 5.0f);
        ImGui::SliderFloat("Decision Accuracy", &selected->decisionAccuracy, 0.0f, 1.0f);
        ImGui::SliderFloat("Micro Level", &selected->microLevel, 0.0f, 1.0f);
        ImGui::Checkbox("Map Hack (Cheats)", &selected->cheats);

        ImGui::Separator();
        ImGui::Text("Economy");
        ImGui::SliderInt("Max Workers", &selected->maxWorkers, 5, 100);
        ImGui::Checkbox("Expands Naturally", &selected->expandsNaturally);
    } else {
        ImGui::Text("Select a difficulty to edit");
    }
    ImGui::EndChild();
}

void AIEditor::RenderStrategyEditor() {
    ImGui::BeginChild("StrategyList", ImVec2(200, 0), true);
    ImGui::Text("Strategies");
    ImGui::Separator();

    for (const auto& strat : m_strategies) {
        bool isSelected = (m_selectedStrategyId == strat.id);
        if (ImGui::Selectable(strat.name.c_str(), isSelected)) {
            SelectStrategy(strat.id);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("+ New Strategy")) {
        AIStrategy newStrat;
        newStrat.id = GenerateStrategyId();
        newStrat.name = "New Strategy";
        CreateStrategy(newStrat);
        SelectStrategy(newStrat.id);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("StrategyDetails", ImVec2(0, 0), true);
    AIStrategy* selected = GetStrategy(m_selectedStrategyId);
    if (selected) {
        char nameBuffer[128];
        strncpy(nameBuffer, selected->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            selected->name = nameBuffer;
        }

        char descBuffer[256];
        strncpy(descBuffer, selected->description.c_str(), sizeof(descBuffer) - 1);
        if (ImGui::InputText("Description", descBuffer, sizeof(descBuffer))) {
            selected->description = descBuffer;
        }

        ImGui::Separator();
        ImGui::Text("Economy");
        ImGui::SliderInt("Target Workers", &selected->targetWorkers, 5, 50);
        ImGui::SliderFloat("Worker Ratio", &selected->workerRatio, 0.0f, 1.0f);
        ImGui::Checkbox("Expand Aggressively", &selected->expandAggressively);

        ImGui::Separator();
        ImGui::Text("Military");
        ImGui::SliderFloat("Aggressiveness", &selected->aggressiveness, 0.0f, 1.0f);
        ImGui::SliderFloat("Army Ratio", &selected->armyRatio, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Summary");
        ImGui::BulletText("Build Order: %zu entries", selected->buildOrder.size());
        ImGui::BulletText("Train Order: %zu entries", selected->trainOrder.size());
        ImGui::BulletText("Attack Waves: %zu", selected->attackWaves.size());

        if (ImGui::Button("Test This Strategy")) {
            TestAI(selected->id);
        }
    } else {
        ImGui::Text("Select a strategy to edit");
    }
    ImGui::EndChild();
}

void AIEditor::RenderBuildOrderEditor() {
    AIStrategy* strategy = GetStrategy(m_selectedStrategyId);
    if (!strategy) {
        ImGui::Text("Select a strategy first");
        return;
    }

    ImGui::Text("Build Order for: %s", strategy->name.c_str());
    ImGui::Separator();

    if (ImGui::Button("+ Add Building")) {
        BuildOrderEntry entry;
        entry.buildingId = "building_barracks";
        entry.priority = static_cast<int>(strategy->buildOrder.size()) + 1;
        entry.targetCount = 1;
        strategy->buildOrder.push_back(entry);
    }

    ImGui::Separator();

    for (size_t i = 0; i < strategy->buildOrder.size(); ++i) {
        BuildOrderEntry& entry = strategy->buildOrder[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::Text("%zu.", i + 1);
        ImGui::SameLine();

        char buildingBuffer[128];
        strncpy(buildingBuffer, entry.buildingId.c_str(), sizeof(buildingBuffer) - 1);
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##building", buildingBuffer, sizeof(buildingBuffer))) {
            entry.buildingId = buildingBuffer;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("Count##", &entry.targetCount);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("Priority##", &entry.priority);

        ImGui::SameLine();
        if (ImGui::Button("X")) {
            strategy->buildOrder.erase(strategy->buildOrder.begin() + i);
            ImGui::PopID();
            break;
        }

        char condBuffer[128];
        strncpy(condBuffer, entry.condition.c_str(), sizeof(condBuffer) - 1);
        if (ImGui::InputText("Condition", condBuffer, sizeof(condBuffer))) {
            entry.condition = condBuffer;
        }

        ImGui::Separator();
        ImGui::PopID();
    }
}

void AIEditor::RenderTrainOrderEditor() {
    AIStrategy* strategy = GetStrategy(m_selectedStrategyId);
    if (!strategy) {
        ImGui::Text("Select a strategy first");
        return;
    }

    ImGui::Text("Train Order for: %s", strategy->name.c_str());
    ImGui::Separator();

    if (ImGui::Button("+ Add Unit Type")) {
        TrainOrderEntry entry;
        entry.unitId = "unit_soldier";
        entry.priority = static_cast<int>(strategy->trainOrder.size()) + 1;
        entry.ratio = 0.25f;
        strategy->trainOrder.push_back(entry);
    }

    ImGui::Separator();

    float totalRatio = 0.0f;
    for (const auto& entry : strategy->trainOrder) {
        totalRatio += entry.ratio;
    }

    if (totalRatio > 0.0f && std::abs(totalRatio - 1.0f) > 0.01f) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                           "Warning: Ratios sum to %.2f (should be 1.0)", totalRatio);
    }

    for (size_t i = 0; i < strategy->trainOrder.size(); ++i) {
        TrainOrderEntry& entry = strategy->trainOrder[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::Text("%zu.", i + 1);
        ImGui::SameLine();

        char unitBuffer[128];
        strncpy(unitBuffer, entry.unitId.c_str(), sizeof(unitBuffer) - 1);
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##unit", unitBuffer, sizeof(unitBuffer))) {
            entry.unitId = unitBuffer;
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("Ratio##", &entry.ratio, 0.0f, 1.0f);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("Max##", &entry.targetCount);

        ImGui::SameLine();
        if (ImGui::Button("X")) {
            strategy->trainOrder.erase(strategy->trainOrder.begin() + i);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }
}

void AIEditor::RenderAttackWaveEditor() {
    AIStrategy* strategy = GetStrategy(m_selectedStrategyId);
    if (!strategy) {
        ImGui::Text("Select a strategy first");
        return;
    }

    ImGui::Text("Attack Waves for: %s", strategy->name.c_str());
    ImGui::Separator();

    if (ImGui::Button("+ Add Attack Wave")) {
        AttackWave wave;
        wave.id = "wave_" + std::to_string(strategy->attackWaves.size() + 1);
        wave.name = "Attack Wave " + std::to_string(strategy->attackWaves.size() + 1);
        wave.minArmySize = 10.0f;
        wave.minTimer = 300.0f;
        strategy->attackWaves.push_back(wave);
    }

    ImGui::Separator();

    for (size_t i = 0; i < strategy->attackWaves.size(); ++i) {
        AttackWave& wave = strategy->attackWaves[i];
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::CollapsingHeader(wave.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            char nameBuffer[128];
            strncpy(nameBuffer, wave.name.c_str(), sizeof(nameBuffer) - 1);
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
                wave.name = nameBuffer;
            }

            ImGui::SliderFloat("Min Army Size", &wave.minArmySize, 1.0f, 100.0f);
            ImGui::SliderFloat("Min Time (seconds)", &wave.minTimer, 0.0f, 1800.0f);
            ImGui::SliderFloat("Repeat Interval", &wave.repeatInterval, 0.0f, 600.0f);

            const char* priorities[] = {"base", "army", "workers", "nearest"};
            int priorityIndex = 0;
            for (int p = 0; p < 4; ++p) {
                if (wave.targetPriority == priorities[p]) {
                    priorityIndex = p;
                    break;
                }
            }
            if (ImGui::Combo("Target Priority", &priorityIndex, priorities, 4)) {
                wave.targetPriority = priorities[priorityIndex];
            }

            ImGui::Checkbox("Wait for All Units", &wave.waitForAll);

            if (ImGui::Button("Remove Wave")) {
                strategy->attackWaves.erase(strategy->attackWaves.begin() + i);
                ImGui::PopID();
                break;
            }
        }

        ImGui::PopID();
    }
}

void AIEditor::RenderAIAssignment() {
    ImGui::Text("AI Assignment per Player");
    ImGui::Separator();

    for (int i = 0; i < 8; ++i) {
        std::string playerId = "player_" + std::to_string(i);
        ImGui::PushID(i);

        ImGui::Text("Player %d:", i + 1);
        ImGui::SameLine();

        AIConfig* config = GetAIConfig(playerId);
        std::string currentDiff = config ? config->difficultyId : "normal";
        std::string currentStrat = config ? config->strategyId : "balanced";

        // Difficulty dropdown
        if (ImGui::BeginCombo("Difficulty##", currentDiff.c_str())) {
            for (const auto& diff : m_difficulties) {
                if (ImGui::Selectable(diff.name.c_str(), currentDiff == diff.id)) {
                    AIConfig newConfig;
                    newConfig.playerId = playerId;
                    newConfig.difficultyId = diff.id;
                    newConfig.strategyId = currentStrat;
                    SetAIConfig(playerId, newConfig);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // Strategy dropdown
        if (ImGui::BeginCombo("Strategy##", currentStrat.c_str())) {
            for (const auto& strat : m_strategies) {
                if (ImGui::Selectable(strat.name.c_str(), currentStrat == strat.id)) {
                    AIConfig newConfig;
                    newConfig.playerId = playerId;
                    newConfig.difficultyId = currentDiff;
                    newConfig.strategyId = strat.id;
                    SetAIConfig(playerId, newConfig);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PopID();
    }
}

void AIEditor::RenderTestPanel() {
    ImGui::Text("AI Testing");
    ImGui::Separator();

    // Strategy selection for test
    if (ImGui::BeginCombo("Strategy to Test", m_selectedStrategyId.c_str())) {
        for (const auto& strat : m_strategies) {
            if (ImGui::Selectable(strat.name.c_str(), m_selectedStrategyId == strat.id)) {
                m_selectedStrategyId = strat.id;
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Run Test")) {
        TestAI(m_selectedStrategyId);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log")) {
        m_testLog.clear();
    }

    ImGui::Separator();
    ImGui::Text("Test Log:");

    ImGui::BeginChild("TestLog", ImVec2(0, 300), true);
    for (const auto& line : m_testLog) {
        ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::EndChild();
}

std::string AIEditor::GenerateStrategyId() {
    return "strategy_" + std::to_string(m_nextStrategyId++);
}

} // namespace Vehement
