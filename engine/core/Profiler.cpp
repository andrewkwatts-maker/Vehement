/**
 * @file Profiler.cpp
 * @brief Implementation of the Nova3D profiling system
 */

#include "Profiler.hpp"
#include <glad/gl.h>
#include <imgui.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Nova {

// =============================================================================
// GPUProfiler Implementation
// =============================================================================

GPUProfiler::~GPUProfiler() {
    Shutdown();
}

bool GPUProfiler::Initialize() {
    if (m_initialized) {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create double-buffered query objects for each slot
    for (size_t i = 0; i < PROFILER_MAX_GPU_QUERIES; ++i) {
        glGenQueries(2, m_queries[i].queryId);
        if (m_queries[i].queryId[0] == 0 || m_queries[i].queryId[1] == 0) {
            // Cleanup on failure
            for (size_t j = 0; j <= i; ++j) {
                if (m_queries[j].queryId[0] != 0) {
                    glDeleteQueries(2, m_queries[j].queryId);
                    m_queries[j].queryId[0] = 0;
                    m_queries[j].queryId[1] = 0;
                }
            }
            return false;
        }
        m_queries[i].active = false;
        m_queries[i].currentBuffer = 0;
        m_queries[i].timeMs = 0.0;
        m_queries[i].name.clear();
    }

    m_initialized = true;
    m_currentFrame = 0;
    m_nextQuerySlot = 0;
    m_activeQueryIndex = -1;
    m_totalGPUTime = 0.0;

    return true;
}

void GPUProfiler::Shutdown() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& query : m_queries) {
        if (query.queryId[0] != 0) {
            glDeleteQueries(2, query.queryId);
            query.queryId[0] = 0;
            query.queryId[1] = 0;
        }
        query.active = false;
        query.name.clear();
    }

    m_initialized = false;
}

void GPUProfiler::BeginFrame() {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Swap buffers for double-buffering
    m_currentFrame = (m_currentFrame + 1) % 2;
    m_nextQuerySlot = 0;
    m_totalGPUTime = 0.0;

    // Reset all queries for this frame
    for (auto& query : m_queries) {
        query.active = false;
    }
}

void GPUProfiler::EndFrame() {
    if (!m_initialized) return;

    // Collect results from previous frame
    CollectResults();
}

int GPUProfiler::BeginQuery(std::string_view name) {
    if (!m_initialized) return -1;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_activeQueryIndex >= 0) {
        // Already have an active query - end it first
        return -1;
    }

    if (m_nextQuerySlot >= static_cast<int>(PROFILER_MAX_GPU_QUERIES)) {
        return -1;
    }

    int slot = m_nextQuerySlot++;
    m_queries[slot].name = std::string(name);
    m_queries[slot].active = true;
    m_queries[slot].currentBuffer = m_currentFrame;

    glBeginQuery(GL_TIME_ELAPSED, m_queries[slot].queryId[m_currentFrame]);
    m_activeQueryIndex = slot;

    return slot;
}

void GPUProfiler::EndQuery() {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_activeQueryIndex < 0) return;

    glEndQuery(GL_TIME_ELAPSED);
    m_activeQueryIndex = -1;
}

void GPUProfiler::CollectResults() {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    int previousFrame = (m_currentFrame + 1) % 2;
    m_totalGPUTime = 0.0;

    for (auto& query : m_queries) {
        if (!query.active) continue;

        // Check if result is available from the previous frame's buffer
        GLint available = 0;
        glGetQueryObjectiv(query.queryId[previousFrame], GL_QUERY_RESULT_AVAILABLE, &available);

        if (available) {
            GLuint64 timeNs = 0;
            glGetQueryObjectui64v(query.queryId[previousFrame], GL_QUERY_RESULT, &timeNs);
            query.timeMs = static_cast<double>(timeNs) / 1000000.0;
            m_totalGPUTime += query.timeMs;

            // Also record this in the main profiler
            Profiler::Instance().RecordSample("GPU_" + query.name, query.timeMs);
        }
    }
}

