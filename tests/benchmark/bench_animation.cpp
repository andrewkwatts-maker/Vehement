/**
 * @file bench_animation.cpp
 * @brief Performance benchmarks for animation system
 */

#include <benchmark/benchmark.h>

#include "animation/Keyframe.hpp"
#include "animation/Animation.hpp"
#include "animation/AnimationChannel.hpp"
#include "animation/AnimationBlend.hpp"
#include "animation/AnimationController.hpp"

#include "utils/Generators.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <vector>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Keyframe Benchmarks
// =============================================================================

static void BM_Keyframe_Construction(benchmark::State& state) {
    for (auto _ : state) {
        Keyframe kf;
        kf.time = 0.0f;
        kf.position = glm::vec3(1.0f, 2.0f, 3.0f);
        kf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        kf.scale = glm::vec3(1.0f);
        benchmark::DoNotOptimize(kf);
    }
}
BENCHMARK(BM_Keyframe_Construction);

static void BM_Keyframe_Lerp(benchmark::State& state) {
    Keyframe a, b;
    a.time = 0.0f;
    a.position = glm::vec3(0.0f);
    a.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    a.scale = glm::vec3(1.0f);

    b.time = 1.0f;
    b.position = glm::vec3(10.0f, 20.0f, 30.0f);
    b.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    b.scale = glm::vec3(2.0f);

    for (auto _ : state) {
        Keyframe result = Keyframe::Lerp(a, b, 0.5f);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Keyframe_Lerp);

static void BM_Keyframe_Slerp_Quaternion(benchmark::State& state) {
    glm::quat a = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat b = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    for (auto _ : state) {
        glm::quat result = glm::slerp(a, b, 0.5f);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Keyframe_Slerp_Quaternion);

static void BM_Keyframe_ToMatrix(benchmark::State& state) {
    Keyframe kf;
    kf.position = glm::vec3(10.0f, 20.0f, 30.0f);
    kf.rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    kf.scale = glm::vec3(2.0f);

    for (auto _ : state) {
        glm::mat4 matrix = KeyframeUtils::ToMatrix(kf);
        benchmark::DoNotOptimize(matrix);
    }
}
BENCHMARK(BM_Keyframe_ToMatrix);

static void BM_Keyframe_FromMatrix(benchmark::State& state) {
    glm::mat4 matrix = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f)) *
                       glm::mat4_cast(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f))) *
                       glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));

    for (auto _ : state) {
        Keyframe kf = KeyframeUtils::FromMatrix(matrix);
        benchmark::DoNotOptimize(kf);
    }
}
BENCHMARK(BM_Keyframe_FromMatrix);

// =============================================================================
// Animation Channel Benchmarks
// =============================================================================

class AnimationBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) {
        rng = RandomGenerator(42);
    }

    AnimationChannel CreateTestChannel(int keyframeCount) {
        AnimationChannel channel;
        channel.SetBoneName("test_bone");

        float duration = static_cast<float>(keyframeCount - 1) * 0.1f;

        for (int i = 0; i < keyframeCount; ++i) {
            Keyframe kf;
            kf.time = static_cast<float>(i) * 0.1f;
            kf.position = glm::vec3(
                std::sin(kf.time * 3.14159f) * 10.0f,
                std::cos(kf.time * 3.14159f) * 5.0f,
                kf.time
            );
            kf.rotation = glm::angleAxis(kf.time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            kf.scale = glm::vec3(1.0f + 0.1f * kf.time);
            channel.AddKeyframe(kf);
        }

        return channel;
    }

    RandomGenerator rng{42};
};

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Channel_Evaluate_Linear)(benchmark::State& state) {
    int keyframeCount = state.range(0);
    AnimationChannel channel = CreateTestChannel(keyframeCount);
    channel.SetInterpolation(InterpolationType::Linear);

    float maxTime = (keyframeCount - 1) * 0.1f;
    float time = maxTime * 0.5f;

    for (auto _ : state) {
        Keyframe result = channel.Evaluate(time);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Channel_Evaluate_Linear)
    ->RangeMultiplier(2)
    ->Range(10, 1000);

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Channel_Evaluate_CubicSpline)(benchmark::State& state) {
    int keyframeCount = state.range(0);
    AnimationChannel channel = CreateTestChannel(keyframeCount);
    channel.SetInterpolation(InterpolationType::CubicSpline);

    float maxTime = (keyframeCount - 1) * 0.1f;
    float time = maxTime * 0.5f;

    for (auto _ : state) {
        Keyframe result = channel.Evaluate(time);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Channel_Evaluate_CubicSpline)
    ->RangeMultiplier(2)
    ->Range(10, 1000);

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Channel_Evaluate_Step)(benchmark::State& state) {
    int keyframeCount = state.range(0);
    AnimationChannel channel = CreateTestChannel(keyframeCount);
    channel.SetInterpolation(InterpolationType::Step);

    float maxTime = (keyframeCount - 1) * 0.1f;
    float time = maxTime * 0.5f;

    for (auto _ : state) {
        Keyframe result = channel.Evaluate(time);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Channel_Evaluate_Step)
    ->RangeMultiplier(2)
    ->Range(10, 1000);

// =============================================================================
// Full Animation Benchmarks
// =============================================================================

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Animation_Evaluate_MultiBone)(benchmark::State& state) {
    int boneCount = state.range(0);
    int keyframeCount = 100;

    Animation anim;
    anim.SetName("test_animation");
    anim.SetDuration((keyframeCount - 1) * 0.1f);

    for (int bone = 0; bone < boneCount; ++bone) {
        AnimationChannel channel = CreateTestChannel(keyframeCount);
        channel.SetBoneName("bone_" + std::to_string(bone));
        anim.AddChannel(std::move(channel));
    }

    float time = anim.GetDuration() * 0.5f;

    for (auto _ : state) {
        auto result = anim.Evaluate(time);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * boneCount);
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Animation_Evaluate_MultiBone)
    ->RangeMultiplier(2)
    ->Range(10, 100);

