#pragma once

/**
 * @file PlatformDetect.hpp
 * @brief Comprehensive platform, architecture, and compiler detection
 *
 * This header provides compile-time detection for:
 * - Target platform (Windows, Linux, macOS, iOS, Android, Web)
 * - CPU architecture (x86, x64, ARM, ARM64, WASM)
 * - Compiler (MSVC, GCC, Clang, Apple Clang)
 * - Build configuration (Debug, Release)
 * - Platform capabilities and features
 */

// =============================================================================
// Ensure Clean Slate
// =============================================================================

// Clear any existing platform definitions
#undef NOVA_PLATFORM_WINDOWS
#undef NOVA_PLATFORM_LINUX
#undef NOVA_PLATFORM_MACOS
#undef NOVA_PLATFORM_IOS
#undef NOVA_PLATFORM_ANDROID
#undef NOVA_PLATFORM_WEB
#undef NOVA_PLATFORM_DESKTOP
#undef NOVA_PLATFORM_MOBILE
#undef NOVA_PLATFORM_APPLE
#undef NOVA_PLATFORM_UNIX

// =============================================================================
// Platform Detection
// =============================================================================

// Windows Detection
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
    #define NOVA_PLATFORM_WINDOWS 1
    #define NOVA_PLATFORM_DESKTOP 1
    #define NOVA_PLATFORM_NAME "Windows"

    // Windows version detection
    #if defined(_WIN64)
        #define NOVA_WINDOWS_64BIT 1
    #else
        #define NOVA_WINDOWS_32BIT 1
    #endif

    // UWP detection
    #if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
        #define NOVA_WINDOWS_UWP 1
    #else
        #define NOVA_WINDOWS_DESKTOP 1
    #endif
#endif

// Android Detection (must check before Linux)
#if defined(__ANDROID__) || defined(ANDROID)
    #define NOVA_PLATFORM_ANDROID 1
    #define NOVA_PLATFORM_MOBILE 1
    #define NOVA_PLATFORM_UNIX 1
    #define NOVA_PLATFORM_NAME "Android"
    #undef NOVA_PLATFORM_LINUX

    // Android API level
    #if defined(__ANDROID_API__)
        #define NOVA_ANDROID_API_LEVEL __ANDROID_API__
    #endif
#endif

// Apple Platform Detection
#if defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>

    #define NOVA_PLATFORM_APPLE 1
    #define NOVA_PLATFORM_UNIX 1

    #if TARGET_OS_IOS || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define NOVA_PLATFORM_IOS 1
        #define NOVA_PLATFORM_MOBILE 1
        #define NOVA_PLATFORM_NAME "iOS"

        #if TARGET_IPHONE_SIMULATOR
            #define NOVA_IOS_SIMULATOR 1
        #endif

        #if TARGET_OS_MACCATALYST
            #define NOVA_CATALYST 1
        #endif
    #elif TARGET_OS_TV
        #define NOVA_PLATFORM_TVOS 1
        #define NOVA_PLATFORM_MOBILE 1
        #define NOVA_PLATFORM_NAME "tvOS"
    #elif TARGET_OS_WATCH
        #define NOVA_PLATFORM_WATCHOS 1
        #define NOVA_PLATFORM_MOBILE 1
        #define NOVA_PLATFORM_NAME "watchOS"
    #elif TARGET_OS_MAC
        #define NOVA_PLATFORM_MACOS 1
        #define NOVA_PLATFORM_DESKTOP 1
        #define NOVA_PLATFORM_NAME "macOS"
    #endif
#endif

// Linux Detection (desktop, not Android)
#if defined(__linux__) && !defined(NOVA_PLATFORM_ANDROID)
    #define NOVA_PLATFORM_LINUX 1
    #define NOVA_PLATFORM_DESKTOP 1
    #define NOVA_PLATFORM_UNIX 1
    #define NOVA_PLATFORM_NAME "Linux"
#endif

// BSD Detection
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define NOVA_PLATFORM_BSD 1
    #define NOVA_PLATFORM_DESKTOP 1
    #define NOVA_PLATFORM_UNIX 1
    #ifndef NOVA_PLATFORM_NAME
        #define NOVA_PLATFORM_NAME "BSD"
    #endif
#endif

// Web/Emscripten Detection
#if defined(__EMSCRIPTEN__)
    #define NOVA_PLATFORM_WEB 1
    #define NOVA_PLATFORM_NAME "Web"
    #undef NOVA_PLATFORM_DESKTOP
    #undef NOVA_PLATFORM_MOBILE
#endif

// =============================================================================
// Architecture Detection
// =============================================================================

// 64-bit detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || \
    defined(__arm64__) || defined(_M_ARM64) || defined(__ppc64__) || \
    defined(__LP64__) || defined(_LP64)
    #define NOVA_ARCH_64BIT 1
    #define NOVA_POINTER_SIZE 8
