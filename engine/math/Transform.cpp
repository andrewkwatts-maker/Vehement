#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <tuple>

namespace Nova {

namespace Transform {

/**
 * @brief Compose a transformation matrix from position, rotation, and scale
 * @param position Translation component
 * @param rotation Rotation as quaternion
 * @param scale Scale factors
 * @return Combined transformation matrix (TRS order)
 */
[[nodiscard]] glm::mat4 Compose(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) noexcept {
    // Optimized: compute directly without intermediate matrices
    glm::mat4 result = glm::mat4_cast(rotation);

    // Apply scale
    result[0] *= scale.x;
    result[1] *= scale.y;
    result[2] *= scale.z;

    // Apply translation
    result[3] = glm::vec4(position, 1.0f);

    return result;
}

/**
 * @brief Decompose a transformation matrix into position, rotation, and scale
 * @param matrix The transformation matrix to decompose
 * @param position Output translation component
 * @param rotation Output rotation as quaternion
 * @param scale Output scale factors
 * @note Assumes the matrix has no shear; results are undefined for sheared matrices
 */
void Decompose(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale) noexcept {
    position = glm::vec3(matrix[3]);

    // Extract scale (length of each basis vector)
    scale.x = glm::length(glm::vec3(matrix[0]));
    scale.y = glm::length(glm::vec3(matrix[1]));
    scale.z = glm::length(glm::vec3(matrix[2]));

    // Handle zero scale to avoid division by zero
    constexpr float epsilon = 1e-6f;
    const float sx = scale.x > epsilon ? scale.x : 1.0f;
    const float sy = scale.y > epsilon ? scale.y : 1.0f;
    const float sz = scale.z > epsilon ? scale.z : 1.0f;

    // Normalize rotation matrix
    glm::mat3 rotMat(
        glm::vec3(matrix[0]) / sx,
        glm::vec3(matrix[1]) / sy,
        glm::vec3(matrix[2]) / sz
    );
    rotation = glm::quat_cast(rotMat);
}

/**
 * @brief Decompose returning a tuple (more functional style)
 */
[[nodiscard]] std::tuple<glm::vec3, glm::quat, glm::vec3> Decompose(const glm::mat4& matrix) noexcept {
    glm::vec3 position, scale;
    glm::quat rotation;
    Decompose(matrix, position, rotation, scale);
    return {position, rotation, scale};
}

/**
 * @brief Create a view matrix looking from position toward target
 */
[[nodiscard]] glm::mat4 LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) noexcept {
    return glm::lookAt(position, target, up);
}

/**
 * @brief Create a perspective projection matrix
 * @param fovDegrees Vertical field of view in degrees
 * @param aspect Aspect ratio (width/height)
 * @param nearPlane Near clipping plane distance (must be > 0)
 * @param farPlane Far clipping plane distance (must be > near)
 */
[[nodiscard]] glm::mat4 Perspective(float fovDegrees, float aspect, float nearPlane, float farPlane) noexcept {
    return glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
}

/**
 * @brief Create an orthographic projection matrix
 */
[[nodiscard]] glm::mat4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) noexcept {
    return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
}

/**
 * @brief Linearly interpolate between two transforms
 */
[[nodiscard]] glm::mat4 Lerp(const glm::mat4& a, const glm::mat4& b, float t) noexcept {
    auto [posA, rotA, scaleA] = Decompose(a);
    auto [posB, rotB, scaleB] = Decompose(b);

    glm::vec3 position = glm::mix(posA, posB, t);
    glm::quat rotation = glm::slerp(rotA, rotB, t);
    glm::vec3 scale = glm::mix(scaleA, scaleB, t);

    return Compose(position, rotation, scale);
}

/**
 * @brief Get the forward direction from a transformation matrix
 */
[[nodiscard]] glm::vec3 Forward(const glm::mat4& matrix) noexcept {
    return -glm::normalize(glm::vec3(matrix[2]));
}

/**
 * @brief Get the right direction from a transformation matrix
 */
[[nodiscard]] glm::vec3 Right(const glm::mat4& matrix) noexcept {
    return glm::normalize(glm::vec3(matrix[0]));
}

/**
 * @brief Get the up direction from a transformation matrix
 */
[[nodiscard]] glm::vec3 Up(const glm::mat4& matrix) noexcept {
    return glm::normalize(glm::vec3(matrix[1]));
}

} // namespace Transform

} // namespace Nova
