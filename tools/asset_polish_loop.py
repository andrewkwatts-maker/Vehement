#!/usr/bin/env python3
"""
Asset Polish Loop - Polish individual game assets to movie quality

Workflow:
1. Place an asset on landscape
2. Setup camera to frame the asset nicely
3. Take screenshots
4. Analyze visuals (pose, animation, lighting, materials)
5. Improve the asset's JSON (idle animation, materials, lighting)
6. Polish iteratively to fantasy movie quality
7. Move to next asset and repeat
"""

import subprocess
import time
import os
import json
from pathlib import Path

# Check PIL availability
try:
    from PIL import Image, ImageGrab
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("PIL not available - will use manual screenshot method")

class AssetPolishLoop:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.build_dir = self.project_root / "build"
        self.assets_dir = self.project_root / "game" / "assets"
        self.screenshots_dir = self.project_root / "screenshots" / "asset_polish"
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)

        self.current_asset = None
        self.asset_type = None  # unit, hero, building, decoration, etc.
        self.iteration = 0
        self.max_iterations = 10

    def check_build(self):
        """Check if nova_demo.exe exists"""
        exe_path = self.build_dir / "bin/Debug/nova_demo.exe"
        if not exe_path.exists():
            print(f"ERROR: {exe_path} not found")
            print("Please build the project first")
            return False
        print(f"OK: Found {exe_path}")
        return True

    def list_available_assets(self):
        """List all available assets to polish"""
        print("\n=== Available Assets ===")

        assets = []

        # Find all JSON assets in game/assets
        for asset_type in ["units", "heroes", "buildings", "decorations"]:
            type_dir = self.assets_dir / asset_type
            if type_dir.exists():
                for json_file in type_dir.glob("*.json"):
                    assets.append({
                        'type': asset_type,
                        'name': json_file.stem,
                        'path': json_file
                    })

        for i, asset in enumerate(assets, 1):
            print(f"  {i}. [{asset['type']}] {asset['name']}")

        return assets

    def select_asset(self, assets):
        """Let user select which asset to polish"""
        while True:
            try:
                choice = int(input("\nSelect asset number (or 0 to exit): "))
                if choice == 0:
                    return None
                if 1 <= choice <= len(assets):
                    asset = assets[choice - 1]
                    self.current_asset = asset
                    self.asset_type = asset['type']
                    print(f"\nSelected: {asset['name']} ({asset['type']})")
                    return asset
                else:
                    print("Invalid choice, try again")
            except:
                print("Invalid input, try again")

    def load_asset_json(self):
        """Load the current asset's JSON file"""
        if not self.current_asset:
            return None

        try:
            with open(self.current_asset['path'], 'r') as f:
                return json.load(f)
        except Exception as e:
            print(f"ERROR loading asset JSON: {e}")
            return None

    def save_asset_json(self, asset_data):
        """Save updated asset JSON"""
        if not self.current_asset:
            return False

        try:
            with open(self.current_asset['path'], 'w') as f:
                json.dump(asset_data, f, indent=2)
            print(f"  Saved: {self.current_asset['path'].name}")
            return True
        except Exception as e:
            print(f"ERROR saving asset JSON: {e}")
            return False

    def launch_demo_with_asset(self):
        """Launch nova_demo.exe with the selected asset"""
        exe_path = self.build_dir / "bin/Debug/nova_demo.exe"

        print(f"\nLaunching demo with asset: {self.current_asset['name']}...")
        print("The asset should be placed in the world automatically")
        print("Position your camera to frame the asset nicely")

        # Launch the demo
        process = subprocess.Popen(
            [str(exe_path)],
            cwd=exe_path.parent
        )

        # Give user time to set up camera
        print("\nWaiting 10 seconds for you to position the camera...")
        time.sleep(10)

        return process

    def take_screenshot(self):
        """Take screenshot of the asset"""
        asset_name = self.current_asset['name']
        screenshot_path = self.screenshots_dir / f"{asset_name}_iter_{self.iteration:03d}.png"

        print(f"\nTaking screenshot {self.iteration}...")

        if PIL_AVAILABLE:
            try:
                screenshot = ImageGrab.grab()
                screenshot.save(screenshot_path)
                print(f"  Saved: {screenshot_path.name}")
                return screenshot_path
            except Exception as e:
                print(f"  PIL screenshot failed: {e}")

        # Fallback: manual screenshot
        print("  Press Print Screen, then paste into Paint and save manually")
        print(f"  Save to: {screenshot_path}")
        input("  Press Enter when screenshot is saved...")

        if screenshot_path.exists():
            return screenshot_path
        else:
            print("  Screenshot not found, skipping this iteration")
            return None

    def analyze_asset_visual(self, screenshot_path, asset_data):
        """Analyze the asset visually and provide feedback"""
        print("\n=== VISUAL ANALYSIS ===")

        analysis = {}

        if PIL_AVAILABLE and screenshot_path:
            # Automatic analysis
            img = Image.open(screenshot_path)
            pixels = list(img.getdata())

            # Brightness
            brightness = sum(sum(p[:3]) for p in pixels) / (len(pixels) * 3 * 255)

            # Contrast
            brightnesses = [sum(p[:3])/3 for p in pixels]
            contrast = (max(brightnesses) - min(brightnesses)) / 255

            # Color warmth
            r_avg = sum(p[0] for p in pixels) / len(pixels)
            b_avg = sum(p[2] for p in pixels) / len(pixels)
            warmth = (r_avg - b_avg) / 255

            analysis['brightness'] = brightness
            analysis['contrast'] = contrast
            analysis['warmth'] = warmth
        else:
            # Manual analysis
            print("Rate the following aspects (1-10):")
            try:
                pose_quality = int(input("  Pose/animation quality (1-10): "))
                lighting_quality = int(input("  Lighting quality (1-10): "))
                material_quality = int(input("  Material quality (1-10): "))
                overall_quality = int(input("  Overall movie quality (1-10): "))
            except:
                pose_quality = lighting_quality = material_quality = overall_quality = 5

            analysis['pose'] = pose_quality / 10.0
            analysis['lighting'] = lighting_quality / 10.0
            analysis['materials'] = material_quality / 10.0
            analysis['overall'] = overall_quality / 10.0

        print(f"\n  Analysis:")
        for key, value in analysis.items():
            print(f"    {key.capitalize()}: {value * 10:.1f}/10")

        return analysis

    def generate_improvement_suggestions(self, analysis, asset_data):
        """Generate specific improvement suggestions based on analysis"""
        suggestions = []

        # Check if asset has idle animation
        if 'animationSet' in asset_data:
            anim_set = asset_data['animationSet']
            if 'idle' not in str(anim_set).lower():
                suggestions.append({
                    'category': 'animation',
                    'issue': 'Missing idle animation',
                    'fix': 'Add an idle animation with subtle breathing/movement'
                })

        # Check materials
        if 'sdfModel' in asset_data:
            model = asset_data['sdfModel']
            if 'primitives' in model:
                for prim in model['primitives']:
                    if prim.get('metallic', 0) == 0 and prim.get('roughness', 0.5) == 0.5:
                        suggestions.append({
                            'category': 'material',
                            'issue': f"Primitive '{prim.get('id', 'unknown')}' uses default materials",
                            'fix': 'Adjust metallic/roughness for more realistic appearance'
                        })

        # Check lighting based on analysis
        if analysis.get('lighting', 0.5) < 0.6:
            suggestions.append({
                'category': 'lighting',
                'issue': 'Lighting quality needs improvement',
                'fix': 'Add rim lights, adjust ambient light warmth'
            })

        if analysis.get('brightness', 0.5) < 0.4:
            suggestions.append({
                'category': 'lighting',
                'issue': 'Scene too dark',
                'fix': 'Increase ambient and directional light intensity'
            })
        elif analysis.get('brightness', 0.5) > 0.7:
            suggestions.append({
                'category': 'lighting',
                'issue': 'Scene too bright',
                'fix': 'Reduce light intensity or add shadows'
            })

        if analysis.get('warmth', 0) < -0.1:
            suggestions.append({
                'category': 'lighting',
                'issue': 'Lighting too cold (blue)',
                'fix': 'Warm up color temperature (increase red/orange)'
            })

        # Overall quality check
        overall = analysis.get('overall', analysis.get('lighting', 0.5))
        if overall < 0.7:
            suggestions.append({
                'category': 'general',
                'issue': 'Not yet at movie quality',
                'fix': 'Continue refining materials, lighting, and animations'
            })

        return suggestions

    def display_suggestions(self, suggestions):
        """Display improvement suggestions to user"""
        print("\n=== IMPROVEMENT SUGGESTIONS ===")

        if not suggestions:
            print("  Asset looks great! Movie quality achieved!")
            return

        for i, suggestion in enumerate(suggestions, 1):
            print(f"\n  {i}. [{suggestion['category'].upper()}]")
            print(f"     Issue: {suggestion['issue']}")
            print(f"     Fix: {suggestion['fix']}")

    def apply_automated_improvements(self, asset_data, suggestions):
        """Apply some automated improvements to the asset JSON"""
        print("\n=== APPLYING AUTO-IMPROVEMENTS ===")

        modified = False

        for suggestion in suggestions:
            category = suggestion['category']

            if category == 'animation' and 'Missing idle animation' in suggestion['issue']:
                # Add basic idle animation if missing
                if 'animationSet' not in asset_data:
                    asset_data['animationSet'] = {}

                asset_data['animationSet']['idle'] = {
                    'duration': 2.0,
                    'loop': True,
                    'keyframes': [
                        {'time': 0.0, 'events': []},
                        {'time': 1.0, 'events': []}
                    ]
                }
                print("  Added basic idle animation")
                modified = True

            elif category == 'material':
                # Enhance materials
                if 'sdfModel' in asset_data and 'primitives' in asset_data['sdfModel']:
                    for prim in asset_data['sdfModel']['primitives']:
                        if prim.get('metallic', 0) == 0:
                            prim['metallic'] = 0.2  # Slight metallic
                            prim['roughness'] = 0.6  # More realistic roughness
                            modified = True
                    if modified:
                        print("  Enhanced material properties")

            elif category == 'lighting':
                # Note: Lighting is scene-level, not per-asset
                # The user will need to manually adjust scene lighting
                pass

        return asset_data, modified

    def wait_for_manual_improvements(self):
        """Wait for user to make manual improvements"""
        print("\n=== MANUAL IMPROVEMENTS ===")
        print("Make the suggested improvements to:")
        print(f"  - {self.current_asset['path']}")
        print("  - Scene lighting (if needed)")
        print("  - Material settings")
        print("  - Animation keyframes")
        print("\nThen rebuild if needed")
        input("Press Enter when ready for next iteration...")

    def close_demo(self, process):
        """Close the demo application"""
        if process:
            print("\nClosing demo...")
            process.terminate()
            try:
                process.wait(timeout=5)
            except:
                process.kill()

    def polish_asset(self, asset):
        """Main polish loop for a single asset"""
        self.current_asset = asset
        self.iteration = 0

        print("\n" + "="*60)
        print(f" Polishing Asset: {asset['name']} ({asset['type']})")
        print("="*60)

        # Load asset JSON
        asset_data = self.load_asset_json()
        if not asset_data:
            print("ERROR: Failed to load asset JSON")
            return False

        for self.iteration in range(1, self.max_iterations + 1):
            print(f"\n{'='*60}")
            print(f" Iteration {self.iteration}/{self.max_iterations}")
            print(f"{'='*60}")

            # Launch demo
            process = self.launch_demo_with_asset()

            try:
                # Take screenshot
                screenshot_path = self.take_screenshot()

                if screenshot_path:
                    # Analyze
                    analysis = self.analyze_asset_visual(screenshot_path, asset_data)

                    # Generate suggestions
                    suggestions = self.generate_improvement_suggestions(analysis, asset_data)
                    self.display_suggestions(suggestions)

                    # Check if we're done
                    overall = analysis.get('overall', analysis.get('lighting', 0.5))
                    if overall >= 0.8 and not suggestions:
                        print(f"\n*** EXCELLENT! Movie quality achieved! ***")
                        print(f"Average score: {overall*10:.1f}/10")
                        break

                    # Apply automated improvements
                    asset_data, modified = self.apply_automated_improvements(asset_data, suggestions)
                    if modified:
                        self.save_asset_json(asset_data)

            finally:
                # Always close demo
                self.close_demo(process)

            # Wait for manual improvements if not last iteration
            if self.iteration < self.max_iterations:
                self.wait_for_manual_improvements()
                # Reload asset JSON (user may have edited it)
                asset_data = self.load_asset_json()

        print(f"\n{'='*60}")
        print(f" Asset Polish Complete: {asset['name']}")
        print(f" Screenshots saved to: {self.screenshots_dir}")
        print(f"{'='*60}")

        return True

    def run(self):
        """Main loop - polish multiple assets"""
        print("="*60)
        print(" Asset Polish Loop - Fantasy Movie Quality")
        print("="*60)

        if not self.check_build():
            return

        while True:
            # List available assets
            assets = self.list_available_assets()
            if not assets:
                print("\nNo assets found in game/assets/")
                print("Please create some asset JSON files first")
                return

            # Select asset
            asset = self.select_asset(assets)
            if not asset:
                print("\nExiting...")
                return

            # Polish the selected asset
            self.polish_asset(asset)

            # Ask if user wants to polish another asset
            choice = input("\nPolish another asset? (y/n): ").lower()
            if choice != 'y':
                break

        print("\n" + "="*60)
        print(" All Assets Polished!")
        print("="*60)

if __name__ == "__main__":
    project_root = Path(__file__).parent.parent
    loop = AssetPolishLoop(project_root)
    loop.run()
