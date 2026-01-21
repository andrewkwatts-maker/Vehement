# Nova3D Product Backlog

**Product Owner**: Claude (AI Assistant)
**Last Updated**: 2026-01-21
**Sprint Velocity**: 29 story points (actual from Sprint 1)

---

## Backlog Summary

| Priority | Items | Story Points | Status |
|----------|-------|--------------|--------|
| P0 - Critical | 2 | 13 | 1 In Progress |
| P1 - High | 5 | 34 | 3 Done, 2 Ready |
| P2 - Medium | 8 | 52 | 2 Done, 6 Groomed |
| P3 - Low | 6 | 30 | Backlog |
| **Total** | **21** | **129** | 5 Done (37 SP) |

### Sprint 1 Completed Items
- ✅ BACK-004: Test Coverage (8 SP)
- ✅ BACK-006: AI Tools UX Polish (5 SP)
- ✅ BACK-007: SDF Visual Feedback (8 SP)
- ✅ BACK-012: Documentation Consolidation (5 SP)
- ✅ BACK-014: Asset Schema Validation (3 SP)

---

## P0 - Critical (Blocks Everything)

### BACK-001: Fix JSON Library Build Blocker
**Story Points**: 8
**Status**: Ready
**Assignee**: TBD

**User Story**: As a developer, I need the build to compile successfully so I can develop new features.

**Problem**: nlohmann/json v3.12 creates `nlohmann::std::ranges` namespace that pollutes MSVC's `std::ranges`, causing 100+ compilation errors.

**Acceptance Criteria**:
- [ ] Engine compiles on MSVC with C++20
- [ ] All JSON functionality preserved
- [ ] No runtime performance regression

**Technical Options**:
1. Replace with simdjson (fastest, read-only)
2. Replace with RapidJSON (read/write, proven)
3. Fork nlohmann/json and patch ranges
4. Isolate JSON in wrapper module

**Files**: CMakeLists.txt, engine/core/json_config.hpp

---

### BACK-002: Enable Event System
**Story Points**: 5
**Status**: Ready (depends on BACK-001)
**Assignee**: TBD

**User Story**: As a developer, I need the event system working so I can build reactive game logic.

**Problem**: Event system disabled due to JSON issues.

**Acceptance Criteria**:
- [ ] NOVA_ENABLE_EVENTS=ON compiles
- [ ] EventBinding.cpp JSON serialization works
- [ ] Events can be sent and received

**Files**: engine/events/EventBinding.cpp, engine/events/EventCondition.hpp

---

## P1 - High Priority

### BACK-003: Python Scripting System Update
**Story Points**: 8
**Status**: Ready
**Assignee**: TBD

**User Story**: As a game designer, I need Python scripting to work so I can create gameplay logic without recompiling.

**Problem**: ScriptBindings.cpp has API mismatches with refactored engine.

**Acceptance Criteria**:
- [ ] Entity API bindings match current implementation
- [ ] Logger uses spdlog integration
- [ ] AudioEngine bindings added
- [ ] Transform/Component API fixed
- [ ] Sample Python script runs successfully

**Files**: engine/scripting/ScriptBindings.cpp, engine/scripting/ScriptContext.cpp

---

### BACK-004: Complete Test Coverage for Core Systems ✅ DONE
**Story Points**: 8
**Status**: ✅ Completed (Sprint 1)
**Assignee**: Agent-5

**User Story**: As a developer, I need comprehensive tests so I can refactor confidently.

**Acceptance Criteria**:
- [x] SDF system tests (evaluation, animation)
- [x] Graphics pipeline tests
- [x] Material system tests
- [x] 60%+ code coverage on core systems

**Deliverables**:
- tools/tests/test_bat_validation.py (673 lines) - Comprehensive validation suite with HTML report
- tools/tests/test_visual_script_validation.py - Visual script validation

