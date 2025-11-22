# Animation API

The animation system provides skeletal animation, keyframe interpolation, animation blending, and state machine control.

## Animation Class

**Header**: `engine/animation/Animation.hpp`

Animation clip containing keyframe data for multiple bones.

### Constructor

```cpp
Animation();
explicit Animation(const std::string& name);
```

### Methods

#### AddChannel

```cpp
void AddChannel(const AnimationChannel& channel);
void AddChannel(AnimationChannel&& channel);
```

Add an animation channel for a bone.

#### Evaluate

```cpp
[[nodiscard]] std::unordered_map<std::string, glm::mat4> Evaluate(float time) const;
void EvaluateInto(float time, std::unordered_map<std::string, glm::mat4>& outTransforms) const;
```

Evaluate all channels at the given time. `EvaluateInto` avoids allocation.

#### GetChannel

```cpp
[[nodiscard]] const AnimationChannel* GetChannel(const std::string& nodeName) const;
[[nodiscard]] AnimationChannel* GetChannel(const std::string& nodeName);
```

Get channel by bone name.

#### Properties

```cpp
[[nodiscard]] const std::string& GetName() const noexcept;
void SetName(const std::string& name);

[[nodiscard]] float GetDuration() const noexcept;
void SetDuration(float duration) noexcept;

[[nodiscard]] float GetTicksPerSecond() const noexcept;
void SetTicksPerSecond(float tps) noexcept;

[[nodiscard]] bool IsLooping() const noexcept;
void SetLooping(bool looping) noexcept;

[[nodiscard]] const std::vector<AnimationChannel>& GetChannels() const noexcept;
```

---

## AnimationChannel Struct

Channel containing keyframes for a single bone.

### Members

```cpp
struct AnimationChannel {
    std::string nodeName;
    std::vector<Keyframe> keyframes;
    InterpolationMode interpolationMode = InterpolationMode::Linear;
    mutable size_t lastKeyframeIndex = 0;  // Cache for sequential playback
};
```

### Methods

```cpp
[[nodiscard]] glm::mat4 Evaluate(float time) const;
[[nodiscard]] Keyframe Interpolate(float time) const;
[[nodiscard]] size_t FindKeyframeIndex(float time) const noexcept;
void ResetCache() const noexcept;
```

---

## Keyframe Struct

Single animation keyframe with transform data.

```cpp
struct alignas(16) Keyframe {
    float time = 0.0f;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    bool operator<(const Keyframe& other) const noexcept;
    bool operator<(float t) const noexcept;
};
```

---

## InterpolationMode Enum

```cpp
enum class InterpolationMode : uint8_t {
    Linear,      // Linear interpolation
    Step,        // No interpolation (instant change)
    CatmullRom,  // Smooth spline interpolation
    Cubic        // Cubic bezier interpolation
};
```

---

## BlendMode Enum

```cpp
enum class BlendMode : uint8_t {
    Override,   // Replace previous animation
    Additive,   // Add to previous animation
    Multiply    // Multiply with previous animation
};
```

---

## AnimationController Class

**Header**: `engine/animation/AnimationController.hpp`

Controls animation playback and blending.

### Constructor

```cpp
AnimationController();
explicit AnimationController(Skeleton* skeleton);
```

### Methods

#### SetSkeleton

```cpp
void SetSkeleton(Skeleton* skeleton);
[[nodiscard]] Skeleton* GetSkeleton() const;
```

#### AddAnimation

```cpp
void AddAnimation(const std::string& name, std::shared_ptr<Animation> animation);
```

Add animation to the controller's library.

#### GetAnimation

```cpp
[[nodiscard]] Animation* GetAnimation(const std::string& name);
[[nodiscard]] const Animation* GetAnimation(const std::string& name) const;
```

#### Play

```cpp
void Play(const std::string& name, float blendTime = 0.2f, bool looping = true);
```

Start playing an animation with optional blend from current.

#### CrossFade

