# Animation System Guide

Nova3D Engine features a comprehensive animation system supporting skeletal animation, blend trees, state machines, and animation events. This guide covers all aspects of the animation pipeline.

## Overview

The animation system consists of:

- **Animation**: Keyframe data for bones/nodes
- **Skeleton**: Bone hierarchy and bind pose
- **AnimationController**: Playback and blending
- **AnimationStateMachine**: Transition-based control
- **Blend Trees**: Complex animation blending

## Core Concepts

### Keyframes

A keyframe stores transformation data at a specific time:

```cpp
struct Keyframe {
    float time;           // Time in seconds
    glm::vec3 position;   // Translation
    glm::quat rotation;   // Rotation (quaternion)
    glm::vec3 scale;      // Scale
};
```

### Interpolation Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `Linear` | Straight line between keyframes | Most animations |
| `Step` | Instant change at keyframe | Sprite animation |
| `CatmullRom` | Smooth curve through keyframes | Camera paths |
| `Cubic` | Bezier curve interpolation | Precise control |

### Animation Channels

Each bone has its own animation channel:

```cpp
AnimationChannel channel;
channel.nodeName = "LeftArm";
channel.interpolationMode = InterpolationMode::Linear;
channel.keyframes = {...};
```

## Creating Animations

### In Code

```cpp
#include <engine/animation/Animation.hpp>

// Create animation
Nova::Animation walkAnim("Walk");
walkAnim.SetDuration(1.0f);
walkAnim.SetTicksPerSecond(30.0f);
walkAnim.SetLooping(true);

// Create channel for hip bone
Nova::AnimationChannel hipChannel;
hipChannel.nodeName = "Hips";
hipChannel.interpolationMode = Nova::InterpolationMode::Linear;

// Add keyframes
Nova::Keyframe kf1{0.0f, {0, 1, 0}, glm::quat(1, 0, 0, 0), {1, 1, 1}};
Nova::Keyframe kf2{0.5f, {0, 1.1f, 0}, glm::quat(1, 0, 0, 0), {1, 1, 1}};
Nova::Keyframe kf3{1.0f, {0, 1, 0}, glm::quat(1, 0, 0, 0), {1, 1, 1}};

hipChannel.keyframes = {kf1, kf2, kf3};
walkAnim.AddChannel(std::move(hipChannel));
```

### From Files

Animations are typically imported with models:

```cpp
#include <engine/graphics/ModelLoader.hpp>

ModelLoader loader;
auto model = loader.Load("models/character.fbx");

// Access loaded animations
for (const auto& anim : model.animations) {
    controller.AddAnimation(anim.GetName(), std::make_shared<Animation>(anim));
}
```

### Keyframe Utilities

```cpp
using namespace Nova::KeyframeUtils;

// Create translation animation
auto keyframes = CreateTranslationAnimation(
    glm::vec3(0, 0, 0),  // Start
    glm::vec3(10, 0, 0), // End
    2.0f,                 // Duration
    10                    // Keyframe count
);

// Optimize keyframes (remove redundant)
auto optimized = Optimize(keyframes, 0.001f);

// Resample at different rate
auto resampled = Resample(keyframes, 60.0f);

// Reverse animation
Reverse(keyframes);

// Scale time
ScaleTime(keyframes, 0.5f);  // Half speed
```

## AnimationController

### Basic Usage

```cpp
Nova::AnimationController controller;
controller.SetSkeleton(&skeleton);

// Add animations
controller.AddAnimation("idle", idleAnim);
controller.AddAnimation("walk", walkAnim);
controller.AddAnimation("run", runAnim);
controller.AddAnimation("jump", jumpAnim);

// Play animation
controller.Play("idle");

// Update each frame
controller.Update(deltaTime);

// Get bone matrices for skinning
auto matrices = controller.GetBoneMatrices();
```

### Playback Control

```cpp
// Play with blend time
controller.Play("walk", 0.3f);  // 0.3s blend from current

// Crossfade
controller.CrossFade("run", 0.5f);  // 0.5s crossfade

// Stop
controller.Stop(0.2f);  // 0.2s blend out

// Pause/Resume
controller.Pause();
controller.Resume();

// Playback speed
controller.SetPlaybackSpeed(1.5f);  // 1.5x speed
controller.SetPlaybackSpeed(-1.0f); // Reverse playback

// Seek to time
controller.SetCurrentTime(0.5f);
```

### Blending Modes

```cpp
// Override (default) - replaces previous animation
controller.Play("attack", 0.1f, BlendMode::Override);

// Additive - adds to base animation
controller.Play("breathing", 0.0f, BlendMode::Additive);

// Layered - blend by bone mask
```

