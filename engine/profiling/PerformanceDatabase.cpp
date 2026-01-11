#include "PerformanceDatabase.hpp"
#include <sqlite3.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace Engine {
namespace Profiling {

PerformanceDatabase::PerformanceDatabase() {
}

PerformanceDatabase::~PerformanceDatabase() {
    Shutdown();
}

bool PerformanceDatabase::Initialize(const std::string& dbPath) {
    if (m_db) {
        SetError("Database already initialized");
        return false;
    }

    m_dbPath = dbPath;

    // Open database
    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        SetError(std::string("Failed to open database: ") + sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent access
    ExecuteSQL("PRAGMA journal_mode=WAL");
    ExecuteSQL("PRAGMA synchronous=NORMAL");
    ExecuteSQL("PRAGMA cache_size=10000");
    ExecuteSQL("PRAGMA temp_store=MEMORY");

    // Create schema if needed
    if (!CreateSchema()) {
        Shutdown();
        return false;
    }

    // Create indices
    if (!CreateIndices()) {
        Shutdown();
        return false;
    }

    // Prepare statements
    if (!PrepareStatements()) {
        Shutdown();
        return false;
    }

    return true;
}

void PerformanceDatabase::Shutdown() {
    if (m_inBatch) {
        EndBatch();
    }

    FinalizeStatements();

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }

    m_frameBuffer.clear();
    m_stageBuffer.clear();
    m_memoryBuffer.clear();
    m_gpuBuffer.clear();
    m_cpuBuffer.clear();
    m_statsBuffer.clear();
}

bool PerformanceDatabase::CreateSchema() {
    std::vector<std::string> schemas = {
        // Sessions table
        R"(CREATE TABLE IF NOT EXISTS Sessions (
            session_id INTEGER PRIMARY KEY AUTOINCREMENT,
            start_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            end_time DATETIME,
            cpu_model TEXT,
            cpu_cores INTEGER,
            gpu_model TEXT,
            gpu_memory_mb INTEGER,
            system_memory_mb INTEGER,
            driver_version TEXT,
            operating_system TEXT,
            quality_preset TEXT,
            resolution TEXT
        ))",

        // FrameData table
        R"(CREATE TABLE IF NOT EXISTS FrameData (
            frame_id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id INTEGER,
            frame_number INTEGER,
            timestamp REAL,
            total_time_ms REAL,
            fps REAL,
            vsync_enabled INTEGER,
            FOREIGN KEY (session_id) REFERENCES Sessions(session_id) ON DELETE CASCADE
        ))",

        // StageData table
        R"(CREATE TABLE IF NOT EXISTS StageData (
            stage_id INTEGER PRIMARY KEY AUTOINCREMENT,
            frame_id INTEGER,
            stage_name TEXT,
            time_ms REAL,
            percentage REAL,
            gpu_time_ms REAL,
            cpu_time_ms REAL,
            FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE
        ))",

        // MemoryData table
        R"(CREATE TABLE IF NOT EXISTS MemoryData (
            memory_id INTEGER PRIMARY KEY AUTOINCREMENT,
            frame_id INTEGER,
            cpu_used_mb REAL,
            cpu_available_mb REAL,
            gpu_used_mb REAL,
            gpu_available_mb REAL,
            FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE
        ))",

        // GPUData table
        R"(CREATE TABLE IF NOT EXISTS GPUData (
            gpu_id INTEGER PRIMARY KEY AUTOINCREMENT,
            frame_id INTEGER,
            utilization_percent REAL,
            temperature_celsius REAL,
            clock_mhz INTEGER,
            memory_clock_mhz INTEGER,
            FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE
        ))",

        // CPUData table
        R"(CREATE TABLE IF NOT EXISTS CPUData (
            cpu_id INTEGER PRIMARY KEY AUTOINCREMENT,
            frame_id INTEGER,
            core_count INTEGER,
            utilization_percent REAL,
            temperature_celsius REAL,
            clock_mhz INTEGER,
            FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE
        ))",

        // RenderingStats table
        R"(CREATE TABLE IF NOT EXISTS RenderingStats (
            stats_id INTEGER PRIMARY KEY AUTOINCREMENT,
            frame_id INTEGER,
            draw_calls INTEGER,
            triangles INTEGER,
            vertices INTEGER,
            instances INTEGER,
            lights INTEGER,
            shadow_maps INTEGER,
            FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE
        ))",

        // Schema version table
        R"(CREATE TABLE IF NOT EXISTS SchemaVersion (
            version INTEGER PRIMARY KEY
        ))"
    };

    for (const auto& schema : schemas) {
        if (!ExecuteSQL(schema)) {
            return false;
        }
    }

    // Insert schema version
    ExecuteSQL("INSERT OR IGNORE INTO SchemaVersion (version) VALUES (" +
               std::to_string(SCHEMA_VERSION) + ")");

    return true;
}

