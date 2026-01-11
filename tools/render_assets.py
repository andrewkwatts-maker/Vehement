#!/usr/bin/env python3
"""
Asset Icon Rendering Script

Uses the asset_icon_renderer C++ tool to render all game assets to PNG icons.
"""

import os
import json
import subprocess
import sys
from pathlib import Path
from datetime import datetime

# Paths
GAME_ASSETS_DIR = "H:/Github/Old3DEngine/game/assets"
SCREENSHOTS_DIR = "H:/Github/Old3DEngine/screenshots/asset_icons"

# Asset categories
ASSET_CATEGORIES = [
    "heroes",
    "units",
    "buildings",
    "decorations",
    "vehicles",
    "props"
]

# Renderer executable paths to try
RENDERER_PATHS = [
    "H:/Github/Old3DEngine/build/bin/Debug/asset_icon_renderer.exe",
    "H:/Github/Old3DEngine/build/bin/Release/asset_icon_renderer.exe",
    "H:/Github/Old3DEngine/build/Debug/bin/asset_icon_renderer.exe",
    "H:/Github/Old3DEngine/build/Release/bin/asset_icon_renderer.exe",
    "../build/bin/Debug/asset_icon_renderer.exe",
    "../build/bin/Release/asset_icon_renderer.exe",
]


def find_renderer():
    """Find the asset_icon_renderer executable"""
    for path in RENDERER_PATHS:
        if os.path.exists(path):
            return os.path.abspath(path)
    return None


def find_assets():
    """Find all JSON assets with SDF models"""
    assets = []

    for category in ASSET_CATEGORIES:
        category_path = os.path.join(GAME_ASSETS_DIR, category)

        if not os.path.exists(category_path):
            continue

        # Find JSON files
        for root, dirs, files in os.walk(category_path):
            for file in files:
                if file.endswith('.json'):
                    asset_path = os.path.join(root, file)

                    # Check if it has an SDF model
                    try:
                        with open(asset_path, 'r', encoding='utf-8') as f:
                            data = json.load(f)

                        if 'sdfModel' in data and 'type' in data:
                            assets.append({
                                'path': asset_path,
                                'category': category,
                                'name': data.get('name', file.replace('.json', '')),
                                'id': data.get('id', file.replace('.json', ''))
                            })
                    except:
                        pass

    return assets


def get_output_path(asset_path):
    """Get output PNG path for asset"""
    rel_path = os.path.relpath(asset_path, GAME_ASSETS_DIR)
    png_path = rel_path.replace('.json', '.png')
    return os.path.join(SCREENSHOTS_DIR, png_path)


def render_asset(renderer_exe, asset, output_path, width=512, height=512):
    """Render a single asset to PNG"""
    try:
        result = subprocess.run(
            [renderer_exe, asset['path'], output_path, str(width), str(height)],
            capture_output=True,
            text=True,
            timeout=30
        )

        return result.returncode == 0, result.stderr if result.returncode != 0 else None

    except subprocess.TimeoutExpired:
        return False, "Rendering timed out"
    except Exception as e:
        return False, str(e)


def main():
    print("=" * 70)
    print("ASSET ICON RENDERING SYSTEM")
    print("=" * 70)
    print()

    # Find renderer
    print("Looking for asset_icon_renderer executable...")
    renderer_exe = find_renderer()

    if not renderer_exe:
        print("ERROR: asset_icon_renderer.exe not found!")
        print()
        print("Please build the project first:")
        print("  cd build")
        print("  cmake --build . --config Debug")
        print()
        print("Searched in:")
        for path in RENDERER_PATHS:
            print(f"  - {path}")
        return 1

    print(f"Found: {renderer_exe}")
    print()

    # Find assets
    print("Scanning for assets...")
    assets = find_assets()

    if not assets:
        print("No assets found!")
        return 1

    print(f"Found {len(assets)} assets")
    print()

    # Group by category
    by_category = {}
    for asset in assets:
        cat = asset['category']
        if cat not in by_category:
            by_category[cat] = []
        by_category[cat].append(asset)

    # Render assets
    print("=" * 70)
    print("RENDERING ASSETS")
    print("=" * 70)
    print()

    success_count = 0
    error_count = 0
    total_count = 0

    for category, cat_assets in by_category.items():
        print(f"\n{category.upper()} ({len(cat_assets)} assets):")

        for asset in cat_assets:
            total_count += 1
            output_path = get_output_path(asset['path'])

            print(f"  [{total_count}/{len(assets)}] {asset['name']}...", end=' ', flush=True)

            # Create output directory
            os.makedirs(os.path.dirname(output_path), exist_ok=True)

            # Render
            success, error = render_asset(renderer_exe, asset, output_path)

            if success:
                print("OK")
                success_count += 1
            else:
                print(f"FAILED")
                if error:
                    print(f"      Error: {error[:100]}")
                error_count += 1

    # Summary
    print()
    print("=" * 70)
    print("RENDERING COMPLETE")
    print("=" * 70)
    print(f"Total assets:  {len(assets)}")
    print(f"Rendered:      {success_count}")
    print(f"Failed:        {error_count}")
    print()
    print(f"Output directory: {SCREENSHOTS_DIR}")
    print("=" * 70)

    return 0 if error_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