## State Machines

### Creating a State Machine

```cpp
Nova::AnimationStateMachine stateMachine(&controller);

// Add states with associated animations
stateMachine.AddState("idle", "idle_anim", true);    // Looping
stateMachine.AddState("walk", "walk_anim", true);
stateMachine.AddState("run", "run_anim", true);
stateMachine.AddState("jump", "jump_anim", false);   // Not looping
stateMachine.AddState("fall", "fall_anim", true);
stateMachine.AddState("land", "land_anim", false);

// Set initial state
stateMachine.SetInitialState("idle");
```

### Defining Transitions

```cpp
// Variables for conditions
float speed = 0.0f;
bool isGrounded = true;
bool jumpPressed = false;

// Idle <-> Walk transitions
stateMachine.AddTransition("idle", "walk",
    [&]() { return speed > 0.1f && isGrounded; },
    0.2f);  // 0.2s blend time

stateMachine.AddTransition("walk", "idle",
    [&]() { return speed < 0.1f && isGrounded; },
    0.2f);

// Walk <-> Run transitions
stateMachine.AddTransition("walk", "run",
    [&]() { return speed > 5.0f && isGrounded; },
    0.3f);

stateMachine.AddTransition("run", "walk",
    [&]() { return speed < 5.0f && speed > 0.1f && isGrounded; },
    0.3f);

// Jump from any ground state
stateMachine.AddAnyStateTransition("jump",
    [&]() { return jumpPressed && isGrounded; },
    0.1f);

// Jump -> Fall
stateMachine.AddTransition("jump", "fall",
    [&]() { return !isGrounded; },
    0.1f);

// Fall -> Land
stateMachine.AddTransition("fall", "land",
    [&]() { return isGrounded; },
    0.05f);

// Land -> Idle
stateMachine.AddTransition("land", "idle",
    [&]() { return true; },  // Auto-transition
    0.2f);
```

### Updating

```cpp
void Update(float deltaTime) {
    // Update variables used in conditions
    speed = characterController.GetSpeed();
    isGrounded = characterController.IsGrounded();
    jumpPressed = input.IsKeyPressed(Key::Space);

    // Update state machine
    stateMachine.Update(deltaTime);

    // Get current state for gameplay logic
    if (stateMachine.GetCurrentState() == "attack") {
        // Handle attack logic
    }
}
```

### Force Transitions

```cpp
// Force immediate state change
stateMachine.ForceState("death", 0.1f);
```

## Blend Trees

### Linear Blend

Blend between two animations based on a parameter:

```cpp
class LinearBlendTree {
    Animation* animA;
    Animation* animB;
    float blendFactor;  // 0 = A, 1 = B

    void Evaluate(float time, std::unordered_map<std::string, glm::mat4>& out) {
        auto transformsA = animA->Evaluate(time);
        auto transformsB = animB->Evaluate(time);

        for (const auto& [bone, matA] : transformsA) {
            auto it = transformsB.find(bone);
            if (it != transformsB.end()) {
                out[bone] = BlendTransforms(matA, it->second, blendFactor);
            } else {
                out[bone] = matA;
            }
        }
    }
};
```

### 2D Blend Space

Blend based on two parameters (e.g., speed and direction):

```cpp
class BlendSpace2D {
    struct BlendPoint {
        Animation* animation;
        glm::vec2 position;  // Position in blend space
    };

    std::vector<BlendPoint> points;
    glm::vec2 currentPosition;

    void Evaluate(float time, std::unordered_map<std::string, glm::mat4>& out) {
        // Find triangle containing current position
        auto [p0, p1, p2, weights] = FindTriangle(currentPosition);

        // Blend three animations
        auto t0 = p0->animation->Evaluate(time);
        auto t1 = p1->animation->Evaluate(time);
        auto t2 = p2->animation->Evaluate(time);

        for (const auto& [bone, mat0] : t0) {
            glm::mat4 mat1 = t1[bone];
            glm::mat4 mat2 = t2[bone];

            // Barycentric blend
            out[bone] = mat0 * weights.x + mat1 * weights.y + mat2 * weights.z;
        }
    }
};
```

### Locomotion Blend

Common setup for character movement:

```cpp
// Movement blend space
BlendSpace2D locomotion;
locomotion.AddPoint(idleAnim, {0, 0});
locomotion.AddPoint(walkForwardAnim, {0, 1});
locomotion.AddPoint(walkBackAnim, {0, -1});
locomotion.AddPoint(walkLeftAnim, {-1, 0});
locomotion.AddPoint(walkRightAnim, {1, 0});
locomotion.AddPoint(runForwardAnim, {0, 2});

// Update based on input
void UpdateLocomotion(glm::vec2 inputDir, float speed) {
    locomotion.SetPosition(inputDir * speed);
}
```

