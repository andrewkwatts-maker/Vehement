#pragma once

#include "PCGContext.hpp"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Vehement {

/**
 * @brief Script validation result
 */
struct PCGScriptValidation {
    bool valid = false;
    std::string errorMessage;
    std::vector<std::string> warnings;

    // Script metadata
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> requiredFunctions;
};

/**
 * @brief Script execution result
 */
struct PCGScriptResult {
    bool success = false;
    std::string errorMessage;
    float executionTime = 0.0f;

    // Output statistics
    int tilesModified = 0;
    int entitiesSpawned = 0;
    int foliageSpawned = 0;
};

/**
 * @brief Python script wrapper for PCG
 *
 * Provides:
 * - Script loading and validation
 * - Python environment management
 * - Binding of PCGContext to Python
 * - Standard API: generate(context) -> TileData
 * - Error handling and reporting
 *
 * The script wrapper exposes the following Python API:
 *
 * Context Properties:
 *   ctx.width - Region width in tiles
 *   ctx.height - Region height in tiles
 *   ctx.seed - Random seed
 *   ctx.world_x - World X offset
 *   ctx.world_y - World Y offset
 *
 * Tile Functions:
 *   ctx.set_tile(x, y, type) - Set tile by type name
 *   ctx.get_tile(x, y) -> str - Get tile type name
 *   ctx.set_wall(x, y, type, height) - Set wall
 *   ctx.fill_rect(x, y, w, h, type) - Fill rectangle
 *   ctx.draw_line(x1, y1, x2, y2, type) - Draw line
 *   ctx.set_elevation(x, y, elev) - Set height map
 *
 * Geographic Data:
 *   ctx.get_elevation(x, y) -> float - Get real elevation
 *   ctx.get_biome(x, y) -> str - Get biome name
 *   ctx.is_water(x, y) -> bool - Check water
 *   ctx.is_road(x, y) -> bool - Check road
 *   ctx.get_road_type(x, y) -> str - Get road type
 *   ctx.get_building(x, y) -> dict|None - Get building info
 *   ctx.get_population_density(x, y) -> float
 *   ctx.get_tree_density(x, y) -> float
 *
 * Random Functions:
 *   ctx.random() -> float - Random [0,1)
 *   ctx.random(min, max) -> float - Random [min, max)
 *   ctx.random_int(min, max) -> int - Random integer
 *   ctx.random_bool(prob=0.5) -> bool - Random boolean
 *
 * Noise Functions:
 *   ctx.perlin(x, y, freq=1, octaves=1) -> float
 *   ctx.simplex(x, y, freq=1, octaves=1) -> float
 *   ctx.worley(x, y, freq=1) -> float
 *   ctx.ridged(x, y, freq=1, octaves=4) -> float
 *   ctx.billow(x, y, freq=1, octaves=4) -> float
 *
 * Spawning:
 *   ctx.spawn_foliage(x, y, type, scale=1.0)
 *   ctx.spawn_entity(x, y, type, props={})
 *
 * Utility:
 *   ctx.in_bounds(x, y) -> bool
 *   ctx.is_walkable(x, y) -> bool
 *   ctx.is_occupied(x, y) -> bool
 *   ctx.mark_occupied(x, y)
 *   ctx.get_zone(x, y) -> str
 *   ctx.mark_zone(x, y, w, h, zone)
 *   ctx.distance(x1, y1, x2, y2) -> float
 *   ctx.find_path(x1, y1, x2, y2) -> list[(x,y)]
 *   ctx.has_line_of_sight(x1, y1, x2, y2) -> bool
 *
 * Data Storage:
 *   ctx.set_data(key, value)
 *   ctx.get_data(key) -> str
 *   ctx.has_data(key) -> bool
 */
class PCGScript {
public:
    /**
     * @brief Script entry points
     */
    static constexpr const char* FUNC_GENERATE = "generate";
    static constexpr const char* FUNC_PREVIEW = "preview";
    static constexpr const char* FUNC_VALIDATE = "validate";
    static constexpr const char* FUNC_INIT = "init";
    static constexpr const char* FUNC_CLEANUP = "cleanup";

    PCGScript();
    ~PCGScript();

    // Non-copyable
    PCGScript(const PCGScript&) = delete;
    PCGScript& operator=(const PCGScript&) = delete;
    PCGScript(PCGScript&&) noexcept;
    PCGScript& operator=(PCGScript&&) noexcept;

    /**
     * @brief Initialize Python environment
     * @return true if successful
     */
    static bool InitializePython();

    /**
     * @brief Shutdown Python environment
     */
    static void ShutdownPython();

    /**
     * @brief Check if Python is initialized
     */
    static bool IsPythonInitialized();

    /**
     * @brief Add search path for Python modules
     */
    static void AddSearchPath(const std::string& path);

    // ========== Script Loading ==========

    /**
     * @brief Load script from file
     * @param filepath Path to .py file
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& filepath);

    /**
     * @brief Load script from string
     * @param source Python source code
     * @param name Script name for error reporting
     * @return true if loaded successfully
     */
    bool LoadFromString(const std::string& source, const std::string& name = "inline");

    /**
     * @brief Unload current script
     */
    void Unload();