#else
    #define NOVA_ARCH_32BIT 1
    #define NOVA_POINTER_SIZE 4
#endif

// x86 family
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define NOVA_ARCH_X64 1
    #define NOVA_ARCH_X86_FAMILY 1
    #define NOVA_ARCH_NAME "x86_64"
    #define NOVA_ARCH_LITTLE_ENDIAN 1
#elif defined(__i386__) || defined(_M_IX86) || defined(__i386)
    #define NOVA_ARCH_X86 1
    #define NOVA_ARCH_X86_FAMILY 1
    #define NOVA_ARCH_NAME "x86"
    #define NOVA_ARCH_LITTLE_ENDIAN 1
#endif

// ARM family
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    #define NOVA_ARCH_ARM64 1
    #define NOVA_ARCH_ARM_FAMILY 1
    #define NOVA_ARCH_NAME "arm64"
    #define NOVA_ARCH_LITTLE_ENDIAN 1

    // NEON is always available on ARM64
    #define NOVA_ARM_NEON 1
#elif defined(__arm__) || defined(_M_ARM) || defined(__ARM_ARCH)
    #define NOVA_ARCH_ARM32 1
    #define NOVA_ARCH_ARM_FAMILY 1
    #define NOVA_ARCH_NAME "arm"

    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define NOVA_ARM_NEON 1
    #endif

    // Check for VFP
    #if defined(__VFP_FP__) || defined(__ARM_VFPV3__) || defined(__ARM_VFPV4__)
        #define NOVA_ARM_VFP 1
    #endif
#endif

// PowerPC
#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
    #if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
        #define NOVA_ARCH_PPC64 1
        #define NOVA_ARCH_NAME "ppc64"
    #else
        #define NOVA_ARCH_PPC 1
        #define NOVA_ARCH_NAME "ppc"
    #endif
    #define NOVA_ARCH_BIG_ENDIAN 1
#endif

// WebAssembly
#if defined(__wasm__) || defined(__EMSCRIPTEN__)
    #define NOVA_ARCH_WASM 1
    #ifndef NOVA_ARCH_NAME
        #define NOVA_ARCH_NAME "wasm"
    #endif
    #define NOVA_ARCH_LITTLE_ENDIAN 1
#endif

// RISC-V
#if defined(__riscv)
    #if __riscv_xlen == 64
        #define NOVA_ARCH_RISCV64 1
        #define NOVA_ARCH_NAME "riscv64"
    #else
        #define NOVA_ARCH_RISCV32 1
        #define NOVA_ARCH_NAME "riscv32"
    #endif
    #define NOVA_ARCH_LITTLE_ENDIAN 1
#endif

// Default endianness if not set
#ifndef NOVA_ARCH_LITTLE_ENDIAN
    #ifndef NOVA_ARCH_BIG_ENDIAN
        // Default to little endian for modern platforms
        #define NOVA_ARCH_LITTLE_ENDIAN 1
    #endif
#endif

// =============================================================================
// Compiler Detection
// =============================================================================

#if defined(__clang__)
    #define NOVA_COMPILER_CLANG 1
    #define NOVA_COMPILER_NAME "Clang"
    #define NOVA_COMPILER_VERSION_MAJOR __clang_major__
    #define NOVA_COMPILER_VERSION_MINOR __clang_minor__
    #define NOVA_COMPILER_VERSION_PATCH __clang_patchlevel__
    #define NOVA_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)

    #if defined(__apple_build_version__)
        #define NOVA_COMPILER_APPLECLANG 1
    #endif
#elif defined(__GNUC__)
    #define NOVA_COMPILER_GCC 1
    #define NOVA_COMPILER_NAME "GCC"
    #define NOVA_COMPILER_VERSION_MAJOR __GNUC__
    #define NOVA_COMPILER_VERSION_MINOR __GNUC_MINOR__
    #define NOVA_COMPILER_VERSION_PATCH __GNUC_PATCHLEVEL__
    #define NOVA_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
    #define NOVA_COMPILER_MSVC 1
    #define NOVA_COMPILER_NAME "MSVC"
    #define NOVA_COMPILER_VERSION _MSC_VER

    // MSVC version mapping
    #if _MSC_VER >= 1940
        #define NOVA_MSVC_2022_17_10 1  // VS 2022 17.10+
    #elif _MSC_VER >= 1930
        #define NOVA_MSVC_2022 1        // VS 2022
    #elif _MSC_VER >= 1920
        #define NOVA_MSVC_2019 1        // VS 2019
    #elif _MSC_VER >= 1910
        #define NOVA_MSVC_2017 1        // VS 2017
    #endif
#elif defined(__INTEL_COMPILER) || defined(__ICC)
    #define NOVA_COMPILER_INTEL 1
    #define NOVA_COMPILER_NAME "Intel"
    #define NOVA_COMPILER_VERSION __INTEL_COMPILER