bool PerformanceDatabase::CreateIndices() {
    std::vector<std::string> indices = {
        "CREATE INDEX IF NOT EXISTS idx_frame_session ON FrameData(session_id)",
        "CREATE INDEX IF NOT EXISTS idx_frame_number ON FrameData(frame_number)",
        "CREATE INDEX IF NOT EXISTS idx_frame_timestamp ON FrameData(timestamp)",
        "CREATE INDEX IF NOT EXISTS idx_stage_frame ON StageData(frame_id)",
        "CREATE INDEX IF NOT EXISTS idx_stage_name ON StageData(stage_name)",
        "CREATE INDEX IF NOT EXISTS idx_memory_frame ON MemoryData(frame_id)",
        "CREATE INDEX IF NOT EXISTS idx_gpu_frame ON GPUData(frame_id)",
        "CREATE INDEX IF NOT EXISTS idx_cpu_frame ON CPUData(frame_id)",
        "CREATE INDEX IF NOT EXISTS idx_stats_frame ON RenderingStats(frame_id)"
    };

    for (const auto& index : indices) {
        if (!ExecuteSQL(index)) {
            return false;
        }
    }

    return true;
}

bool PerformanceDatabase::PrepareStatements() {
    // Prepare insert frame statement
    const char* insertFrameSQL =
        "INSERT INTO FrameData (session_id, frame_number, timestamp, total_time_ms, fps, vsync_enabled) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertFrameSQL, -1, &m_insertFrameStmt, nullptr) != SQLITE_OK) {
        SetError("Failed to prepare insert frame statement");
        return false;
    }

    // Prepare insert stage statement
    const char* insertStageSQL =
        "INSERT INTO StageData (frame_id, stage_name, time_ms, percentage, gpu_time_ms, cpu_time_ms) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertStageSQL, -1, &m_insertStageStmt, nullptr) != SQLITE_OK) {
        SetError("Failed to prepare insert stage statement");
        return false;
    }

    // Prepare insert memory statement
    const char* insertMemorySQL =
        "INSERT INTO MemoryData (frame_id, cpu_used_mb, cpu_available_mb, gpu_used_mb, gpu_available_mb) "
        "VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertMemorySQL, -1, &m_insertMemoryStmt, nullptr) != SQLITE_OK) {
        SetError("Failed to prepare insert memory statement");
        return false;
    }

    // Prepare insert GPU statement
    const char* insertGPUSQL =
        "INSERT INTO GPUData (frame_id, utilization_percent, temperature_celsius, clock_mhz, memory_clock_mhz) "
        "VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertGPUSQL, -1, &m_insertGPUStmt, nullptr) != SQLITE_OK) {
        SetError("Failed to prepare insert GPU statement");
        return false;
    }

    // Prepare insert CPU statement
    const char* insertCPUSQL =
        "INSERT INTO CPUData (frame_id, core_count, utilization_percent, temperature_celsius, clock_mhz) "
        "VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertCPUSQL, -1, &m_insertCPUStmt, nullptr) != SQLITE_OK) {
        SetError("Failed to prepare insert CPU statement");
        return false;
    }

    // Prepare insert stats statement
    const char* insertStatsSQL =
        "INSERT INTO RenderingStats (frame_id, draw_calls, triangles, vertices, instances, lights, shadow_maps) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, insertStatsSQL, -1, &m_insertStatsStmt, nullptr) != SQLITE_OK) {
        SetError("Failed to prepare insert stats statement");
        return false;
    }

    return true;
}