double GPUProfiler::GetQueryTime(std::string_view name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& query : m_queries) {
        if (query.name == name) {
            return query.timeMs;
        }
    }
    return 0.0;
}

std::vector<std::pair<std::string, double>> GPUProfiler::GetAllResults() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::pair<std::string, double>> results;
    results.reserve(m_nextQuerySlot);

    for (int i = 0; i < m_nextQuerySlot; ++i) {
        if (!m_queries[i].name.empty()) {
            results.emplace_back(m_queries[i].name, m_queries[i].timeMs);
        }
    }

    return results;
}

// =============================================================================
// Profiler::ProfileScope Implementation
// =============================================================================

Profiler::ProfileScope::ProfileScope(Profiler& profiler, std::string_view name, bool gpuScope)
    : m_profiler(&profiler)
    , m_name(name)
    , m_gpuScope(gpuScope)
    , m_gpuQueryIndex(-1)
{
    if (!m_profiler->IsEnabled()) {
        m_profiler = nullptr;
        return;
    }

    // Get thread-local scope stack for hierarchy tracking
    ScopeStack& stack = m_profiler->GetThreadScopeStack();
    m_parentName = stack.GetParentName();
    m_depth = static_cast<uint32_t>(stack.Depth());
    stack.Push(name);

    // Start GPU timing if requested
    if (m_gpuScope && GPUProfiler::Instance().IsInitialized()) {
        m_gpuQueryIndex = GPUProfiler::Instance().BeginQuery(name);
    }
}

Profiler::ProfileScope::~ProfileScope() {
    if (!m_profiler) return;

    double elapsedMs = m_timer.ElapsedMs();

    // End GPU query if we started one
    if (m_gpuScope && m_gpuQueryIndex >= 0) {
        GPUProfiler::Instance().EndQuery();
    }

    // Pop from scope stack
    ScopeStack& stack = m_profiler->GetThreadScopeStack();
    stack.Pop();

    // Record the timing
    m_profiler->EndScopeInternal(m_name, elapsedMs, m_depth, m_parentName);
}

Profiler::ProfileScope::ProfileScope(ProfileScope&& other) noexcept
    : m_profiler(other.m_profiler)
    , m_name(std::move(other.m_name))
    , m_parentName(std::move(other.m_parentName))
    , m_depth(other.m_depth)
    , m_timer(other.m_timer)
    , m_gpuScope(other.m_gpuScope)
    , m_gpuQueryIndex(other.m_gpuQueryIndex)
{
    other.m_profiler = nullptr;
}

// =============================================================================
// Profiler Implementation
// =============================================================================

Profiler::Profiler() {
    m_recentFrameTimes.fill(16.67);  // Default to ~60 FPS
}

Profiler::~Profiler() {
    Shutdown();
}

bool Profiler::Initialize() {
    if (m_initialized) return true;

    // Initialize GPU profiler
    if (!GPUProfiler::Instance().Initialize()) {
        // GPU profiling not available, but CPU profiling still works
    }

    m_initialized = true;
    m_frameCount = 0;
    m_frameTimeIndex = 0;

    return true;
}

void Profiler::Shutdown() {
    if (!m_initialized) return;

    GPUProfiler::Instance().Shutdown();

    Clear();
    m_initialized = false;
}

ScopeStack& Profiler::GetThreadScopeStack() {
    std::lock_guard<std::mutex> lock(m_threadStacksMutex);

    auto threadId = std::this_thread::get_id();
    auto it = m_threadStacks.find(threadId);
    if (it == m_threadStacks.end()) {
        auto result = m_threadStacks.emplace(threadId, std::make_unique<ScopeStack>());
        return *result.first->second;
    }
    return *it->second;
}

void Profiler::BeginFrame() {
    if (!m_enabled) return;

    m_frameTimer.Reset();
    ++m_frameCount;

    // Reset current frame stats
    m_currentFrameStats = FrameStats{};
    m_currentFrameStats.frameNumber = m_frameCount;

    // Begin GPU frame
    if (GPUProfiler::Instance().IsInitialized()) {
        GPUProfiler::Instance().BeginFrame();
    }
}

