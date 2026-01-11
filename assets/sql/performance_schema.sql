-- ============================================================================
-- Performance Monitoring Database Schema
-- Version: 1.0
-- Description: SQLite schema for comprehensive performance metrics tracking
-- ============================================================================

-- Enable foreign key constraints
PRAGMA foreign_keys = ON;

-- Enable WAL mode for better concurrent access
PRAGMA journal_mode = WAL;

-- ============================================================================
-- Schema Version Table
-- ============================================================================
CREATE TABLE IF NOT EXISTS SchemaVersion (
    version INTEGER PRIMARY KEY,
    applied_date DATETIME DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);

INSERT OR IGNORE INTO SchemaVersion (version, description)
VALUES (1, 'Initial schema with comprehensive performance tracking');

-- ============================================================================
-- Sessions Table
-- Tracks recording sessions with hardware configuration
-- ============================================================================
CREATE TABLE IF NOT EXISTS Sessions (
    session_id INTEGER PRIMARY KEY AUTOINCREMENT,
    start_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    end_time DATETIME,

    -- Hardware configuration
    cpu_model TEXT NOT NULL,
    cpu_cores INTEGER NOT NULL,
    gpu_model TEXT NOT NULL,
    gpu_memory_mb INTEGER NOT NULL,
    system_memory_mb INTEGER NOT NULL,
    driver_version TEXT,
    operating_system TEXT,

    -- Session configuration
    quality_preset TEXT NOT NULL,
    resolution TEXT NOT NULL,

    -- Session metadata
    notes TEXT,
    tags TEXT,  -- Comma-separated tags for categorization

    CONSTRAINT valid_cpu_cores CHECK (cpu_cores > 0),
    CONSTRAINT valid_gpu_memory CHECK (gpu_memory_mb > 0),
    CONSTRAINT valid_system_memory CHECK (system_memory_mb > 0)
);

-- ============================================================================
-- FrameData Table
-- Stores per-frame performance metrics
-- ============================================================================
CREATE TABLE IF NOT EXISTS FrameData (
    frame_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    frame_number INTEGER NOT NULL,
    timestamp REAL NOT NULL,  -- Seconds since session start
    total_time_ms REAL NOT NULL,
    fps REAL NOT NULL,
    vsync_enabled INTEGER NOT NULL DEFAULT 0,  -- Boolean: 0 = false, 1 = true

    FOREIGN KEY (session_id) REFERENCES Sessions(session_id) ON DELETE CASCADE,

    CONSTRAINT valid_frame_number CHECK (frame_number >= 0),
    CONSTRAINT valid_timestamp CHECK (timestamp >= 0),
    CONSTRAINT valid_total_time CHECK (total_time_ms >= 0),
    CONSTRAINT valid_fps CHECK (fps >= 0),
    CONSTRAINT unique_frame_per_session UNIQUE (session_id, frame_number)
);

-- ============================================================================
-- StageData Table
-- Stores timing for individual rendering stages
-- ============================================================================
CREATE TABLE IF NOT EXISTS StageData (
    stage_id INTEGER PRIMARY KEY AUTOINCREMENT,
    frame_id INTEGER NOT NULL,
    stage_name TEXT NOT NULL,
    time_ms REAL NOT NULL,
    percentage REAL NOT NULL,
    gpu_time_ms REAL NOT NULL,
    cpu_time_ms REAL NOT NULL,

    FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE,

    CONSTRAINT valid_time CHECK (time_ms >= 0),
    CONSTRAINT valid_percentage CHECK (percentage >= 0 AND percentage <= 100),
    CONSTRAINT valid_gpu_time CHECK (gpu_time_ms >= 0),
    CONSTRAINT valid_cpu_time CHECK (cpu_time_ms >= 0)
);

-- ============================================================================
-- MemoryData Table
-- Tracks CPU and GPU memory usage
-- ============================================================================
CREATE TABLE IF NOT EXISTS MemoryData (
    memory_id INTEGER PRIMARY KEY AUTOINCREMENT,
    frame_id INTEGER NOT NULL,
    cpu_used_mb REAL NOT NULL,
    cpu_available_mb REAL NOT NULL,
    gpu_used_mb REAL NOT NULL,
    gpu_available_mb REAL NOT NULL,

    FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE,

    CONSTRAINT valid_cpu_used CHECK (cpu_used_mb >= 0),
    CONSTRAINT valid_cpu_available CHECK (cpu_available_mb >= 0),
    CONSTRAINT valid_gpu_used CHECK (gpu_used_mb >= 0),
    CONSTRAINT valid_gpu_available CHECK (gpu_available_mb >= 0),
    CONSTRAINT unique_memory_per_frame UNIQUE (frame_id)
);