#elif defined(__MINGW32__) || defined(__MINGW64__)
    #define NOVA_COMPILER_MINGW 1
    #define NOVA_COMPILER_NAME "MinGW"
#else
    #define NOVA_COMPILER_UNKNOWN 1
    #define NOVA_COMPILER_NAME "Unknown"
#endif

// =============================================================================
// C++ Standard Detection
// =============================================================================

#if defined(_MSVC_LANG)
    #define NOVA_CPLUSPLUS _MSVC_LANG
#else
    #define NOVA_CPLUSPLUS __cplusplus
#endif

#if NOVA_CPLUSPLUS >= 202302L
    #define NOVA_CPP23 1
    #define NOVA_CPP_VERSION 23
#elif NOVA_CPLUSPLUS >= 202002L
    #define NOVA_CPP20 1
    #define NOVA_CPP_VERSION 20
#elif NOVA_CPLUSPLUS >= 201703L
    #define NOVA_CPP17 1
    #define NOVA_CPP_VERSION 17
#elif NOVA_CPLUSPLUS >= 201402L
    #define NOVA_CPP14 1
    #define NOVA_CPP_VERSION 14
#elif NOVA_CPLUSPLUS >= 201103L
    #define NOVA_CPP11 1
    #define NOVA_CPP_VERSION 11
#else
    #define NOVA_CPP_VERSION 98
#endif

// =============================================================================
// Compiler Feature Macros
// =============================================================================

// Force inline
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_FORCE_INLINE __forceinline
    #define NOVA_NO_INLINE __declspec(noinline)
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_FORCE_INLINE __attribute__((always_inline)) inline
    #define NOVA_NO_INLINE __attribute__((noinline))
#else
    #define NOVA_FORCE_INLINE inline
    #define NOVA_NO_INLINE
#endif

// Likely/Unlikely branch hints
#if defined(NOVA_CPP20)
    #define NOVA_LIKELY [[likely]]
    #define NOVA_UNLIKELY [[unlikely]]
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_LIKELY
    #define NOVA_UNLIKELY
    #define NOVA_EXPECT_TRUE(x) __builtin_expect(!!(x), 1)
    #define NOVA_EXPECT_FALSE(x) __builtin_expect(!!(x), 0)
#else
    #define NOVA_LIKELY
    #define NOVA_UNLIKELY
    #define NOVA_EXPECT_TRUE(x) (x)
    #define NOVA_EXPECT_FALSE(x) (x)
#endif

// Unreachable code marker
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_UNREACHABLE() __assume(0)
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_UNREACHABLE() __builtin_unreachable()
#else
    #define NOVA_UNREACHABLE() ((void)0)
#endif

// Restrict pointer aliasing
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_RESTRICT __restrict
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_RESTRICT __restrict__
#else
    #define NOVA_RESTRICT
#endif

// Alignment
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_ALIGN(n) __declspec(align(n))
    #define NOVA_ALIGNED_STRUCT(n) __declspec(align(n)) struct
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_ALIGN(n) __attribute__((aligned(n)))
    #define NOVA_ALIGNED_STRUCT(n) struct __attribute__((aligned(n)))
#else
    #define NOVA_ALIGN(n)
    #define NOVA_ALIGNED_STRUCT(n) struct
#endif

// Deprecation
#if defined(NOVA_CPP14)
    #define NOVA_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(NOVA_COMPILER_MSVC)
    #define NOVA_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define NOVA_DEPRECATED(msg)
#endif

// Nodiscard
#if defined(NOVA_CPP17)
    #define NOVA_NODISCARD [[nodiscard]]
    #define NOVA_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_NODISCARD __attribute__((warn_unused_result))
    #define NOVA_NODISCARD_MSG(msg) __attribute__((warn_unused_result))
#else
    #define NOVA_NODISCARD
    #define NOVA_NODISCARD_MSG(msg)
#endif

// Maybe unused
#if defined(NOVA_CPP17)
    #define NOVA_MAYBE_UNUSED [[maybe_unused]]
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_MAYBE_UNUSED __attribute__((unused))
#else
    #define NOVA_MAYBE_UNUSED
#endif

// =============================================================================
// DLL Export/Import
// =============================================================================

#if defined(NOVA_PLATFORM_WINDOWS)
    #if defined(NOVA_BUILD_SHARED)
        #define NOVA_API __declspec(dllexport)
    #elif defined(NOVA_SHARED)
        #define NOVA_API __declspec(dllimport)
    #else
        #define NOVA_API
    #endif
    #define NOVA_API_LOCAL
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_API __attribute__((visibility("default")))
    #define NOVA_API_LOCAL __attribute__((visibility("hidden")))
#else
    #define NOVA_API
    #define NOVA_API_LOCAL
