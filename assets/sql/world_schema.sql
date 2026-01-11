-- ============================================================================
-- Nova Engine - World Persistence Database Schema
-- ============================================================================
-- SQLite database schema for privately hosted game servers
-- Supports: World state, chunks, entities, players, buildings, quests
-- Features: Spatial indexing, versioning, change tracking, backups
-- ============================================================================

-- ============================================================================
-- WORLD METADATA
-- ============================================================================

CREATE TABLE IF NOT EXISTS WorldMeta (
    world_id INTEGER PRIMARY KEY AUTOINCREMENT,
    world_name TEXT NOT NULL UNIQUE,
    world_description TEXT,
    seed INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_saved DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
    schema_version INTEGER DEFAULT 1,
    world_size_x INTEGER DEFAULT 1000,
    world_size_y INTEGER DEFAULT 256,
    world_size_z INTEGER DEFAULT 1000,
    spawn_x REAL DEFAULT 0.0,
    spawn_y REAL DEFAULT 100.0,
    spawn_z REAL DEFAULT 0.0,
    game_time REAL DEFAULT 0.0,        -- In-game time (seconds)
    real_play_time INTEGER DEFAULT 0,   -- Real world playtime (seconds)
    difficulty TEXT DEFAULT 'normal',
    game_mode TEXT DEFAULT 'survival',  -- survival, creative, adventure
    custom_data TEXT,                   -- JSON for additional settings
    is_active BOOLEAN DEFAULT 1
);

CREATE INDEX IF NOT EXISTS idx_world_active ON WorldMeta(is_active);
CREATE INDEX IF NOT EXISTS idx_world_name ON WorldMeta(world_name);

-- ============================================================================
-- CHUNK STORAGE (Terrain Data)
-- ============================================================================

CREATE TABLE IF NOT EXISTS Chunks (
    chunk_id INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id INTEGER NOT NULL,
    chunk_x INTEGER NOT NULL,
    chunk_y INTEGER NOT NULL,
    chunk_z INTEGER NOT NULL,
    data BLOB,                          -- Compressed chunk terrain data
    biome_data BLOB,                    -- Biome information
    lighting_data BLOB,                 -- Cached lighting
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_generated BOOLEAN DEFAULT 0,
    is_populated BOOLEAN DEFAULT 0,     -- Trees, structures added
    is_dirty BOOLEAN DEFAULT 0,         -- Needs saving
    load_priority INTEGER DEFAULT 0,    -- Higher = load first
    access_count INTEGER DEFAULT 0,     -- For LRU caching
    last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
    checksum TEXT,                      -- For integrity checking
    compression_type TEXT DEFAULT 'zlib', -- zlib, lz4, none
    uncompressed_size INTEGER DEFAULT 0,
    UNIQUE(world_id, chunk_x, chunk_y, chunk_z),
    FOREIGN KEY (world_id) REFERENCES WorldMeta(world_id) ON DELETE CASCADE
);

-- Spatial index for chunk queries
CREATE INDEX IF NOT EXISTS idx_chunk_spatial ON Chunks(world_id, chunk_x, chunk_y, chunk_z);
CREATE INDEX IF NOT EXISTS idx_chunk_modified ON Chunks(modified_at);
CREATE INDEX IF NOT EXISTS idx_chunk_dirty ON Chunks(is_dirty);
CREATE INDEX IF NOT EXISTS idx_chunk_priority ON Chunks(load_priority DESC);
CREATE INDEX IF NOT EXISTS idx_chunk_access ON Chunks(last_accessed);

-- ============================================================================
-- ENTITIES (Players, NPCs, Buildings, Items, Projectiles)
-- ============================================================================

