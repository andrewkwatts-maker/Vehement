# Nova3D AI Tools Rollout Plan

## Overview

This document outlines the comprehensive plan to integrate AI-powered features across all Nova3D editor tools, providing users with intelligent assistance for asset creation, visual scripting, level design, and more.

## Configuration Architecture

### API Key Storage

```
User Config Location (per-platform):
├── Windows: %APPDATA%\Nova3D\gemini_config.json
├── Linux:   ~/.config/nova3d/gemini_config.json
└── macOS:   ~/.config/nova3d/gemini_config.json

Config File Format:
{
  "gemini_api_key": "AI...",
  "openai_api_key": "",        // Future
  "anthropic_api_key": "",     // Future
  "model_thinking": "gemini-2.0-flash-thinking-exp",
  "model_fast": "gemini-2.0-flash",
  "max_refinements": 5,
  "quality_threshold": 0.85
}
```

### Environment Variables (Alternative)
- `GEMINI_API_KEY` - Primary Google Gemini key
- `OPENAI_API_KEY` - OpenAI key (future)
- `ANTHROPIC_API_KEY` - Claude key (future)

### Security
- Config files are in `.gitignore` - **NEVER COMMITTED**
- API keys stored in user-specific directories
- Environment variables take precedence over config files

---

## Batch File Tools

### 1. Setup & Configuration

| Tool | Purpose | Status |
|------|---------|--------|
| `setup_ai_config.bat` | Initial setup wizard for API keys | ✅ Done |
| `verify_ai_setup.bat` | Verify installation and dependencies | 📋 Planned |

### 2. Asset Generation & Management

| Tool | Purpose | Status |
|------|---------|--------|
| `concept_to_sdf.bat` | Generate SDF models from concept art/text | ✅ Done |
| `ai_asset_helper.bat` | General asset configuration and updates | 📋 Planned |
| `ai_material_gen.bat` | Generate PBR materials from references | 📋 Planned |
| `ai_texture_gen.bat` | Generate textures from descriptions | 📋 Planned |
| `ai_animation_assist.bat` | Animation keyframe generation | 📋 Planned |

### 3. Level Design

| Tool | Purpose | Status |
|------|---------|--------|
| `ai_level_design.bat` | AI-assisted level layout generation | 📋 Planned |
| `ai_terrain_gen.bat` | Procedural terrain from descriptions | 📋 Planned |
| `ai_prop_placement.bat` | Intelligent prop placement suggestions | 📋 Planned |

### 4. Visual Scripting

| Tool | Purpose | Status |
|------|---------|--------|
| `ai_visual_script.bat` | Generate visual script nodes from descriptions | 📋 Planned |
| `ai_behavior_gen.bat` | AI/NPC behavior tree generation | 📋 Planned |
| `ai_event_script.bat` | Event-based scripting assistance | 📋 Planned |

### 5. Code & Documentation

| Tool | Purpose | Status |
|------|---------|--------|
| `ai_code_review.bat` | Code analysis and suggestions | 📋 Planned |
| `ai_doc_gen.bat` | Auto-generate documentation | 📋 Planned |

---

## Editor Integration Points

### Asset Browser Panel
**File:** `engine/editor/AssetBrowser.hpp`

**AI Features:**
- [x] `ShowAIGenerateAssetDialog()` - Generate assets from text/images
- [ ] AI-powered search with natural language
- [ ] Automatic asset tagging
- [ ] Duplicate detection
- [ ] Asset recommendations

**Batch File:** `ai_asset_helper.bat`

### SDF Asset Editor
**File:** `engine/editor/SDFAssetEditor.hpp`

**AI Features:**
- [x] `ShowAIEnhanceDialog()` - AI suggestions for SDF models
- [x] `GenerateFromDescription()` - Text-to-SDF generation
- [ ] Automatic LOD generation
- [ ] CSG operation optimization
- [ ] Animation rig suggestions

**Batch File:** `concept_to_sdf.bat`

### Material Editor
**File:** `engine/editor/MaterialEditor.hpp`

**AI Features:**
- [x] `ShowAIMaterialGenerator()` - AI material generation
- [x] `GenerateMaterialFromImage()` - Reference-based generation
- [x] `SuggestMaterialImprovements()` - Quality suggestions
- [ ] Material variation generation
- [ ] PBR parameter estimation from photos

**Batch File:** `ai_material_gen.bat`

### Console Panel
**File:** `engine/editor/ConsolePanel.hpp`

**AI Features:**
- [x] `AnalyzeErrorsWithAI()` - Error diagnosis
- [x] `ShowAIDiagnosticsPanel()` - AI diagnostics
- [ ] Performance bottleneck detection
- [ ] Automatic fix suggestions
- [ ] Log pattern analysis

### Animation Timeline
**File:** `engine/editor/AnimationTimeline.hpp`