#endif

// =============================================================================
// SIMD Detection
// =============================================================================

// SSE/AVX detection
#if defined(NOVA_ARCH_X86_FAMILY)
    #if defined(__SSE__) || defined(_M_IX86_FP) || defined(NOVA_ARCH_X64)
        #define NOVA_SIMD_SSE 1
    #endif
    #if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(NOVA_ARCH_X64)
        #define NOVA_SIMD_SSE2 1
    #endif
    #if defined(__SSE3__)
        #define NOVA_SIMD_SSE3 1
    #endif
    #if defined(__SSSE3__)
        #define NOVA_SIMD_SSSE3 1
    #endif
    #if defined(__SSE4_1__)
        #define NOVA_SIMD_SSE41 1
    #endif
    #if defined(__SSE4_2__)
        #define NOVA_SIMD_SSE42 1
    #endif
    #if defined(__AVX__)
        #define NOVA_SIMD_AVX 1
    #endif
    #if defined(__AVX2__)
        #define NOVA_SIMD_AVX2 1
    #endif
    #if defined(__AVX512F__)
        #define NOVA_SIMD_AVX512 1
    #endif
    #if defined(__FMA__)
        #define NOVA_SIMD_FMA 1
    #endif
#endif

// ARM NEON detection (already handled in ARM detection above)
// ARM SVE detection
#if defined(__ARM_FEATURE_SVE)
    #define NOVA_SIMD_SVE 1
#endif

// WASM SIMD detection
#if defined(__wasm_simd128__)
    #define NOVA_SIMD_WASM128 1
#endif

// =============================================================================
// Debug/Release Detection
// =============================================================================

#if defined(NDEBUG) || defined(_NDEBUG)
    #define NOVA_RELEASE 1
    #define NOVA_BUILD_TYPE "Release"
#else
    #define NOVA_DEBUG 1
    #define NOVA_BUILD_TYPE "Debug"
#endif

// =============================================================================
// Platform Capability Macros
// =============================================================================

// Thread local storage
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_THREAD_LOCAL __declspec(thread)
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_THREAD_LOCAL __thread
#elif defined(NOVA_CPP11)
    #define NOVA_THREAD_LOCAL thread_local
#else
    #define NOVA_THREAD_LOCAL
#endif

// Function signature
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_FUNCTION_SIGNATURE __FUNCSIG__
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #define NOVA_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#else
    #define NOVA_FUNCTION_SIGNATURE __func__
#endif

// Breakpoint
#if defined(NOVA_COMPILER_MSVC)
    #define NOVA_DEBUGBREAK() __debugbreak()
#elif defined(NOVA_COMPILER_GCC) || defined(NOVA_COMPILER_CLANG)
    #if defined(NOVA_ARCH_X86_FAMILY)
        #define NOVA_DEBUGBREAK() __asm__ volatile("int $0x03")
    #elif defined(NOVA_ARCH_ARM_FAMILY)
        #define NOVA_DEBUGBREAK() __builtin_trap()
    #else
        #define NOVA_DEBUGBREAK() __builtin_trap()
    #endif
#else
    #include <signal.h>
    #define NOVA_DEBUGBREAK() raise(SIGTRAP)
#endif

// =============================================================================
// Platform-Specific Includes Guard
// =============================================================================

#if defined(NOVA_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef UNICODE
        #define UNICODE
    #endif
    #ifndef _UNICODE
        #define _UNICODE
    #endif
#endif

// =============================================================================
// Assertions
// =============================================================================

#if defined(NOVA_DEBUG)
    #include <cassert>
    #define NOVA_ASSERT(condition) assert(condition)
    #define NOVA_ASSERT_MSG(condition, msg) assert((condition) && (msg))
    #define NOVA_VERIFY(condition) assert(condition)
#else
    #define NOVA_ASSERT(condition) ((void)0)
    #define NOVA_ASSERT_MSG(condition, msg) ((void)0)
    #define NOVA_VERIFY(condition) ((void)(condition))
#endif

// Static assertions
#define NOVA_STATIC_ASSERT(condition, msg) static_assert(condition, msg)

// =============================================================================
// Stringify Macros
// =============================================================================

#define NOVA_STRINGIFY_IMPL(x) #x
#define NOVA_STRINGIFY(x) NOVA_STRINGIFY_IMPL(x)
#define NOVA_CONCAT_IMPL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_IMPL(a, b)

// =============================================================================
// Version Info
// =============================================================================

#define NOVA_VERSION_MAJOR 1
#define NOVA_VERSION_MINOR 0
#define NOVA_VERSION_PATCH 0
#define NOVA_VERSION_STRING "1.0.0"
#define NOVA_VERSION_NUMBER ((NOVA_VERSION_MAJOR * 10000) + (NOVA_VERSION_MINOR * 100) + NOVA_VERSION_PATCH)