## Animation Events

### Defining Events

```cpp
struct AnimationEvent {
    float time;              // When to fire
    std::string name;        // Event name
    std::any data;           // Optional data
};

class AnimationWithEvents : public Animation {
    std::vector<AnimationEvent> events;

    void AddEvent(float time, const std::string& name) {
        events.push_back({time, name, {}});
    }

    std::vector<AnimationEvent> GetEventsBetween(float startTime, float endTime) {
        std::vector<AnimationEvent> fired;
        for (const auto& evt : events) {
            if (evt.time > startTime && evt.time <= endTime) {
                fired.push_back(evt);
            }
        }
        return fired;
    }
};
```

### Processing Events

```cpp
class AnimationControllerWithEvents : public AnimationController {
    float lastTime = 0.0f;

    void Update(float deltaTime) {
        float newTime = GetCurrentTime() + deltaTime;

        // Get events in this frame
        auto events = currentAnimation->GetEventsBetween(lastTime, newTime);

        for (const auto& evt : events) {
            ProcessEvent(evt);
        }

        lastTime = newTime;
        AnimationController::Update(deltaTime);
    }

    void ProcessEvent(const AnimationEvent& evt) {
        if (evt.name == "footstep") {
            PlayFootstepSound();
        } else if (evt.name == "attack_hit") {
            CheckAttackHit();
        } else if (evt.name == "spawn_particle") {
            SpawnParticle(std::any_cast<std::string>(evt.data));
        }
    }
};
```

### Common Events

| Event | Description | Typical Time |
|-------|-------------|--------------|
| `footstep_left` | Left foot hits ground | Varies |
| `footstep_right` | Right foot hits ground | Varies |
| `attack_start` | Attack begins | 0.0 |
| `attack_hit` | Damage frame | ~0.3-0.5 |
| `attack_end` | Attack recovers | ~0.8-1.0 |
| `jump_takeoff` | Character leaves ground | ~0.2 |
| `jump_land` | Character lands | End |

## Retargeting

### Bone Mapping

Map animations between different skeletons:

```cpp
struct BoneMapping {
    std::unordered_map<std::string, std::string> sourceToTarget;

    void AddMapping(const std::string& source, const std::string& target) {
        sourceToTarget[source] = target;
    }

    std::string GetTargetBone(const std::string& sourceBone) const {
        auto it = sourceToTarget.find(sourceBone);
        return it != sourceToTarget.end() ? it->second : "";
    }
};

// Setup mapping
BoneMapping mapping;
mapping.AddMapping("mixamorig:Hips", "Hips");
mapping.AddMapping("mixamorig:Spine", "Spine");
mapping.AddMapping("mixamorig:LeftArm", "Arm_L");
// ... more mappings
```

### Retargeting Animation

```cpp
Animation RetargetAnimation(const Animation& source,
                            const BoneMapping& mapping,
                            const Skeleton& targetSkeleton) {
    Animation retargeted(source.GetName() + "_retargeted");
    retargeted.SetDuration(source.GetDuration());
    retargeted.SetTicksPerSecond(source.GetTicksPerSecond());

    for (const auto& channel : source.GetChannels()) {
        std::string targetBone = mapping.GetTargetBone(channel.nodeName);
        if (targetBone.empty()) continue;

        AnimationChannel newChannel;
        newChannel.nodeName = targetBone;
        newChannel.interpolationMode = channel.interpolationMode;
        newChannel.keyframes = channel.keyframes;

        // Adjust for skeleton differences
        // ...

        retargeted.AddChannel(std::move(newChannel));
    }

    return retargeted;
}
```

## Performance Tips

### Optimization Strategies

1. **Reduce Keyframe Count**
```cpp
// Optimize keyframes with tolerance
auto optimized = KeyframeUtils::Optimize(keyframes, 0.01f);
```

2. **Use Binary Search for Keyframes**
```cpp
// AnimationChannel uses cached index for sequential playback
size_t FindKeyframeIndex(float time) const noexcept {
    // Try cached index first
    if (lastKeyframeIndex < keyframes.size() - 1) {
        if (keyframes[lastKeyframeIndex].time <= time &&
            keyframes[lastKeyframeIndex + 1].time > time) {
            return lastKeyframeIndex;
        }
    }
    // Fall back to binary search
    return std::lower_bound(...);
}
```

3. **Pre-allocate Bone Matrices**
```cpp
std::vector<glm::mat4> boneMatrices;
boneMatrices.reserve(skeleton.GetBoneCount());
controller.GetBoneMatricesInto(boneMatrices);
```

