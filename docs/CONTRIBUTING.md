# Contributing Guide

Thank you for your interest in contributing to Nova3D Engine! This guide covers code style, the pull request process, testing requirements, and documentation standards.

## Getting Started

### Setting Up Development Environment

1. **Fork the Repository**
   - Click "Fork" on GitHub
   - Clone your fork locally

2. **Configure Remotes**
   ```bash
   git clone https://github.com/YOUR_USERNAME/Nova3DEngine.git
   cd Nova3DEngine
   git remote add upstream https://github.com/original/Nova3DEngine.git
   ```

3. **Create Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

4. **Build and Test**
   ```bash
   cmake -B build -DNOVA_BUILD_TESTS=ON
   cmake --build build
   ./build/bin/nova_tests
   ```

## Code Style

### C++ Style Guidelines

We follow a modern C++ style based on the C++ Core Guidelines.

#### Naming Conventions

```cpp
// Classes: PascalCase
class AnimationController;

// Functions/Methods: PascalCase
void ProcessFrame();

// Variables: camelCase
float deltaTime;

// Member variables: m_ prefix
float m_currentTime;

// Constants: ALL_CAPS or k prefix
constexpr int MAX_BONES = 256;
constexpr float kDefaultSpeed = 1.0f;

// Enums: PascalCase for type and values
enum class InterpolationMode {
    Linear,
    Cubic,
    CatmullRom
};

// Namespaces: PascalCase
namespace Nova {
namespace Reflect {
}
}

// Files: PascalCase.hpp / PascalCase.cpp
// AnimationController.hpp
// AnimationController.cpp
```

#### Formatting

```cpp
// Braces: Same line for functions and control structures
void Function() {
    if (condition) {
        // ...
    } else {
        // ...
    }

    for (int i = 0; i < count; ++i) {
        // ...
    }
}

// Indentation: 4 spaces (no tabs)
class MyClass {
    void Method() {
        if (condition) {
            DoSomething();
        }
    }
};

// Line length: 100 characters max
// Break long lines logically

// Function parameters: align or one per line
void LongFunction(int param1, int param2, int param3,
                  int param4, int param5);

// Or:
void LongFunction(
    int param1,
    int param2,
    int param3);

// Includes: grouped and sorted
#include "MyClass.hpp"           // Own header first

#include "engine/core/Engine.hpp" // Project headers
#include "engine/math/Math.hpp"

#include <algorithm>              // Standard library
#include <vector>

#include <glm/glm.hpp>            // External libraries
```

#### Modern C++ Features

```cpp
// Use auto for complex types
auto it = container.begin();
auto result = std::make_unique<MyClass>();

// Use nullptr, not NULL
MyClass* ptr = nullptr;

// Use enum class
enum class State { Running, Stopped };

// Use override and final
class Derived : public Base {
    void Method() override;
    void FinalMethod() final;
};

// Use [[nodiscard]] for functions with important return values
[[nodiscard]] bool Initialize();

// Use noexcept where appropriate
void Swap(MyClass& other) noexcept;

// Use constexpr for compile-time constants
constexpr float Pi = 3.14159f;

// Use structured bindings
auto [x, y, z] = GetPosition();

// Use std::optional for optional returns
[[nodiscard]] std::optional<int> FindIndex(const std::string& name);

// Use std::string_view for string parameters
void Process(std::string_view text);
```

#### Documentation

```cpp
/**
 * @brief Brief description of the class
 *
 * Detailed description if needed. Can span
 * multiple lines.
 *
 * Usage:
 * @code
 * MyClass obj;
 * obj.DoSomething();
 * @endcode
 */
class MyClass {
public:
    /**
     * @brief Brief description of method
     * @param param1 Description of param1
     * @param param2 Description of param2
     * @return Description of return value
     * @throws std::runtime_error If something fails
     */
    [[nodiscard]] int Method(int param1, const std::string& param2);

private:
    int m_value;  ///< Brief member description
};
```

### Python Style Guidelines

Follow PEP 8 with these additions:

```python
# Use type hints
def process_entity(entity_id: int, damage: float) -> bool:
    pass

# Docstrings: Google style
def calculate_damage(base: float, multiplier: float) -> float:
    """Calculate final damage value.

    Args:
        base: Base damage amount.
        multiplier: Damage multiplier.

    Returns:
        The calculated damage value.

    Raises:
        ValueError: If base is negative.
    """
    pass

# Import order
import os                    # Standard library
import sys

import numpy as np           # Third-party

from nova import entities    # Project imports
from nova import events
```

### JSON Style

```json
{
    "id": "example",
    "name": "Example Config",
    "nested": {
        "property": "value",
        "array": [
            "item1",
            "item2"
        ]
    }
}
```

- 4-space indentation
- Double quotes for strings
- Trailing commas not allowed

