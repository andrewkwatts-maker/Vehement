# Comprehensive Asset Polish Plan
## From Hero Research to Full Race Asset Library

Based on findings from warrior_hero_v3 polish work, this document outlines the systematic approach to polish all race assets to cinematic AAA quality.

---

## Executive Summary

**Goal**: Polish all 7 race assets (heroes, units, buildings) using advanced SDF techniques discovered during hero research, then generate transparent icon screenshots for use in the game UI.

**Timeline**: Create representative samples for each race (2-3 units + 1-2 buildings per race)
**Output**: 50-70 polished assets + transparent PNG icons

---

## Key Findings from Hero Polish

### Advanced SDF Techniques Implemented

1. **ExponentialSmoothUnion** - Organic flowing blends (neck, torso)
   - Use for: Character anatomy, organic connections
   - Smoothness: 0.12-0.18
   - BlendStrength: 8.0-12.0

2. **CubicSmoothUnion** - Smoother joint transitions
   - Use for: Shoulders, elbows, hips, knees, building corners
   - Smoothness: 0.09-0.15
   - Better C2 continuity than standard polynomial

3. **PowerSmoothUnion** - Controllable equipment attachment
   - Use for: Helmets, weapons, armor pieces, building decorations
   - BlendPower: 4.0-8.0
   - Sharp control over blend characteristics

4. **DistanceAwareSmoothUnion** - Prevents unwanted merging
   - Use for: Fingers, tentacles, antennae, thin details
   - DistanceThreshold: 0.04-0.08
   - Critical for animation-safe models

### Material Best Practices

**Metallic Surfaces** (Armor, Weapons, Buildings):
- Metallic: 0.85-0.98 (high values for polished metal)
- Roughness: 0.15-0.30 (lower = shinier)
- Anisotropy: 0.20-0.60 (brushed metal effect)
- Color variation: Distinct tones for different armor pieces

**Organic Surfaces** (Skin, Wood, Fabric):
- Metallic: 0.0
- Roughness: 0.65-0.85
- SubsurfaceScattering: 0.12-0.18 for skin
- Warmer albedo colors (+0.02-0.05 in red channel)

**Glowing/Magical Elements**:
- Emissive: [0.01-0.08 RGB]
- EmissiveStrength: 0.8-1.2
- Metallic: 0.0-0.3
- Use sparingly for focus points

### Animation Enhancements

- **Breathing**: 4-second cycle, chest expansion
- **Weapon sway**: ±1.5° rotation with breathing
- **Micro-movements**: Fingers 2-4.5° curl variation
- **Secondary motion**: Cape lag, delayed response

### Lighting Setup (Studio Quality)

**4-Point Lighting**:
1. Key: Intensity 1.65, warm golden [1.22, 1.12, 1.02]
2. Fill: Intensity 0.38, warm [1.0, 0.92, 0.78]
3. Rim: Intensity 1.15, cool [0.92, 1.0, 1.15]
4. Accent: Intensity 0.55, cool metallic [0.85, 0.88, 1.0]

---

## Race-Specific Asset Plans

### 1. ALIENS (High-Tech Energy-Based)

**Visual Identity**: Sleek tech, glowing energy, blue/purple/cyan colors

**Hero: Alien Commander**
- Base: Humanoid energy being
- Primitives: 35-40 (full anatomy with energy effects)
- Special: Floating energy orbs, holographic displays
- Materials:
  - Body: Metallic 0.9, emissive cyan [0.3, 0.7, 1.0]
  - Armor: Smooth chrome, anisotropy 0.4
  - Energy cores: High emissive 1.2, pulsing animation
- Blending: ExponentialSmoothUnion for energy connections
- Animation: Hovering idle (0.2m vertical oscillation)

**Unit: Alien Warrior**
- Primitives: 25-30
- Capsule-based limbs with energy weapon
- Glowing visor: Emissive [0.25, 0.65, 0.95]
- Power armor with anisotropic finish

**Building: Gateway** (Already exists - polish it!)
- Current: 7 primitives
- Enhance: Add more detail layers (12-15 primitives)
- Add: Pulsing energy rings using animated emissive
- Polish: Increase metallic to 0.9, add anisotropy
- Blend: PowerSmoothUnion for architectural elements

**Building: Nexus** (Already exists - polish it!)
- Add: More pylon details, energy collectors
- Materials: Glowing crystals with subsurface scattering
- Animation: Rotating energy field

### 2. CRYPTIDS (Primal Organic Monsters)

**Visual Identity**: Scales, fur, claws, earth tones, bioluminescence

**Hero: Beast Lord**
- Base: Massive quadruped creature
- Primitives: 40-45 (thick limbs, muscular)
- Special: Bone protrusions, glowing eyes
- Materials:
  - Hide: Rough 0.9, dark browns/greens
  - Scales: Metallic 0.2, anisotropy 0.3 (directional shine)
  - Claws: Metallic 0.6, emissive yellow eyes