void PerformanceDatabase::FinalizeStatements() {
    if (m_insertFrameStmt) {
        sqlite3_finalize(m_insertFrameStmt);
        m_insertFrameStmt = nullptr;
    }
    if (m_insertStageStmt) {
        sqlite3_finalize(m_insertStageStmt);
        m_insertStageStmt = nullptr;
    }
    if (m_insertMemoryStmt) {
        sqlite3_finalize(m_insertMemoryStmt);
        m_insertMemoryStmt = nullptr;
    }
    if (m_insertGPUStmt) {
        sqlite3_finalize(m_insertGPUStmt);
        m_insertGPUStmt = nullptr;
    }
    if (m_insertCPUStmt) {
        sqlite3_finalize(m_insertCPUStmt);
        m_insertCPUStmt = nullptr;
    }
    if (m_insertStatsStmt) {
        sqlite3_finalize(m_insertStatsStmt);
        m_insertStatsStmt = nullptr;
    }
}

int PerformanceDatabase::CreateSession(const HardwareConfig& hw, const std::string& preset, const std::string& resolution) {
    if (!m_db) return -1;

    std::ostringstream sql;
    sql << "INSERT INTO Sessions (cpu_model, cpu_cores, gpu_model, gpu_memory_mb, "
        << "system_memory_mb, driver_version, operating_system, quality_preset, resolution) "
        << "VALUES ("
        << "'" << hw.cpuModel << "', "
        << hw.cpuCoreCount << ", "
        << "'" << hw.gpuModel << "', "
        << hw.gpuMemoryMB << ", "
        << hw.systemMemoryMB << ", "
        << "'" << hw.driverVersion << "', "
        << "'" << hw.operatingSystem << "', "
        << "'" << preset << "', "
        << "'" << resolution << "')";

    if (!ExecuteSQL(sql.str())) {
        return -1;
    }

    return GetLastInsertRowId();
}

void PerformanceDatabase::EndSession(int sessionId) {
    if (!m_db) return;

    std::ostringstream sql;
    sql << "UPDATE Sessions SET end_time = CURRENT_TIMESTAMP WHERE session_id = " << sessionId;
    ExecuteSQL(sql.str());
}

SessionInfo PerformanceDatabase::GetSessionInfo(int sessionId) {
    SessionInfo info;
    if (!m_db) return info;

    std::ostringstream sql;
    sql << "SELECT session_id, start_time, end_time, cpu_model, cpu_cores, gpu_model, "
        << "gpu_memory_mb, system_memory_mb, driver_version, operating_system, quality_preset, resolution "
        << "FROM Sessions WHERE session_id = " << sessionId;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info.sessionId = sqlite3_column_int(stmt, 0);
            info.startTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
                info.endTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            }
            info.hardwareConfig.cpuModel = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            info.hardwareConfig.cpuCoreCount = sqlite3_column_int(stmt, 4);
            info.hardwareConfig.gpuModel = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            info.hardwareConfig.gpuMemoryMB = sqlite3_column_int(stmt, 6);
            info.hardwareConfig.systemMemoryMB = sqlite3_column_int(stmt, 7);
            info.hardwareConfig.driverVersion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            info.hardwareConfig.operatingSystem = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            info.qualityPreset = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            info.resolution = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        }
        sqlite3_finalize(stmt);
    }

    return info;
}