-- ============================================================================
-- GPUData Table
-- Stores GPU hardware metrics
-- ============================================================================
CREATE TABLE IF NOT EXISTS GPUData (
    gpu_id INTEGER PRIMARY KEY AUTOINCREMENT,
    frame_id INTEGER NOT NULL,
    utilization_percent REAL NOT NULL,
    temperature_celsius REAL NOT NULL,
    clock_mhz INTEGER NOT NULL,
    memory_clock_mhz INTEGER NOT NULL,

    FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE,

    CONSTRAINT valid_utilization CHECK (utilization_percent >= 0 AND utilization_percent <= 100),
    CONSTRAINT valid_temperature CHECK (temperature_celsius >= 0 AND temperature_celsius <= 200),
    CONSTRAINT valid_clock CHECK (clock_mhz >= 0),
    CONSTRAINT valid_memory_clock CHECK (memory_clock_mhz >= 0),
    CONSTRAINT unique_gpu_per_frame UNIQUE (frame_id)
);

-- ============================================================================
-- CPUData Table
-- Stores CPU hardware metrics
-- ============================================================================
CREATE TABLE IF NOT EXISTS CPUData (
    cpu_id INTEGER PRIMARY KEY AUTOINCREMENT,
    frame_id INTEGER NOT NULL,
    core_count INTEGER NOT NULL,
    utilization_percent REAL NOT NULL,
    temperature_celsius REAL NOT NULL,
    clock_mhz INTEGER NOT NULL,

    FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE,

    CONSTRAINT valid_core_count CHECK (core_count > 0),
    CONSTRAINT valid_cpu_utilization CHECK (utilization_percent >= 0 AND utilization_percent <= 100),
    CONSTRAINT valid_cpu_temperature CHECK (temperature_celsius >= 0 AND temperature_celsius <= 200),
    CONSTRAINT valid_cpu_clock CHECK (clock_mhz >= 0),
    CONSTRAINT unique_cpu_per_frame UNIQUE (frame_id)
);

-- ============================================================================
-- RenderingStats Table
-- Stores rendering statistics (draw calls, triangles, etc.)
-- ============================================================================
CREATE TABLE IF NOT EXISTS RenderingStats (
    stats_id INTEGER PRIMARY KEY AUTOINCREMENT,
    frame_id INTEGER NOT NULL,
    draw_calls INTEGER NOT NULL,
    triangles INTEGER NOT NULL,
    vertices INTEGER NOT NULL,
    instances INTEGER NOT NULL,
    lights INTEGER NOT NULL,
    shadow_maps INTEGER NOT NULL,

    FOREIGN KEY (frame_id) REFERENCES FrameData(frame_id) ON DELETE CASCADE,

    CONSTRAINT valid_draw_calls CHECK (draw_calls >= 0),
    CONSTRAINT valid_triangles CHECK (triangles >= 0),
    CONSTRAINT valid_vertices CHECK (vertices >= 0),
    CONSTRAINT valid_instances CHECK (instances >= 0),
    CONSTRAINT valid_lights CHECK (lights >= 0),
    CONSTRAINT valid_shadow_maps CHECK (shadow_maps >= 0),
    CONSTRAINT unique_stats_per_frame UNIQUE (frame_id)
);

-- ============================================================================
-- Indices for Performance Optimization
-- ============================================================================

-- Session indices
CREATE INDEX IF NOT EXISTS idx_sessions_start_time ON Sessions(start_time);
CREATE INDEX IF NOT EXISTS idx_sessions_preset ON Sessions(quality_preset);

-- Frame data indices
CREATE INDEX IF NOT EXISTS idx_frame_session ON FrameData(session_id);
CREATE INDEX IF NOT EXISTS idx_frame_number ON FrameData(frame_number);
CREATE INDEX IF NOT EXISTS idx_frame_timestamp ON FrameData(timestamp);
CREATE INDEX IF NOT EXISTS idx_frame_fps ON FrameData(fps);
CREATE INDEX IF NOT EXISTS idx_frame_time ON FrameData(total_time_ms);

-- Stage data indices
CREATE INDEX IF NOT EXISTS idx_stage_frame ON StageData(frame_id);
CREATE INDEX IF NOT EXISTS idx_stage_name ON StageData(stage_name);
CREATE INDEX IF NOT EXISTS idx_stage_time ON StageData(time_ms);
CREATE INDEX IF NOT EXISTS idx_stage_percentage ON StageData(percentage);

-- Memory data indices
CREATE INDEX IF NOT EXISTS idx_memory_frame ON MemoryData(frame_id);

-- GPU data indices
CREATE INDEX IF NOT EXISTS idx_gpu_frame ON GPUData(frame_id);
CREATE INDEX IF NOT EXISTS idx_gpu_utilization ON GPUData(utilization_percent);

-- CPU data indices
CREATE INDEX IF NOT EXISTS idx_cpu_frame ON CPUData(frame_id);
CREATE INDEX IF NOT EXISTS idx_cpu_utilization ON CPUData(utilization_percent);

-- Rendering stats indices
CREATE INDEX IF NOT EXISTS idx_stats_frame ON RenderingStats(frame_id);

-- ============================================================================
-- Views for Common Queries
-- ============================================================================