void Profiler::EndFrame() {
    if (!m_enabled) return;

    double frameMs = m_frameTimer.ElapsedMs();

    // End GPU frame and collect results
    if (GPUProfiler::Instance().IsInitialized()) {
        GPUProfiler::Instance().EndFrame();
        m_currentFrameStats.gpuTimeMs = GPUProfiler::Instance().GetTotalGPUTime();
    }

    // Update frame statistics
    m_currentFrameStats.frameTimeMs = frameMs;
    m_currentFrameStats.cpuTimeMs = frameMs;  // Approximate; GPU time overlaps
    m_currentFrameStats.fps = (frameMs > 0.0) ? (1000.0 / frameMs) : 0.0;

    // Get memory stats
    auto memStats = MemoryTracker::Instance().GetStats();
    m_currentFrameStats.totalMemoryUsed = memStats.currentBytes;
    m_currentFrameStats.peakMemoryUsed = memStats.peakBytes;
    m_currentFrameStats.gpuMemoryUsed = memStats.gpuMemoryBytes;

    // Record frame time
    RecordSample("Frame", frameMs);

    // Track rolling average
    m_recentFrameTimes[m_frameTimeIndex] = frameMs;
    m_frameTimeIndex = (m_frameTimeIndex + 1) % PROFILER_FRAME_HISTORY_SIZE;

    // Add to frame history
    m_frameHistory.push_back(m_currentFrameStats);
    if (m_frameHistory.size() > PROFILER_FRAME_HISTORY_SIZE) {
        m_frameHistory.pop_front();
    }
}

void Profiler::RecordSample(std::string_view name, double milliseconds,
                            uint32_t depth, std::string_view parent) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_statsMutex);

    std::string nameStr(name);
    auto& stats = m_scopeStats[nameStr];
    if (stats.name.empty()) {
        stats.name = nameStr;
        stats.depth = depth;
        stats.parentName = std::string(parent);
    }
    stats.AddSample(milliseconds);
}

void Profiler::EndScopeInternal(const std::string& name, double milliseconds,
                                 uint32_t depth, const std::string& parent) {
    RecordSample(name, milliseconds, depth, parent);

    // Add to current frame's scope timings
    m_currentFrameStats.scopeTimings.emplace_back(name, milliseconds);
}

const ProfilerStats* Profiler::GetScopeStats(std::string_view name) const {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    auto it = m_scopeStats.find(std::string(name));
    return (it != m_scopeStats.end()) ? &it->second : nullptr;
}

std::vector<ProfilerStats> Profiler::GetAllScopeStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    std::vector<ProfilerStats> result;
    result.reserve(m_scopeStats.size());

    for (const auto& [name, stats] : m_scopeStats) {
        result.push_back(stats);
    }

    // Sort by total time (descending)
    std::sort(result.begin(), result.end(),
              [](const ProfilerStats& a, const ProfilerStats& b) {
                  return a.totalMs > b.totalMs;
              });

    return result;
}

std::vector<ProfilerStats> Profiler::GetHierarchicalStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    std::vector<ProfilerStats> result;
    result.reserve(m_scopeStats.size());

    for (const auto& [name, stats] : m_scopeStats) {
        result.push_back(stats);
    }

    // Sort by depth first, then by name for consistent ordering
    std::sort(result.begin(), result.end(),
              [](const ProfilerStats& a, const ProfilerStats& b) {
                  if (a.depth != b.depth) return a.depth < b.depth;
                  return a.name < b.name;
              });

    return result;
}

FrameStats Profiler::GetLastFrameStats() const {
    if (m_frameHistory.empty()) {
        return FrameStats{};
    }
    return m_frameHistory.back();
}

