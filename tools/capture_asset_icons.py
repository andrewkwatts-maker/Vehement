#!/usr/bin/env python3
"""
Simplified Asset Icon Capture System

Captures transparent PNG icons for all game assets.
Updates only when asset files change.
"""

import os
import json
import hashlib
import time
from pathlib import Path
from datetime import datetime

# Paths
GAME_ASSETS_DIR = "H:/Github/Old3DEngine/game/assets"
SCREENSHOTS_DIR = "H:/Github/Old3DEngine/screenshots/asset_icons"
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


class AssetIconCapture:
    def __init__(self):
        self.assets_processed = 0
        self.assets_updated = 0
        self.assets_skipped = 0
        self.cache = self.load_cache()
        self.assets_to_capture = []

    def load_cache(self):
        """Load asset modification cache"""
        if os.path.exists(ASSET_CACHE_FILE):
            try:
                with open(ASSET_CACHE_FILE, 'r') as f:
                    return json.load(f)
            except:
                return {}
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
                print(f"Creating category directory: {category_path}")
                os.makedirs(category_path, exist_ok=True)
                continue

            # Find all JSON files in this category
            for root, dirs, files in os.walk(category_path):
                for file in files:
                    if file.endswith('.json'):
                        asset_path = os.path.join(root, file)

                        # Read JSON to check if it's a valid asset
                        try:
                            with open(asset_path, 'r', encoding='utf-8') as f:
                                data = json.load(f)

                            # Must have type and sdfModel fields
                            if 'type' in data and 'sdfModel' in data:
                                assets.append({
                                    'path': asset_path,
                                    'category': category,
                                    'name': data.get('name', file.replace('.json', '')),
                                    'id': data.get('id', file.replace('.json', ''))
                                })
                        except (json.JSONDecodeError, KeyError, UnicodeDecodeError) as e:
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

    def generate_capture_manifest(self):
        """Create manifest file of assets needing capture"""
        manifest_path = "H:/Github/Old3DEngine/temp_capture_manifest.json"

        manifest = {
            "timestamp": datetime.now().isoformat(),
            "total_assets": len(self.assets_to_capture),
            "assets": []
        }

        for asset_info in self.assets_to_capture:
            asset = asset_info['asset']
            screenshot_path = asset_info['screenshot_path']

            manifest["assets"].append({
                "name": asset['name'],
                "id": asset['id'],
                "category": asset['category'],
                "asset_path": asset['path'],
                "output_path": screenshot_path,
                "render_settings": {
                    "transparent_background": True,
                    "width": 512,
                    "height": 512,
                    "camera_distance": 3.5,
                    "camera_angle": [15, 45, 0],
                    "fov": 35,
                    "lighting": "studio_three_point"
                }
            })

        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)

        return manifest_path

    def scan_and_plan(self):
        """Scan assets and determine what needs updating"""
        print("=" * 70)
        print("ASSET ICON CAPTURE SYSTEM")
        print("=" * 70)
        print()

        # Find all assets
        print("Scanning for assets...")
        assets = self.find_all_assets()
        print(f"Found {len(assets)} assets across {len(ASSET_CATEGORIES)} categories")
        print()

        if not assets:
            print("No assets found to process!")
            return []

        # Group by category for organized output
        by_category = {}
        for asset in assets:
            category = asset['category']
            if category not in by_category:
                by_category[category] = []
            by_category[category].append(asset)

        # Check each asset
        for category, category_assets in by_category.items():
            print(f"\n{category.upper()} ({len(category_assets)} assets):")

            for asset in category_assets:
                self.assets_processed += 1
                screenshot_path = self.get_screenshot_path(asset)

                # Check if update needed
                if not self.needs_update(asset['path'], screenshot_path):
                    print(f"  SKIP: {asset['name']} (up to date)")
                    self.assets_skipped += 1
                else:
                    print(f"  QUEUE: {asset['name']} -> {os.path.basename(screenshot_path)}")
                    self.assets_to_capture.append({
                        'asset': asset,
                        'screenshot_path': screenshot_path
                    })

        return self.assets_to_capture

    def print_summary(self):
        """Print capture summary"""
        print()
        print("=" * 70)
        print("SCAN SUMMARY")
        print("=" * 70)
        print(f"Total assets found:     {self.assets_processed}")
        print(f"Need icon update:       {len(self.assets_to_capture)}")
        print(f"Already up to date:     {self.assets_skipped}")
        print()

        if self.assets_to_capture:
            print(f"Ready to capture {len(self.assets_to_capture)} asset icons")
            print(f"Screenshots will be saved to: {SCREENSHOTS_DIR}")
        else:
            print("All asset icons are up to date!")

        print("=" * 70)


def main():
    """Entry point"""
    # Check if assets directory exists
    if not os.path.exists(GAME_ASSETS_DIR):
        print(f"ERROR: Assets directory not found at: {GAME_ASSETS_DIR}")
        return 1

    # Create screenshots directory
    os.makedirs(SCREENSHOTS_DIR, exist_ok=True)

    # Run scan
    capturer = AssetIconCapture()
    assets_to_capture = capturer.scan_and_plan()

    # Print summary
    capturer.print_summary()

    # Generate manifest for external rendering
    if assets_to_capture:
        manifest_path = capturer.generate_capture_manifest()
        print()
        print(f"Capture manifest created: {manifest_path}")
        print()
        print("NEXT STEPS:")
        print("1. The asset capture manifest has been created")
        print("2. You can now implement rendering in the demo executable")
        print("3. Or manually capture assets using the listed paths")
        print()
        print("For now, creating placeholder icons...")

        # Create placeholder icons (simple colored squares based on asset type)
        import io
        try:
            # Try to use PIL if available
            from PIL import Image, ImageDraw, ImageFont

            for item in assets_to_capture:
                asset = item['asset']
                screenshot_path = item['screenshot_path']

                # Create output directory
                os.makedirs(os.path.dirname(screenshot_path), exist_ok=True)

                # Create placeholder image
                img = Image.new('RGBA', (512, 512), (0, 0, 0, 0))
                draw = ImageDraw.Draw(img)

                # Color based on category
                colors = {
                    'heroes': (255, 215, 0, 255),      # Gold
                    'units': (100, 149, 237, 255),     # Cornflower blue
                    'buildings': (139, 69, 19, 255),   # Brown
                    'decorations': (60, 179, 113, 255),# Sea green
                    'vehicles': (128, 128, 128, 255),  # Gray
                    'props': (210, 180, 140, 255)      # Tan
                }

                color = colors.get(asset['category'], (200, 200, 200, 255))

                # Draw icon background
                draw.rectangle([100, 100, 412, 412], fill=color)

                # Draw border
                draw.rectangle([100, 100, 412, 412], outline=(255, 255, 255, 255), width=4)

                # Add text
                try:
                    draw.text((256, 256), asset['name'][:20], fill=(255, 255, 255, 255), anchor='mm')
                except:
                    pass

                # Save
                img.save(screenshot_path, 'PNG')
                print(f"  Created placeholder: {os.path.basename(screenshot_path)}")

                # Update cache
                capturer.cache[asset['path']] = {
                    'hash': capturer.get_file_hash(asset['path']),
                    'screenshot': screenshot_path,
                    'timestamp': datetime.now().isoformat(),
                    'placeholder': True
                }

            capturer.save_cache()
            print()
            print(f"Created {len(assets_to_capture)} placeholder icons")
            print("Replace these with real renders when rendering system is implemented")

        except ImportError:
            print("PIL not available - skipping placeholder generation")
            print("Install Pillow with: pip install Pillow")

    return 0


if __name__ == "__main__":
    exit(main())
