#include "math/Random.hpp"
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace Nova {

std::mt19937& Random::GetEngine() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

void Random::Seed(unsigned int seed) {
    GetEngine().seed(seed);
}

float Random::Value() {
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(GetEngine());
}

float Random::Range(float min, float max) {
    return min + Value() * (max - min);
}

int Random::Range(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(GetEngine());
}

glm::vec3 Random::InUnitSphere() {
    // Rejection sampling
    glm::vec3 p;
    do {
        p = glm::vec3(
            Range(-1.0f, 1.0f),
            Range(-1.0f, 1.0f),
            Range(-1.0f, 1.0f)
        );
    } while (glm::length2(p) >= 1.0f);
    return p;
}

glm::vec3 Random::OnUnitSphere() {
    return glm::normalize(InUnitSphere());
}

glm::vec2 Random::InUnitCircle() {
    glm::vec2 p;
    do {
        p = glm::vec2(Range(-1.0f, 1.0f), Range(-1.0f, 1.0f));
    } while (glm::length2(p) >= 1.0f);
    return p;
}

glm::vec3 Random::Direction() {
    return OnUnitSphere();
}

glm::vec3 Random::Color() {
    return glm::vec3(Value(), Value(), Value());
}

glm::quat Random::Rotation() {
    // Uniform random rotation
    float u1 = Value();
    float u2 = Value();
    float u3 = Value();

    float sq1 = std::sqrt(1.0f - u1);
    float sq2 = std::sqrt(u1);

    return glm::quat(
        sq1 * std::sin(2.0f * glm::pi<float>() * u2),
        sq1 * std::cos(2.0f * glm::pi<float>() * u2),
        sq2 * std::sin(2.0f * glm::pi<float>() * u3),
        sq2 * std::cos(2.0f * glm::pi<float>() * u3)
    );
}

} // namespace Nova
