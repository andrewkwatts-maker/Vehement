# SDF Character Generation: F to AAA Quality Improvement Plan

## Current State Analysis

### Rendered Output Review
- **Icon**: Gray blob with disconnected purple torus
- **Character**: Unrecognizable, no limbs, no face, no silhouette
- **Materials**: Uniform gray instead of rich PBR
- **Grade**: F (complete failure)

### Root Causes

1. **No PNG Concept Art** - SVGs alone don't provide enough visual detail
2. **Weak Analysis-to-SDF Translation** - 32 body parts in analysis → 21 disconnected primitives
3. **No Iterative Visual Feedback** - Gemini can't see what it's generating
4. **Insufficient SDF Technique Knowledge** - Generic prompts produce generic results
5. **Missing Intermediate Validation** - No quality gates between stages

---

## Phase 1: PNG Concept Art Generation

### 1.1 Add Imagen 3 PNG Generation
Before SVGs, generate photorealistic concept art that Gemini can reference.

```
Pipeline: Prompt → PNG Concept Art → SVG Orthographic Views → SDF Model
```

### 1.2 Required PNG Outputs
- `concept_hero_shot.png` - Dramatic 3/4 view with lighting
- `concept_front.png` - Clean front orthographic
- `concept_side.png` - Profile view
- `concept_turnaround.png` - 4-view turnaround sheet

### 1.3 PNG Quality Requirements
- 1024x1024 minimum resolution
- Consistent lighting (top-left key light)
- Clear silhouette against neutral background
- All key features visible and prominent

---

## Phase 2: Enhanced SVG Generation

### 2.1 SVG Must Reference PNG
```python
# Pass PNG to Gemini along with SVG prompt
prompt = f"""
REFERENCE IMAGE: [attached PNG concept art]

Convert this concept art to technical SVG views:
- Extract exact silhouette proportions
- Match all visible details
- Preserve color palette exactly
- Include construction lines for SDF translation
"""
```

### 2.2 SVG Technical Requirements
- Include measurement annotations (height markers)
- Show primitive decomposition overlay
- Mark bone/joint positions
- Include material zone boundaries

---

## Phase 3: SDF Masterclass Enhancement

### 3.1 Expanded Primitive Library

Add detailed documentation for ALL available SDF primitives:

```
PRIMITIVE ENCYCLOPEDIA:
========================

1. SPHERE
   - Perfect for: eyes, joints, head core, orbs
   - Params: radius
   - Tips: Use scale for ellipsoid effect

2. CAPSULE
   - Perfect for: limbs, fingers, tentacles
   - Params: radius, height
   - Tips: Great for organic tubes

3. BOX / ROUNDED_BOX
   - Perfect for: armor plates, books, crates
   - Params: half_extents, (corner_radius)
   - Tips: Use subtraction for carved details

4. CYLINDER
   - Perfect for: arms, legs, pillars
   - Params: radius, height
   - Tips: Combine with spheres for joints

5. CONE
   - Perfect for: hats, spikes, cloaks
   - Params: radius, height
   - Tips: Invert for funnels

6. TORUS
   - Perfect for: rings, halos, belts
   - Params: major_radius, minor_radius
   - Tips: Great for crowns, bracelets

7. ELLIPSOID
   - Perfect for: torsos, heads, organic masses
   - Params: radii[3]
   - Tips: Primary body primitive

8. PLANE (for cutting)
   - Use with Intersection to slice shapes
   - Params: normal, distance

CSG OPERATIONS:
===============

1. UNION - Combine shapes (default)
2. SMOOTH_UNION - Organic blending
   - smoothness: 0.01-0.1 for subtle
   - smoothness: 0.1-0.3 for very organic
3. SUBTRACTION - Carve out shapes
4. SMOOTH_SUBTRACTION - Soft carving
5. INTERSECTION - Keep only overlap
6. SMOOTH_INTERSECTION - Soft intersection

ARTISTIC TECHNIQUES:
====================

1. LIMB CONSTRUCTION
   - Upper segment: Capsule
   - Joint: Sphere (slightly larger)
   - Lower segment: Capsule
   - Connect with SmoothUnion(0.05)

2. TORSO CONSTRUCTION
   - Chest: Ellipsoid (wide, shallow depth)
   - Waist: Ellipsoid (narrower)
   - Hips: Ellipsoid (wider again)
   - Connect with SmoothUnion(0.08)

3. HEAD CONSTRUCTION
   - Skull: Ellipsoid (main shape)
   - Jaw: Ellipsoid (smaller, lower, forward)
   - Eye sockets: Sphere + SmoothSubtraction
   - Eyes: Small spheres with emissive

4. CLOTH/ROBE CONSTRUCTION
   - Main volume: Large ellipsoid/cone
   - Flow shapes: Overlapping ellipsoids
   - Folds: Thin ellipsoids with SmoothUnion
   - Use 0.1+ smoothness for fabric feel

5. ARMOR/HARD SURFACES
   - Plates: Rounded boxes
   - Trim: Thin boxes with subtraction
   - Use lower smoothness (0.02-0.05)
```