CREATE TABLE IF NOT EXISTS Entities (
    entity_id INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id INTEGER NOT NULL,
    entity_type TEXT NOT NULL,          -- 'player', 'npc', 'building', 'item', 'projectile'
    entity_subtype TEXT,                -- Specific type (e.g., 'zombie', 'arrow', 'chest')
    entity_uuid TEXT UNIQUE NOT NULL,   -- UUID for network sync
    chunk_x INTEGER NOT NULL,
    chunk_y INTEGER NOT NULL,
    chunk_z INTEGER NOT NULL,
    position_x REAL NOT NULL,
    position_y REAL NOT NULL,
    position_z REAL NOT NULL,
    rotation_x REAL DEFAULT 0.0,        -- Quaternion
    rotation_y REAL DEFAULT 0.0,
    rotation_z REAL DEFAULT 0.0,
    rotation_w REAL DEFAULT 1.0,
    velocity_x REAL DEFAULT 0.0,
    velocity_y REAL DEFAULT 0.0,
    velocity_z REAL DEFAULT 0.0,
    scale_x REAL DEFAULT 1.0,
    scale_y REAL DEFAULT 1.0,
    scale_z REAL DEFAULT 1.0,
    data BLOB,                          -- Serialized component data
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT 1,
    is_static BOOLEAN DEFAULT 0,        -- Static entities don't move
    owner_player_id INTEGER,            -- For owned entities
    health REAL,
    max_health REAL,
    flags INTEGER DEFAULT 0,            -- Bitfield for various flags
    FOREIGN KEY (world_id) REFERENCES WorldMeta(world_id) ON DELETE CASCADE,
    FOREIGN KEY (owner_player_id) REFERENCES Players(player_id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_entity_world ON Entities(world_id);
CREATE INDEX IF NOT EXISTS idx_entity_type ON Entities(entity_type);
CREATE INDEX IF NOT EXISTS idx_entity_chunk ON Entities(world_id, chunk_x, chunk_y, chunk_z);
CREATE INDEX IF NOT EXISTS idx_entity_uuid ON Entities(entity_uuid);
CREATE INDEX IF NOT EXISTS idx_entity_active ON Entities(is_active);
CREATE INDEX IF NOT EXISTS idx_entity_owner ON Entities(owner_player_id);
CREATE INDEX IF NOT EXISTS idx_entity_modified ON Entities(modified_at);

-- Spatial index for entity queries (R-tree for fast radius queries)
CREATE VIRTUAL TABLE IF NOT EXISTS EntitySpatialIndex USING rtree(
    id,                                 -- Entity ID
    min_x, max_x,                      -- Bounding box X
    min_y, max_y,                      -- Bounding box Y
    min_z, max_z                       -- Bounding box Z
);

-- Trigger to update spatial index when entity moves
CREATE TRIGGER IF NOT EXISTS entity_spatial_insert AFTER INSERT ON Entities
BEGIN
    INSERT INTO EntitySpatialIndex (id, min_x, max_x, min_y, max_y, min_z, max_z)
    VALUES (
        NEW.entity_id,
        NEW.position_x - 1.0, NEW.position_x + 1.0,
        NEW.position_y - 1.0, NEW.position_y + 1.0,
        NEW.position_z - 1.0, NEW.position_z + 1.0
    );
END;

CREATE TRIGGER IF NOT EXISTS entity_spatial_update AFTER UPDATE ON Entities
BEGIN
    UPDATE EntitySpatialIndex SET
        min_x = NEW.position_x - 1.0,
        max_x = NEW.position_x + 1.0,
        min_y = NEW.position_y - 1.0,
        max_y = NEW.position_y + 1.0,
        min_z = NEW.position_z - 1.0,
        max_z = NEW.position_z + 1.0
    WHERE id = NEW.entity_id;
END;

CREATE TRIGGER IF NOT EXISTS entity_spatial_delete AFTER DELETE ON Entities
BEGIN
    DELETE FROM EntitySpatialIndex WHERE id = OLD.entity_id;
END;

-- ============================================================================
-- PLAYERS
-- ============================================================================

CREATE TABLE IF NOT EXISTS Players (
    player_id INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_id INTEGER UNIQUE NOT NULL,
    username TEXT UNIQUE NOT NULL,
    display_name TEXT,
    password_hash TEXT,                 -- For authentication
    email TEXT,
    level INTEGER DEFAULT 1,
    experience INTEGER DEFAULT 0,
    health REAL DEFAULT 100.0,
    max_health REAL DEFAULT 100.0,
    mana REAL DEFAULT 100.0,
    max_mana REAL DEFAULT 100.0,
    stamina REAL DEFAULT 100.0,
    max_stamina REAL DEFAULT 100.0,
    hunger REAL DEFAULT 100.0,
    thirst REAL DEFAULT 100.0,
    stats BLOB,                         -- JSON stats (strength, agility, etc.)
    skills BLOB,                        -- JSON skill tree data
    achievements BLOB,                  -- JSON achievements
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_login DATETIME,
    last_logout DATETIME,
    play_time_seconds INTEGER DEFAULT 0,
    deaths INTEGER DEFAULT 0,
    kills INTEGER DEFAULT 0,
    faction TEXT,
    guild_id INTEGER,
    currency_gold INTEGER DEFAULT 0,
    currency_silver INTEGER DEFAULT 0,
    currency_premium INTEGER DEFAULT 0,
    game_mode TEXT DEFAULT 'survival',
    is_online BOOLEAN DEFAULT 0,
    is_banned BOOLEAN DEFAULT 0,
    ban_reason TEXT,
    FOREIGN KEY (entity_id) REFERENCES Entities(entity_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_player_username ON Players(username);
CREATE INDEX IF NOT EXISTS idx_player_online ON Players(is_online);
CREATE INDEX IF NOT EXISTS idx_player_faction ON Players(faction);
CREATE INDEX IF NOT EXISTS idx_player_level ON Players(level);

-- ============================================================================
-- INVENTORY
-- ============================================================================

CREATE TABLE IF NOT EXISTS Inventory (
    inventory_id INTEGER PRIMARY KEY AUTOINCREMENT,
    player_id INTEGER NOT NULL,
    slot_index INTEGER NOT NULL,
    item_id TEXT NOT NULL,              -- Item identifier/template
    quantity INTEGER DEFAULT 1,
    durability REAL DEFAULT 100.0,
    max_durability REAL DEFAULT 100.0,
    item_data BLOB,                     -- JSON item properties (enchantments, etc.)
    is_equipped BOOLEAN DEFAULT 0,
    is_locked BOOLEAN DEFAULT 0,        -- Prevent accidental deletion
    acquired_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(player_id, slot_index),
    FOREIGN KEY (player_id) REFERENCES Players(player_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_inventory_player ON Inventory(player_id);
CREATE INDEX IF NOT EXISTS idx_inventory_item ON Inventory(item_id);
CREATE INDEX IF NOT EXISTS idx_inventory_equipped ON Inventory(is_equipped);

-- ============================================================================
-- EQUIPMENT
-- ============================================================================

CREATE TABLE IF NOT EXISTS Equipment (
    equipment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    player_id INTEGER NOT NULL,
    slot_name TEXT NOT NULL,            -- 'head', 'chest', 'legs', 'weapon', 'shield', etc.
    item_id TEXT,
    durability REAL DEFAULT 100.0,
    max_durability REAL DEFAULT 100.0,
    item_data BLOB,                     -- JSON item properties
    equipped_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(player_id, slot_name),
    FOREIGN KEY (player_id) REFERENCES Players(player_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_equipment_player ON Equipment(player_id);

-- ============================================================================
-- QUESTS
-- ============================================================================

CREATE TABLE IF NOT EXISTS QuestProgress (
    quest_id INTEGER PRIMARY KEY AUTOINCREMENT,
    player_id INTEGER NOT NULL,
    quest_name TEXT NOT NULL,
    quest_type TEXT,                    -- 'main', 'side', 'daily', 'guild'
    stage INTEGER DEFAULT 0,
    objectives BLOB,                    -- JSON quest objectives and progress
    rewards BLOB,                       -- JSON rewards data
    started_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    completed_at DATETIME,
    is_completed BOOLEAN DEFAULT 0,
    is_abandoned BOOLEAN DEFAULT 0,
    FOREIGN KEY (player_id) REFERENCES Players(player_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_quest_player ON QuestProgress(player_id);
CREATE INDEX IF NOT EXISTS idx_quest_completed ON QuestProgress(is_completed);
CREATE INDEX IF NOT EXISTS idx_quest_type ON QuestProgress(quest_type);

-- ============================================================================
-- BUILDINGS (Placed Structures)
-- ============================================================================

CREATE TABLE IF NOT EXISTS Buildings (
    building_id INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_id INTEGER UNIQUE NOT NULL,
    owner_player_id INTEGER,
    building_type TEXT NOT NULL,        -- 'house', 'barracks', 'tower', etc.
    building_name TEXT,
    health REAL NOT NULL,
    max_health REAL NOT NULL,
    faction TEXT,
    construction_progress REAL DEFAULT 100.0, -- Percentage
    is_constructing BOOLEAN DEFAULT 0,
    construction_started DATETIME,
    construction_completed DATETIME,
    storage_data BLOB,                  -- JSON for building inventory
    production_queue BLOB,              -- JSON for unit production
    upgrade_level INTEGER DEFAULT 1,
    FOREIGN KEY (entity_id) REFERENCES Entities(entity_id) ON DELETE CASCADE,
    FOREIGN KEY (owner_player_id) REFERENCES Players(player_id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_building_owner ON Buildings(owner_player_id);
CREATE INDEX IF NOT EXISTS idx_building_type ON Buildings(building_type);
CREATE INDEX IF NOT EXISTS idx_building_faction ON Buildings(faction);

-- ============================================================================
-- SERVER CONFIGURATION
-- ============================================================================

CREATE TABLE IF NOT EXISTS ServerConfig (
    config_id INTEGER PRIMARY KEY DEFAULT 1,
    server_name TEXT NOT NULL,
    server_description TEXT,
    server_version TEXT,
    max_players INTEGER DEFAULT 32,
    password_protected BOOLEAN DEFAULT 0,
    password_hash TEXT,                 -- Server password (hashed)
    pvp_enabled BOOLEAN DEFAULT 1,
    pve_enabled BOOLEAN DEFAULT 1,
    friendly_fire BOOLEAN DEFAULT 0,
    difficulty TEXT DEFAULT 'normal',   -- easy, normal, hard, nightmare
    hardcore_mode BOOLEAN DEFAULT 0,    -- Permadeath
    game_mode TEXT DEFAULT 'survival',
    auto_save_interval INTEGER DEFAULT 300,  -- Seconds
    auto_save_enabled BOOLEAN DEFAULT 1,
    backup_enabled BOOLEAN DEFAULT 1,
    backup_interval INTEGER DEFAULT 3600,    -- Seconds
    backup_retention_count INTEGER DEFAULT 10,
    max_build_height INTEGER DEFAULT 256,
    spawn_protection_radius REAL DEFAULT 50.0,
    day_night_cycle_speed REAL DEFAULT 1.0,
    weather_enabled BOOLEAN DEFAULT 1,
    natural_regeneration BOOLEAN DEFAULT 1,
    mob_spawning BOOLEAN DEFAULT 1,
    mob_griefing BOOLEAN DEFAULT 1,
    fall_damage BOOLEAN DEFAULT 1,
    fire_damage BOOLEAN DEFAULT 1,
    drowning_damage BOOLEAN DEFAULT 1,
    custom_settings TEXT,               -- JSON for additional settings
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    CHECK (config_id = 1)               -- Only one config row
);

-- Insert default config
INSERT OR IGNORE INTO ServerConfig (config_id, server_name) VALUES (1, 'Nova Server');

-- ============================================================================
-- ACCESS CONTROL (Whitelist/Blacklist)
-- ============================================================================

CREATE TABLE IF NOT EXISTS AccessControl (
    entry_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    list_type TEXT NOT NULL,            -- 'whitelist', 'blacklist'
    reason TEXT,
    added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    added_by TEXT,
    expires_at DATETIME,                -- NULL for permanent
    is_active BOOLEAN DEFAULT 1,
    UNIQUE(username, list_type)
);

CREATE INDEX IF NOT EXISTS idx_access_username ON AccessControl(username);
CREATE INDEX IF NOT EXISTS idx_access_type ON AccessControl(list_type);
CREATE INDEX IF NOT EXISTS idx_access_active ON AccessControl(is_active);

-- ============================================================================
-- ADMINISTRATORS
-- ============================================================================

CREATE TABLE IF NOT EXISTS Admins (
    admin_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    permission_level INTEGER DEFAULT 1, -- 1=moderator, 2=admin, 3=owner
    permissions TEXT,                   -- JSON permission flags
    granted_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    granted_by TEXT,
    is_active BOOLEAN DEFAULT 1
);

CREATE INDEX IF NOT EXISTS idx_admin_username ON Admins(username);
CREATE INDEX IF NOT EXISTS idx_admin_level ON Admins(permission_level);

-- ============================================================================
-- CHANGE LOG (For Backup/Restore and Auditing)
-- ============================================================================

CREATE TABLE IF NOT EXISTS ChangeLog (
    change_id INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name TEXT NOT NULL,
    record_id INTEGER NOT NULL,
    operation TEXT NOT NULL,            -- 'INSERT', 'UPDATE', 'DELETE'
    old_data BLOB,                      -- Serialized old state
    new_data BLOB,                      -- Serialized new state
    changed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    changed_by TEXT,                    -- Username or 'SYSTEM'
    transaction_id TEXT                 -- Group related changes
);

CREATE INDEX IF NOT EXISTS idx_changelog_table ON ChangeLog(table_name);
CREATE INDEX IF NOT EXISTS idx_changelog_time ON ChangeLog(changed_at);
CREATE INDEX IF NOT EXISTS idx_changelog_transaction ON ChangeLog(transaction_id);

-- ============================================================================
-- WORLD EVENTS (Global events that affect the world)
-- ============================================================================

CREATE TABLE IF NOT EXISTS WorldEvents (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id INTEGER NOT NULL,
    event_type TEXT NOT NULL,           -- 'invasion', 'boss_spawn', 'weather', etc.
    event_data TEXT,                    -- JSON event parameters
    started_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    ends_at DATETIME,
    is_active BOOLEAN DEFAULT 1,
    FOREIGN KEY (world_id) REFERENCES WorldMeta(world_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_event_world ON WorldEvents(world_id);
CREATE INDEX IF NOT EXISTS idx_event_active ON WorldEvents(is_active);

-- ============================================================================
-- PERFORMANCE STATISTICS
-- ============================================================================

CREATE TABLE IF NOT EXISTS PerformanceStats (
    stat_id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    player_count INTEGER DEFAULT 0,
    entity_count INTEGER DEFAULT 0,
    chunk_count INTEGER DEFAULT 0,
    loaded_chunk_count INTEGER DEFAULT 0,
    avg_tick_time REAL DEFAULT 0.0,     -- Milliseconds
    avg_frame_time REAL DEFAULT 0.0,
    memory_usage INTEGER DEFAULT 0,      -- Bytes
    database_size INTEGER DEFAULT 0,     -- Bytes
    active_connections INTEGER DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_perf_timestamp ON PerformanceStats(timestamp);

-- ============================================================================
-- SCHEMA MIGRATIONS
-- ============================================================================

CREATE TABLE IF NOT EXISTS SchemaMigrations (
    migration_id INTEGER PRIMARY KEY AUTOINCREMENT,
    version INTEGER NOT NULL UNIQUE,
    description TEXT,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    applied_by TEXT DEFAULT 'SYSTEM'
);

-- Record initial schema version
INSERT OR IGNORE INTO SchemaMigrations (version, description)
VALUES (1, 'Initial world persistence schema');

-- ============================================================================
-- VIEWS (Convenient queries)
-- ============================================================================

-- Active players with their entity data
CREATE VIEW IF NOT EXISTS ActivePlayers AS
SELECT
    p.*,
    e.position_x, e.position_y, e.position_z,
    e.chunk_x, e.chunk_y, e.chunk_z
FROM Players p
JOIN Entities e ON p.entity_id = e.entity_id
WHERE p.is_online = 1 AND e.is_active = 1;

-- Building summary with owner info
CREATE VIEW IF NOT EXISTS BuildingSummary AS
SELECT
    b.building_id,
    b.building_type,
    b.building_name,
    b.health,
    b.max_health,
    b.faction,
    p.username as owner_username,
    e.position_x, e.position_y, e.position_z
FROM Buildings b
LEFT JOIN Players p ON b.owner_player_id = p.player_id
JOIN Entities e ON b.entity_id = e.entity_id
WHERE e.is_active = 1;

-- Chunk load statistics
CREATE VIEW IF NOT EXISTS ChunkStats AS
SELECT
    world_id,
    COUNT(*) as total_chunks,
    SUM(CASE WHEN is_generated = 1 THEN 1 ELSE 0 END) as generated_chunks,
    SUM(CASE WHEN is_dirty = 1 THEN 1 ELSE 0 END) as dirty_chunks,
    AVG(access_count) as avg_access_count,
    SUM(LENGTH(data)) as total_data_size
FROM Chunks
GROUP BY world_id;

-- ============================================================================
-- MAINTENANCE PROCEDURES (Comments for reference)
-- ============================================================================

-- Vacuum database (reclaim space): VACUUM;
-- Analyze for query optimization: ANALYZE;
-- Check integrity: PRAGMA integrity_check;
-- Reindex: REINDEX;

-- Cleanup old performance stats (older than 7 days)
-- DELETE FROM PerformanceStats WHERE timestamp < datetime('now', '-7 days');

-- Cleanup old change log entries (older than 30 days)
-- DELETE FROM ChangeLog WHERE changed_at < datetime('now', '-30 days');
