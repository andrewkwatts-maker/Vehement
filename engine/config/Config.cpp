#include "config/Config.hpp"
#include <spdlog/spdlog.h>

namespace Nova {

Config& Config::Instance() {
    static Config instance;
    return instance;
}

std::expected<void, ConfigError> Config::Load(const std::filesystem::path& filepath) {
    std::unique_lock lock(m_mutex);
    m_filepath = filepath;

    if (!std::filesystem::exists(filepath)) {
        spdlog::warn("Config file not found: {}. Creating default.", filepath.string());
        CreateDefault(filepath);
    }

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("Failed to open config file: {}", filepath.string());
            return std::unexpected(ConfigError::FileNotFound);
        }

        m_data = nlohmann::json::parse(file);
        m_cache.clear();
        spdlog::info("Loaded configuration from: {}", filepath.string());
        return {};
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse config file: {}", e.what());
        return std::unexpected(ConfigError::ParseError);
    }
}

std::expected<void, ConfigError> Config::Save(const std::filesystem::path& filepath) {
    std::shared_lock lock(m_mutex);
    const auto& path = filepath.empty() ? m_filepath : filepath;

    try {
        // Create parent directories if they don't exist
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open config file for writing: {}", path.string());
            return std::unexpected(ConfigError::WriteError);
        }

        file << std::setw(4) << m_data << std::endl;
        spdlog::info("Saved configuration to: {}", path.string());
        return {};
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config file: {}", e.what());
        return std::unexpected(ConfigError::WriteError);
    }
}

std::expected<void, ConfigError> Config::Reload() {
    // Note: Load acquires its own lock
    if (m_filepath.empty()) {
        spdlog::warn("No config file path set, cannot reload");
        return std::unexpected(ConfigError::FileNotFound);
    }
    return Load(m_filepath);
}

bool Config::Has(std::string_view key) const {
    std::shared_lock lock(m_mutex);
    return NavigateToKey(key) != nullptr;
}

void Config::ClearCache() {
    std::unique_lock lock(m_mutex);
    m_cache.clear();
}

nlohmann::json* Config::NavigateToKey(std::string_view key, bool create) {
    // Parse key path into parts
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = 0;
    while ((end = key.find('.', start)) != std::string_view::npos) {
        parts.emplace_back(key.substr(start, end - start));
        start = end + 1;
    }
    parts.emplace_back(key.substr(start));

    nlohmann::json* current = &m_data;
    for (const auto& p : parts) {
        if (create) {
            if (!current->contains(p)) {
                (*current)[p] = nlohmann::json::object();
            }
            current = &(*current)[p];
        } else {
            if (!current->contains(p)) {
                return nullptr;
            }
            current = &(*current)[p];
        }
    }
    return current;
}

const nlohmann::json* Config::NavigateToKey(std::string_view key) const {
    // Parse key path into parts without stringstream for better performance
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = 0;
    while ((end = key.find('.', start)) != std::string_view::npos) {
        parts.emplace_back(key.substr(start, end - start));
        start = end + 1;
    }
    parts.emplace_back(key.substr(start));

    const nlohmann::json* current = &m_data;
    for (const auto& p : parts) {
        if (!current->contains(p)) {
            return nullptr;
        }
        current = &(*current)[p];
    }
    return current;
}

void Config::CreateDefault(const std::filesystem::path& filepath) {
    nlohmann::json config;

    // Window settings
    config["window"]["width"] = 1920;
    config["window"]["height"] = 1080;
    config["window"]["title"] = "Nova3D Engine";
    config["window"]["fullscreen"] = false;
    config["window"]["vsync"] = true;
    config["window"]["resizable"] = true;
    config["window"]["samples"] = 4;

    // Camera settings
    config["camera"]["fov"] = 45.0;
    config["camera"]["near_plane"] = 0.1;
    config["camera"]["far_plane"] = 1000.0;
    config["camera"]["move_speed"] = 10.0;
    config["camera"]["look_speed"] = 0.1;
    config["camera"]["default_position"] = {10.0, 10.0, 10.0};
    config["camera"]["default_target"] = {0.0, 0.0, 0.0};

    // Rendering settings
    config["render"]["clear_color"] = {0.1, 0.1, 0.15, 1.0};
    config["render"]["enable_shadows"] = true;
    config["render"]["shadow_map_size"] = 2048;
    config["render"]["shadow_bias"] = 0.005;
    config["render"]["enable_hdr"] = false;
    config["render"]["gamma"] = 2.2;

    // Debug settings
    config["debug"]["show_grid"] = true;
    config["debug"]["grid_size"] = 20;
    config["debug"]["grid_spacing"] = 1.0;
    config["debug"]["grid_color"] = {0.5, 0.5, 0.5, 1.0};
    config["debug"]["axis_x_color"] = {1.0, 0.0, 0.0, 1.0};
    config["debug"]["axis_y_color"] = {0.0, 1.0, 0.0, 1.0};
    config["debug"]["axis_z_color"] = {0.0, 0.0, 1.0, 1.0};
    config["debug"]["show_fps"] = true;
    config["debug"]["show_stats"] = true;

    // Particle settings
    config["particles"]["max_particles"] = 10000;
    config["particles"]["default_lifespan"] = 2.0;
    config["particles"]["default_size"] = 0.1;

    // Terrain settings
    config["terrain"]["chunk_size"] = 64;
    config["terrain"]["view_distance"] = 4;
    config["terrain"]["height_scale"] = 50.0;
    config["terrain"]["noise_frequency"] = 0.02;
    config["terrain"]["octaves"] = 6;
    config["terrain"]["persistence"] = 0.5;
    config["terrain"]["lacunarity"] = 2.0;

    // Pathfinding settings
    config["pathfinding"]["default_node_radius"] = 0.5;
    config["pathfinding"]["max_iterations"] = 10000;
    config["pathfinding"]["heuristic_weight"] = 1.0;

    // Animation settings
    config["animation"]["default_fps"] = 30;
    config["animation"]["blend_time"] = 0.2;

    // Input settings
    config["input"]["mouse_sensitivity"] = 0.1;
    config["input"]["invert_y"] = false;

    // Create parent directories if needed
    if (filepath.has_parent_path()) {
        std::filesystem::create_directories(filepath.parent_path());
    }

    // Write to file
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << std::setw(4) << config << std::endl;
        spdlog::info("Created default configuration file: {}", filepath.string());
    }
}

} // namespace Nova