- Blending: CubicSmoothUnion for organic muscle flow
- Animation: Heavy breathing, tail sway

**Unit: Cryptid Hunter**
- Primitives: 28-32
- Asymmetric design (one large arm, one normal)
- Fur texture via material roughness variation
- Natural camouflage colors

**Building: Den**
- Base: Cave-like structure with organic shapes
- Primitives: 15-20
- Materials: Rock (rough 0.95), moss (rough 0.85)
- Use: SmoothUnion for natural rock formation
- Add: Glowing mushrooms, roots

### 3. ELVES (Graceful Magical Nature)

**Visual Identity**: Wood, leaves, silver, moonlight, elegant curves

**Hero: Elven Ranger**
- Base: Slender humanoid, graceful proportions
- Primitives: 38-42
- Special: Flowing cape, intricate bow, leaf motifs
- Materials:
  - Skin: Pale, subsurface 0.15
  - Armor: Polished wood (metallic 0.3, warm brown)
  - Silver trim: Metallic 0.95, cool tint
  - Magic runes: Subtle emissive [0.02, 0.05, 0.08]
- Blending: ExponentialSmoothUnion for flowing robes
- Animation: Graceful idle, bow string tension

**Unit: Elven Archer**
- Primitives: 25-28
- Lightweight armor, longbow
- Leaf-shaped pauldrons
- Green/brown natural palette

**Building: Ancient Tree**
- Base: Massive tree with platforms
- Primitives: 20-25
- Trunk: Multiple capsules with CubicSmoothUnion
- Branches: Procedural placement
- Materials: Bark texture (rough 0.9), glowing sap

### 4. FAIRIES (Ethereal Magical Tiny)

**Visual Identity**: Gossamer wings, sparkles, pastel colors, delicate

**Hero: Fairy Queen**
- Base: Tiny humanoid with large wings
- Primitives: 35-40 (detailed despite size)
- Special: Translucent wings, crown, wand
- Materials:
  - Skin: Very light, subsurface 0.2
  - Wings: Metallic 0.0, roughness 0.1, iridescence effect
  - Crown: Gold metallic 0.98, emissive sparkles
  - Magic trail: Particle emissive
- Blending: DistanceAwareSmoothUnion for delicate features
- Animation: Hovering, wing flutter

**Unit: Sprite Warrior**
- Primitives: 20-25
- Tiny sword/wand
- Flowing dress with motion
- Pastel palette (pinks, purples, blues)

**Building: Enchanted Mushroom**
- Base: Giant glowing mushroom
- Primitives: 12-15
- Cap: Smooth gradient emissive
- Stem: Organic capsule form
- Doors/windows in stem

### 5. HUMANS (Medieval Fantasy Warriors)

**Visual Identity**: Steel, leather, heraldry, practical designs

**Hero: Human Knight**
- Base: Armored warrior (similar to current hero)
- Primitives: 42-45
- Special: Full plate armor, great sword, shield
- Materials:
  - Plate: Metallic 0.9, anisotropy 0.35
  - Chainmail: Special material with normal map
  - Leather: Rough 0.85, brown
  - Heraldry: Colored enamel on shield
- Blending: PowerSmoothUnion for armor attachment
- Animation: Heroic stance, shield ready

**Unit: Human Soldier**
- Primitives: 28-32
- Mix of plate and chainmail
- Spear and round shield
- Practical design

**Building: Castle Tower**
- Base: Stone fortification
- Primitives: 18-22
- Materials: Stone (metallic 0.1, rough 0.95)
- Windows with glowing interiors
- Flags with cloth simulation

### 6. MECHS (Industrial Robots)

**Visual Identity**: Hard edges, panels, hydraulics, mechanical joints

**Hero: Mech Commander**
- Base: Large bipedal robot
- Primitives: 50-55 (lots of mechanical detail)
- Special: Exposed hydraulics, weapon systems, antenna
- Materials:
  - Hull: Metallic 0.85, dark grey
  - Joints: Metallic 0.7, anisotropy 0.5 (worn metal)
  - Lights: Emissive red/orange
  - Rust: Roughness 0.95 on damage areas
- Blending: PowerSmoothUnion for panel attachment
- Animation: Mechanical movement, servo sounds

**Unit: War Machine**
- Primitives: 35-40
- Tank-like lower body, gun turret
- Heavy armor plating
- Industrial color scheme

**Building: Factory**
- Base: Industrial structure with smokestacks
- Primitives: 25-30
- Rotating gears, conveyor belts
- Steam/smoke emitters
- Metallic construction

### 7. UNDEAD (Dark Necromantic)

**Visual Identity**: Bone, decay, purple/green magic, shadows

**Hero: Lich King**
- Base: Skeletal figure in robes
- Primitives: 38-42
- Special: Exposed skull, tattered robes, staff, crown
- Materials:
  - Bone: Metallic 0.0, rough 0.7, off-white
  - Robes: Rough 0.95, dark purple/black
  - Magic: Emissive sickly green [0.3, 0.8, 0.2]
  - Crown: Tarnished gold, metallic 0.6
