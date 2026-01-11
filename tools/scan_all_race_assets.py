#!/usr/bin/env python3
"""
Comprehensive Race Asset Scanner

Scans all race assets (units, buildings, heroes) and creates an inventory
with statistics about what needs polishing.
"""

import os
import json
from pathlib import Path
from collections import defaultdict

GAME_DIR = "H:/Github/Old3DEngine/game"

# Race names from the RTS game
RACES = [
    "aliens",
    "cryptids",
    "elves",
    "fairies",
    "humans",
    "mechs",
    "undead"
]

def analyze_sdf_model(sdf_model):
    """Analyze an SDF model and return statistics"""
    stats = {
        'primitive_count': 0,
        'has_blending': False,
        'blending_modes': set(),
        'primitive_types': set(),
        'has_animation': False,
        'has_materials': False,
        'has_lighting': False,
        'needs_polish': []
    }

    if not sdf_model or not isinstance(sdf_model, dict):
        return stats

    # Check primitives
    primitives = sdf_model.get('primitives', [])
    stats['primitive_count'] = len(primitives)

    for prim in primitives:
        # Count primitive types
        prim_type = prim.get('type', 'unknown')
        stats['primitive_types'].add(prim_type)

        # Check for blending
        csg_op = prim.get('csgOperation', 'union')
        if 'smooth' in csg_op.lower():
            stats['has_blending'] = True
            stats['blending_modes'].add(csg_op)

        # Check materials
        if any(key in prim for key in ['albedo', 'metallic', 'roughness']):
            stats['has_materials'] = True

    # Check for blending flag
    if sdf_model.get('blendingEnabled'):
        stats['has_blending'] = True

    # Needs polish checks
    if stats['primitive_count'] < 5:
        stats['needs_polish'].append('LOW_DETAIL')

    if not stats['has_blending']:
        stats['needs_polish'].append('NO_SMOOTH_BLENDING')

    if 'capsule' not in stats['primitive_types']:
        stats['needs_polish'].append('NO_CAPSULES')

    if not stats['has_materials']:
        stats['needs_polish'].append('NO_MATERIALS')

    return stats

def scan_race_assets():
    """Scan all race assets and categorize them"""

    results = {
        'races': {},
        'summary': {
            'total_assets': 0,
            'by_category': defaultdict(int),
            'by_race': defaultdict(int),
            'needs_polish': 0,
            'ready_for_icons': 0
        }
    }

    for race in RACES:
        race_data = {
            'units': [],
            'buildings': [],
            'heroes': [],
            'components': []
        }

        # Search for race assets
        race_paths = [
            os.path.join(GAME_DIR, "assets", "configs", "units", race),
            os.path.join(GAME_DIR, "assets", "configs", "buildings", race),
            os.path.join(GAME_DIR, "assets", "configs", "heroes", race),
            os.path.join(GAME_DIR, "assets", "heroes", race),
            os.path.join(GAME_DIR, "assets", "units", race),
            os.path.join(GAME_DIR, "assets", "buildings", race),
        ]

        for base_path in race_paths:
            if not os.path.exists(base_path):
                continue

            # Walk directory tree
            for root, dirs, files in os.walk(base_path):
                for file in files:
                    if not file.endswith('.json'):
                        continue

                    filepath = os.path.join(root, file)

                    try:
                        with open(filepath, 'r', encoding='utf-8') as f:
                            data = json.load(f)

                        # Check if it has an SDF model
                        if 'sdfModel' not in data:
                            continue

                        # Analyze the asset
                        stats = analyze_sdf_model(data.get('sdfModel'))

                        asset_info = {
                            'name': data.get('name', file.replace('.json', '')),
                            'id': data.get('id', file.replace('.json', '')),
                            'type': data.get('type', 'unknown'),
                            'path': filepath,
                            'rel_path': os.path.relpath(filepath, GAME_DIR),
                            'stats': stats
                        }

                        # Categorize
                        if 'component' in file.lower() or 'component' in filepath:
                            category = 'components'
                        elif 'unit' in data.get('type', '') or 'units' in filepath:
                            category = 'units'
                        elif 'building' in data.get('type', '') or 'buildings' in filepath:
                            category = 'buildings'
                        elif 'hero' in data.get('type', '') or 'heroes' in filepath:
                            category = 'heroes'
                        else:
                            category = 'components'

                        race_data[category].append(asset_info)

                        # Update summary
                        results['summary']['total_assets'] += 1
                        results['summary']['by_category'][category] += 1
                        results['summary']['by_race'][race] += 1

                        if stats['needs_polish']:
                            results['summary']['needs_polish'] += 1
                        else:
                            results['summary']['ready_for_icons'] += 1

                    except (json.JSONDecodeError, UnicodeDecodeError, Exception) as e:
                        print(f"Error reading {filepath}: {e}")

        results['races'][race] = race_data

    return results

