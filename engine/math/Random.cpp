#include "math/Random.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Nova {

std::mt19937& Random::GetEngine() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

std::mutex& Random::GetMutex() {
    static std::mutex mutex;
    return mutex;
}

void Random::Seed(unsigned int seed) {
    std::lock_guard lock(GetMutex());
    GetEngine().seed(seed);
}

float Random::Value() {
    std::lock_guard lock(GetMutex());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(GetEngine());
}

float Random::Range(float min, float max) {
    std::lock_guard lock(GetMutex());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(GetEngine());
}

int Random::Range(int min, int max) {
    std::lock_guard lock(GetMutex());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(GetEngine());
}

bool Random::Bool(float probability) {
    return Value() < probability;
}

glm::vec3 Random::InUnitSphere() {
    // Rejection sampling - thread-safe through Range calls
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
    // More efficient than normalizing rejection sample
    float theta = Range(0.0f, glm::two_pi<float>());
    float phi = std::acos(Range(-1.0f, 1.0f));
    float sinPhi = std::sin(phi);
    return glm::vec3(
        sinPhi * std::cos(theta),
        sinPhi * std::sin(theta),
        std::cos(phi)
    );
}

glm::vec2 Random::InUnitCircle() {
    glm::vec2 p;
    do {
        p = glm::vec2(Range(-1.0f, 1.0f), Range(-1.0f, 1.0f));
    } while (glm::length2(p) >= 1.0f);
    return p;
}

glm::vec2 Random::OnUnitCircle() {
    float angle = Angle();
    return glm::vec2(std::cos(angle), std::sin(angle));
}

glm::vec3 Random::Direction() {
    return OnUnitSphere();
}

glm::vec2 Random::Direction2D() {
    return OnUnitCircle();
}

glm::vec3 Random::Color() {
    return glm::vec3(Value(), Value(), Value());
}

glm::vec4 Random::Color(float alpha) {
    return glm::vec4(Value(), Value(), Value(), alpha);
}

float Random::Angle() {
    return Range(0.0f, glm::two_pi<float>());
}

int Random::Sign() {
    return Bool() ? 1 : -1;
}

glm::quat Random::Rotation() {
    // Uniform random rotation using Shoemake's method
    float u1 = Value();
    float u2 = Value();
    float u3 = Value();

    float sq1 = std::sqrt(1.0f - u1);
    float sq2 = std::sqrt(u1);
    float twoPiU2 = glm::two_pi<float>() * u2;
    float twoPiU3 = glm::two_pi<float>() * u3;

    return glm::quat(
        sq1 * std::sin(twoPiU2),
        sq1 * std::cos(twoPiU2),
        sq2 * std::sin(twoPiU3),
        sq2 * std::cos(twoPiU3)
    );
}

} // namespace Nova
