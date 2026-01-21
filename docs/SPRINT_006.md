# Sprint 6: Complete TODO Cleanup

**Sprint Goal**: Implement all remaining TODOs across engine and game code
**Duration**: 1 sprint cycle
**Velocity Target**: 50 story points
**Start Date**: 2026-01-21
**Status**: IN PROGRESS

---

## Sprint Backlog

| Agent | Focus Area | TODOs | SP | Status |
|-------|------------|-------|-----|--------|
| Agent-1 | ShaderGraphEditor Core | 5 | 5 | Pending |
| Agent-2 | ShaderGraphEditor Advanced | 6 | 5 | Pending |
| Agent-3 | UI Widgets & Binding | 7 | 5 | Pending |
| Agent-4 | Campaign UI - Objectives & Briefing | 8 | 5 | Pending |
| Agent-5 | Campaign UI - Cinematic & Campaign | 6 | 5 | Pending |
| Agent-6 | RTS Input Controller | 10 | 5 | Pending |
| Agent-7 | RTS AI & Trading | 6 | 5 | Pending |
| Agent-8 | Graphics - RTX & Rasterizer | 10 | 5 | Pending |
| Agent-9 | Graphics - GBuffer & Depth | 5 | 5 | Pending |
| Agent-10 | Core Systems & Networking | 7 | 5 | Pending |
| **Total** | | **70** | **50** | |

---

## Agent Assignments

### Agent-1: ShaderGraphEditor Core
**Files**: engine/materials/ShaderGraphEditor.cpp
- Line 605: Use proper font
- Line 929: Multi-selection editing
- Line 1185: Serialize node creation action
- Line 1192: Type compatibility checking
- Line 1258: Apply undo implementation

### Agent-2: ShaderGraphEditor Advanced
**Files**: engine/materials/ShaderGraphEditor.cpp
- Line 1270: Apply redo implementation
- Line 1282: Remove node from graph
- Line 1288: Implement duplication
- Line 1416: Update node connections from links
- Line 1639: Load material
- Line 1718: Parameter node editors

### Agent-3: UI Widgets & Binding
**Files**: engine/ui/widgets/UIWidget.cpp, engine/ui/editor/UIEditor.cpp
- Line 74: Subscribe to property changes via reflection
- Line 133: Handle nested object navigation
- Line 142: Implement source update for two-way binding
- Line 341: Individual style property setting
- Line 346: Individual style property getting
- Line 456: Serialize children back to HTML
- UIEditor.cpp:765: Hit test widgets

### Agent-4: Campaign UI - Objectives & Briefing
**Files**: game/src/ui/campaign/ObjectiveUI.cpp, BriefingUI.cpp
- ObjectiveUI:206: Track animation time
- ObjectiveUI:216: Register JavaScript callbacks
- ObjectiveUI:230: Send objective data to HTML
- ObjectiveUI:244: Play sound through audio
- BriefingUI:89,94,99,105: Audio play/stop/pause/resume
- BriefingUI:146: Register JavaScript callbacks
- BriefingUI:166: Send briefing data to HTML
- BriefingUI:170: Update voiceover state

### Agent-5: Campaign UI - Cinematic & Campaign
**Files**: game/src/ui/campaign/CinematicUI.cpp, CampaignUI.cpp
- CinematicUI:144: Register JavaScript callbacks
- CinematicUI:183: Send UI state to HTML
- CampaignUI:147: Request data from CampaignManager
- CampaignUI:185: Get current chapter/mission
- CampaignUI:215: Register JavaScript callbacks
- CampaignUI:239-247: Carousel animation and HTML sync

### Agent-6: RTS Input Controller
**Files**: game/src/rts/RTSInputController.cpp
- Line 338: Building deletion
- Line 386: Patrol mode
- Line 392: Building menu
- Line 402: Gamepad controls
- Line 509: Get player units
- Line 514: Select same type units
- Line 519: Raycast entity selection
- Line 525: Rectangle entity selection
- Line 549-605: Unit commands (move, attack, stop, hold, patrol, attack target)
- Line 673,818,828: Validation and preview rendering

### Agent-7: RTS AI & Trading
**Files**: game/src/rts/WorkerAI.cpp, Trading.cpp, WorldEvent.cpp, ResourceUI.cpp
- WorkerAI:140: Find food source
- WorkerAI:277: Attack behavior
- WorkerAI:339: Job assignment
- WorkerAI:686: Food source target
- Trading:386: Firebase sync
- WorldEvent:520: Name template placeholders
- ResourceUI:198,413: Breakdown and sounds

### Agent-8: Graphics - RTX & Rasterizer
**Files**: engine/graphics/RTXSupport.cpp, RTXAccelerationStructure.cpp, PolygonRasterizer.cpp
- RTXSupport:206: Benchmark
- RTXSupport:261,268: Vulkan/DXR detection
- RTXSupport:332,337: Capability queries
- RTXAccel:174,196,212: Build/update/refit
- RTXAccel:334,385,436,454,462: Instance updates and mesh conversion
- PolygonRasterizer:286,317,347,408: Sort, instancing, cascades, materials

### Agent-9: Graphics - GBuffer & Depth
**Files**: engine/graphics/GBuffer.cpp, HybridDepthMerge.cpp, MeshToSDFConverter.cpp, SDFRasterizer.cpp, RTGIPipeline.cpp
- GBuffer:482: Debug visualization
- HybridDepthMerge:34,64,221: Depth texture, resize, shaders
- MeshToSDFConverter:114,777,788,837: Geometry extraction, OBB, convex decomp
- SDFRasterizer:394: Debug visualization
- RTGIPipeline:131: Frame timer

### Agent-10: Core Systems & Networking
**Files**: engine/core/PropertySystem.cpp, engine/procedural/ProcGenGraph.cpp, engine/networking/FirebaseClient.cpp
- PropertySystem:187,193: SQLite serialization/deserialization
- ProcGenGraph:172,177: Cache serialization/deserialization
- FirebaseClient:76: Token refresh
- FirebaseClient:451,460,472,480,487: HTTP GET/POST/PUT/PATCH/DELETE

---

## Definition of Done

- [ ] TODO comment replaced with working implementation
- [ ] No new compilation errors introduced
- [ ] Code follows existing patterns in file