double Profiler::GetAverageFPS() const {
    double sum = 0.0;
    size_t count = std::min(m_frameHistory.size(), static_cast<size_t>(60));
    if (count == 0) return 0.0;

    auto it = m_frameHistory.rbegin();
    for (size_t i = 0; i < count && it != m_frameHistory.rend(); ++i, ++it) {
        sum += it->fps;
    }

    return sum / static_cast<double>(count);
}

double Profiler::GetAverageFrameTime() const {
    double sum = 0.0;
    size_t count = std::min(m_frameHistory.size(), static_cast<size_t>(60));
    if (count == 0) return 0.0;

    auto it = m_frameHistory.rbegin();
    for (size_t i = 0; i < count && it != m_frameHistory.rend(); ++i, ++it) {
        sum += it->frameTimeMs;
    }

    return sum / static_cast<double>(count);
}

std::string Profiler::GenerateReport() const {
    auto stats = GetAllScopeStats();

    std::ostringstream report;
    report << std::fixed << std::setprecision(3);

    report << "================================================================================\n";
    report << "                        Nova3D Performance Profile Report\n";
    report << "================================================================================\n\n";

    // Summary
    report << "SUMMARY\n";
    report << "-------\n";
    report << "  Total Frames: " << m_frameCount << "\n";
    report << "  Average FPS: " << GetAverageFPS() << "\n";
    report << "  Average Frame Time: " << GetAverageFrameTime() << " ms\n";

    auto memStats = MemoryTracker::Instance().GetStats();
    report << "  Current Memory: " << (memStats.currentBytes / (1024.0 * 1024.0)) << " MB\n";
    report << "  Peak Memory: " << (memStats.peakBytes / (1024.0 * 1024.0)) << " MB\n";
    report << "\n";

    // Detailed statistics
    report << "SCOPE STATISTICS\n";
    report << "----------------\n\n";

    report << std::left;
    report << std::setw(32) << "Scope Name"
           << std::setw(12) << "Total(ms)"
           << std::setw(10) << "Avg(ms)"
           << std::setw(10) << "Min(ms)"
           << std::setw(10) << "Max(ms)"
           << std::setw(10) << "P50(ms)"
           << std::setw(10) << "P95(ms)"
           << std::setw(10) << "P99(ms)"
           << std::setw(10) << "StdDev"
           << std::setw(10) << "Calls"
           << "\n";

    report << std::string(122, '-') << "\n";

    for (const auto& s : stats) {
        // Indent based on depth
        std::string indent(s.depth * 2, ' ');
        std::string displayName = indent + s.name;
        if (displayName.length() > 31) {
            displayName = displayName.substr(0, 28) + "...";
        }

        report << std::setw(32) << displayName
               << std::setw(12) << s.totalMs
               << std::setw(10) << s.avgMs
               << std::setw(10) << (s.minMs == std::numeric_limits<double>::max() ? 0.0 : s.minMs)
               << std::setw(10) << s.maxMs
               << std::setw(10) << s.p50Ms
               << std::setw(10) << s.p95Ms
               << std::setw(10) << s.p99Ms
               << std::setw(10) << s.stdDevMs
               << std::setw(10) << s.callCount
               << "\n";
    }

    report << "\n================================================================================\n";

    return report.str();
}

bool Profiler::ExportCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) return false;

    file << "Scope,Depth,Parent,TotalMs,AvgMs,MinMs,MaxMs,P50Ms,P95Ms,P99Ms,StdDevMs,CallCount\n";

    auto stats = GetAllScopeStats();
    for (const auto& s : stats) {
        double minMs = (s.minMs == std::numeric_limits<double>::max()) ? 0.0 : s.minMs;
        file << "\"" << s.name << "\","
             << s.depth << ","
             << "\"" << s.parentName << "\","
             << s.totalMs << ","
             << s.avgMs << ","
             << minMs << ","
             << s.maxMs << ","
             << s.p50Ms << ","
             << s.p95Ms << ","
             << s.p99Ms << ","
             << s.stdDevMs << ","
             << s.callCount << "\n";
    }

    return true;
}