**AI Features (Planned):**
- [ ] Keyframe interpolation suggestions
- [ ] Motion capture cleanup
- [ ] Animation style transfer
- [ ] Pose generation from descriptions

**Batch File:** `ai_animation_assist.bat`

### Scene Outliner
**File:** `engine/editor/SceneOutliner.hpp`

**AI Features (Planned):**
- [ ] Scene organization suggestions
- [ ] Automatic grouping
- [ ] Naming conventions enforcement
- [ ] Unused asset detection

---

## Implementation Phases

### Phase 1: Foundation (Current) ✅
- [x] Gemini API integration
- [x] Config file management
- [x] Git security (.gitignore)
- [x] Basic SDF generation pipeline
- [x] Iterative refinement with feedback
- [x] Core editor panel hooks

### Phase 2: Asset Tools
- [ ] Material generation pipeline
- [ ] Texture generation pipeline
- [ ] Animation assistance
- [ ] Batch processing tools

### Phase 3: Level Design
- [ ] Level layout generation
- [ ] Terrain generation
- [ ] Prop placement AI
- [ ] Lighting suggestions

### Phase 4: Visual Scripting
- [ ] Node generation from text
- [ ] Behavior tree generation
- [ ] Event scripting
- [ ] Logic optimization

### Phase 5: Advanced Features
- [ ] Multi-model support (OpenAI, Claude)
- [ ] Local model support (Ollama)
- [ ] Fine-tuned models for game assets
- [ ] Real-time collaboration features

---

## Batch File Template

Each AI batch file follows this structure:

```batch
@echo off
REM ==========================================================================
REM Nova3D AI [Tool Name]
REM ==========================================================================

setlocal enabledelayedexpansion

REM Load shared configuration
call "%~dp0_ai_common.bat"

REM Tool-specific logic
python "%~dp0[script_name].py" %*

endlocal
```

### Shared Configuration (`_ai_common.bat`)
```batch
@echo off
REM Shared AI configuration loader

REM Check for config file
set "CONFIG_FILE=%APPDATA%\Nova3D\gemini_config.json"
if not exist "%CONFIG_FILE%" (
    echo WARNING: No AI configuration found.
    echo Run setup_ai_config.bat first.
)

REM Check for environment variable fallback
if not defined GEMINI_API_KEY (
    if not exist "%CONFIG_FILE%" (
        echo ERROR: No API key configured.
        exit /b 1
    )
)

REM Verify Python
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found.
    exit /b 1
)
```

---

## Usage Examples

### Generate SDF Asset from Concept Art
```batch
concept_to_sdf.bat --input "concept_art.png" --name "fire_mage" --quality 0.9
```

### Generate Material from Reference
```batch
ai_material_gen.bat --reference "metal_plate.jpg" --name "rusty_metal" --type pbr
```

### AI-Assisted Level Layout
```batch
ai_level_design.bat --prompt "medieval castle courtyard with fountain" --size 100x100
```

### Visual Script from Description
```batch
ai_visual_script.bat --prompt "enemy patrol between waypoints, attack player if seen"
```

---

## Quality Assurance

### Quality Metrics
- **Silhouette Accuracy**: Does shape match concept?
- **Proportions**: Are sizes correct?
- **Detail Level**: Enough primitives?
- **Color Accuracy**: Colors match?
- **Material Quality**: PBR appropriate?
- **Animation Ready**: Proper hierarchy?

### Iterative Refinement
1. Initial generation with thinking model
2. Render preview from multiple angles
3. AI evaluates against concept
4. Apply fixes with fast model
5. Repeat until quality threshold met

---

## Future Considerations

### Multi-Model Support
- Allow users to choose between Gemini, OpenAI, Claude
- Automatic fallback if one API fails
- Cost optimization by using cheaper models for simple tasks

### Local Models
- Support Ollama for offline usage
- Fine-tuned models for game-specific assets
- Privacy-focused workflows

### Team Collaboration
- Shared API key pools for teams
- Usage tracking and quotas
- Asset generation history

---

## Getting Started

1. **Run Setup**
   ```batch
   cd tools
   setup_ai_config.bat
   ```

2. **Verify Installation**
   ```batch
   python -c "import google.generativeai; print('Ready!')"
   ```

3. **Generate Your First Asset**
   ```batch
   concept_to_sdf.bat --prompt "heroic knight in shining armor" --name "knight"
   ```

4. **Check Output**
   ```
   generated_assets/knight/
   ├── svgs/           # Orthographic SVG views
   ├── pngs/           # Converted PNGs
   ├── previews/       # Iteration previews
   ├── final/          # Final icon and video
   └── knight_sdf.json # SDF model file
   ```

---

*Document Version: 1.0*
*Last Updated: 2026-01-19*