### 3.2 Character Archetype Templates

Provide reference SDF structures for common character types:

```
HUMANOID TEMPLATE (30-40 primitives):
=====================================
ROOT: hips (Ellipsoid)
├── spine (Capsule) → SmoothUnion(0.06)
│   ├── chest (Ellipsoid) → SmoothUnion(0.05)
│   │   ├── neck (Capsule) → SmoothUnion(0.04)
│   │   │   └── head (Ellipsoid)
│   │   │       ├── left_eye (Sphere, emissive)
│   │   │       ├── right_eye (Sphere, emissive)
│   │   │       └── jaw (Ellipsoid) → SmoothUnion(0.03)
│   │   ├── left_shoulder (Sphere) → SmoothUnion(0.04)
│   │   │   ├── left_upper_arm (Capsule)
│   │   │   │   └── left_forearm (Capsule)
│   │   │   │       └── left_hand (Ellipsoid)
│   │   └── right_shoulder (Sphere) → SmoothUnion(0.04)
│   │       └── [mirror left arm]
├── left_hip_joint (Sphere) → SmoothUnion(0.05)
│   ├── left_thigh (Capsule)
│   │   └── left_calf (Capsule)
│   │       └── left_foot (RoundedBox)
└── right_hip_joint (Sphere) → SmoothUnion(0.05)
    └── [mirror left leg]

ROBED MAGE ADDITIONS:
- robe_collar (Torus around neck)
- robe_chest_drape (Ellipsoid, front)
- robe_skirt_front (Cone, large)
- robe_skirt_back (Ellipsoid, trailing)
- robe_sleeve_left (Ellipsoid around forearm)
- robe_sleeve_right (Ellipsoid around forearm)
- hood (Ellipsoid, behind head)
- staff/weapon (Capsule chain)
```

---

## Phase 4: Recursive Polish Loop

### 4.1 Visual Feedback Integration

After each SDF generation, render and feed back to Gemini:

```python
def polish_loop(sdf_model, concept_png, max_iterations=5):
    for i in range(max_iterations):
        # Render current state
        render_png = render_sdf_to_png(sdf_model)

        # Send both concept and render to Gemini
        critique = gemini.analyze([concept_png, render_png], """
        Compare these two images:
        1. CONCEPT ART (target)
        2. CURRENT SDF RENDER (actual)

        Identify SPECIFIC differences:
        - Missing body parts
        - Wrong proportions
        - Missing details
        - Color/material issues

        For each issue, provide:
        - What primitive to add/modify
        - Exact position [x, y, z]
        - Exact size parameters
        - Material properties
        """)

        # Apply fixes
        sdf_model = gemini.fix_sdf(sdf_model, critique)

        # Check quality
        quality = validate_against_concept(render_png, concept_png)
        if quality > 0.9:
            break

    return sdf_model
```

### 4.2 Multi-Stage Validation

