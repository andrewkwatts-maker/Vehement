#include "PerformanceMonitorPanel.hpp"
#include "PerformanceGraphs.hpp"
#include "../engine/profiling/DetailedFrameProfiler.hpp"
#include "../engine/profiling/PerformanceDatabase.hpp"
#include "../engine/profiling/PerformanceAnalyzer.hpp"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace Engine {
namespace Profiling {

// Define UI colors
const ImVec4 PerformanceMonitorPanel::COLOR_GOOD = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
const ImVec4 PerformanceMonitorPanel::COLOR_WARNING = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
const ImVec4 PerformanceMonitorPanel::COLOR_CRITICAL = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);

PerformanceMonitorPanel::PerformanceMonitorPanel()
    : m_isOpen(false)
    , m_recording(false)
    , m_initialized(false)
    , m_selectedSessionA(-1)
    , m_selectedSessionB(-1)
    , m_currentSession(-1)
    , m_currentTab(0)
    , m_updateTimer(0.0f)
    , m_updateInterval(0.1f) {

    strcpy(m_sessionPresetBuffer, "High");
    strcpy(m_sessionResolutionBuffer, "1920x1080");
    strcpy(m_exportFilenameBuffer, "performance_report");
}

PerformanceMonitorPanel::~PerformanceMonitorPanel() {
    Shutdown();
}

bool PerformanceMonitorPanel::Initialize(const std::string& databasePath) {
    if (m_initialized) return true;

    // Create profiler
    m_profiler = std::make_unique<DetailedFrameProfiler>();

    // Create and initialize database
    m_database = std::make_unique<PerformanceDatabase>();
    if (!m_database->Initialize(databasePath)) {
        return false;
    }

    // Initialize profiler with database
    if (!m_profiler->Initialize(m_database.get())) {
        return false;
    }

    // Create analyzer
    m_analyzer = std::make_unique<PerformanceAnalyzer>(m_database.get());

    // Create graphs
    m_graphs = std::make_unique<PerformanceGraphs>();
    m_graphs->Initialize(m_profiler.get());
    m_graphs->SetHistorySize(m_settings.historySize);
    m_graphs->SetTargetFPS(m_settings.targetFPS);

    // Load sessions
    RefreshSessionList();

    m_initialized = true;
    return true;
}

void PerformanceMonitorPanel::Shutdown() {
    if (!m_initialized) return;

    if (IsSessionActive()) {
        EndSession();
    }

    if (m_graphs) {
        m_graphs->Shutdown();
        m_graphs.reset();
    }

    if (m_profiler) {
        m_profiler->Shutdown();
        m_profiler.reset();
    }

    if (m_database) {
        m_database->Shutdown();
        m_database.reset();
    }

    m_analyzer.reset();
    m_initialized = false;
}

void PerformanceMonitorPanel::Update() {
    if (!m_initialized || !m_profiler) return;

    m_updateTimer += 0.016f;  // Assuming ~60 FPS

    if (m_updateTimer >= m_updateInterval) {
        UpdateGraphs();
        m_updateTimer = 0.0f;
    }
}

void PerformanceMonitorPanel::Render() {
    if (!m_isOpen || !m_initialized) return;

    RenderUI();
}