- Blending: ExponentialSmoothUnion for ethereal robes
- Animation: Floating, robe billowing

**Unit: Skeleton Warrior**
- Primitives: 25-28
- Exposed bones, tattered armor
- Rusty weapons
- Green glowing eyes

**Building: Necropolis**
- Base: Dark tower with floating stones
- Primitives: 20-25
- Crumbling architecture
- Floating debris with magic
- Purple/green fog effects

---

## Implementation Workflow

### Phase 1: Template Creation (Week 1)
1. Create base templates for each race archetype
2. Establish material libraries per race
3. Set up lighting presets per race theme
4. Test one complete asset per race

### Phase 2: Parallel Production (Week 2-3)
Deploy 7 parallel agents (one per race):
- Each agent creates 1 hero, 2 units, 2 buildings
- Apply advanced SDF techniques from research
- Implement race-specific visual identity
- Add proper animations and materials

### Phase 3: Icon Generation (Week 4)
1. Run `capture_asset_icons.py` on completed assets
2. Verify all icons are transparent PNG 512x512
3. Organize in screenshots/asset_icons/ by race
4. Create icon atlas for UI integration

### Phase 4: Quality Review
1. Visual consistency check per race
2. Performance profiling (target: <100ms per asset render)
3. Animation smoothness verification
4. Icon clarity at 64x64 display size

---

## Asset Creation Guidelines

### Primitive Count Targets
- **Heroes**: 35-50 primitives (high detail, player focal point)
- **Units**: 25-35 primitives (medium detail, many on screen)
- **Buildings**: 15-30 primitives (static, background)
- **Decorations**: 5-15 primitives (environmental filler)

### Blending Mode Selection Chart

| Element Type | Recommended Blend Mode | Smoothness | Notes |
|--------------|------------------------|------------|-------|
| Body joints (organic) | CubicSmoothUnion | 0.09-0.15 | Shoulders, hips, knees |
| Torso/neck (organic) | ExponentialSmoothUnion | 0.12-0.18 | Flowing muscle |
| Equipment attachment | PowerSmoothUnion | power 4-8 | Helmets, weapons |
| Fine details | DistanceAwareSmoothUnion | threshold 0.05 | Fingers, tentacles |
| Building corners | CubicSmoothUnion | 0.10-0.20 | Organic buildings |
| Tech panels | PowerSmoothUnion | power 6-10 | Sharp control |
| Natural formations | ExponentialSmoothUnion | 0.15-0.25 | Rocks, trees |

### Material Templates

**Metal Armor**:
```json
{
  "albedo": [0.4, 0.4, 0.42],
  "metallic": 0.9,
  "roughness": 0.25,
  "anisotropy": 0.35
}
```

**Organic Skin**:
```json
{
  "albedo": [0.9, 0.75, 0.65],
  "metallic": 0.0,
  "roughness": 0.75,
  "subsurfaceScattering": 0.15
}
```

**Glowing Magic**:
```json
{
  "albedo": [0.3, 0.7, 1.0],
  "metallic": 0.0,
  "roughness": 0.1,
  "emissive": [0.3, 0.7, 1.0],
  "emissiveStrength": 1.0
}
```

**Rough Stone**:
```json
{
  "albedo": [0.45, 0.42, 0.38],
  "metallic": 0.05,
  "roughness": 0.95
}
```

---

## Success Criteria

✅ **Visual Quality**
- Cinematic presentation (matches warrior_hero_v3 quality)
- Clear race identity visible at icon size
- Proper use of advanced SDF blending
- Physically-based materials

✅ **Technical Quality**
- All assets have transparent background icons
- Performance within budget (<100ms render)
- Animation smooth (if applicable)
- No visual artifacts (unwanted blending, clipping)

✅ **Completeness**
- At minimum: 1 hero + 2 units + 2 buildings per race
- All 7 races represented
- Icons generated for all assets
- Organized in asset_icons/ folder

---

## Next Steps

1. **Review this plan** - Confirm approach and scope
2. **Launch parallel agents** - One per race for asset creation
3. **Monitor progress** - Track completion per race
4. **Generate icons** - Run automated screenshot capture
5. **Integration** - Import into game UI system

**Estimated Total Time**: 3-4 weeks for full production pipeline
**Quick Prototype**: 1 week for 1 asset per race (7 total)

---

## Appendix: Tool Chain

1. **scan_all_race_assets.py** - Inventory existing assets
2. **Advanced SDF blending** - Engine features (implemented)
3. **Asset creation agents** - Parallel production
4. **capture_asset_icons.py** - Icon generation
5. **Asset browser** - UI integration

**Documentation**:
- ASSET_ICON_SYSTEM.md - Icon capture details
- This document - Production plan
- warrior_hero_v3 polish report - Reference quality
