#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <cstring>
#include <array>
#include <algorithm>

// Detect SIMD support
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    #define NOVA_SSE_SUPPORT 1
    #include <xmmintrin.h>  // SSE
#endif

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    #define NOVA_SSE2_SUPPORT 1
    #include <emmintrin.h>  // SSE2
#endif

#if defined(__SSE3__)
    #define NOVA_SSE3_SUPPORT 1
    #include <pmmintrin.h>  // SSE3
#endif

#if defined(__SSE4_1__)
    #define NOVA_SSE4_SUPPORT 1
    #include <smmintrin.h>  // SSE4.1
#endif

#if defined(__AVX__)
    #define NOVA_AVX_SUPPORT 1
    #include <immintrin.h>  // AVX
#endif

#if defined(__AVX2__)
    #define NOVA_AVX2_SUPPORT 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define NOVA_NEON_SUPPORT 1
    #include <arm_neon.h>
#endif

namespace Nova {
namespace SIMD {

/**
 * @brief SIMD capability detection at runtime
 */
struct Capabilities {
    bool sse = false;
    bool sse2 = false;
    bool sse3 = false;
    bool sse4 = false;
    bool avx = false;
    bool avx2 = false;
    bool neon = false;

    static const Capabilities& Get() {
        static Capabilities caps = Detect();
        return caps;
    }

private:
    static Capabilities Detect() {
        Capabilities caps;
#ifdef NOVA_SSE_SUPPORT
        caps.sse = true;
#endif
#ifdef NOVA_SSE2_SUPPORT
        caps.sse2 = true;
#endif
#ifdef NOVA_SSE3_SUPPORT
        caps.sse3 = true;
#endif
#ifdef NOVA_SSE4_SUPPORT
        caps.sse4 = true;
#endif
#ifdef NOVA_AVX_SUPPORT
        caps.avx = true;
#endif
#ifdef NOVA_AVX2_SUPPORT
        caps.avx2 = true;
#endif
#ifdef NOVA_NEON_SUPPORT
        caps.neon = true;
#endif
        return caps;
    }
};

// =============================================================================
// SIMD Vector Types
// =============================================================================

#ifdef NOVA_SSE_SUPPORT

/**
 * @brief SSE 4-float vector wrapper
 */
struct alignas(16) Vec4f {
    __m128 data;

    Vec4f() : data(_mm_setzero_ps()) {}
    Vec4f(__m128 v) : data(v) {}
    Vec4f(float x, float y, float z, float w) : data(_mm_set_ps(w, z, y, x)) {}
    Vec4f(float scalar) : data(_mm_set1_ps(scalar)) {}
    explicit Vec4f(const glm::vec4& v) : data(_mm_set_ps(v.w, v.z, v.y, v.x)) {}
    explicit Vec4f(const glm::vec3& v, float w = 0.0f) : data(_mm_set_ps(w, v.z, v.y, v.x)) {}

    // Convert back to GLM
    [[nodiscard]] glm::vec4 ToGLM() const {
        alignas(16) float arr[4];
        _mm_store_ps(arr, data);
        return glm::vec4(arr[0], arr[1], arr[2], arr[3]);
    }

    [[nodiscard]] glm::vec3 ToGLM3() const {
        alignas(16) float arr[4];
        _mm_store_ps(arr, data);
        return glm::vec3(arr[0], arr[1], arr[2]);
    }

    // Element access
    [[nodiscard]] float X() const { return _mm_cvtss_f32(data); }
    [[nodiscard]] float Y() const { return _mm_cvtss_f32(_mm_shuffle_ps(data, data, _MM_SHUFFLE(1, 1, 1, 1))); }
    [[nodiscard]] float Z() const { return _mm_cvtss_f32(_mm_shuffle_ps(data, data, _MM_SHUFFLE(2, 2, 2, 2))); }
    [[nodiscard]] float W() const { return _mm_cvtss_f32(_mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 3, 3, 3))); }

    // Arithmetic operators
    Vec4f operator+(Vec4f other) const { return _mm_add_ps(data, other.data); }
    Vec4f operator-(Vec4f other) const { return _mm_sub_ps(data, other.data); }
    Vec4f operator*(Vec4f other) const { return _mm_mul_ps(data, other.data); }
    Vec4f operator/(Vec4f other) const { return _mm_div_ps(data, other.data); }

    Vec4f operator*(float scalar) const { return _mm_mul_ps(data, _mm_set1_ps(scalar)); }
    Vec4f operator/(float scalar) const { return _mm_div_ps(data, _mm_set1_ps(scalar)); }