```
Stage 1: Silhouette Match
- Render SDF silhouette
- Compare to concept silhouette
- Require 85%+ overlap

Stage 2: Proportion Check
- Head-to-body ratio
- Limb lengths
- Key feature sizes

Stage 3: Material Verification
- Color zones match concept
- Emissive elements present
- Metallic/roughness appropriate

Stage 4: Detail Pass
- All key features present
- No floating/disconnected parts
- Smooth transitions
```

---

## Phase 5: Enhanced Prompting Strategy

### 5.1 Structured Output Format

Force Gemini to output in strict format with validation:

```json
{
  "primitives": [
    {
      "id": "unique_snake_case_id",
      "name": "Human Readable Name",
      "type": "Ellipsoid|Capsule|Sphere|...",
      "purpose": "What this primitive represents",
      "params": { ... },
      "transform": {
        "position": [x, y, z],
        "rotation": [qx, qy, qz, qw],
        "scale": [sx, sy, sz]
      },
      "material": {
        "baseColor": [r, g, b, a],
        "metallic": 0.0-1.0,
        "roughness": 0.0-1.0,
        "emissive": [r, g, b],
        "emissiveIntensity": 0.0+
      },
      "operation": "Union|SmoothUnion|Subtraction|...",
      "smoothness": 0.0-0.5,
      "parent": "parent_primitive_id or null"
    }
  ],
  "quality_checklist": {
    "has_head": true,
    "has_torso": true,
    "has_left_arm": true,
    "has_right_arm": true,
    "has_left_leg": true,
    "has_right_leg": true,
    "has_key_features": ["feature1", "feature2"],
    "primitive_count": 35,
    "hierarchy_depth": 5
  }
}
```

### 5.2 Chain-of-Thought Decomposition

```
STEP 1: Analyze the concept art
- What is the overall silhouette?
- What are the major body masses?
- What unique features define this character?

STEP 2: Plan the skeleton
- Where are the joint positions?
- What are the limb proportions?
- How does the spine curve?

STEP 3: Block in major forms
- Torso group (3-5 primitives)
- Head group (3-5 primitives)
- Arm chains (4 primitives each)
- Leg chains (4 primitives each)

STEP 4: Add character-defining features
- Costume elements
- Accessories
- Special effects (glow, particles)

STEP 5: Refine and connect
- Adjust smoothness values
- Fix any gaps
- Verify hierarchy

STEP 6: Validate
- Does it match the concept?
- Are all parts visible?
- Is the silhouette recognizable?
```

---

## Phase 6: Implementation Checklist

### New Files to Create

1. `tools/generate_concept_png.py` - Imagen 3 PNG generation
2. `tools/sdf_visual_feedback.py` - Render → Critique → Fix loop
3. `tools/sdf_masterclass.py` - Enhanced prompts and templates
4. `tools/quality_validator.py` - Multi-stage validation

### Files to Modify

1. `tools/aaa_sdf_generator.py`:
   - Add PNG generation step
   - Integrate visual feedback loop
   - Enhance prompts with masterclass content

2. `tools/generate_character.py`:
   - Add PNG concept stage
   - Add validation gates
   - Add recursive polish

### New Data Files

1. `tools/data/sdf_primitive_encyclopedia.json` - Full primitive documentation
2. `tools/data/character_templates.json` - Archetype structures
3. `tools/data/quality_rubric.json` - Validation criteria

---

## Success Criteria

### Minimum Viable (B Grade)
- [ ] Character is recognizable
- [ ] All major body parts present
- [ ] Silhouette matches concept
- [ ] Materials show variation

### Target (A Grade)
- [ ] All key features visible
- [ ] Smooth organic connections
- [ ] Proper material zones
- [ ] Dynamic pose capability

### Stretch (AAA Grade)
- [ ] Production-ready quality
- [ ] Animatable hierarchy
- [ ] Rich material detail
- [ ] Iconic silhouette
- [ ] Distinct visual identity

---

## Estimated Changes

| Component | Current | After |
|-----------|---------|-------|
| Pipeline stages | 6 | 10 |
| Primitives per character | 21 | 35-50 |
| Quality iterations | 5 | 10-15 |
| Validation gates | 1 | 5 |
| Prompt size | ~500 tokens | ~3000 tokens |
| Generation time | ~2 min | ~10 min |
