# Nova3D Launch Backlog

**Product Owner**: Claude (AI Assistant)
**Launch Goal**: Engine ready to launch with full content pipeline
**Last Updated**: 2026-01-21
**Target Velocity**: 25 SP per sprint

---

## Launch Criteria

The engine is ready to launch when:
- [ ] All editor UI menu items functional (no placeholder TODOs)
- [ ] Can generate all asset types via Gemini
- [ ] Can generate and edit levels with PCG
- [ ] Can place and manage assets in scenes
- [ ] All content has icons/thumbnails
- [ ] Assets can be updated via Gemini prompts

---

## Sprint 3 Backlog (Launch Critical)

### LAUNCH-001: Editor File Operations (8 SP)
**Priority**: P0 - BLOCKING
**Status**: To Do

Implement core file operations in EditorApplication.cpp:
- [ ] Project load/save (JSON format)
- [ ] Scene load/save serialization
- [ ] File browser dialog (native or ImGui)
- [ ] New/Open/Save asset workflows

**Files**: engine/editor/EditorApplication.cpp:569-736

---

### LAUNCH-002: AI Tool Menu Integration (5 SP)
**Priority**: P0 - BLOCKING
**Status**: To Do

Connect AI menu items to actual functionality:
- [ ] "Generate Character" → calls generate_character.py
- [ ] "AI Polish" → calls ai_asset_helper.py
- [ ] "Get Suggestions" → Gemini API call
- [ ] "Generate Variations" → Gemini API call
- [ ] "Setup API Key" → shows setup wizard
- [ ] "AI Settings" → shows settings panel

**Files**: engine/editor/EditorApplication.cpp:1558-1619

---

### LAUNCH-003: Asset Creation Workflow (5 SP)
**Priority**: P0 - BLOCKING
**Status**: To Do

Implement asset creation pipeline:
- [ ] New Asset dialog with type selection
- [ ] Asset template system
- [ ] Save asset to correct location
- [ ] Register in asset browser

**Files**: engine/editor/EditorApplication.cpp:1270-1291, 1642-1676

---

### LAUNCH-004: Icon/Thumbnail Generation (5 SP)
**Priority**: P1 - HIGH
**Status**: To Do

Complete thumbnail system:
- [ ] Fix AssetThumbnailCache TODOs
- [ ] Generate icons for all asset types
- [ ] Cache thumbnails to disk
- [ ] Background generation queue

**Files**: engine/editor/AssetThumbnailCache.cpp

---

### LAUNCH-005: Level Editor PCG Integration (8 SP)
**Priority**: P1 - HIGH
**Status**: To Do

Connect level generation to editor:
- [ ] PCG panel in editor
- [ ] Terrain generation from prompts
- [ ] Asset auto-placement
- [ ] Level save/load

**Files**: engine/terrain/*.cpp, tools/ai_level_design.py

---

### LAUNCH-006: Edit Menu Operations (3 SP)
**Priority**: P1 - HIGH
**Status**: To Do

Implement edit operations:
- [ ] Cut/Copy/Paste for entities
- [ ] Undo/Redo system connection
- [ ] Selection management

**Files**: engine/editor/EditorApplication.cpp:1728-1734

---

### LAUNCH-007: Gemini Prompt Asset Updates (5 SP)
**Priority**: P1 - HIGH
**Status**: To Do

Allow updating existing assets via prompts:
- [ ] "Describe changes" dialog
- [ ] Parse existing asset JSON
- [ ] Send to Gemini with modification prompt
- [ ] Apply changes and save

**Files**: tools/ai_asset_helper.py, engine/editor/AIAssistantPanel.hpp

---

### LAUNCH-008: Play Mode Serialization (5 SP)
**Priority**: P2 - MEDIUM
**Status**: To Do

Fix play mode:
- [ ] Serialize scene state before play
- [ ] Restore scene state on stop
- [ ] Handle multi-scene

**Files**: engine/editor/EditorApplication.cpp:1004-1023, PlayMode.cpp

---

## Sprint 3 Summary

| ID | Item | SP | Priority |
|----|------|-----|----------|
| LAUNCH-001 | Editor File Operations | 8 | P0 |
| LAUNCH-002 | AI Tool Menu Integration | 5 | P0 |
| LAUNCH-003 | Asset Creation Workflow | 5 | P0 |
| LAUNCH-004 | Icon/Thumbnail Generation | 5 | P1 |
| LAUNCH-005 | Level Editor PCG Integration | 8 | P1 |
| **Total** | | **31** | |

**Note**: Taking 31 SP based on improved agent efficiency

---

## Sprint 4 Backlog (Polish)

| ID | Item | SP | Priority |
|----|------|-----|----------|
| LAUNCH-006 | Edit Menu Operations | 3 | P1 |
| LAUNCH-007 | Gemini Prompt Updates | 5 | P1 |
| LAUNCH-008 | Play Mode Serialization | 5 | P2 |
| LAUNCH-009 | Documentation Links | 2 | P2 |
| LAUNCH-010 | Validation Integration | 3 | P2 |
| **Total** | | **18** | |

---

## Completed (Sprints 1-2)

- ✅ JSON wrapper for build fix
- ✅ Asset JSON schemas (SDF, animation, material, visual script)
- ✅ Documentation (Getting Started, Troubleshooting)
- ✅ AI tools UX polish
- ✅ Test validation framework
- ✅ Python scripting bindings
- ✅ Event system fixes

---

## Definition of Done (Launch)

- [ ] Feature works end-to-end in editor
- [ ] No placeholder TODOs in implementation
- [ ] Error handling for edge cases
- [ ] Works with Gemini API
- [ ] Tested with sample content