void PerformanceMonitorPanel::RenderUI() {
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Performance Monitor", &m_isOpen)) {
        ImGui::End();
        return;
    }

    // Control buttons
    if (ImGui::Button(m_recording ? "Stop Recording" : "Start Recording")) {
        if (m_recording) {
            StopRecording();
        } else {
            StartRecording();
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Clear History")) {
        if (m_profiler) {
            m_profiler->ClearHistory();
        }
        if (m_graphs) {
            m_graphs->ClearData();
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Export Report")) {
        ExportReport();
    }
    ImGui::SameLine();

    // Recording indicator
    if (m_recording) {
        ImGui::TextColored(COLOR_GOOD, "RECORDING");
    } else {
        ImGui::TextColored(COLOR_WARNING, "PAUSED");
    }

    ImGui::Separator();

    // Current frame stats (always visible)
    RenderCurrentFrameStats();

    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("PerfTabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            RenderOverviewTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Frame Breakdown")) {
            RenderBreakdownTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Graphs")) {
            RenderGraphsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Memory")) {
            RenderMemoryTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Database")) {
            RenderDatabaseTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Analysis")) {
            RenderAnalysisTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings")) {
            RenderSettingsTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void PerformanceMonitorPanel::RenderCurrentFrameStats() {
    if (!m_profiler) return;

    ImGui::Columns(5, "CurrentStats", false);

    // FPS
    float fps = m_profiler->GetCurrentFPS();
    ImGui::TextColored(GetPerformanceColor(fps), "FPS: %.1f", fps);
    ImGui::NextColumn();

    // Frame time
    ImGui::Text("Frame Time: %.2f ms", m_profiler->GetCurrentFrameTime());
    ImGui::NextColumn();

    // GPU time
    ImGui::Text("GPU Time: %.2f ms", m_profiler->GetCurrentGPUTime());
    ImGui::NextColumn();

    // CPU time
    ImGui::Text("CPU Time: %.2f ms", m_profiler->GetCurrentCPUTime());
    ImGui::NextColumn();

    // Frame number
    ImGui::Text("Frame: %d", m_profiler->GetCurrentFrameNumber());

    ImGui::Columns(1);
}

void PerformanceMonitorPanel::RenderOverviewTab() {
    RenderAverageStats();
    ImGui::Separator();
    RenderHardwareMetrics();
    ImGui::Separator();

    if (m_graphs) {
        m_graphs->RenderOverviewGraphs();
    }
}

void PerformanceMonitorPanel::RenderAverageStats() {
    if (!m_profiler) return;

    ImGui::Text("Average Statistics (Last 60 frames):");
    ImGui::Columns(4, "AvgStats", true);

    ImGui::Text("Avg FPS");
    ImGui::NextColumn();
    ImGui::Text("Min FPS");
    ImGui::NextColumn();
    ImGui::Text("Max FPS");
    ImGui::NextColumn();
    ImGui::Text("Avg Frame Time");
    ImGui::NextColumn();

    ImGui::Separator();

    ImGui::Text("%.1f", m_profiler->GetAverageFPS(60));
    ImGui::NextColumn();
    ImGui::Text("%.1f", m_profiler->GetMinFPS(60));
    ImGui::NextColumn();
    ImGui::Text("%.1f", m_profiler->GetMaxFPS(60));
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_profiler->GetAverageFrameTime(60));
    ImGui::NextColumn();

    ImGui::Columns(1);
}

void PerformanceMonitorPanel::RenderHardwareMetrics() {
    if (!m_profiler) return;

    const auto& hwMetrics = m_profiler->GetHardwareMetrics();

    ImGui::Text("Hardware Metrics:");
    ImGui::Columns(2, "HWMetrics", true);

    // GPU metrics
    ImGui::Text("GPU Utilization:");
    ImGui::NextColumn();
    ImGui::ProgressBar(hwMetrics.gpuUtilization / 100.0f, ImVec2(-1, 0),
                      (std::to_string(static_cast<int>(hwMetrics.gpuUtilization)) + "%%").c_str());
    ImGui::NextColumn();

    ImGui::Text("GPU Temperature:");
    ImGui::NextColumn();
    ImGui::Text("%.1f C", hwMetrics.gpuTemperature);
    ImGui::NextColumn();

    ImGui::Text("GPU Clock:");
    ImGui::NextColumn();
    ImGui::Text("%d MHz", hwMetrics.gpuClockMHz);
    ImGui::NextColumn();

    ImGui::Separator();

    // CPU metrics
    ImGui::Text("CPU Utilization:");
    ImGui::NextColumn();
    ImGui::ProgressBar(hwMetrics.cpuUtilization / 100.0f, ImVec2(-1, 0),
                      (std::to_string(static_cast<int>(hwMetrics.cpuUtilization)) + "%%").c_str());
    ImGui::NextColumn();

    ImGui::Text("CPU Temperature:");
    ImGui::NextColumn();
    ImGui::Text("%.1f C", hwMetrics.cpuTemperature);
    ImGui::NextColumn();

    ImGui::Text("CPU Clock:");
    ImGui::NextColumn();
    ImGui::Text("%d MHz", hwMetrics.cpuClockMHz);
    ImGui::NextColumn();

    ImGui::Columns(1);
}

void PerformanceMonitorPanel::RenderBreakdownTab() {
    RenderBreakdownTable();
    ImGui::Separator();

    ImGui::Columns(2, "BreakdownViz", false);

    if (m_graphs) {
        m_graphs->RenderPieChart(100.0f);
    }

    ImGui::NextColumn();

    RenderStageComparison();

    ImGui::Columns(1);
}

void PerformanceMonitorPanel::RenderBreakdownTable() {
    if (!m_profiler) return;

    const auto& breakdown = m_profiler->GetCurrentBreakdown();

    ImGui::Text("Frame Breakdown:");

    if (ImGui::BeginTable("Breakdown", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Stage");
        ImGui::TableSetupColumn("Time (ms)");
        ImGui::TableSetupColumn("Percentage");
        ImGui::TableSetupColumn("GPU (ms)");
        ImGui::TableSetupColumn("CPU (ms)");
        ImGui::TableHeadersRow();

        for (const auto& stage : breakdown.stages) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", stage.name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", stage.timeMs);

            ImGui::TableNextColumn();
            ImGui::ProgressBar(stage.percentage / 100.0f, ImVec2(-1, 0),
                              (std::to_string(static_cast<int>(stage.percentage)) + "%%").c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", stage.gpuTimeMs);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", stage.cpuTimeMs);
        }

        ImGui::EndTable();
    }
}

