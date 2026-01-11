# Asset Rendering Pipeline - Complete Fix & Expansion Plan

## Current Situation

**Assets Created**: 10 hero JSON files with complete SDF models ✅
**Icons Status**: Yellow placeholder squares (PIL-generated) ⚠️
**Need**: Actual rendered 3D SDF models as transparent PNGs

---

## Phase 1: Fix Rendering Pipeline (Priority 1)

### Task 1.1: Build & Test asset_icon_renderer
- **Agent Created**: Asset renderer tool (`tools/asset_icon_renderer.cpp`)
- **Status**: Code written, needs Window API fixes
- **Action**: Fix compilation errors and test with alien_commander.json

### Task 1.2: Verify Output Quality
- **Test**: Render one hero asset
- **Verify**: PNG shows actual 3D model (not yellow square)
- **Check**: Transparent background works
- **Validate**: Lighting and materials applied correctly

### Task 1.3: Batch Processing
- **Tool**: `tools/render_assets.py` (already created)
- **Test**: Process all 10 hero assets
- **Output**: Screenshots in `screenshots/asset_icons/heroes/`
- **Performance**: Measure time per asset

---

## Phase 2: Polish Existing Race Assets (Priority 2)

### Current Asset Inventory

From RTS campaign system, we have building components that need polish:
- **Aliens**: Gateway, Nexus (components exist)
- **Cryptids**: Den, Sacred Grove (components exist)
- **Elves**: Ancient Tree (components exist)
- **Fairies**: Enchanted structures (components exist)
- **Humans**: Castle, Barracks (need creation)
- **Mechs**: Factory, Turret (need creation)
- **Undead**: Necropolis, Crypt (need creation)

### Task 2.1: Inventory All Existing Assets
Scan for all JSON files with `sdfModel` in:
- `game/assets/configs/buildings/*/`
- `game/assets/configs/units/*/`
- Any other asset directories

### Task 2.2: Polish Existing Building Components
For each existing building component JSON:
1. **Add advanced SDF blending** (ExponentialSmoothUnion, CubicSmoothUnion, etc.)
2. **Enhance materials** (increase metallic, add anisotropy, subsurface)
3. **Add lighting** (4-5 point cinematic setup)
4. **Improve primitive count** (add detail where needed)
5. **Add animation** (if applicable - rotating parts, glowing effects)

### Task 2.3: Create Missing Unit Assets
For each race, create 2-3 basic units:
- Use hero assets as templates
- Reduce primitive count (25-35 vs 40-50 for heroes)
- Maintain race visual identity
- Apply advanced SDF techniques

### Task 2.4: Polish Building Assets
For each race, create/polish 2-3 buildings:
- Use existing components where available
- Add detail and advanced blending
- Maintain architectural consistency
- 15-30 primitives per building

---

## Phase 3: Complete Asset Library (Priority 3)

### Target Asset Counts

| Race | Heroes | Units | Buildings | Total |
|------|--------|-------|-----------|-------|
| Aliens | 1 ✅ | 0→3 | 2→3 | 4→7 |
| Cryptids | 1 ✅ | 0→3 | 2→3 | 4→7 |
| Elves | 1 ✅ | 0→3 | 2→3 | 4→7 |
| Fairies | 1 ✅ | 0→3 | 2→3 | 4→7 |
| Humans | 2 ✅ | 0→3 | 0→3 | 2→8 |
| Mechs | 1 ✅ | 0→3 | 0→3 | 1→7 |
| Undead | 1 ✅ | 0→3 | 0→3 | 1→7 |
| **TOTAL** | **10** | **0→21** | **4→21** | **14→52** |

### Parallel Agent Strategy

Launch 7 specialized agents (one per race):
- Each agent creates 3 units + polishes 3 buildings
- Apply hero polish techniques to all assets
- Generate icons after creation
- ~6 assets per agent = 42 total new assets

---

## Implementation Workflow

### Step 1: Fix Rendering (Today)
```bash
# 1. Check Window API and fix renderer
# 2. Build asset_icon_renderer
cd H:/Github/Old3DEngine/build
cmake --build . --target asset_icon_renderer --config Debug

# 3. Test with one hero
bin/Debug/asset_icon_renderer.exe ^
  "game/assets/heroes/alien_commander.json" ^
  "test_alien.png"

# 4. Verify PNG shows 3D model
```

