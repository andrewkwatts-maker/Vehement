/**
 * @file json_config.hpp
 * @brief JSON library configuration - include this BEFORE nlohmann/json.hpp
 *
 * This header ensures proper configuration to avoid C++20 ranges namespace pollution
 * on MSVC. Always include this header instead of nlohmann/json.hpp directly.
 */

#pragma once

// CRITICAL: Disable C++20 ranges support BEFORE nlohmann/json.hpp
// The order matters - these must be defined before ANY JSON headers
#define JSON_HAS_RANGES 0
#define JSON_HAS_CPP_20 0

// Prevent detection of C++20 ranges by undefining the feature test macro
#ifdef __cpp_lib_ranges
#undef __cpp_lib_ranges
#define NOVA_UNDEF_CPP_LIB_RANGES 1
#endif

// Now include the actual JSON library
#include <nlohmann/json.hpp>

// Restore the ranges macro if we undefined it
#ifdef NOVA_UNDEF_CPP_LIB_RANGES
#undef NOVA_UNDEF_CPP_LIB_RANGES
// Note: Can't restore __cpp_lib_ranges as it's compiler-defined
// This is intentional - we don't want JSON to use ranges
#endif