void PerformanceMonitorPanel::RenderStageComparison() {
    ImGui::Text("Current vs. Previous Frame:");

    if (!m_profiler) return;

    const auto& current = m_profiler->GetCurrentBreakdown();
    const auto& previous = m_profiler->GetPreviousBreakdown();

    if (ImGui::BeginTable("StageComparison", 3, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Stage");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Delta");
        ImGui::TableHeadersRow();

        for (const auto& stage : current.stages) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", stage.name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.2f ms", stage.timeMs);

            ImGui::TableNextColumn();

            // Find matching stage in previous frame
            float prevTime = 0.0f;
            for (const auto& prevStage : previous.stages) {
                if (prevStage.name == stage.name) {
                    prevTime = prevStage.timeMs;
                    break;
                }
            }

            float delta = stage.timeMs - prevTime;
            ImVec4 color = (delta > 0) ? COLOR_WARNING : COLOR_GOOD;
            ImGui::TextColored(color, "%+.2f ms", delta);
        }

        ImGui::EndTable();
    }
}

void PerformanceMonitorPanel::RenderGraphsTab() {
    if (!m_graphs) return;

    m_graphs->RenderFPSGraph(-1.0f, 200.0f);
    ImGui::Spacing();

    m_graphs->RenderFrameTimeGraph(-1.0f, 200.0f);
    ImGui::Spacing();

    m_graphs->RenderStackedBreakdown(-1.0f, 300.0f);
}

void PerformanceMonitorPanel::RenderMemoryTab() {
    RenderMemoryUsage();
    ImGui::Separator();

    if (m_graphs) {
        m_graphs->RenderMemoryGraph(-1.0f, 250.0f);
        ImGui::Spacing();
        m_graphs->RenderGPUUtilizationGraph(-1.0f, 150.0f);
        ImGui::Spacing();
        m_graphs->RenderCPUUtilizationGraph(-1.0f, 150.0f);
    }
}

