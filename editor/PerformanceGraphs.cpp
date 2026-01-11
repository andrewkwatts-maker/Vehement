#include "PerformanceGraphs.hpp"
#include "../engine/profiling/DetailedFrameProfiler.hpp"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <fstream>
#include <cmath>

namespace Engine {
namespace Profiling {

// Define color constants
const ImVec4 GraphColors::FPS_LINE = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
const ImVec4 GraphColors::TARGET_FPS_LINE = ImVec4(1.0f, 0.5f, 0.0f, 0.5f);
const ImVec4 GraphColors::FRAME_TIME = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
const ImVec4 GraphColors::GPU_TIME = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
const ImVec4 GraphColors::CPU_TIME = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);

const ImVec4 GraphColors::CULLING = ImVec4(0.8f, 0.3f, 0.3f, 0.8f);
const ImVec4 GraphColors::TERRAIN = ImVec4(0.5f, 0.7f, 0.3f, 0.8f);
const ImVec4 GraphColors::GBUFFER = ImVec4(0.3f, 0.5f, 0.8f, 0.8f);
const ImVec4 GraphColors::LIGHTING = ImVec4(0.9f, 0.7f, 0.2f, 0.8f);
const ImVec4 GraphColors::POST_PROCESSING = ImVec4(0.7f, 0.3f, 0.8f, 0.8f);
const ImVec4 GraphColors::UI_RENDERING = ImVec4(0.3f, 0.8f, 0.8f, 0.8f);
const ImVec4 GraphColors::OVERHEAD = ImVec4(0.5f, 0.5f, 0.5f, 0.8f);

ImVec4 GraphColors::GetStageColor(const std::string& stageName) {
    if (stageName == "Culling") return CULLING;
    if (stageName == "Terrain") return TERRAIN;
    if (stageName == "SDF_GBuffer") return GBUFFER;
    if (stageName == "Deferred_Lighting") return LIGHTING;
    if (stageName == "Post_Processing") return POST_PROCESSING;
    if (stageName == "UI_Rendering") return UI_RENDERING;
    return OVERHEAD;
}

PerformanceGraphs::PerformanceGraphs()
    : m_profiler(nullptr)
    , m_historySize(1000)
    , m_autoScroll(true)
    , m_showGrid(true)
    , m_showLegend(true)
    , m_targetFPS(60.0f)
    , m_frameCounter(0) {
}

PerformanceGraphs::~PerformanceGraphs() {
    Shutdown();
}

void PerformanceGraphs::Initialize(DetailedFrameProfiler* profiler) {
    m_profiler = profiler;

    // Reserve space for data
    m_fpsData.reserve(m_historySize);
    m_frameTimeData.reserve(m_historySize);
    m_targetFPSLine.reserve(m_historySize);
    m_cpuMemoryData.reserve(m_historySize);
    m_gpuMemoryData.reserve(m_historySize);
    m_gpuUtilizationData.reserve(m_historySize);
    m_cpuUtilizationData.reserve(m_historySize);
}

void PerformanceGraphs::Shutdown() {
    ClearData();
    m_profiler = nullptr;
}

void PerformanceGraphs::UpdateData() {
    if (!m_profiler) return;

    // Get current FPS and frame time
    float currentFPS = m_profiler->GetCurrentFPS();
    float currentFrameTime = m_profiler->GetCurrentFrameTime();

    // Add to history
    if (m_fpsData.size() >= m_historySize) {
        m_fpsData.erase(m_fpsData.begin());
    }
    m_fpsData.push_back(currentFPS);

    if (m_frameTimeData.size() >= m_historySize) {
        m_frameTimeData.erase(m_frameTimeData.begin());
    }
    m_frameTimeData.push_back(currentFrameTime);

    // Update target FPS line
    if (m_targetFPSLine.size() >= m_historySize) {
        m_targetFPSLine.erase(m_targetFPSLine.begin());
    }
    m_targetFPSLine.push_back(m_targetFPS);

    // Update memory data
    const auto& memSnapshot = m_profiler->GetMemorySnapshot();
    if (m_cpuMemoryData.size() >= m_historySize) {
        m_cpuMemoryData.erase(m_cpuMemoryData.begin());
    }
    m_cpuMemoryData.push_back(memSnapshot.cpuUsedMB);

    if (m_gpuMemoryData.size() >= m_historySize) {
        m_gpuMemoryData.erase(m_gpuMemoryData.begin());
    }
    m_gpuMemoryData.push_back(memSnapshot.gpuUsedMB);

    // Update utilization data
    const auto& hwMetrics = m_profiler->GetHardwareMetrics();
    if (m_gpuUtilizationData.size() >= m_historySize) {
        m_gpuUtilizationData.erase(m_gpuUtilizationData.begin());
    }
    m_gpuUtilizationData.push_back(hwMetrics.gpuUtilization);

    if (m_cpuUtilizationData.size() >= m_historySize) {
        m_cpuUtilizationData.erase(m_cpuUtilizationData.begin());
    }
    m_cpuUtilizationData.push_back(hwMetrics.cpuUtilization);

    // Prepare stage data for stacked chart
    PrepareStackedData();

    // Calculate pie chart angles
    CalculatePieChartAngles();

    m_frameCounter++;
}

void PerformanceGraphs::ClearData() {
    m_fpsData.clear();
    m_frameTimeData.clear();
    m_targetFPSLine.clear();
    m_stageData.clear();
    m_cpuMemoryData.clear();
    m_gpuMemoryData.clear();
    m_gpuUtilizationData.clear();
    m_cpuUtilizationData.clear();
    m_pieSlices.clear();
    m_frameCounter = 0;
}

void PerformanceGraphs::RenderFPSGraph(float width, float height) {
    if (!m_profiler || m_fpsData.empty()) return;

    RenderFPSLineGraph("FPS Over Time", width, height);
}

void PerformanceGraphs::RenderFrameTimeGraph(float width, float height) {
    if (!m_profiler || m_frameTimeData.empty()) return;

    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);