    /**
     * @brief Check if script is loaded
     */
    bool IsLoaded() const { return m_loaded; }

    /**
     * @brief Get script filepath
     */
    const std::string& GetFilePath() const { return m_filepath; }

    /**
     * @brief Get last error message
     */
    const std::string& GetLastError() const { return m_lastError; }

    // ========== Script Validation ==========

    /**
     * @brief Validate loaded script
     * @return Validation result with metadata
     */
    PCGScriptValidation Validate();

    /**
     * @brief Check if script has a specific function
     */
    bool HasFunction(const std::string& name) const;

    /**
     * @brief Get list of available functions
     */
    std::vector<std::string> GetFunctions() const;

    // ========== Script Execution ==========

    /**
     * @brief Execute generate() function
     * @param context PCG context to pass to script
     * @return Execution result
     */
    PCGScriptResult Generate(PCGContext& context);

    /**
     * @brief Execute preview() function (fast, low-detail)
     * @param context PCG context
     * @return Execution result
     */
    PCGScriptResult Preview(PCGContext& context);

    /**
     * @brief Execute custom function
     * @param functionName Name of function to call
     * @param context PCG context
     * @return Execution result
     */
    PCGScriptResult ExecuteFunction(const std::string& functionName, PCGContext& context);

    /**
     * @brief Execute with timeout
     * @param functionName Name of function to call
     * @param context PCG context
     * @param timeoutMs Timeout in milliseconds
     * @return Execution result
     */
    PCGScriptResult ExecuteWithTimeout(const std::string& functionName, PCGContext& context,
                                        int timeoutMs);

    // ========== Script Configuration ==========

    /**
     * @brief Set global variable in script
     */
    void SetGlobal(const std::string& name, int value);
    void SetGlobal(const std::string& name, float value);
    void SetGlobal(const std::string& name, const std::string& value);
    void SetGlobal(const std::string& name, bool value);

    /**
     * @brief Get global variable from script
     */
    int GetGlobalInt(const std::string& name, int defaultValue = 0) const;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) const;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") const;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) const;

    // ========== Script Callbacks ==========

    /**
     * @brief Progress callback type
     */
    using ProgressCallback = std::function<void(float progress, const std::string& stage)>;

    /**
     * @brief Set progress callback for long-running scripts
     */
    void SetProgressCallback(ProgressCallback callback);

    /**
     * @brief Log callback type
     */
    using LogCallback = std::function<void(const std::string& message, int level)>;

    /**
     * @brief Set logging callback
     */
    void SetLogCallback(LogCallback callback);

    // ========== Script Debugging ==========

    /**
     * @brief Enable script profiling
     */
    void EnableProfiling(bool enabled);

    /**
     * @brief Get profiling data
     */
    struct ProfilingData {
        float totalTime = 0.0f;
        std::unordered_map<std::string, float> functionTimes;
        int callCount = 0;
    };
    ProfilingData GetProfilingData() const;

    /**
     * @brief Clear profiling data
     */
    void ClearProfilingData();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool m_loaded = false;
    std::string m_filepath;
    std::string m_lastError;
    std::string m_source;

    ProgressCallback m_progressCallback;
    LogCallback m_logCallback;
    bool m_profilingEnabled = false;
    ProfilingData m_profilingData;

    // Helper to bind context to Python
    void BindContext(PCGContext& context);
    void UnbindContext();

    // Error handling
    void SetError(const std::string& error);
    std::string GetPythonError();
};

/**
 * @brief PCG Script manager for caching and reloading
 */
class PCGScriptManager {
public:
    static PCGScriptManager& Instance();

    /**
     * @brief Initialize manager and Python
     */
    bool Initialize(const std::string& scriptsPath = "");

    /**
     * @brief Shutdown manager
     */
    void Shutdown();

    /**
     * @brief Get or load script by name
     * @param name Script name (without .py extension)
     * @return Script pointer or nullptr if not found
     */
    PCGScript* GetScript(const std::string& name);

    /**
     * @brief Reload a script
     */
    bool ReloadScript(const std::string& name);

    /**
     * @brief Reload all scripts
     */
    void ReloadAll();

    /**
     * @brief Get list of available scripts
     */
    std::vector<std::string> GetAvailableScripts() const;

    /**
     * @brief Set scripts directory
     */
    void SetScriptsPath(const std::string& path);

    /**
     * @brief Get scripts directory
     */
    const std::string& GetScriptsPath() const { return m_scriptsPath; }

    /**
     * @brief Enable file watching for auto-reload
     */
    void EnableFileWatching(bool enabled);

    /**
     * @brief Check for file changes and reload if needed
     */
    void CheckForChanges();

private:
    PCGScriptManager() = default;
    ~PCGScriptManager();

    PCGScriptManager(const PCGScriptManager&) = delete;
    PCGScriptManager& operator=(const PCGScriptManager&) = delete;

    std::string m_scriptsPath;
    std::unordered_map<std::string, std::unique_ptr<PCGScript>> m_scripts;
    std::unordered_map<std::string, uint64_t> m_fileModTimes;
    bool m_fileWatching = false;
    bool m_initialized = false;
};

} // namespace Vehement