4. **LOD for Animations**
```cpp
// Skip updates for distant characters
float distance = GetDistanceToCamera(entity);
if (distance > 50.0f) {
    // Update at reduced rate
    if (frame % 4 == 0) {
        controller.Update(deltaTime * 4);
    }
}
```

5. **Limit Bone Count**
```cpp
// Use bone masks for upper/lower body separation
std::vector<std::string> upperBodyMask = {
    "Spine", "Chest", "Neck", "Head",
    "LeftArm", "RightArm"
};
```

### Memory Optimization

```cpp
// Share animations between instances
std::shared_ptr<Animation> sharedAnim = animationLibrary.Get("walk");
controller1.AddAnimation("walk", sharedAnim);
controller2.AddAnimation("walk", sharedAnim);

// Use animation compression
// - Quantize quaternions to 16-bit
// - Remove unused channels
// - Use curve fitting for keyframes
```

### GPU Skinning

```cpp
// Upload bone matrices once per frame
glBindBuffer(GL_UNIFORM_BUFFER, boneMatrixUBO);
glBufferSubData(GL_UNIFORM_BUFFER, 0,
                boneMatrices.size() * sizeof(glm::mat4),
                boneMatrices.data());

// Skinning in vertex shader
// layout(std140) uniform BoneMatrices {
//     mat4 bones[MAX_BONES];
// };
```

## Debugging

### Visualizing Skeleton

```cpp
void DrawSkeleton(const Skeleton& skeleton,
                  const std::unordered_map<std::string, glm::mat4>& transforms) {
    for (const auto& bone : skeleton.GetBones()) {
        if (bone.parentIndex >= 0) {
            glm::vec3 bonePos = transforms[bone.name] * glm::vec4(0, 0, 0, 1);
            glm::vec3 parentPos = transforms[parentBone.name] * glm::vec4(0, 0, 0, 1);

            DebugDraw::Line(parentPos, bonePos, glm::vec3(1, 1, 0));
            DebugDraw::Sphere(bonePos, 0.02f, glm::vec3(1, 0, 0));
        }
    }
}
```

### Animation Curves

```cpp
void DrawAnimationCurve(const AnimationChannel& channel,
                        const std::string& property) {
    ImGui::PlotLines(property.c_str(),
        [](void* data, int idx) {
            auto* channel = static_cast<AnimationChannel*>(data);
            return channel->keyframes[idx].position.y;
        },
        (void*)&channel, channel.keyframes.size());
}
```

## Integration with Game Systems

### Combat Animations

```cpp
class CombatAnimationController {
    AnimationStateMachine stateMachine;
    bool attackQueued = false;
    int comboCount = 0;

    void OnAttackInput() {
        if (CanAttack()) {
            attackQueued = true;
        }
    }

    void Update(float dt) {
        // Handle combo system
        if (attackQueued && IsInComboWindow()) {
            comboCount++;
            std::string attackAnim = "attack_" + std::to_string(comboCount);
            stateMachine.ForceState(attackAnim, 0.05f);
            attackQueued = false;
        }

        stateMachine.Update(dt);
    }

    bool IsInComboWindow() {
        return stateMachine.GetCurrentState().starts_with("attack") &&
               controller.GetCurrentTime() > 0.4f;
    }
};
```

### Procedural Animation

```cpp
// IK for foot placement
void ApplyFootIK(Skeleton& skeleton,
                 std::unordered_map<std::string, glm::mat4>& transforms,
                 const glm::vec3& leftFootTarget,
                 const glm::vec3& rightFootTarget) {
    // Two-bone IK for legs
    SolveTwoBoneIK(transforms, "LeftUpLeg", "LeftLeg", "LeftFoot", leftFootTarget);
    SolveTwoBoneIK(transforms, "RightUpLeg", "RightLeg", "RightFoot", rightFootTarget);
}

// Look at target
void ApplyHeadLookAt(std::unordered_map<std::string, glm::mat4>& transforms,
                     const glm::vec3& targetPos,
                     float weight) {
    glm::mat4& headTransform = transforms["Head"];
    glm::vec3 headPos = headTransform[3];
    glm::vec3 lookDir = glm::normalize(targetPos - headPos);

    glm::quat currentRot = glm::quat_cast(headTransform);
    glm::quat targetRot = glm::quatLookAt(lookDir, glm::vec3(0, 1, 0));
    glm::quat finalRot = glm::slerp(currentRot, targetRot, weight);

    headTransform = glm::mat4_cast(finalRot);
    headTransform[3] = glm::vec4(headPos, 1);
}
```
