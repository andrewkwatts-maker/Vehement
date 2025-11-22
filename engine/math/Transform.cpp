#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

namespace Transform {

glm::mat4 Compose(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rot = glm::mat4_cast(rotation);
    glm::mat4 scl = glm::scale(glm::mat4(1.0f), scale);
    return translation * rot * scl;
}

void Decompose(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale) {
    position = glm::vec3(matrix[3]);

    scale.x = glm::length(glm::vec3(matrix[0]));
    scale.y = glm::length(glm::vec3(matrix[1]));
    scale.z = glm::length(glm::vec3(matrix[2]));

    glm::mat3 rotMat(
        glm::vec3(matrix[0]) / scale.x,
        glm::vec3(matrix[1]) / scale.y,
        glm::vec3(matrix[2]) / scale.z
    );
    rotation = glm::quat_cast(rotMat);
}

glm::mat4 LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    return glm::lookAt(position, target, up);
}

glm::mat4 Perspective(float fovDegrees, float aspect, float near, float far) {
    return glm::perspective(glm::radians(fovDegrees), aspect, near, far);
}

glm::mat4 Orthographic(float left, float right, float bottom, float top, float near, float far) {
    return glm::ortho(left, right, bottom, top, near, far);
}

} // namespace Transform

} // namespace Nova