void PerformanceMonitorPanel::RenderMemoryUsage() {
    if (!m_profiler) return;

    const auto& memSnapshot = m_profiler->GetMemorySnapshot();

    ImGui::Text("Memory Usage:");
    ImGui::Columns(2, "MemoryUsage", true);

    // CPU Memory
    ImGui::Text("CPU Memory:");
    ImGui::NextColumn();
    ImGui::Text("%.1f MB / %.1f MB (%.1f%%)",
               memSnapshot.cpuUsedMB,
               memSnapshot.cpuAvailableMB,
               memSnapshot.GetCPUUsagePercent());
    ImGui::NextColumn();

    ImGui::Text("");
    ImGui::NextColumn();
    ImGui::ProgressBar(memSnapshot.GetCPUUsagePercent() / 100.0f, ImVec2(-1, 0));
    ImGui::NextColumn();

    ImGui::Separator();

    // GPU Memory
    ImGui::Text("GPU Memory:");
    ImGui::NextColumn();
    ImGui::Text("%.1f MB / %.1f MB (%.1f%%)",
               memSnapshot.gpuUsedMB,
               memSnapshot.gpuAvailableMB,
               memSnapshot.GetGPUUsagePercent());
    ImGui::NextColumn();

    ImGui::Text("");
    ImGui::NextColumn();
    ImGui::ProgressBar(memSnapshot.GetGPUUsagePercent() / 100.0f, ImVec2(-1, 0));
    ImGui::NextColumn();

    ImGui::Columns(1);
}

void PerformanceMonitorPanel::RenderDatabaseTab() {
    ImGui::Columns(2, "DatabaseLayout", false);

    // Left column - session list
    ImGui::BeginChild("SessionList", ImVec2(0, -30), true);
    RenderSessionList();
    ImGui::EndChild();

    if (ImGui::Button("Refresh Sessions")) {
        RefreshSessionList();
    }

    ImGui::NextColumn();

    // Right column - details
    ImGui::BeginChild("SessionDetails", ImVec2(0, 0), true);
    if (m_selectedSessionA >= 0) {
        RenderSessionDetails();
    } else {
        ImGui::Text("Select a session to view details");
    }
    ImGui::EndChild();

    ImGui::Columns(1);

    ImGui::Separator();
    RenderDatabaseControls();
}

void PerformanceMonitorPanel::RenderSessionList() {
    ImGui::Text("Sessions:");
    ImGui::Separator();

    for (const auto& session : m_sessions) {
        bool isSelected = (session.sessionId == m_selectedSessionA);

        if (ImGui::Selectable((std::string("Session #") + std::to_string(session.sessionId) +
                              " - " + session.startTime).c_str(), isSelected)) {
            m_selectedSessionA = session.sessionId;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Preset: %s", session.qualityPreset.c_str());
            ImGui::Text("Resolution: %s", session.resolution.c_str());
            ImGui::EndTooltip();
        }
    }
}

void PerformanceMonitorPanel::RenderSessionDetails() {
    if (!m_database || !m_analyzer || m_selectedSessionA < 0) return;

    auto sessionInfo = m_database->GetSessionInfo(m_selectedSessionA);
    auto stats = m_database->GetStatistics(m_selectedSessionA);

    ImGui::Text("Session Details:");
    ImGui::Separator();

    ImGui::Text("Session ID: %d", sessionInfo.sessionId);
    ImGui::Text("Start Time: %s", sessionInfo.startTime.c_str());
    ImGui::Text("Preset: %s", sessionInfo.qualityPreset.c_str());
    ImGui::Text("Resolution: %s", sessionInfo.resolution.c_str());

    ImGui::Separator();
    ImGui::Text("Statistics:");

    ImGui::Text("Total Frames: %d", stats.totalFrames);
    ImGui::Text("Average FPS: %.1f", stats.avgFPS);
    ImGui::Text("Min FPS: %.1f", stats.minFPS);
    ImGui::Text("Max FPS: %.1f", stats.maxFPS);
    ImGui::Text("Average Frame Time: %.2f ms", stats.avgFrameTime);
    ImGui::Text("P95 Frame Time: %.2f ms", stats.p95FrameTime);
    ImGui::Text("P99 Frame Time: %.2f ms", stats.p99FrameTime);

    ImGui::Separator();

    if (ImGui::Button("Generate Report")) {
        std::string report = m_analyzer->GenerateTextReport(m_selectedSessionA);
        ImGui::OpenPopup("Report");

        // Save to file
        std::ofstream file("session_" + std::to_string(m_selectedSessionA) + "_report.txt");
        file << report;
        file.close();
    }

    if (ImGui::BeginPopup("Report")) {
        ImGui::Text("Report saved to session_%d_report.txt", m_selectedSessionA);
        ImGui::EndPopup();
    }
}