bool Profiler::ExportFrameHistoryCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) return false;

    file << "FrameNumber,FrameTimeMs,CPUTimeMs,GPUTimeMs,FPS,MemoryBytes,PeakMemoryBytes,GPUMemoryBytes\n";

    for (const auto& frame : m_frameHistory) {
        file << frame.frameNumber << ","
             << frame.frameTimeMs << ","
             << frame.cpuTimeMs << ","
             << frame.gpuTimeMs << ","
             << frame.fps << ","
             << frame.totalMemoryUsed << ","
             << frame.peakMemoryUsed << ","
             << frame.gpuMemoryUsed << "\n";
    }

    return true;
}

bool Profiler::SaveReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) return false;
    file << GenerateReport();
    return true;
}

void Profiler::ResetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    for (auto& [name, stats] : m_scopeStats) {
        stats.Reset();
    }

    m_frameHistory.clear();
    m_recentFrameTimes.fill(16.67);
    m_frameTimeIndex = 0;
}

void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    m_scopeStats.clear();
    m_frameHistory.clear();
    m_recentFrameTimes.fill(16.67);
    m_frameTimeIndex = 0;
    m_frameCount = 0;

    // Clear thread stacks
    std::lock_guard<std::mutex> stackLock(m_threadStacksMutex);
    m_threadStacks.clear();
}

// =============================================================================
// ProfilerWindow Implementation
// =============================================================================

ProfilerWindow::ProfilerWindow() {
    m_frameTimeGraphData.resize(PROFILER_FRAME_HISTORY_SIZE, 0.0f);
    m_fpsGraphData.resize(PROFILER_FRAME_HISTORY_SIZE, 60.0f);
}

bool ProfilerWindow::Initialize() {
    return true;
}

