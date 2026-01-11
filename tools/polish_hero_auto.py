#!/usr/bin/env python3
"""
Automated Hero Asset Polish - Single iteration
"""

import subprocess
import time
import os
import json
from pathlib import Path

PROJECT_ROOT = Path("H:/Github/Old3DEngine")
HERO_JSON = PROJECT_ROOT / "game/assets/heroes/warrior_hero.json"
DEMO_EXE = PROJECT_ROOT / "build/bin/Debug/nova_demo.exe"
SCREENSHOTS_DIR = PROJECT_ROOT / "screenshots/hero_polish"

def main():
    print("=" * 60)
    print(" Hero Asset Polish - Warrior Hero")
    print("=" * 60)

    # Create screenshots directory
    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)

    # Check demo exists
    if not DEMO_EXE.exists():
        print(f"ERROR: {DEMO_EXE} not found")
        return

    # Load hero JSON
    print(f"\nLoading hero asset: {HERO_JSON}")
    with open(HERO_JSON, 'r') as f:
        hero_data = json.load(f)

    print(f"Hero: {hero_data['name']}")
    print(f"  - {len(hero_data['sdfModel']['primitives'])} primitives")
    print(f"  - Idle animation: {hero_data['animationSet']['idle']['duration']}s")

    # Analysis
    print("\n=== Initial Analysis ===")

    # Check materials
    print("\nMaterial Analysis:")
    primitives = hero_data['sdfModel']['primitives']
    for prim in primitives:
        prim_id = prim.get('id', 'unknown')
        metallic = prim.get('metallic', 0)
        roughness = prim.get('roughness', 0.5)
        albedo = prim.get('albedo', [0.5, 0.5, 0.5])

        if prim_id in ['sword', 'helmet', 'shield_boss']:
            if metallic < 0.8:
                print(f"  WARNING: {prim_id}: metallic={metallic} (should be 0.8-0.95 for metal)")

        if prim_id == 'cape':
            if metallic > 0.1:
                print(f"  WARNING: {prim_id}: metallic={metallic} (should be ~0.0 for fabric)")

    # Check animation
    print("\nAnimation Analysis:")
    idle_anim = hero_data['animationSet']['idle']
    keyframe_count = len(idle_anim['keyframes'])
    duration = idle_anim['duration']

    if keyframe_count < 5:
        print(f"  WARNING: Only {keyframe_count} keyframes (consider adding more for smoother animation)")

    if duration < 2.0:
        print(f"  WARNING: Short duration ({duration}s) - consider 3-5s for natural breathing")

    # Check lighting
    print("\nLighting Analysis:")
    lighting = hero_data.get('lighting', {})

    if 'directional' in lighting:
        intensity = lighting['directional'].get('intensity', 1.0)
        if intensity < 1.0:
            print(f"  WARNING: Directional light intensity={intensity} (consider 1.2-1.5 for hero)")

    if 'rim' in lighting:
        rim_intensity = lighting['rim'].get('intensity', 0.5)
        if rim_intensity < 0.6:
            print(f"  WARNING: Rim light intensity={rim_intensity} (consider 0.6-0.8 for heroic silhouette)")
    else:
        print(f"  WARNING: Missing rim light (important for heroic silhouette)")

    # Improvement suggestions
    print("\n=== Improvement Suggestions ===")

    suggestions = []

    # Sword should be more metallic
    for prim in primitives:
        if prim['id'] == 'sword' and prim.get('metallic', 0) < 0.9:
            suggestions.append({
                'category': 'material',
                'item': 'sword',
                'change': 'metallic',
                'from': prim.get('metallic', 0),
                'to': 0.95
            })

        if prim['id'] == 'sword' and prim.get('roughness', 0.5) > 0.25:
            suggestions.append({
                'category': 'material',
                'item': 'sword',
                'change': 'roughness',
                'from': prim.get('roughness', 0.5),
                'to': 0.15
            })

    # Enhance rim light
    if 'rim' in lighting and lighting['rim'].get('intensity', 0) < 0.7:
        suggestions.append({
            'category': 'lighting',
            'item': 'rim',
            'change': 'intensity',
            'from': lighting['rim'].get('intensity', 0.5),
            'to': 0.7
        })

    # Increase directional light
    if 'directional' in lighting and lighting['directional'].get('intensity', 0) < 1.3:
        suggestions.append({
            'category': 'lighting',
            'item': 'directional',
            'change': 'intensity',
            'from': lighting['directional'].get('intensity', 1.0),
            'to': 1.3
        })

    # Add more animation keyframes
    if keyframe_count < 5:
        suggestions.append({
            'category': 'animation',
            'item': 'idle',
            'change': 'keyframes',
            'from': keyframe_count,
            'to': 7
        })

    for i, sug in enumerate(suggestions, 1):
        print(f"\n{i}. {sug['category'].upper()}: {sug['item']}")
        print(f"   Change {sug['change']}: {sug['from']} -> {sug['to']}")

    # Apply automated improvements
    print("\n=== Applying Automated Improvements ===")

    modified = False

    for sug in suggestions:
        if sug['category'] == 'material':
            for prim in primitives:
                if prim['id'] == sug['item']:
                    prim[sug['change']] = sug['to']
                    print(f"OK: Set {sug['item']}.{sug['change']} = {sug['to']}")
                    modified = True

        elif sug['category'] == 'lighting':
            if sug['item'] in lighting:
                lighting[sug['item']][sug['change']] = sug['to']
                print(f"OK: Set lighting.{sug['item']}.{sug['change']} = {sug['to']}")
                modified = True

    # Save if modified
    if modified:
        print(f"\nSaving improved hero to: {HERO_JSON}")
        with open(HERO_JSON, 'w') as f:
            json.dump(hero_data, f, indent=2)
        print("OK: Saved")
    else:
        print("\nNo automated changes made")

    print("\n" + "=" * 60)
    print("Next steps:")
    print("1. Launch nova_demo.exe to view the hero")
    print("2. Take screenshots")
    print("3. Manually adjust pose/animation if needed")
    print("4. Re-run this script to analyze again")
    print("=" * 60)

if __name__ == "__main__":
    main()