void PerformanceMonitorPanel::RenderDatabaseControls() {
    ImGui::Text("Database Controls:");

    if (ImGui::Button("Vacuum Database")) {
        if (m_database) {
            m_database->VacuumDatabase();
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Optimize")) {
        if (m_database) {
            m_database->OptimizeDatabase();
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Delete Old Data")) {
        if (m_database) {
            m_database->DeleteOldSessions(m_settings.dataRetentionDays);
        }
    }

    if (m_database) {
        ImGui::Text("Database Size: %s", FormatBytes(m_database->GetDatabaseSize()).c_str());
        ImGui::Text("Total Frames: %d", m_database->GetTotalFrameCount());
    }
}

void PerformanceMonitorPanel::RenderAnalysisTab() {
    if (!m_analyzer || m_selectedSessionA < 0) {
        ImGui::Text("Select a session in the Database tab to analyze");
        return;
    }

    RenderBottleneckAnalysis();
    ImGui::Separator();
    RenderSpikeDetection();
    ImGui::Separator();
    RenderTrendAnalysis();
    ImGui::Separator();
    RenderPerformanceScore();
}

void PerformanceMonitorPanel::RenderBottleneckAnalysis() {
    if (!m_analyzer) return;

    ImGui::Text("Bottleneck Analysis:");

    auto bottlenecks = m_analyzer->GetBottlenecks(m_selectedSessionA, 15.0f);

    if (ImGui::BeginTable("Bottlenecks", 4, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Stage");
        ImGui::TableSetupColumn("Avg Time");
        ImGui::TableSetupColumn("Avg %%");
        ImGui::TableSetupColumn("Max Time");
        ImGui::TableHeadersRow();

        for (const auto& bottleneck : bottlenecks) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", bottleneck.stageName.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.2f ms", bottleneck.averageTimeMs);

            ImGui::TableNextColumn();
            ImGui::Text("%.1f%%", bottleneck.averagePercent);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f ms", bottleneck.maxTimeMs);
        }

        ImGui::EndTable();
    }
}

void PerformanceMonitorPanel::RenderSpikeDetection() {
    if (!m_analyzer) return;

    ImGui::Text("Frame Spikes (>2x average):");

    auto spikes = m_analyzer->FindSpikes(m_selectedSessionA, 2.0f);

    ImGui::Text("Found %zu frame spikes", spikes.size());

    if (!spikes.empty() && ImGui::BeginTable("Spikes", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                                             ImVec2(0, 200))) {
        ImGui::TableSetupColumn("Frame");
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Multiplier");
        ImGui::TableHeadersRow();

        int displayCount = std::min(static_cast<int>(spikes.size()), 20);
        for (int i = 0; i < displayCount; ++i) {
            const auto& spike = spikes[i];

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%d", spike.frameNumber);

            ImGui::TableNextColumn();
            ImGui::TextColored(COLOR_WARNING, "%.2f ms", spike.frameTimeMs);

            ImGui::TableNextColumn();
            ImGui::Text("%.1fx", spike.multiplier);
        }

        ImGui::EndTable();
    }
}

void PerformanceMonitorPanel::RenderTrendAnalysis() {
    if (!m_analyzer) return;

    auto trend = m_analyzer->GetTrend(m_selectedSessionA);

    ImGui::Text("Performance Trend:");
    ImGui::Separator();

    ImVec4 trendColor = COLOR_GOOD;
    if (trend.direction == PerformanceTrend::Direction::DEGRADING) {
        trendColor = COLOR_WARNING;
    } else if (trend.direction == PerformanceTrend::Direction::STABLE) {
        trendColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }

    ImGui::Text("Direction:");
    ImGui::SameLine();
    ImGui::TextColored(trendColor, "%s", trend.GetDirectionString().c_str());

    ImGui::Text("Confidence: %.1f%%", trend.confidence * 100.0f);
    ImGui::Text("Sample Count: %d", trend.sampleCount);
}

void PerformanceMonitorPanel::RenderPerformanceScore() {
    if (!m_analyzer) return;

    float score = m_analyzer->CalculatePerformanceScore(m_selectedSessionA);

    ImGui::Text("Performance Score:");
    ImGui::Separator();

    ImVec4 scoreColor = COLOR_GOOD;
    if (score < 60.0f) {
        scoreColor = COLOR_CRITICAL;
    } else if (score < 80.0f) {
        scoreColor = COLOR_WARNING;
    }

    ImGui::TextColored(scoreColor, "%.1f / 100", score);
    ImGui::ProgressBar(score / 100.0f, ImVec2(-1, 0));
}

void PerformanceMonitorPanel::RenderSettingsTab() {
    RenderGeneralSettings();
    ImGui::Separator();
    RenderGraphSettings();
    ImGui::Separator();
    RenderDatabaseSettings();
    ImGui::Separator();
    RenderExportSettings();
}

void PerformanceMonitorPanel::RenderGeneralSettings() {
    ImGui::Text("General Settings:");

    ImGui::Checkbox("Auto-start session", &m_settings.autoStartSession);
    ImGui::SliderInt("Recording interval (frames)", &m_settings.recordingInterval, 1, 60);
    ImGui::Checkbox("Show FPS overlay", &m_settings.showFPSOverlay);
}

void PerformanceMonitorPanel::RenderGraphSettings() {
    ImGui::Text("Graph Settings:");

    if (ImGui::SliderInt("History size", &m_settings.historySize, 100, 10000)) {
        if (m_graphs) {
            m_graphs->SetHistorySize(m_settings.historySize);
        }
    }

    if (ImGui::Checkbox("Auto-scroll", &m_settings.autoScroll)) {
        if (m_graphs) {
            m_graphs->SetAutoScroll(m_settings.autoScroll);
        }
    }

    if (ImGui::Checkbox("Show grid", &m_settings.showGrid)) {
        if (m_graphs) {
            m_graphs->SetShowGrid(m_settings.showGrid);
        }
    }

    if (ImGui::Checkbox("Show legend", &m_settings.showLegend)) {
        if (m_graphs) {
            m_graphs->SetShowLegend(m_settings.showLegend);
        }
    }

    if (ImGui::SliderFloat("Target FPS", &m_settings.targetFPS, 30.0f, 144.0f)) {
        if (m_graphs) {
            m_graphs->SetTargetFPS(m_settings.targetFPS);
        }
        if (m_profiler) {
            m_profiler->SetTargetFPS(m_settings.targetFPS);
        }
    }
}

void PerformanceMonitorPanel::RenderDatabaseSettings() {
    ImGui::Text("Database Settings:");

    ImGui::Checkbox("Enable database", &m_settings.enableDatabase);
    ImGui::Checkbox("Use batch mode", &m_settings.useBatchMode);
    ImGui::SliderInt("Batch size", &m_settings.batchSize, 100, 10000);
    ImGui::SliderInt("Data retention (days)", &m_settings.dataRetentionDays, 1, 90);
}

void PerformanceMonitorPanel::RenderExportSettings() {
    ImGui::Text("Export Settings:");

    ImGui::InputText("Export path", m_settings.exportPath.data(), 256);
    ImGui::Checkbox("Include timestamp", &m_settings.includeTimestamp);
    ImGui::Checkbox("Export all sessions", &m_settings.exportAllSessions);
}

void PerformanceMonitorPanel::StartRecording() {
    if (!m_profiler || m_recording) return;

    m_recording = true;

    if (m_settings.enableDatabase && m_settings.autoStartSession && !IsSessionActive()) {
        StartSession(m_sessionPresetBuffer, m_sessionResolutionBuffer);
    }

    if (m_profiler) {
        m_profiler->EnableDatabaseRecording(m_settings.enableDatabase);
        m_profiler->SetRecordingInterval(m_settings.recordingInterval);
    }
}

void PerformanceMonitorPanel::StopRecording() {
    if (!m_profiler || !m_recording) return;

    m_recording = false;

    if (m_profiler) {
        m_profiler->EnableDatabaseRecording(false);
    }
}

void PerformanceMonitorPanel::StartSession(const std::string& preset, const std::string& resolution) {
    if (!m_profiler || IsSessionActive()) return;

    m_profiler->StartSession(preset, resolution);
    m_currentSession = m_profiler->GetSessionId();
}

void PerformanceMonitorPanel::EndSession() {
    if (!m_profiler || !IsSessionActive()) return;

    m_profiler->EndSession();
    m_currentSession = -1;
    RefreshSessionList();
}

bool PerformanceMonitorPanel::IsSessionActive() const {
    return m_profiler && m_profiler->IsSessionActive();
}

void PerformanceMonitorPanel::UpdateGraphs() {
    if (m_graphs && m_recording) {
        m_graphs->UpdateData();
    }
}

void PerformanceMonitorPanel::RefreshSessionList() {
    if (!m_database) return;

    m_sessions = m_database->GetRecentSessions(50);
}

void PerformanceMonitorPanel::ExportReport() {
    std::string filename = std::string(m_exportFilenameBuffer);

    if (m_settings.includeTimestamp) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "_%Y%m%d_%H%M%S");
        filename += oss.str();
    }

    // Export to multiple formats
    ExportToCSV(m_settings.exportPath + filename + ".csv");
    ExportToJSON(m_settings.exportPath + filename + ".json");
    ExportToHTML(m_settings.exportPath + filename + ".html");
}