// =============================================================================
// Animation Blending Benchmarks
// =============================================================================

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Animation_Blend_Two)(benchmark::State& state) {
    int boneCount = state.range(0);
    int keyframeCount = 50;

    Animation anim1, anim2;
    anim1.SetName("anim1");
    anim1.SetDuration((keyframeCount - 1) * 0.1f);
    anim2.SetName("anim2");
    anim2.SetDuration((keyframeCount - 1) * 0.1f);

    for (int bone = 0; bone < boneCount; ++bone) {
        AnimationChannel channel1 = CreateTestChannel(keyframeCount);
        channel1.SetBoneName("bone_" + std::to_string(bone));
        anim1.AddChannel(std::move(channel1));

        AnimationChannel channel2 = CreateTestChannel(keyframeCount);
        channel2.SetBoneName("bone_" + std::to_string(bone));
        anim2.AddChannel(std::move(channel2));
    }

    float time1 = anim1.GetDuration() * 0.3f;
    float time2 = anim2.GetDuration() * 0.7f;
    float blendWeight = 0.5f;

    for (auto _ : state) {
        auto pose1 = anim1.Evaluate(time1);
        auto pose2 = anim2.Evaluate(time2);

        // Blend poses
        std::unordered_map<std::string, Keyframe> blendedPose;
        for (const auto& [name, kf1] : pose1) {
            auto it = pose2.find(name);
            if (it != pose2.end()) {
                blendedPose[name] = Keyframe::Lerp(kf1, it->second, blendWeight);
            }
        }
        benchmark::DoNotOptimize(blendedPose);
    }

    state.SetItemsProcessed(state.iterations() * boneCount);
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Animation_Blend_Two)
    ->RangeMultiplier(2)
    ->Range(10, 100);

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Animation_Blend_Additive)(benchmark::State& state) {
    int boneCount = state.range(0);
    int keyframeCount = 50;

    Animation baseAnim, additiveAnim;
    baseAnim.SetName("base");
    baseAnim.SetDuration((keyframeCount - 1) * 0.1f);
    additiveAnim.SetName("additive");
    additiveAnim.SetDuration((keyframeCount - 1) * 0.1f);

    for (int bone = 0; bone < boneCount; ++bone) {
        AnimationChannel channel1 = CreateTestChannel(keyframeCount);
        channel1.SetBoneName("bone_" + std::to_string(bone));
        baseAnim.AddChannel(std::move(channel1));

        AnimationChannel channel2 = CreateTestChannel(keyframeCount);
        channel2.SetBoneName("bone_" + std::to_string(bone));
        additiveAnim.AddChannel(std::move(channel2));
    }

    float time = baseAnim.GetDuration() * 0.5f;
    float additiveWeight = 0.3f;

    for (auto _ : state) {
        auto basePose = baseAnim.Evaluate(time);
        auto additivePose = additiveAnim.Evaluate(time);

        // Apply additive blending
        for (auto& [name, kf] : basePose) {
            auto it = additivePose.find(name);
            if (it != additivePose.end()) {
                kf.position += it->second.position * additiveWeight;
                // Additive rotation would use quaternion multiplication
            }
        }
        benchmark::DoNotOptimize(basePose);
    }

    state.SetItemsProcessed(state.iterations() * boneCount);
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Animation_Blend_Additive)
    ->RangeMultiplier(2)
    ->Range(10, 100);

