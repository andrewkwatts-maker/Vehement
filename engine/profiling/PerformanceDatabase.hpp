#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

// Forward declaration for SQLite
struct sqlite3;
struct sqlite3_stmt;

namespace Engine {
namespace Profiling {

// Hardware configuration snapshot
struct HardwareConfig {
    std::string cpuModel;
    int cpuCoreCount;
    std::string gpuModel;
    size_t gpuMemoryMB;
    size_t systemMemoryMB;
    std::string driverVersion;
    std::string operatingSystem;
};

// Frame data structure
struct FrameData {
    int frameId = -1;           // Assigned by database
    int sessionId = -1;
    int frameNumber = 0;
    double timestamp = 0.0;     // Seconds since session start
    float totalTimeMs = 0.0f;
    float fps = 0.0f;
    bool vsyncEnabled = false;
};

// Stage timing data
struct StageData {
    int stageId = -1;           // Assigned by database
    int frameId = -1;
    std::string stageName;
    float timeMs = 0.0f;
    float percentage = 0.0f;
    float gpuTimeMs = 0.0f;
    float cpuTimeMs = 0.0f;
};

// Memory usage data
struct MemoryData {
    int memoryId = -1;          // Assigned by database
    int frameId = -1;
    float cpuUsedMB = 0.0f;
    float cpuAvailableMB = 0.0f;
    float gpuUsedMB = 0.0f;
    float gpuAvailableMB = 0.0f;
};

// GPU metrics
struct GPUData {
    int gpuId = -1;             // Assigned by database
    int frameId = -1;
    float utilizationPercent = 0.0f;
    float temperatureCelsius = 0.0f;
    int clockMHz = 0;
    int memoryClockMHz = 0;
};

// CPU metrics
struct CPUData {
    int cpuId = -1;             // Assigned by database
    int frameId = -1;
    int coreCount = 0;
    float utilizationPercent = 0.0f;
    float temperatureCelsius = 0.0f;
    int clockMHz = 0;
};

// Rendering statistics
struct RenderingStats {
    int statsId = -1;           // Assigned by database
    int frameId = -1;
    int drawCalls = 0;
    int triangles = 0;
    int vertices = 0;
    int instances = 0;
    int lights = 0;
    int shadowMaps = 0;
};

// Session information
struct SessionInfo {
    int sessionId = -1;
    std::string startTime;
    std::string endTime;
    HardwareConfig hardwareConfig;
    std::string qualityPreset;
    std::string resolution;
    int frameCount = 0;
    float totalDurationSeconds = 0.0f;
};

// Frame statistics summary
struct FrameStatistics {
    float avgFPS = 0.0f;
    float minFPS = 0.0f;
    float maxFPS = 0.0f;
    float avgFrameTime = 0.0f;
    float p50FrameTime = 0.0f;  // Median
    float p95FrameTime = 0.0f;
    float p99FrameTime = 0.0f;
    int totalFrames = 0;
    float totalDuration = 0.0f;
};

// Query filter for advanced queries
struct QueryFilter {
    int sessionId = -1;
    double startTime = -1.0;
    double endTime = -1.0;
    int limit = 1000;
    int offset = 0;
    std::string orderBy = "frame_number";
    bool ascending = true;
};

/**
 * @class PerformanceDatabase
 * @brief SQLite-based performance metrics storage and retrieval
 *
 * Features:
 * - Automatic schema creation and migration
 * - Batch insert optimization (1000+ frames buffered)
 * - Session management with hardware tracking
 * - Query API for analysis and reporting
 * - Automatic data retention and cleanup
 * - Thread-safe operations
 */
class PerformanceDatabase {
public:
    PerformanceDatabase();
    ~PerformanceDatabase();

    // Initialization
    bool Initialize(const std::string& dbPath);
    void Shutdown();
    bool IsInitialized() const { return m_db != nullptr; }

    // Session management
    int CreateSession(const HardwareConfig& hw, const std::string& preset, const std::string& resolution);
    void EndSession(int sessionId);
    SessionInfo GetSessionInfo(int sessionId);
    std::vector<SessionInfo> GetAllSessions(int limit = 100);
    std::vector<SessionInfo> GetRecentSessions(int count = 10);

    // Frame data recording
    int RecordFrame(int sessionId, const FrameData& frame);
    void RecordStage(int frameId, const StageData& stage);
    void RecordMemory(int frameId, const MemoryData& memory);
    void RecordGPU(int frameId, const GPUData& gpu);
    void RecordCPU(int frameId, const CPUData& cpu);
    void RecordRenderingStats(int frameId, const RenderingStats& stats);

    // Batch operations (for performance)
    void BeginBatch();
    void EndBatch();  // Commits all buffered inserts
    bool IsInBatch() const { return m_inBatch; }
    void FlushBatch(); // Force flush without ending batch