bool PerformanceMonitorPanel::ExportToCSV(const std::string& filename) {
    if (!m_database || m_selectedSessionA < 0) return false;
    return m_database->ExportSessionToCSV(m_selectedSessionA, filename);
}

bool PerformanceMonitorPanel::ExportToJSON(const std::string& filename) {
    if (!m_database || m_selectedSessionA < 0) return false;
    return m_database->ExportSessionToJSON(m_selectedSessionA, filename);
}

bool PerformanceMonitorPanel::ExportToHTML(const std::string& filename) {
    if (!m_database || !m_analyzer || m_selectedSessionA < 0) return false;

    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "<!DOCTYPE html>\n<html>\n<head>\n";
    file << "<title>Performance Report - Session " << m_selectedSessionA << "</title>\n";
    file << "<style>body { font-family: Arial; } table { border-collapse: collapse; width: 100%; }\n";
    file << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }</style>\n";
    file << "</head>\n<body>\n";
    file << "<h1>Performance Report</h1>\n";
    file << "<pre>" << m_analyzer->GenerateTextReport(m_selectedSessionA) << "</pre>\n";
    file << "</body>\n</html>";

    file.close();
    return true;
}

std::string PerformanceMonitorPanel::FormatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit]);
    return std::string(buffer);
}

std::string PerformanceMonitorPanel::FormatDuration(float seconds) {
    int hours = static_cast<int>(seconds / 3600);
    int minutes = static_cast<int>((seconds - hours * 3600) / 60);
    int secs = static_cast<int>(seconds - hours * 3600 - minutes * 60);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
    return std::string(buffer);
}

ImVec4 PerformanceMonitorPanel::GetPerformanceColor(float fps) {
    if (fps >= m_settings.targetFPS * 0.9f) return COLOR_GOOD;
    if (fps >= m_settings.targetFPS * 0.6f) return COLOR_WARNING;
    return COLOR_CRITICAL;
}

} // namespace Profiling
} // namespace Engine