// =============================================================================
// Skeletal Update Benchmarks
// =============================================================================

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Skeleton_ComputeGlobalTransforms)(benchmark::State& state) {
    int boneCount = state.range(0);

    // Create a linear bone hierarchy
    std::vector<int> parentIndices(boneCount);
    std::vector<glm::mat4> localTransforms(boneCount);
    std::vector<glm::mat4> globalTransforms(boneCount);

    for (int i = 0; i < boneCount; ++i) {
        parentIndices[i] = i > 0 ? i - 1 : -1;  // Linear chain
        localTransforms[i] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    for (auto _ : state) {
        // Compute global transforms
        for (int i = 0; i < boneCount; ++i) {
            if (parentIndices[i] >= 0) {
                globalTransforms[i] = globalTransforms[parentIndices[i]] * localTransforms[i];
            } else {
                globalTransforms[i] = localTransforms[i];
            }
        }
        benchmark::DoNotOptimize(globalTransforms);
    }

    state.SetItemsProcessed(state.iterations() * boneCount);
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Skeleton_ComputeGlobalTransforms)
    ->RangeMultiplier(2)
    ->Range(10, 200);

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Skeleton_ComputeSkinningMatrices)(benchmark::State& state) {
    int boneCount = state.range(0);

    std::vector<glm::mat4> globalTransforms(boneCount);
    std::vector<glm::mat4> inverseBindMatrices(boneCount);
    std::vector<glm::mat4> skinningMatrices(boneCount);

    // Initialize with identity
    for (int i = 0; i < boneCount; ++i) {
        globalTransforms[i] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, static_cast<float>(i), 0.0f));
        inverseBindMatrices[i] = glm::inverse(globalTransforms[i]);
    }

    for (auto _ : state) {
        for (int i = 0; i < boneCount; ++i) {
            skinningMatrices[i] = globalTransforms[i] * inverseBindMatrices[i];
        }
        benchmark::DoNotOptimize(skinningMatrices);
    }

    state.SetItemsProcessed(state.iterations() * boneCount);
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Skeleton_ComputeSkinningMatrices)
    ->RangeMultiplier(2)
    ->Range(10, 200);

// =============================================================================
// Matrix Operations Benchmarks
// =============================================================================

static void BM_Matrix_Multiply(benchmark::State& state) {
    glm::mat4 a = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
    glm::mat4 b = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    for (auto _ : state) {
        glm::mat4 result = a * b;
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Matrix_Multiply);

static void BM_Matrix_Inverse(benchmark::State& state) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f)) *
                  glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                  glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));

    for (auto _ : state) {
        glm::mat4 result = glm::inverse(m);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Matrix_Inverse);

static void BM_Matrix_Decompose(benchmark::State& state) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f)) *
                  glm::mat4_cast(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f))) *
                  glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));

    for (auto _ : state) {
        glm::vec3 scale, translation, skew;
        glm::quat rotation;
        glm::vec4 perspective;
        glm::decompose(m, scale, rotation, translation, skew, perspective);
        benchmark::DoNotOptimize(translation);
        benchmark::DoNotOptimize(rotation);
        benchmark::DoNotOptimize(scale);
    }
}
BENCHMARK(BM_Matrix_Decompose);

// =============================================================================
// Animation Sampling at Different Rates
// =============================================================================

BENCHMARK_DEFINE_F(AnimationBenchmark, BM_Animation_Sample_Rate)(benchmark::State& state) {
    int samplesPerSecond = state.range(0);
    int boneCount = 50;
    int keyframeCount = 100;

    Animation anim;
    anim.SetName("test_animation");
    anim.SetDuration(10.0f);  // 10 second animation

    for (int bone = 0; bone < boneCount; ++bone) {
        AnimationChannel channel = CreateTestChannel(keyframeCount);
        channel.SetBoneName("bone_" + std::to_string(bone));
        anim.AddChannel(std::move(channel));
    }

    float deltaTime = 1.0f / static_cast<float>(samplesPerSecond);

    for (auto _ : state) {
        // Sample entire animation at given rate
        float time = 0.0f;
        while (time < anim.GetDuration()) {
            auto result = anim.Evaluate(time);
            benchmark::DoNotOptimize(result);
            time += deltaTime;
        }
    }

    int totalSamples = static_cast<int>(anim.GetDuration() * samplesPerSecond);
    state.SetItemsProcessed(state.iterations() * totalSamples * boneCount);
}
BENCHMARK_REGISTER_F(AnimationBenchmark, BM_Animation_Sample_Rate)
    ->Arg(30)   // 30 FPS
    ->Arg(60)   // 60 FPS
    ->Arg(120); // 120 FPS

// Main function for benchmark
BENCHMARK_MAIN();

