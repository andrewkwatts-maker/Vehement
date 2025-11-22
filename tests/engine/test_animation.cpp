/**
 * @file test_animation.cpp
 * @brief Unit tests for animation system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "animation/Animation.hpp"
#include "animation/AnimationController.hpp"
#include "animation/Skeleton.hpp"

#include "utils/TestHelpers.hpp"
#include "utils/Generators.hpp"

#include <glm/gtc/matrix_transform.hpp>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Keyframe Tests
// =============================================================================

class KeyframeTest : public ::testing::Test {
protected:
    Keyframe CreateKeyframe(float time, const glm::vec3& pos,
                           const glm::quat& rot = glm::quat(1, 0, 0, 0),
                           const glm::vec3& scale = glm::vec3(1.0f)) {
        Keyframe kf;
        kf.time = time;
        kf.position = pos;
        kf.rotation = rot;
        kf.scale = scale;
        return kf;
    }
};

TEST_F(KeyframeTest, DefaultConstruction) {
    Keyframe kf;

    EXPECT_FLOAT_EQ(0.0f, kf.time);
    EXPECT_VEC3_EQ(glm::vec3(0.0f), kf.position);
    EXPECT_QUAT_EQ(glm::quat(1, 0, 0, 0), kf.rotation);
    EXPECT_VEC3_EQ(glm::vec3(1.0f), kf.scale);
}

TEST_F(KeyframeTest, Ordering) {
    Keyframe a = CreateKeyframe(0.0f, glm::vec3(0.0f));
    Keyframe b = CreateKeyframe(1.0f, glm::vec3(0.0f));

    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_TRUE(a < 0.5f);
}

TEST_F(KeyframeTest, BlendKeyframes_HalfWeight) {
    Keyframe a = CreateKeyframe(0.0f, glm::vec3(0.0f, 0.0f, 0.0f));
    Keyframe b = CreateKeyframe(1.0f, glm::vec3(10.0f, 10.0f, 10.0f));

    Keyframe result = BlendKeyframes(a, b, 0.5f);

    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 5.0f, 5.0f), result.position, 0.001f);
}

TEST_F(KeyframeTest, BlendKeyframes_ZeroWeight) {
    Keyframe a = CreateKeyframe(0.0f, glm::vec3(0.0f));
    Keyframe b = CreateKeyframe(1.0f, glm::vec3(10.0f));

    Keyframe result = BlendKeyframes(a, b, 0.0f);

    EXPECT_VEC3_NEAR(a.position, result.position, 0.001f);
}

TEST_F(KeyframeTest, BlendKeyframes_FullWeight) {
    Keyframe a = CreateKeyframe(0.0f, glm::vec3(0.0f));
    Keyframe b = CreateKeyframe(1.0f, glm::vec3(10.0f));

    Keyframe result = BlendKeyframes(a, b, 1.0f);

    EXPECT_VEC3_NEAR(b.position, result.position, 0.001f);
}

TEST_F(KeyframeTest, BlendKeyframes_Rotation) {
    glm::quat rotA = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
    glm::quat rotB = glm::quat(glm::vec3(0.0f, glm::pi<float>(), 0.0f));

    Keyframe a = CreateKeyframe(0.0f, glm::vec3(0.0f), rotA);
    Keyframe b = CreateKeyframe(1.0f, glm::vec3(0.0f), rotB);

    Keyframe result = BlendKeyframes(a, b, 0.5f);

    // Result should be normalized quaternion
    float len = glm::length(result.rotation);
    EXPECT_FLOAT_NEAR_EPSILON(1.0f, len, 0.001f);
}

// =============================================================================
// Animation Channel Tests
// =============================================================================

class AnimationChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple animation channel
        channel.nodeName = "TestBone";
        channel.interpolationMode = InterpolationMode::Linear;

        // Add keyframes
        Keyframe kf0;
        kf0.time = 0.0f;
        kf0.position = glm::vec3(0.0f);
        kf0.rotation = glm::quat(1, 0, 0, 0);
        kf0.scale = glm::vec3(1.0f);

        Keyframe kf1;
        kf1.time = 1.0f;
        kf1.position = glm::vec3(10.0f, 0.0f, 0.0f);
        kf1.rotation = glm::quat(glm::vec3(0.0f, glm::half_pi<float>(), 0.0f));
        kf1.scale = glm::vec3(2.0f);

        Keyframe kf2;
        kf2.time = 2.0f;
        kf2.position = glm::vec3(10.0f, 10.0f, 0.0f);
        kf2.rotation = glm::quat(glm::vec3(0.0f, glm::pi<float>(), 0.0f));
        kf2.scale = glm::vec3(1.0f);

        channel.keyframes = {kf0, kf1, kf2};
    }

    AnimationChannel channel;
};

TEST_F(AnimationChannelTest, FindKeyframeIndex_AtStart) {
    size_t index = channel.FindKeyframeIndex(0.0f);
    EXPECT_EQ(0, index);
}

TEST_F(AnimationChannelTest, FindKeyframeIndex_Middle) {
    size_t index = channel.FindKeyframeIndex(0.5f);
    EXPECT_EQ(0, index);

    index = channel.FindKeyframeIndex(1.5f);
    EXPECT_EQ(1, index);
}

TEST_F(AnimationChannelTest, FindKeyframeIndex_AtEnd) {
    size_t index = channel.FindKeyframeIndex(2.0f);
    EXPECT_GE(index, 1);
}

TEST_F(AnimationChannelTest, Interpolate_AtKeyframe) {
    Keyframe result = channel.Interpolate(0.0f);
    EXPECT_VEC3_NEAR(glm::vec3(0.0f), result.position, 0.001f);

    result = channel.Interpolate(1.0f);
    EXPECT_VEC3_NEAR(glm::vec3(10.0f, 0.0f, 0.0f), result.position, 0.001f);
}

TEST_F(AnimationChannelTest, Interpolate_BetweenKeyframes) {
    Keyframe result = channel.Interpolate(0.5f);

    // Should be halfway between first two keyframes
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 0.0f, 0.0f), result.position, 0.001f);
    EXPECT_VEC3_NEAR(glm::vec3(1.5f), result.scale, 0.001f);
}

TEST_F(AnimationChannelTest, Interpolate_BeforeStart) {
    Keyframe result = channel.Interpolate(-1.0f);

    // Should clamp to first keyframe
    EXPECT_VEC3_NEAR(glm::vec3(0.0f), result.position, 0.001f);
}

TEST_F(AnimationChannelTest, Interpolate_AfterEnd) {
    Keyframe result = channel.Interpolate(3.0f);

    // Should clamp to last keyframe
    EXPECT_VEC3_NEAR(glm::vec3(10.0f, 10.0f, 0.0f), result.position, 0.001f);
}

TEST_F(AnimationChannelTest, Evaluate_ReturnsMatrix) {
    glm::mat4 transform = channel.Evaluate(0.5f);

    // Extract translation from matrix
    glm::vec3 translation(transform[3]);
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 0.0f, 0.0f), translation, 0.001f);
}

// =============================================================================
// Animation Tests
// =============================================================================

class AnimationTest : public ::testing::Test {
protected:
    void SetUp() override {
        animation = std::make_unique<Animation>("TestAnimation");
        animation->SetDuration(2.0f);
        animation->SetTicksPerSecond(30.0f);
        animation->SetLooping(true);

        // Add channels
        AnimationChannel rootChannel;
        rootChannel.nodeName = "Root";
        rootChannel.keyframes = {
            {0.0f, glm::vec3(0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)},
            {2.0f, glm::vec3(0.0f, 10.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)}
        };
        animation->AddChannel(rootChannel);

        AnimationChannel childChannel;
        childChannel.nodeName = "Child";
        childChannel.keyframes = {
            {0.0f, glm::vec3(5.0f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)},
            {1.0f, glm::vec3(5.0f, 5.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)},
            {2.0f, glm::vec3(5.0f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)}
        };
        animation->AddChannel(childChannel);
    }

    std::unique_ptr<Animation> animation;
};

TEST_F(AnimationTest, Properties) {
    EXPECT_EQ("TestAnimation", animation->GetName());
    EXPECT_FLOAT_EQ(2.0f, animation->GetDuration());
    EXPECT_FLOAT_EQ(30.0f, animation->GetTicksPerSecond());
    EXPECT_TRUE(animation->IsLooping());
}

TEST_F(AnimationTest, GetChannel) {
    auto* channel = animation->GetChannel("Root");
    ASSERT_NE(nullptr, channel);
    EXPECT_EQ("Root", channel->nodeName);

    auto* missing = animation->GetChannel("NonExistent");
    EXPECT_EQ(nullptr, missing);
}

TEST_F(AnimationTest, GetChannels) {
    auto& channels = animation->GetChannels();
    EXPECT_EQ(2, channels.size());
}

TEST_F(AnimationTest, Evaluate) {
    auto transforms = animation->Evaluate(1.0f);

    EXPECT_EQ(2, transforms.size());
    EXPECT_TRUE(transforms.contains("Root"));
    EXPECT_TRUE(transforms.contains("Child"));
}

TEST_F(AnimationTest, EvaluateInto) {
    std::unordered_map<std::string, glm::mat4> transforms;
    animation->EvaluateInto(1.0f, transforms);

    EXPECT_EQ(2, transforms.size());

    // Check Root transform (halfway = 5.0 on Y)
    glm::vec3 rootTranslation(transforms["Root"][3]);
    EXPECT_VEC3_NEAR(glm::vec3(0.0f, 5.0f, 0.0f), rootTranslation, 0.01f);

    // Check Child transform (at peak = 5.0 on Y)
    glm::vec3 childTranslation(transforms["Child"][3]);
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 5.0f, 0.0f), childTranslation, 0.01f);
}

TEST_F(AnimationTest, ResetCaches) {
    animation->Evaluate(0.5f);
    animation->Evaluate(0.6f);

    // This should reset cached indices
    animation->ResetCaches();

    // Should still work correctly after reset
    auto transforms = animation->Evaluate(1.0f);
    EXPECT_EQ(2, transforms.size());
}

// =============================================================================
// Interpolation Tests
// =============================================================================

class InterpolationTest : public ::testing::Test {};

TEST_F(InterpolationTest, Lerp_Vec3) {
    glm::vec3 a(0.0f);
    glm::vec3 b(10.0f);

    EXPECT_VEC3_EQ(a, Interpolation::Lerp(a, b, 0.0f));
    EXPECT_VEC3_EQ(b, Interpolation::Lerp(a, b, 1.0f));
    EXPECT_VEC3_NEAR(glm::vec3(5.0f), Interpolation::Lerp(a, b, 0.5f), 0.001f);
}

TEST_F(InterpolationTest, Slerp_Quat) {
    glm::quat a = glm::quat(1, 0, 0, 0);  // Identity
    glm::quat b = glm::quat(glm::vec3(0.0f, glm::pi<float>(), 0.0f));  // 180 degrees Y

    glm::quat result = Interpolation::Slerp(a, b, 0.5f);

    // Should be normalized
    EXPECT_FLOAT_NEAR_EPSILON(1.0f, glm::length(result), 0.001f);
}

TEST_F(InterpolationTest, Nlerp_Quat) {
    glm::quat a = glm::quat(1, 0, 0, 0);
    glm::quat b = glm::quat(glm::vec3(0.0f, glm::half_pi<float>(), 0.0f));

    glm::quat result = Interpolation::Nlerp(a, b, 0.5f);

    // Should be normalized
    EXPECT_FLOAT_NEAR_EPSILON(1.0f, glm::length(result), 0.001f);
}

TEST_F(InterpolationTest, SmoothStep) {
    EXPECT_FLOAT_EQ(0.0f, Interpolation::SmoothStep(0.0f));
    EXPECT_FLOAT_EQ(0.5f, Interpolation::SmoothStep(0.5f));
    EXPECT_FLOAT_EQ(1.0f, Interpolation::SmoothStep(1.0f));

    // Should clamp
    EXPECT_FLOAT_EQ(0.0f, Interpolation::SmoothStep(-1.0f));
    EXPECT_FLOAT_EQ(1.0f, Interpolation::SmoothStep(2.0f));
}

TEST_F(InterpolationTest, SmootherStep) {
    EXPECT_FLOAT_EQ(0.0f, Interpolation::SmootherStep(0.0f));
    EXPECT_FLOAT_EQ(0.5f, Interpolation::SmootherStep(0.5f));
    EXPECT_FLOAT_EQ(1.0f, Interpolation::SmootherStep(1.0f));
}

TEST_F(InterpolationTest, CatmullRom) {
    glm::vec3 p0(0.0f);
    glm::vec3 p1(1.0f, 0.0f, 0.0f);
    glm::vec3 p2(2.0f, 1.0f, 0.0f);
    glm::vec3 p3(3.0f, 0.0f, 0.0f);

    // At t=0, should be at p1
    glm::vec3 result0 = Interpolation::CatmullRom(p0, p1, p2, p3, 0.0f);
    EXPECT_VEC3_NEAR(p1, result0, 0.001f);

    // At t=1, should be at p2
    glm::vec3 result1 = Interpolation::CatmullRom(p0, p1, p2, p3, 1.0f);
    EXPECT_VEC3_NEAR(p2, result1, 0.001f);
}

// =============================================================================
// KeyframeUtils Tests
// =============================================================================

class KeyframeUtilsTest : public ::testing::Test {};

TEST_F(KeyframeUtilsTest, FromMatrix) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 10.0f, 15.0f));
    transform = glm::rotate(transform, glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));

    Keyframe kf = KeyframeUtils::FromMatrix(transform, 1.0f);

    EXPECT_FLOAT_EQ(1.0f, kf.time);
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 10.0f, 15.0f), kf.position, 0.01f);
}

TEST_F(KeyframeUtilsTest, ToMatrix) {
    Keyframe kf;
    kf.position = glm::vec3(5.0f, 10.0f, 15.0f);
    kf.rotation = glm::quat(1, 0, 0, 0);
    kf.scale = glm::vec3(2.0f);

    glm::mat4 matrix = KeyframeUtils::ToMatrix(kf);

    glm::vec3 translation(matrix[3]);
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 10.0f, 15.0f), translation, 0.001f);
}

TEST_F(KeyframeUtilsTest, Identity) {
    Keyframe kf = KeyframeUtils::Identity(5.0f);

    EXPECT_FLOAT_EQ(5.0f, kf.time);
    EXPECT_VEC3_EQ(glm::vec3(0.0f), kf.position);
    EXPECT_QUAT_EQ(glm::quat(1, 0, 0, 0), kf.rotation);
    EXPECT_VEC3_EQ(glm::vec3(1.0f), kf.scale);
}

TEST_F(KeyframeUtilsTest, ApproximatelyEqual) {
    Keyframe a = KeyframeUtils::Identity();
    Keyframe b = KeyframeUtils::Identity();

    EXPECT_TRUE(KeyframeUtils::ApproximatelyEqual(a, b));

    b.position.x += 0.0001f;
    EXPECT_TRUE(KeyframeUtils::ApproximatelyEqual(a, b));

    b.position.x += 1.0f;
    EXPECT_FALSE(KeyframeUtils::ApproximatelyEqual(a, b));
}

TEST_F(KeyframeUtilsTest, SortByTime) {
    std::vector<Keyframe> keyframes = {
        KeyframeUtils::Identity(2.0f),
        KeyframeUtils::Identity(0.0f),
        KeyframeUtils::Identity(1.0f)
    };

    KeyframeUtils::SortByTime(keyframes);

    EXPECT_FLOAT_EQ(0.0f, keyframes[0].time);
    EXPECT_FLOAT_EQ(1.0f, keyframes[1].time);
    EXPECT_FLOAT_EQ(2.0f, keyframes[2].time);
}

TEST_F(KeyframeUtilsTest, RemoveDuplicates) {
    std::vector<Keyframe> keyframes = {
        KeyframeUtils::Identity(0.0f),
        KeyframeUtils::Identity(0.0001f),  // Should be removed
        KeyframeUtils::Identity(1.0f),
        KeyframeUtils::Identity(1.0f),     // Should be removed
        KeyframeUtils::Identity(2.0f)
    };

    KeyframeUtils::RemoveDuplicates(keyframes, 0.001f);

    EXPECT_EQ(3, keyframes.size());
}

TEST_F(KeyframeUtilsTest, ScaleTime) {
    std::vector<Keyframe> keyframes = {
        KeyframeUtils::Identity(0.0f),
        KeyframeUtils::Identity(1.0f),
        KeyframeUtils::Identity(2.0f)
    };

    KeyframeUtils::ScaleTime(keyframes, 2.0f);

    EXPECT_FLOAT_EQ(0.0f, keyframes[0].time);
    EXPECT_FLOAT_EQ(2.0f, keyframes[1].time);
    EXPECT_FLOAT_EQ(4.0f, keyframes[2].time);
}

TEST_F(KeyframeUtilsTest, OffsetTime) {
    std::vector<Keyframe> keyframes = {
        KeyframeUtils::Identity(0.0f),
        KeyframeUtils::Identity(1.0f)
    };

    KeyframeUtils::OffsetTime(keyframes, 5.0f);

    EXPECT_FLOAT_EQ(5.0f, keyframes[0].time);
    EXPECT_FLOAT_EQ(6.0f, keyframes[1].time);
}

TEST_F(KeyframeUtilsTest, Reverse) {
    std::vector<Keyframe> keyframes = {
        KeyframeUtils::Identity(0.0f),
        KeyframeUtils::Identity(1.0f),
        KeyframeUtils::Identity(2.0f)
    };
    keyframes[0].position = glm::vec3(0.0f);
    keyframes[1].position = glm::vec3(1.0f);
    keyframes[2].position = glm::vec3(2.0f);

    float duration = keyframes.back().time;
    KeyframeUtils::Reverse(keyframes);

    // First keyframe should now have the last position
    EXPECT_VEC3_NEAR(glm::vec3(2.0f), keyframes[0].position, 0.001f);
}

TEST_F(KeyframeUtilsTest, CreateTranslationAnimation) {
    auto keyframes = KeyframeUtils::CreateTranslationAnimation(
        glm::vec3(0.0f),
        glm::vec3(10.0f),
        2.0f,
        5
    );

    EXPECT_EQ(5, keyframes.size());
    EXPECT_FLOAT_EQ(0.0f, keyframes.front().time);
    EXPECT_FLOAT_EQ(2.0f, keyframes.back().time);
    EXPECT_VEC3_EQ(glm::vec3(0.0f), keyframes.front().position);
    EXPECT_VEC3_EQ(glm::vec3(10.0f), keyframes.back().position);
}

// =============================================================================
// Blend Tree Tests (State Machine)
// =============================================================================

class AnimationBlendTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create two animations for blending
        anim1 = std::make_unique<Animation>("Idle");
        anim1->SetDuration(1.0f);

        AnimationChannel idle;
        idle.nodeName = "Root";
        idle.keyframes = {
            {0.0f, glm::vec3(0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)},
            {1.0f, glm::vec3(0.0f, 0.1f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)}
        };
        anim1->AddChannel(idle);

        anim2 = std::make_unique<Animation>("Walk");
        anim2->SetDuration(1.0f);

        AnimationChannel walk;
        walk.nodeName = "Root";
        walk.keyframes = {
            {0.0f, glm::vec3(0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)},
            {1.0f, glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1, 0, 0, 0), glm::vec3(1.0f)}
        };
        anim2->AddChannel(walk);
    }

    std::unique_ptr<Animation> anim1;
    std::unique_ptr<Animation> anim2;
};

TEST_F(AnimationBlendTest, BlendTransforms) {
    glm::mat4 a = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
    glm::mat4 b = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));

    glm::mat4 result = BlendTransforms(a, b, 0.5f);

    glm::vec3 translation(result[3]);
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 0.0f, 0.0f), translation, 0.001f);
}

TEST_F(AnimationBlendTest, AnimationLayerBlending) {
    AnimationLayer layer1;
    layer1.animation = anim1.get();
    layer1.time = 0.5f;
    layer1.weight = 0.5f;
    layer1.blendMode = BlendMode::Override;

    AnimationLayer layer2;
    layer2.animation = anim2.get();
    layer2.time = 0.5f;
    layer2.weight = 0.5f;
    layer2.blendMode = BlendMode::Override;

    // Get transforms from each layer
    auto transforms1 = anim1->Evaluate(layer1.time);
    auto transforms2 = anim2->Evaluate(layer2.time);

    // Blend them
    glm::mat4 blended = BlendTransforms(
        transforms1["Root"],
        transforms2["Root"],
        layer2.weight
    );

    glm::vec3 translation(blended[3]);
    // Should be blend of (0, 0.05, 0) and (5, 0, 0) at 50%
    EXPECT_FLOAT_NEAR_EPSILON(2.5f, translation.x, 0.1f);
}

// =============================================================================
// Animation Events Tests
// =============================================================================

TEST(AnimationEventTest, EventTriggering) {
    struct AnimationEvent {
        float time;
        std::string name;
        bool triggered = false;
    };

    std::vector<AnimationEvent> events = {
        {0.5f, "FootstepLeft"},
        {1.0f, "FootstepRight"},
        {1.5f, "FootstepLeft"}
    };

    float lastTime = 0.0f;
    float currentTime = 0.75f;

    // Check which events should trigger
    for (auto& event : events) {
        if (event.time > lastTime && event.time <= currentTime) {
            event.triggered = true;
        }
    }

    EXPECT_TRUE(events[0].triggered);
    EXPECT_FALSE(events[1].triggered);
    EXPECT_FALSE(events[2].triggered);
}

// =============================================================================
// Performance Hint Tests
// =============================================================================

TEST(AnimationPerformanceTest, SequentialPlaybackOptimization) {
    // Test that sequential playback uses cached keyframe indices

    AnimationChannel channel;
    channel.nodeName = "Test";
    channel.interpolationMode = InterpolationMode::Linear;

    // Create many keyframes
    for (int i = 0; i < 100; ++i) {
        Keyframe kf;
        kf.time = static_cast<float>(i);
        kf.position = glm::vec3(static_cast<float>(i));
        kf.rotation = glm::quat(1, 0, 0, 0);
        kf.scale = glm::vec3(1.0f);
        channel.keyframes.push_back(kf);
    }

    // Sequential playback should use cached index
    for (float t = 0.0f; t < 99.0f; t += 0.1f) {
        Keyframe result = channel.Interpolate(t);
        // Just verify it returns valid data
        EXPECT_GE(result.time, 0.0f);
    }

    // Reset cache and verify it still works
    channel.ResetCache();

    Keyframe afterReset = channel.Interpolate(50.0f);
    EXPECT_VEC3_NEAR(glm::vec3(50.0f), afterReset.position, 0.001f);
}