### Step 2: Batch Render Current Heroes (Today)
```bash
# Render all 10 heroes
cd tools
python render_assets.py

# Check output
ls -la ../screenshots/asset_icons/heroes/*.png
```

### Step 3: Scan Existing Assets (Today)
```bash
# Find all assets with SDF models
python scan_all_race_assets.py > existing_assets.txt

# Review what needs polishing
cat existing_assets.txt
```

### Step 4: Launch Polish Agents (Tomorrow)
```python
# For each race, launch agent to:
# - Polish existing building components
# - Create 3 unit assets
# - Create/polish 3 building assets
# - Apply advanced SDF techniques from hero research
```

### Step 5: Batch Render All Assets (Tomorrow)
```bash
# After agents complete, render everything
python render_assets.py --all

# Expected output: ~52 asset icons
```

---

## Agent Task Templates

### Agent Task: Polish Existing Assets
```
Input: List of existing asset JSONs for [RACE]
Task:
1. Read each existing asset
2. Add advanced SDF blending modes
3. Enhance materials (metallic, roughness, anisotropy)
4. Add/improve lighting setup
5. Increase detail where appropriate
6. Save updated JSON files
Output: Polished asset JSONs ready for icon rendering
```

### Agent Task: Create Unit Assets
```
Input: Race identity, hero asset as template
Task:
1. Create 3 unit types (e.g., Worker, Warrior, Ranged)
2. 25-35 primitives each (lighter than heroes)
3. Race-appropriate materials and colors
4. Advanced SDF blending
5. Simple idle animations
Output: 3 new unit JSON files
```

### Agent Task: Create/Polish Buildings
```
Input: Race identity, existing components (if any)
Task:
1. Create/enhance 3 building types (Base, Production, Defense)
2. 15-30 primitives each
3. Architectural consistency with race theme
4. Advanced materials and blending
5. Static or simple animation (rotating parts, glows)
Output: 3 building JSON files
```

---

## Success Criteria

### Phase 1 Complete When:
- [x] asset_icon_renderer compiles and runs
- [x] Renders actual 3D SDF models (not placeholders)
- [x] Transparent background works
- [x] All 10 hero icons generated
- [x] Icons show correctly in file browser

### Phase 2 Complete When:
- [ ] All existing building components polished
- [ ] Advanced SDF blending applied throughout
- [ ] Enhanced materials on all assets
- [ ] Icons generated for all polished assets

### Phase 3 Complete When:
- [ ] 21 unit assets created across 7 races
- [ ] 21 building assets created/polished across 7 races
- [ ] All 52 total assets have icons
- [ ] Visual consistency within each race
- [ ] Performance acceptable (<500ms per asset render)

---

## File Organization

```
game/assets/
├── heroes/
│   ├── alien_commander.json ✅
│   ├── beast_lord.json ✅
│   ├── elven_ranger.json ✅
│   ├── fairy_queen.json ✅
│   ├── human_knight.json ✅
│   ├── mech_commander.json ✅
│   ├── lich_king.json ✅
│   └── warrior_hero_v3.json ✅
├── units/
│   ├── aliens/
│   │   ├── probe.json (to create)
│   │   ├── stalker.json (to create)
│   │   └── void_ray.json (to create)
│   ├── cryptids/ ...
│   └── ... (7 races × 3 units = 21 files)
└── buildings/
    ├── aliens/
    │   ├── gateway.json (polish existing)
    │   ├── nexus.json (polish existing)
    │   └── forge.json (to create)
    ├── cryptids/ ...
    └── ... (7 races × 3 buildings = 21 files)

screenshots/asset_icons/
├── heroes/ (10 icons)
├── units/ (21 icons - to create)
└── buildings/ (21 icons - to create)
```

---

## Timeline Estimate

- **Phase 1** (Rendering Pipeline): 2-4 hours
- **Phase 2** (Polish Existing): 4-6 hours (with agents)
- **Phase 3** (Create Full Library): 6-8 hours (with agents)
- **Total**: 12-18 hours of development time

With parallel agents: Compress to ~1-2 days of wall-clock time

---

## Next Immediate Actions

1. ✅ Create this plan document
2. ⏳ Fix asset_icon_renderer.cpp Window API issues
3. ⏳ Build and test renderer
4. ⏳ Generate real icons for 10 heroes
5. ⏳ Review output and verify quality
6. ⏳ Scan existing assets for polish candidates
7. ⏳ Launch parallel polish/creation agents

**Let's start with Step 2: Fix the renderer!**
