/**
 * @file json_config.hpp
 * @brief JSON library configuration - include this BEFORE nlohmann/json.hpp
 *
 * This header ensures proper configuration to avoid C++20 ranges namespace pollution
 * on MSVC. Always include this header instead of nlohmann/json.hpp directly.
 */

#pragma once

// Disable C++20 ranges support to avoid MSVC std::ranges namespace pollution
#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 0
#endif

#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20 0
#endif

// Now include the actual JSON library
#include <nlohmann/json.hpp>