-- Complete frame information with all metrics
CREATE VIEW IF NOT EXISTS v_FrameComplete AS
SELECT
    f.frame_id,
    f.session_id,
    f.frame_number,
    f.timestamp,
    f.total_time_ms,
    f.fps,
    f.vsync_enabled,
    m.cpu_used_mb,
    m.cpu_available_mb,
    m.gpu_used_mb,
    m.gpu_available_mb,
    g.utilization_percent AS gpu_utilization,
    g.temperature_celsius AS gpu_temperature,
    g.clock_mhz AS gpu_clock,
    c.utilization_percent AS cpu_utilization,
    c.temperature_celsius AS cpu_temperature,
    c.clock_mhz AS cpu_clock,
    r.draw_calls,
    r.triangles,
    r.vertices,
    r.instances,
    r.lights,
    r.shadow_maps
FROM FrameData f
LEFT JOIN MemoryData m ON f.frame_id = m.frame_id
LEFT JOIN GPUData g ON f.frame_id = g.frame_id
LEFT JOIN CPUData c ON f.frame_id = c.frame_id
LEFT JOIN RenderingStats r ON f.frame_id = r.frame_id;

-- Session summary with statistics
CREATE VIEW IF NOT EXISTS v_SessionSummary AS
SELECT
    s.session_id,
    s.start_time,
    s.end_time,
    s.quality_preset,
    s.resolution,
    COUNT(f.frame_id) AS total_frames,
    AVG(f.fps) AS avg_fps,
    MIN(f.fps) AS min_fps,
    MAX(f.fps) AS max_fps,
    AVG(f.total_time_ms) AS avg_frame_time,
    MAX(f.total_time_ms) AS max_frame_time,
    AVG(m.gpu_used_mb) AS avg_gpu_memory,
    MAX(m.gpu_used_mb) AS peak_gpu_memory
FROM Sessions s
LEFT JOIN FrameData f ON s.session_id = f.session_id
LEFT JOIN MemoryData m ON f.frame_id = m.frame_id
GROUP BY s.session_id;

-- Stage performance summary per session
CREATE VIEW IF NOT EXISTS v_StagePerformance AS
SELECT
    f.session_id,
    s.stage_name,
    COUNT(*) AS occurrence_count,
    AVG(s.time_ms) AS avg_time_ms,
    MIN(s.time_ms) AS min_time_ms,
    MAX(s.time_ms) AS max_time_ms,
    AVG(s.percentage) AS avg_percentage,
    AVG(s.gpu_time_ms) AS avg_gpu_time,
    AVG(s.cpu_time_ms) AS avg_cpu_time
FROM StageData s
JOIN FrameData f ON s.frame_id = f.frame_id
GROUP BY f.session_id, s.stage_name;

-- ============================================================================
-- Triggers for Data Integrity
-- ============================================================================

-- Auto-update end_time when inserting final frame
CREATE TRIGGER IF NOT EXISTS trg_update_session_end_time
AFTER INSERT ON FrameData
BEGIN
    UPDATE Sessions
    SET end_time = CURRENT_TIMESTAMP
    WHERE session_id = NEW.session_id;
END;

-- ============================================================================
-- Maintenance Procedures (Comment blocks for reference)
-- ============================================================================

-- To delete sessions older than N days:
-- DELETE FROM Sessions WHERE start_time < datetime('now', '-N days');

-- To vacuum and optimize database:
-- VACUUM;
-- ANALYZE;

-- To get database size:
-- SELECT page_count * page_size as size FROM pragma_page_count(), pragma_page_size();

-- ============================================================================
-- Example Queries
-- ============================================================================

-- Get average FPS for a session:
-- SELECT AVG(fps) FROM FrameData WHERE session_id = ?;

-- Get frame time percentiles:
-- SELECT
--     MIN(total_time_ms) AS min,
--     MAX(total_time_ms) AS max,
--     (SELECT total_time_ms FROM FrameData WHERE session_id = ? ORDER BY total_time_ms LIMIT 1 OFFSET (SELECT COUNT(*) FROM FrameData WHERE session_id = ?) * 0.50) AS p50,
--     (SELECT total_time_ms FROM FrameData WHERE session_id = ? ORDER BY total_time_ms LIMIT 1 OFFSET (SELECT COUNT(*) FROM FrameData WHERE session_id = ?) * 0.95) AS p95
-- FROM FrameData WHERE session_id = ?;

-- Find bottleneck stages (>20% of frame time):
-- SELECT stage_name, AVG(percentage) AS avg_pct
-- FROM StageData s
-- JOIN FrameData f ON s.frame_id = f.frame_id
-- WHERE f.session_id = ?
-- GROUP BY stage_name
-- HAVING avg_pct > 20
-- ORDER BY avg_pct DESC;

-- Find frame spikes (>2x average):
-- SELECT frame_number, total_time_ms
-- FROM FrameData
-- WHERE session_id = ?
-- AND total_time_ms > (SELECT AVG(total_time_ms) * 2 FROM FrameData WHERE session_id = ?)
-- ORDER BY total_time_ms DESC;

-- Get memory trend:
-- SELECT frame_number, gpu_used_mb, cpu_used_mb
-- FROM FrameData f
-- JOIN MemoryData m ON f.frame_id = m.frame_id
-- WHERE f.session_id = ?
-- ORDER BY frame_number;

-- ============================================================================
-- End of Schema
-- ============================================================================