```cpp
void CrossFade(const std::string& name, float fadeTime = 0.3f, bool looping = true);
```

Crossfade to another animation.

#### Stop

```cpp
void Stop(float blendOutTime = 0.2f);
```

Stop all animations with blend out.

#### Pause / Resume

```cpp
void Pause();
void Resume();
```

#### Update

```cpp
void Update(float deltaTime);
```

Update animation state. Call every frame.

#### GetBoneTransforms

```cpp
[[nodiscard]] const std::unordered_map<std::string, glm::mat4>& GetBoneTransforms() const;
```

Get current blended bone transforms.

#### GetBoneMatrices

```cpp
[[nodiscard]] std::vector<glm::mat4> GetBoneMatrices() const;
void GetBoneMatricesInto(std::span<glm::mat4> outMatrices) const;
```

Get final bone matrices for GPU skinning.

#### Playback State

```cpp
[[nodiscard]] bool IsPlaying() const;
[[nodiscard]] float GetCurrentTime() const;
void SetCurrentTime(float time);
[[nodiscard]] float GetPlaybackSpeed() const;
void SetPlaybackSpeed(float speed);
[[nodiscard]] const std::string& GetCurrentAnimationName() const;
```

#### Layer Control

```cpp
void SetLayerWeight(size_t layerIndex, float weight);
[[nodiscard]] float GetLayerWeight(size_t layerIndex) const;
```

---

## AnimationStateMachine Class

**Header**: `engine/animation/AnimationController.hpp`

Transition-based animation control.

### Constructor

```cpp
AnimationStateMachine();
explicit AnimationStateMachine(AnimationController* controller);
```

### Methods

#### AddState

```cpp
void AddState(const std::string& stateName, const std::string& animationName, bool looping = true);
```

Add a state with associated animation.

#### AddTransition

```cpp
void AddTransition(const std::string& from, const std::string& to,
                   std::function<bool()> condition, float blendTime = 0.2f);
```

Add transition between states.

#### AddAnyStateTransition

```cpp
void AddAnyStateTransition(const std::string& to,
                            std::function<bool()> condition, float blendTime = 0.2f);
```

Add transition from any state.

#### SetInitialState

```cpp
void SetInitialState(const std::string& stateName);
```

Set the starting state.

#### Update

```cpp
void Update(float deltaTime);
```

Update state machine, checking transitions.

#### ForceState

```cpp
void ForceState(const std::string& stateName, float blendTime = 0.2f);
```

Force immediate state change.

#### GetCurrentState

```cpp
[[nodiscard]] const std::string& GetCurrentState() const;
```

---

## Interpolation Namespace

**Header**: `engine/animation/Animation.hpp`

Interpolation utility functions.

```cpp
namespace Nova::Interpolation {
    [[nodiscard]] glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) noexcept;
    [[nodiscard]] glm::quat Slerp(const glm::quat& a, const glm::quat& b, float t) noexcept;
    [[nodiscard]] glm::quat Nlerp(const glm::quat& a, const glm::quat& b, float t) noexcept;
    [[nodiscard]] constexpr float SmoothStep(float t) noexcept;
    [[nodiscard]] constexpr float SmootherStep(float t) noexcept;
    [[nodiscard]] float EaseInOut(float t) noexcept;
    [[nodiscard]] glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1,
                                        const glm::vec3& p2, const glm::vec3& p3, float t) noexcept;
    [[nodiscard]] glm::vec3 CubicBezier(const glm::vec3& p0, const glm::vec3& p1,
                                         const glm::vec3& p2, const glm::vec3& p3, float t) noexcept;
    [[nodiscard]] glm::quat Squad(const glm::quat& q0, const glm::quat& q1,
                                   const glm::quat& s0, const glm::quat& s1, float t) noexcept;
}
```

---

## KeyframeUtils Namespace

**Header**: `engine/animation/Animation.hpp`

Keyframe manipulation utilities.

