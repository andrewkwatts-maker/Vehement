# Nova3D AI Integration - Full Implementation Plan

## Executive Summary

Based on learnings from Dreadlady generation:
- Initial quality: 62% → Final: 85% (5 iterations)
- 17 primitives generated
- Good material structure with assignments
- Rate limiting caused incomplete animation generation

This plan addresses all issues and implements full editor integration.

---

## Part 1: Quality Improvement Learnings

### Issues Identified

| Issue | Impact | Solution |
|-------|--------|----------|
| Low initial quality (62%) | Extra iterations needed | Enhanced prompts with detailed constraints |
| Rate limiting (429 errors) | Incomplete generation | Exponential backoff, request batching |
| Unicode encoding | Windows console errors | ASCII-safe output, UTF-8 file encoding |
| Model unavailability | Generation failures | Model fallback chain |
| Missing schema validation | Potential invalid output | Strict JSON schema enforcement |

### Enhanced Prompt Strategy

**Before (Basic):**
```
Generate an SDF model for a dark sorceress
```

**After (Enhanced):**
```
Generate an SDF model following these STRICT requirements:

CHARACTER: Dark sorceress "Dreadlady"
- Flowing black robes (use ellipsoids with smooth union, smoothness 0.15-0.25)
- Glowing purple eyes (small spheres, emissive 1.5+)
- Spectral crown (torus base + cone spikes, emissive 0.8)
- Skeletal fingers (capsules, radius 0.01-0.02)

TECHNICAL CONSTRAINTS:
- Minimum 25 primitives, maximum 50
- Use parentIndex for hierarchy (root = -1)
- Operations: 3 = smooth union, 4 = smooth subtraction
- All transforms must have position, rotation, scale
- All materials must have baseColor (RGBA), metallic, roughness, emissive

OUTPUT FORMAT: Strict JSON matching schema exactly
```

---

## Part 2: JSON Schema Definitions

### SDF Model Schema

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["name", "version", "primitives"],
  "properties": {
    "name": { "type": "string", "minLength": 1 },
    "version": { "type": "string", "pattern": "^\\d+\\.\\d+$" },
    "analysis": { "type": "string" },
    "primitives": {
      "type": "array",
      "minItems": 5,
      "items": {
        "type": "object",
        "required": ["name", "type", "parentIndex", "transform", "operation", "material"],
        "properties": {
          "name": { "type": "string" },
          "type": { "type": "integer", "minimum": 0, "maximum": 9 },
          "parentIndex": { "type": "integer", "minimum": -1 },
          "transform": {
            "type": "object",
            "required": ["position", "rotation", "scale"],
            "properties": {
              "position": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 },
              "rotation": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 },
              "scale": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 }
            }
          },
          "parameters": { "type": "object" },
          "operation": { "type": "integer", "minimum": 0, "maximum": 6 },
          "op_params": { "type": "object" },
          "material": {
            "type": "object",
            "required": ["baseColor", "metallic", "roughness"],
            "properties": {
              "baseColor": { "type": "array", "items": { "type": "number" }, "minItems": 4, "maxItems": 4 },
              "metallic": { "type": "number", "minimum": 0, "maximum": 1 },
              "roughness": { "type": "number", "minimum": 0, "maximum": 1 },
              "emissive": { "type": "number", "minimum": 0 }
            }
          }
        }
      }
    }
  }
}
```

### Animation Schema

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["name", "duration", "fps", "loop", "tracks"],
  "properties": {
    "name": { "type": "string" },
    "duration": { "type": "number", "exclusiveMinimum": 0 },
    "fps": { "type": "integer", "minimum": 1, "maximum": 120 },
    "loop": { "type": "boolean" },
    "tracks": {
      "type": "array",
      "minItems": 1,
      "items": {
        "type": "object",
        "required": ["bone", "property", "keyframes"],
        "properties": {
          "bone": { "type": "string" },
          "property": { "enum": ["position", "rotation", "scale"] },
          "keyframes": {
            "type": "array",
            "minItems": 2,
            "items": {
              "type": "object",
              "required": ["time", "value"],
              "properties": {
                "time": { "type": "number", "minimum": 0 },
                "value": { "type": "array", "items": { "type": "number" } },
                "easing": { "type": "string" }
              }
            }
          }
        }
      }
    }
  }
}
```