## Pull Request Process

### Before Submitting

1. **Update Your Branch**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run Tests**
   ```bash
   cmake --build build --target test
   ```

3. **Run Linter**
   ```bash
   ./scripts/lint.sh
   ```

4. **Check Formatting**
   ```bash
   ./scripts/format-check.sh
   ```

### Commit Messages

Follow conventional commits format:

```
type(scope): subject

body

footer
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting
- `refactor`: Code restructuring
- `test`: Tests
- `chore`: Maintenance

**Examples:**
```
feat(animation): add blend tree support

Implement 1D and 2D blend trees for animation blending.
Includes editor UI for visual blend space editing.

Closes #123
```

```
fix(rendering): correct shadow map cascade boundaries

The cascade boundaries were calculated incorrectly for
non-square viewports, causing shadow artifacts.
```

### PR Description Template

```markdown
## Description
Brief description of changes.

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
Describe how you tested the changes.

## Screenshots
If applicable, add screenshots.

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-reviewed code
- [ ] Added comments for complex code
- [ ] Updated documentation
- [ ] Added tests
- [ ] All tests pass
```

### Review Process

1. Create PR against `main` branch
2. CI runs automated tests
3. Maintainer reviews code
4. Address feedback
5. Squash and merge when approved

## Testing Requirements

### Unit Tests

Write tests for new functionality:

```cpp
#include <gtest/gtest.h>
#include <engine/animation/Animation.hpp>

TEST(AnimationTest, InterpolatesKeyframes) {
    Nova::Animation anim("test");
    Nova::AnimationChannel channel;
    channel.nodeName = "bone";
    channel.keyframes = {
        {0.0f, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}},
        {1.0f, {10, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}}
    };
    anim.AddChannel(channel);

    auto transforms = anim.Evaluate(0.5f);

    ASSERT_TRUE(transforms.contains("bone"));
    // Check interpolated position is approximately (5, 0, 0)
    auto pos = ExtractPosition(transforms["bone"]);
    EXPECT_NEAR(pos.x, 5.0f, 0.001f);
}
```

### Integration Tests

Test systems working together:

```cpp
TEST(ScriptingIntegration, PythonCallsCppFunction) {
    auto& engine = PythonEngine::Instance();
    engine.Initialize({});

    // Register test function
    engine.RegisterFunction("test_add", [](int a, int b) {
        return a + b;
    });

    // Call from Python
    auto result = engine.ExecuteString("result = test_add(2, 3)");
    ASSERT_TRUE(result.success);

    auto value = engine.GetGlobal<int>("__main__", "result");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 5);
}
```

### Test Naming

```cpp
TEST(ClassName_MethodName_Condition_ExpectedResult)
TEST(Animation_Evaluate_EmptyAnimation_ReturnsEmpty)
TEST(Animation_Evaluate_SingleKeyframe_ReturnsKeyframeTransform)
TEST(Animation_Evaluate_BetweenKeyframes_InterpolatesCorrectly)
```

### Coverage

- Aim for 80% code coverage on new code
- Critical systems (animation, networking) should have higher coverage
- Don't sacrifice code quality for coverage numbers

## Documentation Standards

### Code Documentation

- All public APIs must have Doxygen comments
- Complex algorithms need inline comments
- Include usage examples for non-obvious APIs

### User Documentation

When adding features, update relevant docs:

- `API_REFERENCE.md` for new APIs
- `EDITOR_GUIDE.md` for editor features
- `SCRIPTING_GUIDE.md` for Python APIs
- Tutorial files for new workflows

### Documentation Format

```markdown
# Feature Name

Brief description.

## Overview

More detailed description.

## Usage

```cpp
// Code example
```

## API Reference

### ClassName

Description.

#### Methods

| Method | Description |
|--------|-------------|
| `Method1()` | Does X |
| `Method2()` | Does Y |

## Examples

### Basic Example

```cpp
// Full working example
```

## See Also

- [Related Doc](link)
```

## Issue Guidelines

### Bug Reports

Include:
- Nova3D version
- Operating system
- Steps to reproduce
- Expected vs actual behavior
- Error messages/logs
- Minimal reproduction code

### Feature Requests

Include:
- Use case description
- Proposed solution
- Alternative approaches considered
- Willingness to implement

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Accept constructive criticism
- Focus on what's best for the project
- Show empathy to others

### Enforcement

Violations may result in:
1. Warning
2. Temporary ban
3. Permanent ban

Report issues to maintainers.

## Getting Help

- **Discord**: Join our Discord server
- **Discussions**: Use GitHub Discussions
- **Issues**: For bugs and features
- **Email**: maintainers@example.com

## Recognition

Contributors are recognized in:
- CONTRIBUTORS.md
- Release notes
- Project website

Thank you for contributing to Nova3D Engine!
