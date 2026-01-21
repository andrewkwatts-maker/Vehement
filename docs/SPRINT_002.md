# Sprint 2: Core Systems & C++ Fixes

**Sprint Goal**: Fix critical C++ build issues and enable core game systems
**Duration**: 1 sprint cycle
**Velocity Target**: 29 story points (based on Sprint 1 actual)
**Start Date**: 2026-01-21
**End Date**: 2026-01-21
**Status**: ✅ COMPLETED

---

## Sprint Results Summary

| Metric | Value |
|--------|-------|
| Planned Story Points | 23 |
| Completed Story Points | 23 |
| Velocity | 23 SP |
| Items Completed | 4/4 (100%) |

---

## Sprint Backlog

| ID | Item | SP | Priority | Status |
|----|------|-----|----------|--------|
| BACK-001 | Complete JSON Library Fix | 5 | P0 | ✅ Done |
| BACK-002 | Enable Event System | 5 | P0 | ✅ Done |
| BACK-003 | Python Scripting System Update | 8 | P1 | ✅ Done |
| BACK-005 | Audio System Integration | 5 | P1 | ✅ Done |
| BACK-009 | Complete Editor Features | 8 | P2 | Deferred |
| **Total** | | **23** | | **23 completed** |

**Note**: BACK-009 deferred to Sprint 3 - core systems took priority.

---

## Sprint Focus: C++ Quality

Per CLAUDE.md development philosophy:
> **CRITICAL: Always push forward with C++ solutions in the editor/game. Never create Python/script workarounds when encountering C++ build bugs.**

This sprint prioritizes proper C++ fixes over workarounds.

---

## Sprint Tasks Breakdown

### BACK-001: Complete JSON Library Fix (5 SP)
**Priority**: P0 - BLOCKING
**Agent**: Build-Specialist

**Tasks**:
1. Evaluate RapidJSON vs simdjson for replacement (1 SP)
2. Create JSON abstraction layer in engine/core/json_config.hpp (2 SP)
3. Migrate existing nlohmann::json usage to abstraction (1 SP)
4. Verify build compiles on MSVC with C++20 (1 SP)

**Acceptance Criteria**:
- [ ] `cmake --build .` succeeds without JSON namespace errors
- [ ] All JSON read/write operations work correctly
- [ ] EventBinding.cpp compiles with NOVA_ENABLE_EVENTS=ON
- [ ] No performance regression in asset loading

**Technical Approach**:
```cpp
// engine/core/json_config.hpp
#pragma once

// Option 1: RapidJSON (read/write, proven)
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
using JsonValue = rapidjson::Value;
using JsonDocument = rapidjson::Document;

// Wrapper functions for compatibility
namespace Nova::Json {
    JsonDocument Parse(const std::string& str);
    std::string Stringify(const JsonDocument& doc);
    // ... migration helpers
}
```

---

### BACK-002: Enable Event System (5 SP)
**Priority**: P0 - Depends on BACK-001
**Agent**: Systems-Engineer

**Tasks**:
1. Update EventBinding.cpp JSON includes (1 SP)
2. Fix EventCondition.hpp JSON serialization (1 SP)
3. Update TypeInfo.hpp JSON usage (1 SP)
4. Write event system tests (1 SP)
5. Verify event send/receive works (1 SP)

**Acceptance Criteria**:
- [ ] NOVA_ENABLE_EVENTS=ON compiles
- [ ] Events can be registered and triggered
- [ ] Event bindings can be serialized to JSON
- [ ] Event conditions evaluate correctly

**Files**:
- engine/events/EventBinding.cpp
- engine/events/EventCondition.hpp
- engine/reflection/TypeInfo.hpp

---

### BACK-003: Python Scripting System Update (8 SP)
**Priority**: P1
**Agent**: Scripting-Specialist

**Tasks**:
1. Update Entity API bindings to match current implementation (2 SP)
2. Fix Transform/Component API bindings (2 SP)
3. Update Logger to use spdlog integration (1 SP)
4. Add AudioEngine bindings (1 SP)
5. Create sample Python script demonstrating all features (1 SP)
6. Write scripting system tests (1 SP)

**Acceptance Criteria**:
- [ ] Entity API works: create, destroy, get components
- [ ] Transform API works: position, rotation, scale
- [ ] Logger works: log.info(), log.error(), etc.
- [ ] Audio works: play_sound(), play_music()
- [ ] Sample script runs successfully

**Files**:
- engine/scripting/ScriptBindings.cpp
- engine/scripting/ScriptContext.cpp
- examples/scripts/sample_gameplay.py

---

### BACK-005: Audio System Integration (5 SP)
**Priority**: P1
**Agent**: Audio-Engineer

**Tasks**:
1. Initialize AudioEngine with game loop (1 SP)
2. Implement 3D positional audio (1 SP)
3. Implement music playback system (1 SP)
4. Connect sound effects to game events (1 SP)
5. Write audio system tests (1 SP)

**Acceptance Criteria**:
- [ ] AudioEngine initializes on engine start
- [ ] 3D sounds have correct attenuation
- [ ] Music can play, pause, stop, loop
- [ ] Sound effects trigger on events

**Files**:
- engine/audio/AudioEngine.cpp
- engine/audio/AudioManager.cpp
- engine/audio/SoundSource.cpp

---

### BACK-009: Complete Editor Features (8 SP) - STRETCH
**Priority**: P2 - Only if time permits
**Agent**: Editor-Developer

**Tasks**:
1. Implement screen-to-world raycasting (2 SP)
2. Add map load/save functionality (2 SP)
3. Create object placement tools (2 SP)
4. Add heightmap import/export (2 SP)

**Acceptance Criteria**:
- [ ] Click in viewport selects objects
- [ ] Maps can be saved and loaded
- [ ] Objects can be placed in editor
- [ ] Heightmaps can be imported/exported

**Files**:
- examples/StandaloneEditor.cpp
- engine/editor/EditorCamera.cpp

---

## Definition of Done (Enhanced)

From Sprint 1 retrospective, adding build verification:

- [ ] Code compiles without warnings (`cmake --build .` succeeds)
- [ ] Unit tests pass
- [ ] No new technical debt introduced
- [ ] Documentation updated (if applicable)
- [ ] Build verified on MSVC with C++20

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| JSON replacement breaks existing code | Medium | High | Create abstraction layer first |
| Event system has hidden dependencies | Medium | Medium | Thorough testing |
| Python bindings need engine refactoring | Low | Medium | Update bindings to match engine |
| Audio system OpenAL issues | Low | Low | Has stub implementation fallback |

---

## Daily Standup Template

**What was completed?**
-

**What's in progress?**
-

**Any blockers?**
-

---

## Sprint Review Checklist

- [ ] All items complete or carried over
- [ ] Build succeeds on MSVC
- [ ] Demo prepared showing working systems
- [ ] Velocity calculated
- [ ] Backlog updated

---

## Sprint Retrospective Template

**What went well?**
-

**What could improve?**
-

**Action items for next sprint?**
-