void ProfilerWindow::Render() {
    if (!m_visible) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Profiler", &m_visible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show GPU Stats", nullptr, &m_showGPU);
            ImGui::MenuItem("Show Memory", nullptr, &m_showMemory);
            ImGui::MenuItem("Show Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Show Percentiles", nullptr, &m_showPercentiles);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Export")) {
            if (ImGui::MenuItem("Export CSV...")) {
                Profiler::Instance().ExportCSV("profiler_stats.csv");
            }
            if (ImGui::MenuItem("Export Frame History...")) {
                Profiler::Instance().ExportFrameHistoryCSV("profiler_frames.csv");
            }
            if (ImGui::MenuItem("Save Report...")) {
                Profiler::Instance().SaveReport("profiler_report.txt");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Controls toolbar
    RenderControlsToolbar();

    ImGui::Separator();

    // Frame time graph
    RenderFrameTimeGraph();

    ImGui::Separator();

    // Create tabs for different views
    if (ImGui::BeginTabBar("ProfilerTabs")) {
        if (ImGui::BeginTabItem("Statistics")) {
            RenderStatisticsTable();
            ImGui::EndTabItem();
        }

        if (m_showHierarchy && ImGui::BeginTabItem("Hierarchy")) {
            RenderScopeHierarchy();
            ImGui::EndTabItem();
        }

        if (m_showGPU && ImGui::BeginTabItem("GPU")) {
            RenderGPUStats();
            ImGui::EndTabItem();
        }

        if (m_showMemory && ImGui::BeginTabItem("Memory")) {
            RenderMemoryStats();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ProfilerWindow::RenderControlsToolbar() {
    auto& profiler = Profiler::Instance();

    // Pause/Resume button
    if (m_pauseUpdates) {
        if (ImGui::Button("Resume")) {
            m_pauseUpdates = false;
            profiler.SetEnabled(true);
        }
    } else {
        if (ImGui::Button("Pause")) {
            m_pauseUpdates = true;
            profiler.SetEnabled(false);
        }
    }

    ImGui::SameLine();

    // Reset button
    if (ImGui::Button("Reset Stats")) {
        profiler.ResetStats();
    }

    ImGui::SameLine();

    // Clear button
    if (ImGui::Button("Clear All")) {
        profiler.Clear();
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Frame stats summary
    double avgFps = profiler.GetAverageFPS();
    double avgFrameTime = profiler.GetAverageFrameTime();
    ImGui::Text("FPS: %.1f | Frame: %.2f ms | Frames: %llu",
                avgFps, avgFrameTime,
                static_cast<unsigned long long>(profiler.GetFrameCount()));
}

void ProfilerWindow::RenderFrameTimeGraph() {
    auto& profiler = Profiler::Instance();
    const auto& history = profiler.GetFrameHistory();

    // Update graph data
    m_frameTimeGraphData.clear();
    m_fpsGraphData.clear();

    float maxFrameTime = 0.0f;
    for (const auto& frame : history) {
        m_frameTimeGraphData.push_back(static_cast<float>(frame.frameTimeMs));
        m_fpsGraphData.push_back(static_cast<float>(frame.fps));
        maxFrameTime = std::max(maxFrameTime, static_cast<float>(frame.frameTimeMs));
    }

    // Auto-scale with some headroom
    float graphMax = std::max(m_graphTimeScale, maxFrameTime * 1.2f);

    ImGui::Text("Frame Time (ms)");

    // Draw frame time graph
    ImGui::PlotLines("##FrameTime", m_frameTimeGraphData.data(),
                     static_cast<int>(m_frameTimeGraphData.size()),
                     0, nullptr, 0.0f, graphMax,
                     ImVec2(ImGui::GetContentRegionAvail().x, m_graphHeight));

    // Target frame time lines
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Scale: %.1f ms", graphMax);
    if (ImGui::Button("16.67 (60fps)")) m_graphTimeScale = 16.67f;
    if (ImGui::Button("33.33 (30fps)")) m_graphTimeScale = 33.33f;
    if (ImGui::Button("Auto")) m_graphTimeScale = 0.0f;
    ImGui::EndGroup();
}

void ProfilerWindow::RenderScopeHierarchy() {
    auto stats = Profiler::Instance().GetHierarchicalStats();

    if (stats.empty()) {
        ImGui::TextDisabled("No profiling data available");
        return;
    }

    // Build tree structure
    ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), true);

    for (const auto& s : stats) {
        // Calculate indent based on depth
        float indent = s.depth * 20.0f;
        ImGui::Indent(indent);

        // Create tree node
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        bool selected = (m_selectedScopeIndex >= 0 &&
                         static_cast<size_t>(m_selectedScopeIndex) < stats.size() &&
                         stats[m_selectedScopeIndex].name == s.name);
        if (selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx(s.name.c_str(), flags);

        // Show timing on same line
        ImGui::SameLine(300);
        ImGui::Text("%.3f ms (avg: %.3f, calls: %llu)",
                    s.lastMs, s.avgMs,
                    static_cast<unsigned long long>(s.callCount));

        ImGui::Unindent(indent);
    }

    ImGui::EndChild();
}

void ProfilerWindow::RenderMemoryStats() {
    auto memStats = MemoryTracker::Instance().GetStats();

    ImGui::Text("Memory Usage");
    ImGui::Separator();

    // Current usage
    float currentMB = static_cast<float>(memStats.currentBytes) / (1024.0f * 1024.0f);
    float peakMB = static_cast<float>(memStats.peakBytes) / (1024.0f * 1024.0f);
    float gpuMB = static_cast<float>(memStats.gpuMemoryBytes) / (1024.0f * 1024.0f);

    ImGui::Text("Current: %.2f MB", currentMB);
    ImGui::Text("Peak: %.2f MB", peakMB);
    ImGui::Text("GPU Memory: %.2f MB", gpuMB);

    ImGui::Separator();

    ImGui::Text("Allocations: %llu",
                static_cast<unsigned long long>(memStats.totalAllocations));
    ImGui::Text("Deallocations: %llu",
                static_cast<unsigned long long>(memStats.totalDeallocations));

    // Memory usage bar
    if (peakMB > 0) {
        float ratio = currentMB / peakMB;
        ImGui::ProgressBar(ratio, ImVec2(-1, 0),
                          (std::to_string(static_cast<int>(currentMB)) + " / " +
                           std::to_string(static_cast<int>(peakMB)) + " MB").c_str());
    }
}

void ProfilerWindow::RenderGPUStats() {
    auto results = GPUProfiler::Instance().GetAllResults();
    double totalGPU = GPUProfiler::Instance().GetTotalGPUTime();

    ImGui::Text("GPU Timing (Total: %.3f ms)", totalGPU);
    ImGui::Separator();

    if (results.empty()) {
        ImGui::TextDisabled("No GPU profiling data available");
        return;
    }

    // Table with GPU timings
    if (ImGui::BeginTable("GPUTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("% of Total", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();

        for (const auto& [name, timeMs] : results) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", timeMs);
            ImGui::TableSetColumnIndex(2);
            float percent = (totalGPU > 0) ? (timeMs / totalGPU * 100.0f) : 0.0f;
            ImGui::Text("%.1f%%", percent);
        }

        ImGui::EndTable();
    }
}

void ProfilerWindow::RenderStatisticsTable() {
    auto stats = Profiler::Instance().GetAllScopeStats();

    if (stats.empty()) {
        ImGui::TextDisabled("No profiling data available");
        return;
    }

    // Determine columns based on settings
    int columnCount = m_showPercentiles ? 10 : 7;

    ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("StatsTable", columnCount, flags)) {
        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Total (ms)", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Min (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 70);

        if (m_showPercentiles) {
            ImGui::TableSetupColumn("P50 (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("P95 (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("P99 (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
        }

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Handle sorting
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty && sortSpecs->SpecsCount > 0) {
                const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
                bool ascending = (spec.SortDirection == ImGuiSortDirection_Ascending);

                std::sort(stats.begin(), stats.end(),
                    [&spec, ascending](const ProfilerStats& a, const ProfilerStats& b) {
                        double va = 0, vb = 0;
                        switch (spec.ColumnIndex) {
                            case 0: return ascending ? (a.name < b.name) : (a.name > b.name);
                            case 1: va = a.totalMs; vb = b.totalMs; break;
                            case 2: va = a.avgMs; vb = b.avgMs; break;
                            case 3: va = a.minMs; vb = b.minMs; break;
                            case 4: va = a.maxMs; vb = b.maxMs; break;
                            case 5: va = a.lastMs; vb = b.lastMs; break;
                            case 6: va = static_cast<double>(a.callCount);
                                    vb = static_cast<double>(b.callCount); break;
                            case 7: va = a.p50Ms; vb = b.p50Ms; break;
                            case 8: va = a.p95Ms; vb = b.p95Ms; break;
                            case 9: va = a.p99Ms; vb = b.p99Ms; break;
                        }
                        return ascending ? (va < vb) : (va > vb);
                    });

                sortSpecs->SpecsDirty = false;
            }
        }

        // Render rows
        for (const auto& s : stats) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            std::string indent(s.depth * 2, ' ');
            ImGui::Text("%s%s", indent.c_str(), s.name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", s.totalMs);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", s.avgMs);

            ImGui::TableSetColumnIndex(3);
            double minMs = (s.minMs == std::numeric_limits<double>::max()) ? 0.0 : s.minMs;
            ImGui::Text("%.3f", minMs);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f", s.maxMs);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.3f", s.lastMs);

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%llu", static_cast<unsigned long long>(s.callCount));

            if (m_showPercentiles) {
                ImGui::TableSetColumnIndex(7);
                ImGui::Text("%.3f", s.p50Ms);

                ImGui::TableSetColumnIndex(8);
                ImGui::Text("%.3f", s.p95Ms);

                ImGui::TableSetColumnIndex(9);
                ImGui::Text("%.3f", s.p99Ms);
            }
        }

        ImGui::EndTable();
    }
}

} // namespace Nova