std::vector<SessionInfo> PerformanceDatabase::GetAllSessions(int limit) {
    std::vector<SessionInfo> sessions;
    if (!m_db) return sessions;

    std::ostringstream sql;
    sql << "SELECT session_id FROM Sessions ORDER BY start_time DESC LIMIT " << limit;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int sessionId = sqlite3_column_int(stmt, 0);
            sessions.push_back(GetSessionInfo(sessionId));
        }
        sqlite3_finalize(stmt);
    }

    return sessions;
}

std::vector<SessionInfo> PerformanceDatabase::GetRecentSessions(int count) {
    return GetAllSessions(count);
}

void PerformanceDatabase::BeginBatch() {
    if (!m_db || m_inBatch) return;

    ExecuteSQL("BEGIN TRANSACTION");
    m_inBatch = true;

    m_frameBuffer.clear();
    m_stageBuffer.clear();
    m_memoryBuffer.clear();
    m_gpuBuffer.clear();
    m_cpuBuffer.clear();
    m_statsBuffer.clear();
}

void PerformanceDatabase::EndBatch() {
    if (!m_db || !m_inBatch) return;

    FlushBatch();
    ExecuteSQL("COMMIT");
    m_inBatch = false;
}

void PerformanceDatabase::FlushBatch() {
    if (!m_db) return;

    FlushFrameBuffer();
    FlushStageBuffer();
    FlushMemoryBuffer();
    FlushGPUBuffer();
    FlushCPUBuffer();
    FlushStatsBuffer();
}

int PerformanceDatabase::RecordFrame(int sessionId, const FrameData& frame) {
    if (!m_db) return -1;

    if (m_inBatch) {
        FrameData bufferedFrame = frame;
        bufferedFrame.sessionId = sessionId;
        m_frameBuffer.push_back(bufferedFrame);

        if (m_frameBuffer.size() >= BATCH_SIZE) {
            FlushFrameBuffer();
        }

        return -1; // Frame ID not available in batch mode
    }

    // Direct insert
    sqlite3_reset(m_insertFrameStmt);
    sqlite3_bind_int(m_insertFrameStmt, 1, sessionId);
    sqlite3_bind_int(m_insertFrameStmt, 2, frame.frameNumber);
    sqlite3_bind_double(m_insertFrameStmt, 3, frame.timestamp);
    sqlite3_bind_double(m_insertFrameStmt, 4, frame.totalTimeMs);
    sqlite3_bind_double(m_insertFrameStmt, 5, frame.fps);
    sqlite3_bind_int(m_insertFrameStmt, 6, frame.vsyncEnabled ? 1 : 0);

    if (sqlite3_step(m_insertFrameStmt) != SQLITE_DONE) {
        SetError(std::string("Failed to insert frame: ") + sqlite3_errmsg(m_db));
        return -1;
    }

    return GetLastInsertRowId();
}

