/**
 * @file json_config.hpp
 * @brief JSON library configuration wrapper - MSVC C++20 ranges fix
 *
 * CRITICAL: This header fixes the nlohmann/json + MSVC C++20 ranges namespace
 * pollution issue. The problem is that when <algorithm> is included after
 * nlohmann/json.hpp, MSVC's template instantiation creates a spurious
 * `nlohmann::std::ranges` namespace that conflicts with `std::ranges`.
 *
 * SOLUTION: Include standard library algorithm headers BEFORE nlohmann/json.hpp,
 * and define JSON_HAS_RANGES=0 to disable ranges-specific code paths.
 *
 * This header should be used instead of directly including <nlohmann/json.hpp>.
 *
 * PREFERRED: Use engine/core/json_wrapper.hpp for full Nova::Json namespace
 * with convenient helper functions.
 */

#pragma once

// Include the complete JSON wrapper which handles all configuration
#include "json_wrapper.hpp"

// Re-export for backward compatibility - files using json_config.hpp
// expect 'json' to be available as nlohmann::json
using json = nlohmann::json;