    Vec4f& operator+=(Vec4f other) { data = _mm_add_ps(data, other.data); return *this; }
    Vec4f& operator-=(Vec4f other) { data = _mm_sub_ps(data, other.data); return *this; }
    Vec4f& operator*=(Vec4f other) { data = _mm_mul_ps(data, other.data); return *this; }
    Vec4f& operator*=(float scalar) { data = _mm_mul_ps(data, _mm_set1_ps(scalar)); return *this; }

    Vec4f operator-() const { return _mm_sub_ps(_mm_setzero_ps(), data); }
};

// Vector operations
[[nodiscard]] inline Vec4f Min(Vec4f a, Vec4f b) {
    return _mm_min_ps(a.data, b.data);
}

[[nodiscard]] inline Vec4f Max(Vec4f a, Vec4f b) {
    return _mm_max_ps(a.data, b.data);
}

[[nodiscard]] inline Vec4f Clamp(Vec4f v, Vec4f minVal, Vec4f maxVal) {
    return Min(Max(v, minVal), maxVal);
}

[[nodiscard]] inline Vec4f Abs(Vec4f v) {
    __m128 signMask = _mm_set1_ps(-0.0f);
    return _mm_andnot_ps(signMask, v.data);
}

[[nodiscard]] inline float Dot3(Vec4f a, Vec4f b) {
    __m128 mul = _mm_mul_ps(a.data, b.data);
#ifdef NOVA_SSE3_SUPPORT
    __m128 sum = _mm_hadd_ps(mul, mul);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
#else
    // SSE2 fallback
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
#endif
}

[[nodiscard]] inline float Dot4(Vec4f a, Vec4f b) {
#ifdef NOVA_SSE4_SUPPORT
    return _mm_cvtss_f32(_mm_dp_ps(a.data, b.data, 0xFF));
#else
    __m128 mul = _mm_mul_ps(a.data, b.data);
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
#endif
}

[[nodiscard]] inline float Length3(Vec4f v) {
    return std::sqrt(Dot3(v, v));
}

[[nodiscard]] inline float LengthSquared3(Vec4f v) {
    return Dot3(v, v);
}

[[nodiscard]] inline Vec4f Normalize3(Vec4f v) {
    float len = Length3(v);
    if (len > 0.0f) {
        return v * (1.0f / len);
    }
    return v;
}