### Material Schema

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["materials"],
  "properties": {
    "materials": {
      "type": "object",
      "additionalProperties": {
        "type": "object",
        "required": ["baseColor", "metallic", "roughness"],
        "properties": {
          "baseColor": { "type": "array", "items": { "type": "number", "minimum": 0, "maximum": 1 }, "minItems": 4, "maxItems": 4 },
          "metallic": { "type": "number", "minimum": 0, "maximum": 1 },
          "roughness": { "type": "number", "minimum": 0, "maximum": 1 },
          "emissive": { "type": "number", "minimum": 0 },
          "emissiveColor": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 }
        }
      }
    },
    "material_assignments": {
      "type": "object",
      "additionalProperties": { "type": "string" }
    }
  }
}
```

---

## Part 3: Authentication Options

### Option A: API Key (Current - Recommended)

**Pros:**
- Simple, direct access
- No OAuth complexity
- Works offline after setup

**Cons:**
- User must obtain API key
- Key management burden on user

### Option B: Google OAuth (Research Required)

**Investigation Results:**
- Google Gemini API requires API key, not OAuth tokens
- OAuth is for user data access, not API services
- No direct OAuth → Gemini API pathway exists

**Conclusion:** API Key is the only viable option for Gemini access.

### Improved UX for API Key Setup

1. **First-run wizard** in editor
2. **Link to API key page** with instructions
3. **Key validation** with friendly error messages
4. **Secure storage** in user's AppData

---

## Part 4: Editor UX Integration

### AI Assistant Panel Design

```
┌─────────────────────────────────────────────────────┐
│ 🤖 AI Asset Assistant                          [≡] │
├─────────────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────────────┐ │
│ │ Describe your asset:                           │ │
│ │ ┌───────────────────────────────────────────┐  │ │
│ │ │ Dark sorceress with flowing robes,       │  │ │
│ │ │ glowing purple eyes, spectral crown...   │  │ │
│ │ └───────────────────────────────────────────┘  │ │
│ └─────────────────────────────────────────────────┘ │
│                                                     │
│ Asset Type: [Character ▼]  Quality: [AAA ▼]        │
│                                                     │
│ ☑ Generate Model    ☑ Generate Materials           │
│ ☑ Generate Anims    ☑ Generate Scripts             │
│                                                     │
│ [🚀 Generate] [🔄 Polish Selected] [📋 From Image] │
├─────────────────────────────────────────────────────┤
│ Progress: ████████████░░░░ 75%                     │
│ Stage: Refining model (iteration 3/5)              │
├─────────────────────────────────────────────────────┤
│ Quality: 85% ⭐ AAA                                │
│ Primitives: 25 | Materials: 4 | Animations: 12    │
│                                                     │
│ Issues:                                             │
│ • Minor: Low detail on hands                       │
│                                                     │
│ [Accept & Import] [Refine Further] [Cancel]        │
└─────────────────────────────────────────────────────┘
```

### Menu Integration

```
File > New > AI-Generated...
  > Character
  > Building
  > Prop
  > Weapon
  > Vehicle

Edit > AI Assist
  > Polish Selected Asset
  > Suggest Improvements
  > Generate Variations

Tools > AI Pipeline
  > Open AI Assistant
  > Configure API Key
  > View Quality Report
```

---

## Part 5: Implementation Tasks

### Task 1: Create JSON Schemas (schemas/)
- sdf_model.schema.json
- animation.schema.json
- material.schema.json
- visual_script.schema.json
- config.schema.json

### Task 2: Update Pipeline with Enhanced Prompts
- Improve aaa_sdf_generator.py
- Add detailed constraints to prompts
- Include schema in prompt context

### Task 3: Add Robust Error Handling
- Exponential backoff for rate limits
- Model fallback chain
- Graceful degradation

### Task 4: Create Editor AI Panel (C++)
- AIAssistantPanel.hpp/cpp
- Prompt input with history
- Progress display
- Quality metrics visualization

### Task 5: Integrate with Editor Application
- Register AI panel
- Add menu items
- Connect to AIPipelineRunner

### Task 6: Create API Key Setup Wizard
- First-run detection
- Step-by-step instructions
- Key validation

---

## Implementation Order

1. **Phase A** (Schemas & Validation) - 30 min
2. **Phase B** (Enhanced Prompts) - 30 min
3. **Phase C** (Error Handling) - 20 min
4. **Phase D** (Editor Panel) - 45 min
5. **Phase E** (Menu Integration) - 15 min
6. **Phase F** (Setup Wizard) - 20 min

Total Estimated: ~2.5 hours

---

## Success Criteria

- [ ] All generated assets pass 100% schema validation
- [ ] Initial quality ≥75% (reduced iterations needed)
- [ ] No unhandled errors during generation
- [ ] Editor provides full AI workflow
- [ ] Users can generate assets without command line