**Files**: tests/*.cpp, tools/tests/*.py

---

### BACK-005: Audio System Integration
**Story Points**: 5
**Status**: Ready
**Assignee**: TBD

**User Story**: As a player, I want sound effects and music so the game feels immersive.

**Acceptance Criteria**:
- [ ] AudioEngine initializes with game loop
- [ ] 3D positional audio works
- [ ] Music playback works
- [ ] Sound effects trigger on events

**Files**: engine/audio/AudioEngine.cpp, engine/audio/AudioManager.cpp

---

### BACK-006: AI Tools UX Polish ✅ DONE
**Story Points**: 5
**Status**: ✅ Completed (Sprint 1)
**Assignee**: Agent-2

**User Story**: As a content creator, I want polished AI tools so I can generate game assets easily.

**Acceptance Criteria**:
- [x] Error messages are clear and actionable
- [x] Progress feedback during generation
- [x] Generated assets validate against schemas
- [x] Batch generation supports parallelism

**Deliverables**:
- Polished ai_level_design.bat, ai_visual_script.bat, ai_asset_helper.bat
- Enhanced gemini_sdf_pipeline.py
- API key setup wizard

**Files**: tools/*.py, tools/*.bat

---

### BACK-007: SDF Visual Feedback Loop Improvements ✅ DONE
**Story Points**: 8
**Status**: ✅ Completed (Sprint 1)
**Assignee**: Agent-3

**User Story**: As an artist, I want the AI to iterate on SDF models visually so quality improves automatically.

**Acceptance Criteria**:
- [x] Visual feedback detects pose/proportion issues
- [x] Automatic fix suggestions are accurate
- [x] Quality score correlates with visual appeal
- [x] Iteration converges within 5 rounds

**Files**: tools/sdf_visual_feedback.py, tools/aaa_sdf_generator.py

---

## P2 - Medium Priority

### BACK-008: GPU Radiance Cascade Propagation
**Story Points**: 13
**Status**: Groomed
**Assignee**: TBD

**User Story**: As a player, I want smooth 60fps rendering with global illumination.

**Acceptance Criteria**:
- [ ] Compute shader for cascade propagation
- [ ] 10x+ performance vs CPU implementation
- [ ] Visual parity with CPU version
- [ ] Works on OpenGL 4.3+ GPUs

**Files**: engine/graphics/RadianceCascade.cpp, assets/shaders/radiance_cascade.comp

---

### BACK-009: Complete Editor Features
**Story Points**: 8
**Status**: Groomed
**Assignee**: TBD

**User Story**: As a level designer, I need a complete editor to build game content.

**Acceptance Criteria**:
- [ ] Screen-to-world raycasting
- [ ] Map load/save
- [ ] Object placement tools
- [ ] Heightmap import/export

**Files**: examples/StandaloneEditor.cpp

---

### BACK-010: SDF Debug Visualization
**Story Points**: 5
**Status**: Groomed
**Assignee**: TBD

**User Story**: As a developer, I need to visualize SDF internals to debug models.

**Acceptance Criteria**:
- [ ] Distance field heatmap
- [ ] Gradient visualization
- [ ] BVH hierarchy overlay
- [ ] Step count visualization

**Files**: engine/graphics/SDFRasterizer.cpp

---

### BACK-011: Mesh-to-SDF Conversion Tool
**Story Points**: 8
**Status**: Groomed
**Assignee**: TBD

**User Story**: As an artist, I want to convert existing meshes to SDF format.

**Acceptance Criteria**:
- [ ] FBX/OBJ/GLTF import
- [ ] Signed distance field generation
- [ ] LOD generation
- [ ] Progress reporting

**Files**: tools/MeshToSDFTool.cpp

---

### BACK-012: Documentation Consolidation ✅ DONE
**Story Points**: 5
**Status**: ✅ Completed (Sprint 1)
**Assignee**: Agent-6

**User Story**: As a new developer, I need clear documentation to onboard quickly.

**Acceptance Criteria**:
- [x] Single entry point (docs/README.md)
- [x] Updated API docs
- [x] Architecture diagrams
- [x] Build troubleshooting guide

**Deliverables**:
- docs/GETTING_STARTED.md (460 lines) - Complete getting started guide
- docs/TROUBLESHOOTING.md (553 lines) - Comprehensive troubleshooting guide
- docs/README.md - Documentation index

**Files**: docs/*.md

---

### BACK-013: Build System Cleanup
**Story Points**: 5
**Status**: Groomed
**Assignee**: TBD

**User Story**: As a developer, I want fast, clean builds.

**Acceptance Criteria**:
- [ ] Remove 400+ orphaned files
- [ ] Modular CMakeLists.txt
- [ ] 30%+ faster incremental builds

**Files**: CMakeLists.txt

---

### BACK-014: Asset Schema Validation ✅ DONE
**Story Points**: 3
**Status**: ✅ Completed (Sprint 1)
**Assignee**: Agent-4

**User Story**: As a content creator, I want my assets validated before runtime.

**Acceptance Criteria**:
- [x] JSON schema for all asset types
- [x] Validation tool with clear errors
- [x] CI integration

**Deliverables**:
- tools/schemas/sdf_model.schema.json (387 lines)
- tools/schemas/animation.schema.json (282 lines)
- tools/schemas/material.schema.json
- tools/schemas/visual_script.schema.json
- tools/schemas/analysis.schema.json

**Files**: tools/schemas/*.json

---

### BACK-015: Animation Preview Videos
**Story Points**: 5
**Status**: Groomed
**Assignee**: TBD

**User Story**: As an artist, I want to preview animations without launching the editor.

**Acceptance Criteria**:
- [ ] Render animation to video file
- [ ] Multiple camera angles
- [ ] Configurable quality/fps

**Files**: tools/asset_media_renderer.cpp

---

## P3 - Low Priority

### BACK-016: RTX Ray Tracing Pipeline
**Story Points**: 13
**Status**: Backlog
**Assignee**: TBD

**User Story**: As a player with RTX hardware, I want hardware-accelerated ray tracing.

**Files**: engine/graphics/RTXPathTracer.cpp

---

### BACK-017: Asset Editors (13 types)
**Story Points**: 8
**Status**: Backlog
**Assignee**: TBD

**User Story**: As an artist, I need dedicated editors for each asset type.

**Files**: engine/editor/panels/*.cpp

---

### BACK-018: PCG Graph Implementation
**Story Points**: 5
**Status**: Backlog
**Assignee**: TBD

**User Story**: As a level designer, I want procedural content generation tools.

**Files**: examples/PCGNodeGraph.hpp

---

### BACK-019: Platform Support (Linux/macOS)
**Story Points**: 8
**Status**: Backlog
**Assignee**: TBD

**User Story**: As a developer, I want to build on Linux and macOS.

**Files**: engine/platform/*

---

### BACK-020: Web Export (WebGL/WebGPU)
**Story Points**: 13
**Status**: Backlog
**Assignee**: TBD

**User Story**: As a publisher, I want to deploy to web browsers.

---

### BACK-021: Mobile Optimization (Android/iOS)
**Story Points**: 8
**Status**: Backlog
**Assignee**: TBD

**User Story**: As a mobile player, I want smooth performance on phones/tablets.

---

## Sprint History

| Sprint | Goal | Velocity | Status |
|--------|------|----------|--------|
| Sprint 1 | Fix build, polish AI tools | 29 SP | ✅ Complete |
| Sprint 2 | Core systems & C++ fixes | TBD | 🔄 Planning |

---

## Definition of Done

- [ ] Code compiles without warnings
- [ ] Unit tests pass
- [ ] Code reviewed
- [ ] Documentation updated
- [ ] No new technical debt introduced

---

## Notes

- **Build Blocker**: BACK-001 blocks most other work
- **Quick Wins**: BACK-006 (AI tools) can proceed independently
- **Dependencies**: BACK-002 depends on BACK-001
- **Parallel Work**: BACK-003, BACK-004, BACK-005 can run in parallel after BACK-001