void PerformanceDatabase::RecordStage(int frameId, const StageData& stage) {
    if (!m_db) return;

    if (m_inBatch) {
        StageData bufferedStage = stage;
        bufferedStage.frameId = frameId;
        m_stageBuffer.push_back(bufferedStage);

        if (m_stageBuffer.size() >= BATCH_SIZE) {
            FlushStageBuffer();
        }
        return;
    }

    // Direct insert
    sqlite3_reset(m_insertStageStmt);
    sqlite3_bind_int(m_insertStageStmt, 1, frameId);
    sqlite3_bind_text(m_insertStageStmt, 2, stage.stageName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(m_insertStageStmt, 3, stage.timeMs);
    sqlite3_bind_double(m_insertStageStmt, 4, stage.percentage);
    sqlite3_bind_double(m_insertStageStmt, 5, stage.gpuTimeMs);
    sqlite3_bind_double(m_insertStageStmt, 6, stage.cpuTimeMs);

    sqlite3_step(m_insertStageStmt);
}

void PerformanceDatabase::RecordMemory(int frameId, const MemoryData& memory) {
    if (!m_db) return;

    if (m_inBatch) {
        MemoryData bufferedMemory = memory;
        bufferedMemory.frameId = frameId;
        m_memoryBuffer.push_back(bufferedMemory);

        if (m_memoryBuffer.size() >= BATCH_SIZE) {
            FlushMemoryBuffer();
        }
        return;
    }

    // Direct insert
    sqlite3_reset(m_insertMemoryStmt);
    sqlite3_bind_int(m_insertMemoryStmt, 1, frameId);
    sqlite3_bind_double(m_insertMemoryStmt, 2, memory.cpuUsedMB);
    sqlite3_bind_double(m_insertMemoryStmt, 3, memory.cpuAvailableMB);
    sqlite3_bind_double(m_insertMemoryStmt, 4, memory.gpuUsedMB);
    sqlite3_bind_double(m_insertMemoryStmt, 5, memory.gpuAvailableMB);

    sqlite3_step(m_insertMemoryStmt);
}

void PerformanceDatabase::RecordGPU(int frameId, const GPUData& gpu) {
    if (!m_db) return;

    if (m_inBatch) {
        GPUData bufferedGPU = gpu;
        bufferedGPU.frameId = frameId;
        m_gpuBuffer.push_back(bufferedGPU);

        if (m_gpuBuffer.size() >= BATCH_SIZE) {
            FlushGPUBuffer();
        }
        return;
    }

    // Direct insert
    sqlite3_reset(m_insertGPUStmt);
    sqlite3_bind_int(m_insertGPUStmt, 1, frameId);
    sqlite3_bind_double(m_insertGPUStmt, 2, gpu.utilizationPercent);
    sqlite3_bind_double(m_insertGPUStmt, 3, gpu.temperatureCelsius);
    sqlite3_bind_int(m_insertGPUStmt, 4, gpu.clockMHz);
    sqlite3_bind_int(m_insertGPUStmt, 5, gpu.memoryClockMHz);

    sqlite3_step(m_insertGPUStmt);
}

void PerformanceDatabase::RecordCPU(int frameId, const CPUData& cpu) {
    if (!m_db) return;

    if (m_inBatch) {
        CPUData bufferedCPU = cpu;
        bufferedCPU.frameId = frameId;
        m_cpuBuffer.push_back(bufferedCPU);

        if (m_cpuBuffer.size() >= BATCH_SIZE) {
            FlushCPUBuffer();
        }
        return;
    }

    // Direct insert
    sqlite3_reset(m_insertCPUStmt);
    sqlite3_bind_int(m_insertCPUStmt, 1, frameId);
    sqlite3_bind_int(m_insertCPUStmt, 2, cpu.coreCount);
    sqlite3_bind_double(m_insertCPUStmt, 3, cpu.utilizationPercent);
    sqlite3_bind_double(m_insertCPUStmt, 4, cpu.temperatureCelsius);
    sqlite3_bind_int(m_insertCPUStmt, 5, cpu.clockMHz);

    sqlite3_step(m_insertCPUStmt);
}

void PerformanceDatabase::RecordRenderingStats(int frameId, const RenderingStats& stats) {
    if (!m_db) return;

    if (m_inBatch) {
        RenderingStats bufferedStats = stats;
        bufferedStats.frameId = frameId;
        m_statsBuffer.push_back(bufferedStats);

        if (m_statsBuffer.size() >= BATCH_SIZE) {
            FlushStatsBuffer();
        }
        return;
    }

    // Direct insert
    sqlite3_reset(m_insertStatsStmt);
    sqlite3_bind_int(m_insertStatsStmt, 1, frameId);
    sqlite3_bind_int(m_insertStatsStmt, 2, stats.drawCalls);
    sqlite3_bind_int(m_insertStatsStmt, 3, stats.triangles);
    sqlite3_bind_int(m_insertStatsStmt, 4, stats.vertices);
    sqlite3_bind_int(m_insertStatsStmt, 5, stats.instances);
    sqlite3_bind_int(m_insertStatsStmt, 6, stats.lights);
    sqlite3_bind_int(m_insertStatsStmt, 7, stats.shadowMaps);

    sqlite3_step(m_insertStatsStmt);
}

int PerformanceDatabase::RecordCompleteFrame(int sessionId, const FrameData& frame,
                                             const std::vector<StageData>& stages,
                                             const MemoryData& memory,
                                             const GPUData& gpu,
                                             const CPUData& cpu,
                                             const RenderingStats& stats) {
    int frameId = RecordFrame(sessionId, frame);
    if (frameId < 0 && !m_inBatch) return -1;

    // In batch mode, we need to track the frame ID differently
    // For simplicity, we'll use a counter
    static int batchFrameIdCounter = 1000000; // Start high to avoid conflicts
    if (m_inBatch) {
        frameId = batchFrameIdCounter++;
    }

    for (const auto& stage : stages) {
        RecordStage(frameId, stage);
    }

    RecordMemory(frameId, memory);
    RecordGPU(frameId, gpu);
    RecordCPU(frameId, cpu);
    RecordRenderingStats(frameId, stats);

    return frameId;
}

void PerformanceDatabase::FlushFrameBuffer() {
    if (m_frameBuffer.empty()) return;

    for (const auto& frame : m_frameBuffer) {
        sqlite3_reset(m_insertFrameStmt);
        sqlite3_bind_int(m_insertFrameStmt, 1, frame.sessionId);
        sqlite3_bind_int(m_insertFrameStmt, 2, frame.frameNumber);
        sqlite3_bind_double(m_insertFrameStmt, 3, frame.timestamp);
        sqlite3_bind_double(m_insertFrameStmt, 4, frame.totalTimeMs);
        sqlite3_bind_double(m_insertFrameStmt, 5, frame.fps);
        sqlite3_bind_int(m_insertFrameStmt, 6, frame.vsyncEnabled ? 1 : 0);
        sqlite3_step(m_insertFrameStmt);
    }

    m_frameBuffer.clear();
}

void PerformanceDatabase::FlushStageBuffer() {
    if (m_stageBuffer.empty()) return;

    for (const auto& stage : m_stageBuffer) {
        sqlite3_reset(m_insertStageStmt);
        sqlite3_bind_int(m_insertStageStmt, 1, stage.frameId);
        sqlite3_bind_text(m_insertStageStmt, 2, stage.stageName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(m_insertStageStmt, 3, stage.timeMs);
        sqlite3_bind_double(m_insertStageStmt, 4, stage.percentage);
        sqlite3_bind_double(m_insertStageStmt, 5, stage.gpuTimeMs);
        sqlite3_bind_double(m_insertStageStmt, 6, stage.cpuTimeMs);
        sqlite3_step(m_insertStageStmt);
    }

    m_stageBuffer.clear();
}

void PerformanceDatabase::FlushMemoryBuffer() {
    if (m_memoryBuffer.empty()) return;

    for (const auto& memory : m_memoryBuffer) {
        sqlite3_reset(m_insertMemoryStmt);
        sqlite3_bind_int(m_insertMemoryStmt, 1, memory.frameId);
        sqlite3_bind_double(m_insertMemoryStmt, 2, memory.cpuUsedMB);
        sqlite3_bind_double(m_insertMemoryStmt, 3, memory.cpuAvailableMB);
        sqlite3_bind_double(m_insertMemoryStmt, 4, memory.gpuUsedMB);
        sqlite3_bind_double(m_insertMemoryStmt, 5, memory.gpuAvailableMB);
        sqlite3_step(m_insertMemoryStmt);
    }

    m_memoryBuffer.clear();
}

void PerformanceDatabase::FlushGPUBuffer() {
    if (m_gpuBuffer.empty()) return;

    for (const auto& gpu : m_gpuBuffer) {
        sqlite3_reset(m_insertGPUStmt);
        sqlite3_bind_int(m_insertGPUStmt, 1, gpu.frameId);
        sqlite3_bind_double(m_insertGPUStmt, 2, gpu.utilizationPercent);
        sqlite3_bind_double(m_insertGPUStmt, 3, gpu.temperatureCelsius);
        sqlite3_bind_int(m_insertGPUStmt, 4, gpu.clockMHz);
        sqlite3_bind_int(m_insertGPUStmt, 5, gpu.memoryClockMHz);
        sqlite3_step(m_insertGPUStmt);
    }

    m_gpuBuffer.clear();
}

void PerformanceDatabase::FlushCPUBuffer() {
    if (m_cpuBuffer.empty()) return;

    for (const auto& cpu : m_cpuBuffer) {
        sqlite3_reset(m_insertCPUStmt);
        sqlite3_bind_int(m_insertCPUStmt, 1, cpu.frameId);
        sqlite3_bind_int(m_insertCPUStmt, 2, cpu.coreCount);
        sqlite3_bind_double(m_insertCPUStmt, 3, cpu.utilizationPercent);
        sqlite3_bind_double(m_insertCPUStmt, 4, cpu.temperatureCelsius);
        sqlite3_bind_int(m_insertCPUStmt, 5, cpu.clockMHz);
        sqlite3_step(m_insertCPUStmt);
    }

    m_cpuBuffer.clear();
}

void PerformanceDatabase::FlushStatsBuffer() {
    if (m_statsBuffer.empty()) return;

    for (const auto& stats : m_statsBuffer) {
        sqlite3_reset(m_insertStatsStmt);
        sqlite3_bind_int(m_insertStatsStmt, 1, stats.frameId);
        sqlite3_bind_int(m_insertStatsStmt, 2, stats.drawCalls);
        sqlite3_bind_int(m_insertStatsStmt, 3, stats.triangles);
        sqlite3_bind_int(m_insertStatsStmt, 4, stats.vertices);
        sqlite3_bind_int(m_insertStatsStmt, 5, stats.instances);
        sqlite3_bind_int(m_insertStatsStmt, 6, stats.lights);
        sqlite3_bind_int(m_insertStatsStmt, 7, stats.shadowMaps);
        sqlite3_step(m_insertStatsStmt);
    }

    m_statsBuffer.clear();
}

std::vector<FrameData> PerformanceDatabase::GetFrames(int sessionId, int limit, int offset) {
    std::vector<FrameData> frames;
    if (!m_db) return frames;

    std::ostringstream sql;
    sql << "SELECT frame_id, session_id, frame_number, timestamp, total_time_ms, fps, vsync_enabled "
        << "FROM FrameData WHERE session_id = " << sessionId
        << " ORDER BY frame_number LIMIT " << limit << " OFFSET " << offset;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FrameData frame;
            frame.frameId = sqlite3_column_int(stmt, 0);
            frame.sessionId = sqlite3_column_int(stmt, 1);
            frame.frameNumber = sqlite3_column_int(stmt, 2);
            frame.timestamp = sqlite3_column_double(stmt, 3);
            frame.totalTimeMs = sqlite3_column_double(stmt, 4);
            frame.fps = sqlite3_column_double(stmt, 5);
            frame.vsyncEnabled = sqlite3_column_int(stmt, 6) != 0;
            frames.push_back(frame);
        }
        sqlite3_finalize(stmt);
    }

    return frames;
}

