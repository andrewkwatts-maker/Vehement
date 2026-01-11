#pragma once

#include <vector>
#include <string>
#include <map>
#include <array>

// Forward declaration for ImGui/ImPlot
struct ImVec2;
struct ImVec4;

namespace Engine {
namespace Profiling {

// Forward declarations
class DetailedFrameProfiler;
class PerformanceDatabase;
struct FrameBreakdown;

/**
 * @brief Color scheme for graphs
 */
struct GraphColors {
    static const ImVec4 FPS_LINE;
    static const ImVec4 TARGET_FPS_LINE;
    static const ImVec4 FRAME_TIME;
    static const ImVec4 GPU_TIME;
    static const ImVec4 CPU_TIME;

    // Stage colors
    static const ImVec4 CULLING;
    static const ImVec4 TERRAIN;
    static const ImVec4 GBUFFER;
    static const ImVec4 LIGHTING;
    static const ImVec4 POST_PROCESSING;
    static const ImVec4 UI_RENDERING;
    static const ImVec4 OVERHEAD;

    static ImVec4 GetStageColor(const std::string& stageName);
};

/**
 * @class PerformanceGraphs
 * @brief Real-time performance visualization using ImPlot
 *
 * Features:
 * - FPS line graph
 * - Frame time stacked area chart
 * - Stage breakdown pie chart
 * - Memory usage graphs
 * - GPU/CPU utilization heatmaps
 * - Thread utilization bar charts
 */
class PerformanceGraphs {
public:
    PerformanceGraphs();
    ~PerformanceGraphs();

    // Initialization
    void Initialize(DetailedFrameProfiler* profiler);
    void Shutdown();

    // Main graph rendering
    void RenderFPSGraph(float width = -1.0f, float height = 200.0f);
    void RenderFrameTimeGraph(float width = -1.0f, float height = 200.0f);
    void RenderStackedBreakdown(float width = -1.0f, float height = 300.0f);
    void RenderPieChart(float radius = 80.0f);
    void RenderMemoryGraph(float width = -1.0f, float height = 200.0f);
    void RenderGPUUtilizationGraph(float width = -1.0f, float height = 150.0f);
    void RenderCPUUtilizationGraph(float width = -1.0f, float height = 150.0f);

    // Combined views
    void RenderOverviewGraphs();
    void RenderDetailedGraphs();

    // Configuration
    void SetHistorySize(int size);
    int GetHistorySize() const { return m_historySize; }

    void SetAutoScroll(bool enabled) { m_autoScroll = enabled; }
    bool IsAutoScrollEnabled() const { return m_autoScroll; }

    void SetShowGrid(bool enabled) { m_showGrid = enabled; }
    bool IsGridVisible() const { return m_showGrid; }

    void SetShowLegend(bool enabled) { m_showLegend = enabled; }
    bool IsLegendVisible() const { return m_showLegend; }

    // Target FPS line
    void SetTargetFPS(float fps) { m_targetFPS = fps; }
    float GetTargetFPS() const { return m_targetFPS; }

    // Data management
    void UpdateData();
    void ClearData();

    // Export
    bool ExportGraphDataToCSV(const std::string& filename);

private:
    // Helper rendering methods
    void RenderFPSLineGraph(const char* label, float width, float height);
    void RenderStackedAreaChart(const char* label, float width, float height);
    void RenderStagePieChart(float centerX, float centerY, float radius);
    void RenderMemoryLineGraph(const char* label, float width, float height);
    void RenderUtilizationHeatmap(const char* label, const std::vector<float>& data, float width, float height);

    // Data preparation
    void PrepareStackedData();
    void PrepareMemoryData();
    void CalculatePieChartAngles();

    // Utility
    ImVec4 HSVtoRGB(float h, float s, float v);
    void DrawPieSlice(float centerX, float centerY, float radius, float startAngle, float endAngle,
                     const ImVec4& color, const std::string& label, float percentage);

private:
    DetailedFrameProfiler* m_profiler = nullptr;

    // Configuration
    int m_historySize = 1000;
    bool m_autoScroll = true;
    bool m_showGrid = true;
    bool m_showLegend = true;
    float m_targetFPS = 60.0f;

    // Cached data for rendering
    std::vector<float> m_fpsData;
    std::vector<float> m_frameTimeData;
    std::vector<float> m_targetFPSLine;

    // Stage data for stacked chart
    struct StageData {
        std::string name;
        std::vector<float> values;
        ImVec4 color;
    };
    std::vector<StageData> m_stageData;

    // Memory data
    std::vector<float> m_cpuMemoryData;
    std::vector<float> m_gpuMemoryData;

    // Utilization data
    std::vector<float> m_gpuUtilizationData;
    std::vector<float> m_cpuUtilizationData;

    // Pie chart data
    struct PieSlice {
        std::string label;
        float percentage;
        float startAngle;
        float endAngle;
        ImVec4 color;
    };
    std::vector<PieSlice> m_pieSlices;

    // Frame counter for X-axis
    int m_frameCounter = 0;
};

} // namespace Profiling
} // namespace Engine