    // Bulk recording (convenience methods)
    int RecordCompleteFrame(int sessionId, const FrameData& frame,
                           const std::vector<StageData>& stages,
                           const MemoryData& memory,
                           const GPUData& gpu,
                           const CPUData& cpu,
                           const RenderingStats& stats);

    // Query operations
    std::vector<FrameData> GetFrames(int sessionId, int limit = 1000, int offset = 0);
    std::vector<FrameData> GetFramesInTimeRange(int sessionId, double startTime, double endTime);
    std::vector<StageData> GetStages(int frameId);
    MemoryData GetMemory(int frameId);
    GPUData GetGPU(int frameId);
    CPUData GetCPU(int frameId);
    RenderingStats GetRenderingStats(int frameId);

    // Advanced queries
    std::vector<FrameData> QueryFrames(const QueryFilter& filter);
    FrameStatistics GetStatistics(int sessionId);
    FrameStatistics GetStatisticsInTimeRange(int sessionId, double startTime, double endTime);

    // Stage-specific queries
    std::vector<float> GetStageTimings(int sessionId, const std::string& stageName, int limit = 1000);
    float GetAverageStageTime(int sessionId, const std::string& stageName);
    float GetMaxStageTime(int sessionId, const std::string& stageName);

    // Memory queries
    std::vector<MemoryData> GetMemoryHistory(int sessionId, int limit = 1000);
    float GetPeakGPUMemory(int sessionId);
    float GetPeakCPUMemory(int sessionId);

    // Performance analysis
    std::vector<std::string> GetBottleneckStages(int sessionId, float thresholdPercent = 20.0f);
    std::vector<int> FindFrameSpikes(int sessionId, float multiplier = 2.0f);
    std::vector<FrameData> GetSlowestFrames(int sessionId, int count = 100);
    std::vector<FrameData> GetFastestFrames(int sessionId, int count = 100);

    // Database maintenance
    void VacuumDatabase();
    void DeleteOldSessions(int daysToKeep);
    void DeleteSession(int sessionId);
    void OptimizeDatabase();
    size_t GetDatabaseSize();
    int GetTotalFrameCount();
    int GetSessionFrameCount(int sessionId);

    // Export functionality
    bool ExportSessionToCSV(int sessionId, const std::string& outputPath);
    bool ExportSessionToJSON(int sessionId, const std::string& outputPath);
    bool ExportStatsToHTML(int sessionId, const std::string& outputPath);

    // Error handling
    std::string GetLastError() const { return m_lastError; }
    bool HasError() const { return !m_lastError.empty(); }
    void ClearError() { m_lastError.clear(); }

private:
    // Database setup
    bool CreateSchema();
    bool CreateIndices();
    bool CheckSchemaVersion();
    bool MigrateSchema(int fromVersion, int toVersion);

    // Prepared statements
    bool PrepareStatements();
    void FinalizeStatements();

    // Helper methods
    bool ExecuteSQL(const std::string& sql);
    int GetLastInsertRowId();
    void SetError(const std::string& error);
    std::string GetCurrentTimestamp();

    // Batch buffer management
    void FlushFrameBuffer();
    void FlushStageBuffer();
    void FlushMemoryBuffer();
    void FlushGPUBuffer();
    void FlushCPUBuffer();
    void FlushStatsBuffer();

    // JSON/CSV helpers
    std::string FrameDataToJSON(const FrameData& frame);
    std::string FrameDataToCSV(const FrameData& frame);
    std::string SessionInfoToJSON(const SessionInfo& session);

private:
    sqlite3* m_db = nullptr;
    std::string m_dbPath;
    std::string m_lastError;

    // Batch mode
    bool m_inBatch = false;
    std::vector<FrameData> m_frameBuffer;
    std::vector<StageData> m_stageBuffer;
    std::vector<MemoryData> m_memoryBuffer;
    std::vector<GPUData> m_gpuBuffer;
    std::vector<CPUData> m_cpuBuffer;
    std::vector<RenderingStats> m_statsBuffer;

    // Prepared statements for performance
    sqlite3_stmt* m_insertFrameStmt = nullptr;
    sqlite3_stmt* m_insertStageStmt = nullptr;
    sqlite3_stmt* m_insertMemoryStmt = nullptr;
    sqlite3_stmt* m_insertGPUStmt = nullptr;
    sqlite3_stmt* m_insertCPUStmt = nullptr;
    sqlite3_stmt* m_insertStatsStmt = nullptr;

    // Configuration
    static constexpr int BATCH_SIZE = 1000;
    static constexpr int SCHEMA_VERSION = 1;
};

} // namespace Profiling
} // namespace Engine