FrameStatistics PerformanceDatabase::GetStatistics(int sessionId) {
    FrameStatistics stats;
    if (!m_db) return stats;

    // Get basic statistics
    std::ostringstream sql;
    sql << "SELECT COUNT(*), AVG(fps), MIN(fps), MAX(fps), AVG(total_time_ms) "
        << "FROM FrameData WHERE session_id = " << sessionId;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalFrames = sqlite3_column_int(stmt, 0);
            stats.avgFPS = sqlite3_column_double(stmt, 1);
            stats.minFPS = sqlite3_column_double(stmt, 2);
            stats.maxFPS = sqlite3_column_double(stmt, 3);
            stats.avgFrameTime = sqlite3_column_double(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }

    // Get percentiles
    if (stats.totalFrames > 0) {
        int p50_idx = stats.totalFrames / 2;
        int p95_idx = static_cast<int>(stats.totalFrames * 0.95);
        int p99_idx = static_cast<int>(stats.totalFrames * 0.99);

        sql.str("");
        sql << "SELECT total_time_ms FROM FrameData WHERE session_id = " << sessionId
            << " ORDER BY total_time_ms LIMIT 1 OFFSET " << p50_idx;

        if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                stats.p50FrameTime = sqlite3_column_double(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        sql.str("");
        sql << "SELECT total_time_ms FROM FrameData WHERE session_id = " << sessionId
            << " ORDER BY total_time_ms LIMIT 1 OFFSET " << p95_idx;

        if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                stats.p95FrameTime = sqlite3_column_double(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        sql.str("");
        sql << "SELECT total_time_ms FROM FrameData WHERE session_id = " << sessionId
            << " ORDER BY total_time_ms LIMIT 1 OFFSET " << p99_idx;

        if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                stats.p99FrameTime = sqlite3_column_double(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }

    return stats;
}

std::vector<std::string> PerformanceDatabase::GetBottleneckStages(int sessionId, float thresholdPercent) {
    std::vector<std::string> bottlenecks;
    if (!m_db) return bottlenecks;

    std::ostringstream sql;
    sql << "SELECT DISTINCT s.stage_name, AVG(s.percentage) as avg_pct "
        << "FROM StageData s "
        << "JOIN FrameData f ON s.frame_id = f.frame_id "
        << "WHERE f.session_id = " << sessionId
        << " GROUP BY s.stage_name "
        << "HAVING avg_pct > " << thresholdPercent
        << " ORDER BY avg_pct DESC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            bottlenecks.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }

    return bottlenecks;
}

std::vector<int> PerformanceDatabase::FindFrameSpikes(int sessionId, float multiplier) {
    std::vector<int> spikes;
    if (!m_db) return spikes;

    // First get average frame time
    float avgFrameTime = GetStatistics(sessionId).avgFrameTime;
    float threshold = avgFrameTime * multiplier;

    std::ostringstream sql;
    sql << "SELECT frame_number FROM FrameData "
        << "WHERE session_id = " << sessionId
        << " AND total_time_ms > " << threshold
        << " ORDER BY total_time_ms DESC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            spikes.push_back(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    return spikes;
}

void PerformanceDatabase::VacuumDatabase() {
    if (!m_db) return;
    ExecuteSQL("VACUUM");
}

void PerformanceDatabase::DeleteOldSessions(int daysToKeep) {
    if (!m_db) return;

    std::ostringstream sql;
    sql << "DELETE FROM Sessions WHERE start_time < datetime('now', '-" << daysToKeep << " days')";
    ExecuteSQL(sql.str());
}

void PerformanceDatabase::DeleteSession(int sessionId) {
    if (!m_db) return;

    std::ostringstream sql;
    sql << "DELETE FROM Sessions WHERE session_id = " << sessionId;
    ExecuteSQL(sql.str());
}

bool PerformanceDatabase::ExportSessionToCSV(int sessionId, const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) return false;

    // Write header
    file << "FrameNumber,Timestamp,TotalTimeMs,FPS,VSync\n";

    // Get frames
    auto frames = GetFrames(sessionId, 100000);
    for (const auto& frame : frames) {
        file << frame.frameNumber << ","
             << frame.timestamp << ","
             << frame.totalTimeMs << ","
             << frame.fps << ","
             << (frame.vsyncEnabled ? 1 : 0) << "\n";
    }

    file.close();
    return true;
}

bool PerformanceDatabase::ExecuteSQL(const std::string& sql) {
    if (!m_db) return false;

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        if (errMsg) {
            SetError(std::string("SQL error: ") + errMsg);
            sqlite3_free(errMsg);
        }
        return false;
    }

    return true;
}

int PerformanceDatabase::GetLastInsertRowId() {
    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

void PerformanceDatabase::SetError(const std::string& error) {
    m_lastError = error;
}

} // namespace Profiling
} // namespace Engine
