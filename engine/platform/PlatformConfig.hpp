#pragma once

/**
 * @file PlatformConfig.hpp
 * @brief Platform detection and configuration macros
 *
 * This header provides compile-time platform detection and configuration.
 * Include this at the top of platform-specific files.
 */

// =============================================================================
// Platform Detection
// =============================================================================

// Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
    #ifndef NOVA_PLATFORM_WINDOWS
        #define NOVA_PLATFORM_WINDOWS 1
    #endif
    #define NOVA_PLATFORM_DESKTOP 1
    #define NOVA_PLATFORM_NAME "Windows"
#endif

// Android (must check before Linux since Android defines __linux__)
#if defined(__ANDROID__)
    #ifndef NOVA_PLATFORM_ANDROID
        #define NOVA_PLATFORM_ANDROID 1
    #endif
    #define NOVA_PLATFORM_MOBILE 1
    #define NOVA_PLATFORM_NAME "Android"
    #undef NOVA_PLATFORM_LINUX  // Android is not desktop Linux
#endif

// iOS (must check before macOS since iOS defines __APPLE__)
#if defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #ifndef NOVA_PLATFORM_IOS
            #define NOVA_PLATFORM_IOS 1
        #endif
        #define NOVA_PLATFORM_MOBILE 1
        #define NOVA_PLATFORM_APPLE 1
        #define NOVA_PLATFORM_NAME "iOS"
    #elif TARGET_OS_MAC
        #ifndef NOVA_PLATFORM_MACOS
            #define NOVA_PLATFORM_MACOS 1
        #endif
        #define NOVA_PLATFORM_DESKTOP 1
        #define NOVA_PLATFORM_APPLE 1
        #define NOVA_PLATFORM_NAME "macOS"
    #endif
#endif

// Linux (desktop)
#if defined(__linux__) && !defined(NOVA_PLATFORM_ANDROID)
    #ifndef NOVA_PLATFORM_LINUX
        #define NOVA_PLATFORM_LINUX 1
    #endif
    #define NOVA_PLATFORM_DESKTOP 1
    #define NOVA_PLATFORM_UNIX 1
    #define NOVA_PLATFORM_NAME "Linux"
#endif

// Web (Emscripten)
#if defined(__EMSCRIPTEN__)
    #ifndef NOVA_PLATFORM_WEB
        #define NOVA_PLATFORM_WEB 1
    #endif
    #define NOVA_PLATFORM_NAME "Web"
#endif

// FreeBSD / OpenBSD / NetBSD
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #ifndef NOVA_PLATFORM_BSD
        #define NOVA_PLATFORM_BSD 1
    #endif
    #define NOVA_PLATFORM_DESKTOP 1
    #define NOVA_PLATFORM_UNIX 1
    #ifndef NOVA_PLATFORM_NAME
        #define NOVA_PLATFORM_NAME "BSD"
    #endif
#endif

// =============================================================================
// Architecture Detection
// =============================================================================

// 64-bit detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || \
    defined(__arm64__) || defined(__ppc64__) || defined(__LP64__)
    #define NOVA_ARCH_64BIT 1
#else
    #define NOVA_ARCH_32BIT 1
#endif

// x86 family
#if defined(__x86_64__) || defined(_M_X64)
    #define NOVA_ARCH_X64 1
    #define NOVA_ARCH_X86_64 1
    #define NOVA_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define NOVA_ARCH_X86 1
    #define NOVA_ARCH_X86_32 1
    #define NOVA_ARCH_NAME "x86"
#endif

// ARM family
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    #define NOVA_ARCH_ARM 1
    #define NOVA_ARCH_ARM64 1
    #define NOVA_ARCH_NAME "arm64"
#elif defined(__arm__) || defined(_M_ARM)
    #define NOVA_ARCH_ARM 1
    #define NOVA_ARCH_ARM32 1
    #define NOVA_ARCH_NAME "arm"
#endif

// WebAssembly
#if defined(__EMSCRIPTEN__) || defined(__wasm__)
    #define NOVA_ARCH_WASM 1
    #define NOVA_ARCH_NAME "wasm"
#endif

// =============================================================================
// Compiler Detection
// =============================================================================

#if defined(__clang__)
    #define NOVA_COMPILER_CLANG 1
    #define NOVA_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
    #if defined(__apple_build_version__)
        #define NOVA_COMPILER_APPLECLANG 1
    #endif
#elif defined(__GNUC__)
    #define NOVA_COMPILER_GCC 1
    #define NOVA_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
    #define NOVA_COMPILER_MSVC 1
    #define NOVA_COMPILER_VERSION _MSC_VER
#elif defined(__INTEL_COMPILER)
    #define NOVA_COMPILER_INTEL 1
    #define NOVA_COMPILER_VERSION __INTEL_COMPILER
#endif

// =============================================================================
// Compiler Feature Macros
// =============================================================================

// Force inline
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_FORCE_INLINE __forceinline
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define NOVA_FORCE_INLINE inline
#endif

// No inline
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_NO_INLINE __declspec(noinline)
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_NO_INLINE __attribute__((noinline))
#else
    #define NOVA_NO_INLINE
#endif

// Likely/Unlikely branch hints
#if defined(__cplusplus) && __cplusplus >= 202002L
    #define NOVA_LIKELY(x)   (x) [[likely]]
    #define NOVA_UNLIKELY(x) (x) [[unlikely]]
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define NOVA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define NOVA_LIKELY(x)   (x)
    #define NOVA_UNLIKELY(x) (x)
#endif

// Unreachable code marker
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_UNREACHABLE() __assume(0)
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_UNREACHABLE() __builtin_unreachable()
#else
    #define NOVA_UNREACHABLE() do {} while(0)
#endif

// Deprecated marker
#if defined(__cplusplus) && __cplusplus >= 201402L
    #define NOVA_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(NOVA_COMPILER_MSVC)
    #define NOVA_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define NOVA_DEPRECATED(msg)
#endif

// Export/Import for shared libraries
#if defined(NOVA_PLATFORM_WINDOWS)
    #if defined(NOVA_BUILD_SHARED)
        #define NOVA_API __declspec(dllexport)
    #elif defined(NOVA_SHARED)
        #define NOVA_API __declspec(dllimport)
    #else
        #define NOVA_API
    #endif
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_API __attribute__((visibility("default")))
#else
    #define NOVA_API
#endif

// =============================================================================
// Platform-Specific Includes
// =============================================================================

#if defined(NOVA_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

// =============================================================================
// Debug/Release Mode
// =============================================================================

#if defined(NDEBUG) || defined(_NDEBUG)
    #define NOVA_RELEASE 1
#else
    #define NOVA_DEBUG 1
#endif

// =============================================================================
// Assertions
// =============================================================================

#if defined(NOVA_DEBUG)
    #include <cassert>
    #define NOVA_ASSERT(condition) assert(condition)
    #define NOVA_ASSERT_MSG(condition, msg) assert((condition) && (msg))
#else
    #define NOVA_ASSERT(condition) ((void)0)
    #define NOVA_ASSERT_MSG(condition, msg) ((void)0)
#endif

// =============================================================================
// Static Assertions
// =============================================================================

#define NOVA_STATIC_ASSERT(condition, msg) static_assert(condition, msg)

// =============================================================================
// Version Info
// =============================================================================

#define NOVA_VERSION_MAJOR 1
#define NOVA_VERSION_MINOR 0
#define NOVA_VERSION_PATCH 0
#define NOVA_VERSION_STRING "1.0.0"