def print_report(results):
    """Print comprehensive asset report"""

    print("=" * 80)
    print(" " * 20 + "RACE ASSET INVENTORY REPORT")
    print("=" * 80)
    print()

    summary = results['summary']

    print("OVERALL SUMMARY")
    print("-" * 80)
    print(f"Total SDF Assets Found:     {summary['total_assets']}")
    print(f"Need Polish/Updates:        {summary['needs_polish']}")
    print(f"Ready for Icon Capture:     {summary['ready_for_icons']}")
    print()

    print("BY CATEGORY")
    print("-" * 80)
    for category, count in sorted(summary['by_category'].items()):
        print(f"  {category.capitalize():<20} {count:>4} assets")
    print()

    print("BY RACE")
    print("-" * 80)
    for race, count in sorted(summary['by_race'].items()):
        print(f"  {race.capitalize():<20} {count:>4} assets")
    print()

    # Detailed race breakdown
    for race, race_data in sorted(results['races'].items()):
        total_race = sum(len(race_data[cat]) for cat in race_data)

        if total_race == 0:
            continue

        print("=" * 80)
        print(f"RACE: {race.upper()} ({total_race} total assets)")
        print("=" * 80)

        for category in ['heroes', 'units', 'buildings', 'components']:
            assets = race_data[category]

            if not assets:
                continue

            print(f"\n{category.upper()} ({len(assets)} assets):")
            print("-" * 80)

            for asset in sorted(assets, key=lambda x: x['name']):
                stats = asset['stats']
                status = "✓ READY" if not stats['needs_polish'] else "⚠ NEEDS POLISH"

                polish_notes = ", ".join(stats['needs_polish']) if stats['needs_polish'] else ""

                print(f"  [{status}] {asset['name']:<40}")
                print(f"           Primitives: {stats['primitive_count']}, " +
                      f"Types: {', '.join(sorted(stats['primitive_types']))}")

                if stats['needs_polish']:
                    print(f"           Issues: {polish_notes}")

                if stats['has_blending']:
                    modes = ', '.join(sorted(stats['blending_modes']))
                    print(f"           Blending: {modes}")

                print(f"           Path: {asset['rel_path']}")
                print()

    print()
    print("=" * 80)
    print("POLISH RECOMMENDATIONS")
    print("=" * 80)
    print()

    needs_attention = []
    for race, race_data in results['races'].items():
        for category in race_data:
            for asset in race_data[category]:
                if asset['stats']['needs_polish']:
                    needs_attention.append({
                        'race': race,
                        'category': category,
                        'asset': asset
                    })

    # Group by issue type
    by_issue = defaultdict(list)
    for item in needs_attention:
        for issue in item['asset']['stats']['needs_polish']:
            by_issue[issue].append(item)

    for issue, items in sorted(by_issue.items()):
        print(f"{issue}: {len(items)} assets")
        print("-" * 80)
        for item in items[:5]:  # Show first 5
            print(f"  - {item['race']}/{item['category']}/{item['asset']['name']}")
        if len(items) > 5:
            print(f"  ... and {len(items) - 5} more")
        print()

def save_inventory(results):
    """Save inventory to JSON file"""
    output_path = "H:/Github/Old3DEngine/game/assets/ASSET_INVENTORY.json"

    # Convert sets to lists for JSON serialization
    clean_results = json.loads(
        json.dumps(results, default=lambda x: list(x) if isinstance(x, set) else x)
    )

    with open(output_path, 'w') as f:
        json.dump(clean_results, f, indent=2)

    print(f"Inventory saved to: {output_path}")

def main():
    print("Scanning race assets...")
    print()

    results = scan_race_assets()
    print_report(results)
    save_inventory(results)

    print()
    print("=" * 80)
    print("Next Steps:")
    print("  1. Review ASSET_INVENTORY.json for complete details")
    print("  2. Use findings to create polish plan")
    print("  3. Apply advanced SDF techniques from hero research")
    print("  4. Run capture_asset_icons.py to generate all icons")
    print("=" * 80)

if __name__ == "__main__":
    main()