```cpp
namespace Nova::KeyframeUtils {
    [[nodiscard]] Keyframe FromMatrix(const glm::mat4& matrix, float time = 0.0f);
    [[nodiscard]] glm::mat4 ToMatrix(const Keyframe& kf);
    [[nodiscard]] Keyframe Identity(float time = 0.0f);
    [[nodiscard]] bool ApproximatelyEqual(const Keyframe& a, const Keyframe& b, float epsilon = 0.0001f);
    [[nodiscard]] float Distance(const Keyframe& a, const Keyframe& b);
    [[nodiscard]] std::vector<Keyframe> Optimize(const std::vector<Keyframe>& keyframes, float tolerance = 0.001f);
    [[nodiscard]] std::vector<Keyframe> Resample(const std::vector<Keyframe>& keyframes, float newFrameRate);
    void SortByTime(std::vector<Keyframe>& keyframes);
    void RemoveDuplicates(std::vector<Keyframe>& keyframes, float timeEpsilon = 0.0001f);
    void ScaleTime(std::vector<Keyframe>& keyframes, float factor);
    void OffsetTime(std::vector<Keyframe>& keyframes, float offset);
    void Reverse(std::vector<Keyframe>& keyframes);
    [[nodiscard]] std::vector<Keyframe> CreateTranslationAnimation(
        const glm::vec3& start, const glm::vec3& end,
        float duration, size_t numKeyframes = 2);
    [[nodiscard]] std::vector<Keyframe> CreateRotationAnimation(
        const glm::quat& start, const glm::quat& end,
        float duration, size_t numKeyframes = 2);
}
```

---

## Blending Functions

```cpp
[[nodiscard]] Keyframe BlendKeyframes(const Keyframe& a, const Keyframe& b, float weight);
[[nodiscard]] glm::mat4 BlendTransforms(const glm::mat4& a, const glm::mat4& b, float weight);
```

---

## Skeleton Class

**Header**: `engine/animation/Skeleton.hpp`

Bone hierarchy management.

### Bone Struct

```cpp
struct Bone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 localTransform;
    glm::mat4 inverseBindMatrix;
};
```

### Methods

```cpp
void AddBone(const Bone& bone);
[[nodiscard]] const Bone* GetBone(const std::string& name) const;
[[nodiscard]] int GetBoneIndex(const std::string& name) const;
[[nodiscard]] size_t GetBoneCount() const;
[[nodiscard]] const std::vector<Bone>& GetBones() const;
void CalculateGlobalTransforms(const std::unordered_map<std::string, glm::mat4>& localTransforms,
                                std::vector<glm::mat4>& outGlobal) const;
```

---

## Usage Example

```cpp
#include <engine/animation/Animation.hpp>
#include <engine/animation/AnimationController.hpp>

// Load model with skeleton and animations
auto model = ModelLoader::Load("character.fbx");
auto& skeleton = model.skeleton;

// Setup controller
Nova::AnimationController controller(&skeleton);
for (auto& anim : model.animations) {
    controller.AddAnimation(anim.GetName(), std::make_shared<Animation>(anim));
}

// Setup state machine
Nova::AnimationStateMachine stateMachine(&controller);
stateMachine.AddState("idle", "Idle", true);
stateMachine.AddState("walk", "Walk", true);
stateMachine.AddState("run", "Run", true);

stateMachine.AddTransition("idle", "walk", [&]() { return speed > 0.1f; }, 0.2f);
stateMachine.AddTransition("walk", "idle", [&]() { return speed < 0.1f; }, 0.2f);
stateMachine.AddTransition("walk", "run", [&]() { return speed > 5.0f; }, 0.3f);
stateMachine.AddTransition("run", "walk", [&]() { return speed < 5.0f; }, 0.3f);

stateMachine.SetInitialState("idle");

// Game loop
void Update(float dt) {
    stateMachine.Update(dt);
    auto boneMatrices = controller.GetBoneMatrices();
    // Upload to GPU for skinning
}
```