    if (ImPlot::BeginPlot("Frame Time", ImVec2(width, height))) {
        // Setup axes
        ImPlot::SetupAxes("Frame", "Time (ms)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_historySize, m_autoScroll ? ImPlotCond_Always : ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 50, ImPlotCond_Once);

        // Plot frame time
        ImPlot::SetNextLineStyle(GraphColors::FRAME_TIME);
        ImPlot::PlotLine("Frame Time", m_frameTimeData.data(), m_frameTimeData.size());

        // Plot 16.67ms line (60 FPS target)
        std::vector<float> targetLine(m_frameTimeData.size(), 16.67f);
        ImPlot::SetNextLineStyle(GraphColors::TARGET_FPS_LINE);
        ImPlot::PlotLine("16.67ms (60 FPS)", targetLine.data(), targetLine.size());

        ImPlot::EndPlot();
    }

    ImPlot::PopStyleVar();
}

void PerformanceGraphs::RenderStackedBreakdown(float width, float height) {
    if (!m_profiler || m_stageData.empty()) return;

    RenderStackedAreaChart("Frame Breakdown", width, height);
}

void PerformanceGraphs::RenderPieChart(float radius) {
    if (!m_profiler || m_pieSlices.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImVec2(radius * 2.5f, radius * 2.5f);

    // Draw background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                           IM_COL32(30, 30, 30, 255));

    float centerX = canvasPos.x + radius * 1.25f;
    float centerY = canvasPos.y + radius * 1.25f;

    RenderStagePieChart(centerX, centerY, radius);

    // Reserve space
    ImGui::Dummy(canvasSize);
}

void PerformanceGraphs::RenderMemoryGraph(float width, float height) {
    if (!m_profiler || m_cpuMemoryData.empty()) return;

    RenderMemoryLineGraph("Memory Usage", width, height);
}

void PerformanceGraphs::RenderGPUUtilizationGraph(float width, float height) {
    if (!m_profiler || m_gpuUtilizationData.empty()) return;

    if (ImPlot::BeginPlot("GPU Utilization", ImVec2(width, height))) {
        ImPlot::SetupAxes("Frame", "Utilization (%)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_historySize, m_autoScroll ? ImPlotCond_Always : ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImPlotCond_Always);

        ImPlot::SetNextLineStyle(GraphColors::GPU_TIME);
        ImPlot::PlotLine("GPU", m_gpuUtilizationData.data(), m_gpuUtilizationData.size());

        ImPlot::EndPlot();
    }
}

void PerformanceGraphs::RenderCPUUtilizationGraph(float width, float height) {
    if (!m_profiler || m_cpuUtilizationData.empty()) return;

    if (ImPlot::BeginPlot("CPU Utilization", ImVec2(width, height))) {
        ImPlot::SetupAxes("Frame", "Utilization (%)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_historySize, m_autoScroll ? ImPlotCond_Always : ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImPlotCond_Always);

        ImPlot::SetNextLineStyle(GraphColors::CPU_TIME);
        ImPlot::PlotLine("CPU", m_cpuUtilizationData.data(), m_cpuUtilizationData.size());

        ImPlot::EndPlot();
    }
}

void PerformanceGraphs::RenderOverviewGraphs() {
    RenderFPSGraph(-1.0f, 200.0f);
    ImGui::Spacing();
    RenderFrameTimeGraph(-1.0f, 200.0f);
}

void PerformanceGraphs::RenderDetailedGraphs() {
    RenderFPSGraph(-1.0f, 200.0f);
    ImGui::Spacing();
    RenderStackedBreakdown(-1.0f, 300.0f);
    ImGui::Spacing();
    RenderMemoryGraph(-1.0f, 200.0f);
}

void PerformanceGraphs::RenderFPSLineGraph(const char* label, float width, float height) {
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);

    if (ImPlot::BeginPlot(label, ImVec2(width, height))) {
        // Setup axes
        ImPlot::SetupAxes("Frame", "FPS");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_historySize, m_autoScroll ? ImPlotCond_Always : ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 200, ImPlotCond_Once);

        // Plot FPS
        ImPlot::SetNextLineStyle(GraphColors::FPS_LINE);
        ImPlot::PlotLine("FPS", m_fpsData.data(), m_fpsData.size());

        // Plot target FPS
        ImPlot::SetNextLineStyle(GraphColors::TARGET_FPS_LINE);
        ImPlot::PlotLine("Target", m_targetFPSLine.data(), m_targetFPSLine.size());

        ImPlot::EndPlot();
    }

    ImPlot::PopStyleVar();
}

void PerformanceGraphs::RenderStackedAreaChart(const char* label, float width, float height) {
    if (m_stageData.empty()) return;

    if (ImPlot::BeginPlot(label, ImVec2(width, height))) {
        ImPlot::SetupAxes("Frame", "Time (ms)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_historySize, m_autoScroll ? ImPlotCond_Always : ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 33, ImPlotCond_Once);

        // Plot each stage as shaded area
        for (const auto& stage : m_stageData) {
            if (stage.values.empty()) continue;

            ImPlot::SetNextFillStyle(stage.color);
            ImPlot::PlotShaded(stage.name.c_str(), stage.values.data(), stage.values.size());
        }

        ImPlot::EndPlot();
    }
}

void PerformanceGraphs::RenderStagePieChart(float centerX, float centerY, float radius) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& slice : m_pieSlices) {
        DrawPieSlice(centerX, centerY, radius, slice.startAngle, slice.endAngle,
                    slice.color, slice.label, slice.percentage);
    }

    // Draw center circle (donut chart style)
    drawList->AddCircleFilled(ImVec2(centerX, centerY), radius * 0.4f, IM_COL32(40, 40, 40, 255), 32);
}

void PerformanceGraphs::RenderMemoryLineGraph(const char* label, float width, float height) {
    if (ImPlot::BeginPlot(label, ImVec2(width, height))) {
        ImPlot::SetupAxes("Frame", "Memory (MB)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_historySize, m_autoScroll ? ImPlotCond_Always : ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 4096, ImPlotCond_Once);

        ImPlot::SetNextLineStyle(GraphColors::CPU_TIME);
        ImPlot::PlotLine("CPU Memory", m_cpuMemoryData.data(), m_cpuMemoryData.size());

        ImPlot::SetNextLineStyle(GraphColors::GPU_TIME);
        ImPlot::PlotLine("GPU Memory", m_gpuMemoryData.data(), m_gpuMemoryData.size());

        ImPlot::EndPlot();
    }
}

void PerformanceGraphs::PrepareStackedData() {
    if (!m_profiler) return;

    const auto& breakdown = m_profiler->GetCurrentBreakdown();

    // Update or create stage data
    for (const auto& stage : breakdown.stages) {
        auto it = std::find_if(m_stageData.begin(), m_stageData.end(),
                              [&](const StageData& sd) { return sd.name == stage.name; });

        if (it == m_stageData.end()) {
            // New stage
            StageData newStage;
            newStage.name = stage.name;
            newStage.color = GraphColors::GetStageColor(stage.name);
            newStage.values.reserve(m_historySize);
            m_stageData.push_back(newStage);
            it = m_stageData.end() - 1;
        }

        // Add value
        if (it->values.size() >= m_historySize) {
            it->values.erase(it->values.begin());
        }
        it->values.push_back(stage.timeMs);
    }
}

void PerformanceGraphs::CalculatePieChartAngles() {
    if (!m_profiler) return;

    const auto& breakdown = m_profiler->GetCurrentBreakdown();
    m_pieSlices.clear();

    float currentAngle = -3.14159f / 2.0f;  // Start at top

    for (const auto& stage : breakdown.stages) {
        if (stage.percentage < 1.0f) continue;  // Skip very small slices

        PieSlice slice;
        slice.label = stage.name;
        slice.percentage = stage.percentage;
        slice.startAngle = currentAngle;

        float angleSize = (stage.percentage / 100.0f) * 2.0f * 3.14159f;
        slice.endAngle = currentAngle + angleSize;
        slice.color = GraphColors::GetStageColor(stage.name);

        m_pieSlices.push_back(slice);

        currentAngle = slice.endAngle;
    }
}

void PerformanceGraphs::DrawPieSlice(float centerX, float centerY, float radius,
                                    float startAngle, float endAngle,
                                    const ImVec4& color, const std::string& label,
                                    float percentage) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw pie slice
    const int segments = 32;
    float angleStep = (endAngle - startAngle) / segments;

    ImU32 col32 = ImGui::ColorConvertFloat4ToU32(color);

    for (int i = 0; i < segments; ++i) {
        float a1 = startAngle + angleStep * i;
        float a2 = startAngle + angleStep * (i + 1);

        ImVec2 p1(centerX + std::cos(a1) * radius, centerY + std::sin(a1) * radius);
        ImVec2 p2(centerX + std::cos(a2) * radius, centerY + std::sin(a2) * radius);

        drawList->AddTriangleFilled(ImVec2(centerX, centerY), p1, p2, col32);
    }

    // Draw label
    float midAngle = (startAngle + endAngle) * 0.5f;
    float labelRadius = radius * 0.7f;
    ImVec2 labelPos(
        centerX + std::cos(midAngle) * labelRadius,
        centerY + std::sin(midAngle) * labelRadius
    );

    char labelText[64];
    snprintf(labelText, sizeof(labelText), "%.1f%%", percentage);

    ImVec2 textSize = ImGui::CalcTextSize(labelText);
    labelPos.x -= textSize.x * 0.5f;
    labelPos.y -= textSize.y * 0.5f;

    drawList->AddText(labelPos, IM_COL32(255, 255, 255, 255), labelText);
}

ImVec4 PerformanceGraphs::HSVtoRGB(float h, float s, float v) {
    ImVec4 result;
    ImGui::ColorConvertHSVtoRGB(h, s, v, result.x, result.y, result.z);
    result.w = 1.0f;
    return result;
}

void PerformanceGraphs::SetHistorySize(int size) {
    m_historySize = size;
    m_fpsData.reserve(size);
    m_frameTimeData.reserve(size);
    m_targetFPSLine.reserve(size);
}

bool PerformanceGraphs::ExportGraphDataToCSV(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "Frame,FPS,FrameTime\n";
    for (size_t i = 0; i < m_fpsData.size(); ++i) {
        file << i << "," << m_fpsData[i] << "," << m_frameTimeData[i] << "\n";
    }

    file.close();
    return true;
}

} // namespace Profiling
} // namespace Engine
