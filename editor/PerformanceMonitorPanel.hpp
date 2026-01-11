#pragma once

#include <string>
#include <memory>
#include <vector>

namespace Engine {
namespace Profiling {

// Forward declarations
class DetailedFrameProfiler;
class PerformanceDatabase;
class PerformanceAnalyzer;
class PerformanceGraphs;
struct SessionInfo;

/**
 * @class PerformanceMonitorPanel
 * @brief Main UI panel for performance monitoring and analysis
 *
 * Features:
 * - Real-time performance monitoring
 * - Frame breakdown visualization
 * - Live graphs (FPS, frame time, per-stage timing)
 * - Memory and hardware monitoring
 * - Database session management
 * - Export functionality (CSV, JSON, HTML)
 * - Historical analysis
 */
class PerformanceMonitorPanel {
public:
    PerformanceMonitorPanel();
    ~PerformanceMonitorPanel();

    // Initialization
    bool Initialize(const std::string& databasePath);
    void Shutdown();

    // Main rendering
    void Render();
    void Update();

    // Window state
    void Show() { m_isOpen = true; }
    void Hide() { m_isOpen = false; }
    void Toggle() { m_isOpen = !m_isOpen; }
    bool IsOpen() const { return m_isOpen; }

    // Access to profiler
    DetailedFrameProfiler* GetProfiler() { return m_profiler.get(); }
    const DetailedFrameProfiler* GetProfiler() const { return m_profiler.get(); }

    // Recording control
    void StartRecording();
    void StopRecording();
    bool IsRecording() const { return m_recording; }

    // Session management
    void StartSession(const std::string& preset, const std::string& resolution);
    void EndSession();
    bool IsSessionActive() const;

private:
    // Tab rendering
    void RenderUI();
    void RenderOverviewTab();
    void RenderBreakdownTab();
    void RenderGraphsTab();
    void RenderMemoryTab();
    void RenderDatabaseTab();
    void RenderAnalysisTab();
    void RenderSettingsTab();

    // Overview tab components
    void RenderCurrentFrameStats();
    void RenderAverageStats();
    void RenderHardwareMetrics();

    // Breakdown tab components
    void RenderBreakdownTable();
    void RenderBreakdownPieChart();
    void RenderStageComparison();

    // Graphs tab components
    void RenderFPSGraphs();
    void RenderFrameTimeGraphs();
    void RenderStageGraphs();

    // Memory tab components
    void RenderMemoryUsage();
    void RenderMemoryGraphs();
    void RenderMemoryStatistics();

    // Database tab components
    void RenderSessionList();
    void RenderSessionDetails();
    void RenderSessionComparison();
    void RenderDatabaseControls();

    // Analysis tab components
    void RenderBottleneckAnalysis();
    void RenderSpikeDetection();
    void RenderTrendAnalysis();
    void RenderPerformanceScore();

    // Settings tab components
    void RenderGeneralSettings();
    void RenderGraphSettings();
    void RenderDatabaseSettings();
    void RenderExportSettings();

    // Export functionality
    void ExportReport();
    bool ExportToCSV(const std::string& filename);
    bool ExportToJSON(const std::string& filename);
    bool ExportToHTML(const std::string& filename);

    // Helper methods
    void UpdateGraphs();
    void RefreshSessionList();
    std::string FormatBytes(size_t bytes);
    std::string FormatDuration(float seconds);
    ImVec4 GetPerformanceColor(float fps);

private:
    // Core components
    std::unique_ptr<DetailedFrameProfiler> m_profiler;
    std::unique_ptr<PerformanceDatabase> m_database;
    std::unique_ptr<PerformanceAnalyzer> m_analyzer;
    std::unique_ptr<PerformanceGraphs> m_graphs;

    // UI state
    bool m_isOpen = false;
    bool m_recording = false;
    bool m_initialized = false;

    // Settings
    struct Settings {
        // General
        bool autoStartSession = true;
        int recordingInterval = 1;  // Record every N frames
        bool showFPSOverlay = true;

        // Graphs
        int historySize = 1000;
        bool autoScroll = true;
        bool showGrid = true;
        bool showLegend = true;
        float targetFPS = 60.0f;

        // Database
        bool enableDatabase = true;
        bool useBatchMode = true;
        int batchSize = 1000;
        int dataRetentionDays = 30;

        // Export
        std::string exportPath = "./exports/";
        bool includeTimestamp = true;
        bool exportAllSessions = false;
    } m_settings;

    // Session management
    std::vector<SessionInfo> m_sessions;
    int m_selectedSessionA = -1;
    int m_selectedSessionB = -1;
    int m_currentSession = -1;

    // UI state
    int m_currentTab = 0;
    char m_sessionPresetBuffer[64] = "High";
    char m_sessionResolutionBuffer[64] = "1920x1080";
    char m_exportFilenameBuffer[256] = "performance_report";

    // Performance tracking
    float m_updateTimer = 0.0f;
    float m_updateInterval = 0.1f;  // Update UI 10 times per second

    // Colors for UI
    static const ImVec4 COLOR_GOOD;
    static const ImVec4 COLOR_WARNING;
    static const ImVec4 COLOR_CRITICAL;
};

} // namespace Profiling
} // namespace Engine
