[[nodiscard]] float DistanceToSegmentSquared(const glm::vec3& point, const glm::vec3& a, const glm::vec3& b) noexcept {
    const glm::vec3 diff = point - ClosestPointOnSegment(point, a, b);
    return glm::dot(diff, diff);
}

/**
 * @brief Test if a point is inside a sphere
 */
[[nodiscard]] bool PointInSphere(const glm::vec3& point, const glm::vec3& center, float radius) noexcept {
    const glm::vec3 diff = point - center;
    return glm::dot(diff, diff) <= radius * radius;
}
