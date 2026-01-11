#!/usr/bin/env python3
"""
Asset Screenshot Capture System

Automatically generates icon screenshots for all game assets:
- Heroes, units, buildings, decorations, etc.
- Transparent background (no terrain/sky)
- Automatic updates when assets change
- Organized output structure matching asset folders
"""

import os
import json
import hashlib
import subprocess
import time
from pathlib import Path
from datetime import datetime

# Paths
GAME_ASSETS_DIR = "H:/Github/Old3DEngine/game/assets"
SCREENSHOTS_DIR = "H:/Github/Old3DEngine/screenshots/asset_icons"
DEMO_EXECUTABLE = "H:/Github/Old3DEngine/build/bin/Debug/nova_rts_demo.exe"
ASSET_CACHE_FILE = "H:/Github/Old3DEngine/screenshots/.asset_cache.json"

# Asset categories to process
ASSET_CATEGORIES = [
    "heroes",
    "units",
    "buildings",
    "decorations",
    "vehicles",
    "props"
]

# Screenshot settings
SCREENSHOT_WIDTH = 512
SCREENSHOT_HEIGHT = 512
CAMERA_DISTANCE = 3.5  # Units from asset
LIGHTING_PRESET = "studio"  # Clean, even lighting for icons


class AssetScreenshotCapture:
    def __init__(self):
        self.assets_processed = 0
        self.assets_updated = 0
        self.assets_skipped = 0
        self.cache = self.load_cache()

    def load_cache(self):
        """Load asset modification cache"""
        if os.path.exists(ASSET_CACHE_FILE):
            with open(ASSET_CACHE_FILE, 'r') as f:
                return json.load(f)
        return {}

    def save_cache(self):
        """Save asset modification cache"""
        os.makedirs(os.path.dirname(ASSET_CACHE_FILE), exist_ok=True)
        with open(ASSET_CACHE_FILE, 'w') as f:
            json.dump(self.cache, f, indent=2)

    def get_file_hash(self, filepath):
        """Calculate MD5 hash of file for change detection"""
        hasher = hashlib.md5()
        with open(filepath, 'rb') as f:
            hasher.update(f.read())
        return hasher.hexdigest()

    def needs_update(self, asset_path, screenshot_path):
        """Check if screenshot needs regeneration"""
        # Screenshot doesn't exist
        if not os.path.exists(screenshot_path):
            return True

        # Asset file has been modified
        current_hash = self.get_file_hash(asset_path)
        cached_hash = self.cache.get(asset_path, {}).get('hash')

        if current_hash != cached_hash:
            return True

        # Screenshot is older than asset
        asset_mtime = os.path.getmtime(asset_path)
        screenshot_mtime = os.path.getmtime(screenshot_path)

        if asset_mtime > screenshot_mtime:
            return True

        return False

    def find_all_assets(self):
        """Scan asset directories for all JSON assets"""
        assets = []

        for category in ASSET_CATEGORIES:
            category_path = os.path.join(GAME_ASSETS_DIR, category)

            if not os.path.exists(category_path):
                print(f"Warning: Category path not found: {category_path}")
                continue

            # Find all JSON files in this category
            for root, dirs, files in os.walk(category_path):
                for file in files:
                    if file.endswith('.json'):
                        asset_path = os.path.join(root, file)

                        # Read JSON to check if it's a valid asset
                        try:
                            with open(asset_path, 'r') as f:
                                data = json.load(f)

                            # Must have type and sdfModel fields
                            if 'type' in data and 'sdfModel' in data:
                                assets.append({
                                    'path': asset_path,
                                    'category': category,
                                    'name': data.get('name', file.replace('.json', '')),
                                    'id': data.get('id', file.replace('.json', ''))
                                })
                        except (json.JSONDecodeError, KeyError) as e:
                            print(f"Skipping invalid asset: {asset_path} - {e}")

        return assets

    def get_screenshot_path(self, asset):
        """Generate screenshot output path matching asset structure"""
        # Get relative path from assets dir
        rel_path = os.path.relpath(asset['path'], GAME_ASSETS_DIR)

        # Replace .json with .png
        screenshot_rel = rel_path.replace('.json', '.png')

        # Create full output path
        screenshot_path = os.path.join(SCREENSHOTS_DIR, screenshot_rel)

        return screenshot_path

    def create_render_config(self, asset, output_path):
        """Create temporary render configuration for asset screenshot"""
        config = {
            "render_mode": "asset_icon",
            "asset_path": asset['path'],
            "output_path": output_path,
            "width": SCREENSHOT_WIDTH,
            "height": SCREENSHOT_HEIGHT,
            "transparent_background": True,
            "camera": {
                "type": "auto_frame",  # Automatically frame asset
                "distance": CAMERA_DISTANCE,
                "angle": [15, 45, 0],  # Slight top-down, 45° rotation
                "fov": 35  # Tighter FOV for icons
            },
            "lighting": {
                "preset": LIGHTING_PRESET,
                "setup": "three_point",
                "key_intensity": 1.5,
                "fill_intensity": 0.6,
                "rim_intensity": 0.8,
                "ambient": 0.4
            },
            "rendering": {
                "samples": 64,  # Good quality but fast
                "max_bounces": 4,
                "background_alpha": 0.0,
                "anti_aliasing": True
            },
            "post_processing": {
                "enabled": False  # Clean output for icons
            }
        }

        # Save temporary config
        temp_config_path = "H:/Github/Old3DEngine/temp_render_config.json"
        with open(temp_config_path, 'w') as f:
            json.dump(config, f, indent=2)

        return temp_config_path

    def capture_screenshot(self, asset, screenshot_path):
        """Render asset screenshot using demo executable"""
        print(f"  Capturing: {asset['name']} -> {os.path.basename(screenshot_path)}")

        # Create output directory
        os.makedirs(os.path.dirname(screenshot_path), exist_ok=True)

        # Create render config
        config_path = self.create_render_config(asset, screenshot_path)

        try:
            # Launch demo with headless render mode
            cmd = [
                DEMO_EXECUTABLE,
                "--headless",
                "--render-config", config_path,
                "--exit-after-render"
            ]

            # Run with timeout
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
                cwd=os.path.dirname(DEMO_EXECUTABLE)
            )

            # Check if screenshot was created
            if os.path.exists(screenshot_path):
                print(f"    SUCCESS: Screenshot saved")

                # Update cache
                self.cache[asset['path']] = {
                    'hash': self.get_file_hash(asset['path']),
                    'screenshot': screenshot_path,
                    'timestamp': datetime.now().isoformat(),
                    'size': os.path.getsize(screenshot_path)
                }

                return True
            else:
                print(f"    ERROR: Screenshot file not created")
                if result.stderr:
                    print(f"    Error output: {result.stderr[:200]}")
                return False

        except subprocess.TimeoutExpired:
            print(f"    ERROR: Render timeout (30s)")
            return False
        except Exception as e:
            print(f"    ERROR: {str(e)}")
            return False
        finally:
            # Cleanup temp config
            if os.path.exists(config_path):
                os.remove(config_path)

    def process_all_assets(self):
        """Main processing loop"""
        print("=" * 70)
        print("ASSET SCREENSHOT CAPTURE SYSTEM")
        print("=" * 70)
        print()

        # Find all assets
        print("Scanning for assets...")
        assets = self.find_all_assets()
        print(f"Found {len(assets)} assets across {len(ASSET_CATEGORIES)} categories")
        print()

        if not assets:
            print("No assets found to process!")
            return

        # Group by category for organized output
        by_category = {}
        for asset in assets:
            category = asset['category']
            if category not in by_category:
                by_category[category] = []
            by_category[category].append(asset)

        # Process each category
        for category, category_assets in by_category.items():
            print(f"\n{'='*70}")
            print(f"CATEGORY: {category.upper()} ({len(category_assets)} assets)")
            print(f"{'='*70}\n")

            for asset in category_assets:
                self.assets_processed += 1
                screenshot_path = self.get_screenshot_path(asset)

                # Check if update needed
                if not self.needs_update(asset['path'], screenshot_path):
                    print(f"  SKIP: {asset['name']} (up to date)")
                    self.assets_skipped += 1
                    continue

                # Capture screenshot
                if self.capture_screenshot(asset, screenshot_path):
                    self.assets_updated += 1
                else:
                    print(f"  FAILED: {asset['name']}")

        # Save cache
        self.save_cache()

        # Print summary
        print()
        print("=" * 70)
        print("CAPTURE SUMMARY")
        print("=" * 70)
        print(f"Total assets processed: {self.assets_processed}")
        print(f"Screenshots updated:    {self.assets_updated}")
        print(f"Assets skipped:         {self.assets_skipped}")
        print(f"Screenshots directory:  {SCREENSHOTS_DIR}")
        print()
        print("Asset icons are ready for use in the game UI!")
        print("=" * 70)


def main():
    """Entry point"""
    # Check if demo executable exists
    if not os.path.exists(DEMO_EXECUTABLE):
        print(f"ERROR: Demo executable not found at: {DEMO_EXECUTABLE}")
        print("Please build the project first.")
        return 1

    # Check if assets directory exists
    if not os.path.exists(GAME_ASSETS_DIR):
        print(f"ERROR: Assets directory not found at: {GAME_ASSETS_DIR}")
        return 1

    # Run capture
    capturer = AssetScreenshotCapture()
    capturer.process_all_assets()

    return 0


if __name__ == "__main__":
    exit(main())