[[nodiscard]] inline Vec4f Cross3(Vec4f a, Vec4f b) {
    __m128 a_yzx = _mm_shuffle_ps(a.data, a.data, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_yzx = _mm_shuffle_ps(b.data, b.data, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 a_zxy = _mm_shuffle_ps(a.data, a.data, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_zxy = _mm_shuffle_ps(b.data, b.data, _MM_SHUFFLE(3, 1, 0, 2));
    return _mm_sub_ps(_mm_mul_ps(a_yzx, b_zxy), _mm_mul_ps(a_zxy, b_yzx));
}

[[nodiscard]] inline Vec4f Lerp(Vec4f a, Vec4f b, float t) {
    __m128 vt = _mm_set1_ps(t);
    return _mm_add_ps(a.data, _mm_mul_ps(_mm_sub_ps(b.data, a.data), vt));
}

/**
 * @brief SSE 4x4 matrix wrapper
 */
struct alignas(16) Mat4f {
    __m128 rows[4];

    Mat4f() {
        rows[0] = _mm_set_ps(0, 0, 0, 1);
        rows[1] = _mm_set_ps(0, 0, 1, 0);
        rows[2] = _mm_set_ps(0, 1, 0, 0);
        rows[3] = _mm_set_ps(1, 0, 0, 0);
    }

    Mat4f(__m128 r0, __m128 r1, __m128 r2, __m128 r3) {
        rows[0] = r0;
        rows[1] = r1;
        rows[2] = r2;
        rows[3] = r3;
    }

    explicit Mat4f(const glm::mat4& m) {
        // GLM is column-major, we store row-major for SIMD efficiency
        rows[0] = _mm_set_ps(m[3][0], m[2][0], m[1][0], m[0][0]);
        rows[1] = _mm_set_ps(m[3][1], m[2][1], m[1][1], m[0][1]);
        rows[2] = _mm_set_ps(m[3][2], m[2][2], m[1][2], m[0][2]);
        rows[3] = _mm_set_ps(m[3][3], m[2][3], m[1][3], m[0][3]);
    }

    [[nodiscard]] glm::mat4 ToGLM() const {
        alignas(16) float r0[4], r1[4], r2[4], r3[4];
        _mm_store_ps(r0, rows[0]);
        _mm_store_ps(r1, rows[1]);
        _mm_store_ps(r2, rows[2]);
        _mm_store_ps(r3, rows[3]);

        return glm::mat4(
            r0[0], r1[0], r2[0], r3[0],
            r0[1], r1[1], r2[1], r3[1],
            r0[2], r1[2], r2[2], r3[2],
            r0[3], r1[3], r2[3], r3[3]
        );
    }

    // Matrix-vector multiplication
    [[nodiscard]] Vec4f operator*(Vec4f v) const {
        __m128 x = _mm_shuffle_ps(v.data, v.data, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 y = _mm_shuffle_ps(v.data, v.data, _MM_SHUFFLE(1, 1, 1, 1));
        __m128 z = _mm_shuffle_ps(v.data, v.data, _MM_SHUFFLE(2, 2, 2, 2));
        __m128 w = _mm_shuffle_ps(v.data, v.data, _MM_SHUFFLE(3, 3, 3, 3));

        __m128 result = _mm_mul_ps(rows[0], x);
        result = _mm_add_ps(result, _mm_mul_ps(rows[1], y));
        result = _mm_add_ps(result, _mm_mul_ps(rows[2], z));
        result = _mm_add_ps(result, _mm_mul_ps(rows[3], w));

        return result;
    }

    // Matrix-matrix multiplication
    [[nodiscard]] Mat4f operator*(const Mat4f& other) const {
        Mat4f result;
        for (int i = 0; i < 4; ++i) {
            __m128 x = _mm_shuffle_ps(rows[i], rows[i], _MM_SHUFFLE(0, 0, 0, 0));
            __m128 y = _mm_shuffle_ps(rows[i], rows[i], _MM_SHUFFLE(1, 1, 1, 1));
            __m128 z = _mm_shuffle_ps(rows[i], rows[i], _MM_SHUFFLE(2, 2, 2, 2));
            __m128 w = _mm_shuffle_ps(rows[i], rows[i], _MM_SHUFFLE(3, 3, 3, 3));

            result.rows[i] = _mm_mul_ps(other.rows[0], x);
            result.rows[i] = _mm_add_ps(result.rows[i], _mm_mul_ps(other.rows[1], y));
            result.rows[i] = _mm_add_ps(result.rows[i], _mm_mul_ps(other.rows[2], z));
            result.rows[i] = _mm_add_ps(result.rows[i], _mm_mul_ps(other.rows[3], w));
        }
        return result;
    }

    static Mat4f Identity() {
        return Mat4f();
    }
};

#else // Fallback implementations

using Vec4f = glm::vec4;
using Mat4f = glm::mat4;

[[nodiscard]] inline Vec4f Min(Vec4f a, Vec4f b) { return glm::min(a, b); }
[[nodiscard]] inline Vec4f Max(Vec4f a, Vec4f b) { return glm::max(a, b); }
[[nodiscard]] inline Vec4f Clamp(Vec4f v, Vec4f minVal, Vec4f maxVal) { return glm::clamp(v, minVal, maxVal); }
[[nodiscard]] inline Vec4f Abs(Vec4f v) { return glm::abs(v); }
[[nodiscard]] inline float Dot3(Vec4f a, Vec4f b) { return glm::dot(glm::vec3(a), glm::vec3(b)); }
[[nodiscard]] inline float Dot4(Vec4f a, Vec4f b) { return glm::dot(a, b); }
[[nodiscard]] inline float Length3(Vec4f v) { return glm::length(glm::vec3(v)); }
[[nodiscard]] inline float LengthSquared3(Vec4f v) { auto v3 = glm::vec3(v); return glm::dot(v3, v3); }
[[nodiscard]] inline Vec4f Normalize3(Vec4f v) { return Vec4f(glm::normalize(glm::vec3(v)), v.w); }
[[nodiscard]] inline Vec4f Lerp(Vec4f a, Vec4f b, float t) { return glm::mix(a, b, t); }

#endif // NOVA_SSE_SUPPORT

// =============================================================================
// Batch Operations
// =============================================================================

/**
 * @brief Transform multiple positions by a matrix (SIMD batch)
 */
inline void TransformPositions(const Mat4f& matrix,
                                const glm::vec3* input,
                                glm::vec3* output,
                                size_t count) {
#ifdef NOVA_SSE_SUPPORT
    for (size_t i = 0; i < count; ++i) {
        Vec4f pos(input[i], 1.0f);
        Vec4f result = matrix * pos;
        output[i] = result.ToGLM3();
    }
#else
    glm::mat4 m = matrix;
    for (size_t i = 0; i < count; ++i) {
        glm::vec4 result = m * glm::vec4(input[i], 1.0f);
        output[i] = glm::vec3(result);
    }
#endif
}

/**
 * @brief Transform multiple directions by a matrix (SIMD batch)
 */
inline void TransformDirections(const Mat4f& matrix,
                                 const glm::vec3* input,
                                 glm::vec3* output,
                                 size_t count) {
#ifdef NOVA_SSE_SUPPORT
    for (size_t i = 0; i < count; ++i) {
        Vec4f dir(input[i], 0.0f);
        Vec4f result = matrix * dir;
        output[i] = result.ToGLM3();
    }
#else
    glm::mat4 m = matrix;
    for (size_t i = 0; i < count; ++i) {
        glm::vec4 result = m * glm::vec4(input[i], 0.0f);
        output[i] = glm::vec3(result);
    }
#endif
}

/**
 * @brief Compute squared distances (SIMD batch)
 */
inline void ComputeDistancesSquared(const glm::vec3& origin,
                                     const glm::vec3* positions,
                                     float* distances,
                                     size_t count) {
#ifdef NOVA_SSE_SUPPORT
    Vec4f org(origin, 0.0f);
    for (size_t i = 0; i < count; ++i) {
        Vec4f diff = Vec4f(positions[i], 0.0f) - org;
        distances[i] = LengthSquared3(diff);
    }
#else
    for (size_t i = 0; i < count; ++i) {
        glm::vec3 diff = positions[i] - origin;
        distances[i] = glm::dot(diff, diff);
    }
#endif
}

/**
 * @brief AABB-AABB intersection test (SIMD)
 */
[[nodiscard]] inline bool AABBIntersects(const glm::vec3& minA, const glm::vec3& maxA,
                                          const glm::vec3& minB, const glm::vec3& maxB) {
#ifdef NOVA_SSE_SUPPORT
    Vec4f vMinA(minA, 0);
    Vec4f vMaxA(maxA, 0);
    Vec4f vMinB(minB, 0);
    Vec4f vMaxB(maxB, 0);

    // Test all axes simultaneously
    __m128 gtMin = _mm_cmpgt_ps(vMaxA.data, vMinB.data);
    __m128 ltMax = _mm_cmplt_ps(vMinA.data, vMaxB.data);
    __m128 overlap = _mm_and_ps(gtMin, ltMax);

    // Check first 3 components
    return (_mm_movemask_ps(overlap) & 0x7) == 0x7;
#else
    return (maxA.x > minB.x && minA.x < maxB.x) &&
           (maxA.y > minB.y && minA.y < maxB.y) &&
           (maxA.z > minB.z && minA.z < maxB.z);
#endif
}

/**
 * @brief Frustum culling test (SIMD)
 */
[[nodiscard]] inline bool SphereInFrustum(const glm::vec4 planes[6],
                                           const glm::vec3& center,
                                           float radius) {
#ifdef NOVA_SSE_SUPPORT
    Vec4f c(center, 1.0f);
    __m128 r = _mm_set1_ps(-radius);

    for (int i = 0; i < 6; ++i) {
        Vec4f plane(planes[i]);
        float dist = Dot4(plane, c);
        if (dist < -radius) {
            return false;
        }
    }
    return true;
#else
    for (int i = 0; i < 6; ++i) {
        float dist = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (dist < -radius) {
            return false;
        }
    }
    return true;
#endif
}

/**
 * @brief Batch add vectors (SIMD)
 */
inline void AddVectors(const float* a, const float* b, float* result, size_t count) {
#ifdef NOVA_SSE_SUPPORT
    size_t simdCount = count / 4;
    for (size_t i = 0; i < simdCount; ++i) {
        __m128 va = _mm_loadu_ps(a + i * 4);
        __m128 vb = _mm_loadu_ps(b + i * 4);
        _mm_storeu_ps(result + i * 4, _mm_add_ps(va, vb));
    }
    // Handle remainder
    for (size_t i = simdCount * 4; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
#else
    for (size_t i = 0; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
#endif
}

/**
 * @brief Batch multiply-add (a * b + c) vectors (SIMD)
 */
inline void MultiplyAddVectors(const float* a, const float* b, const float* c,
                                float* result, size_t count) {
#ifdef NOVA_SSE_SUPPORT
    size_t simdCount = count / 4;
    for (size_t i = 0; i < simdCount; ++i) {
        __m128 va = _mm_loadu_ps(a + i * 4);
        __m128 vb = _mm_loadu_ps(b + i * 4);
        __m128 vc = _mm_loadu_ps(c + i * 4);
        _mm_storeu_ps(result + i * 4, _mm_add_ps(_mm_mul_ps(va, vb), vc));
    }
    for (size_t i = simdCount * 4; i < count; ++i) {
        result[i] = a[i] * b[i] + c[i];
    }
#else
    for (size_t i = 0; i < count; ++i) {
        result[i] = a[i] * b[i] + c[i];
    }
#endif
}

} // namespace SIMD
} // namespace Nova
