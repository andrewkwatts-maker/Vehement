#!/usr/bin/env python3
"""
Simple Polish Loop - Start with existing demo

Takes screenshots, analyzes them, and suggests/applies improvements
to lighting, colors, and scene composition for fantasy movie quality.
"""

import subprocess
import time
import os
from pathlib import Path

# Check if we have the required modules
try:
    from PIL import Image, ImageGrab
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("PIL not available - will use basic screenshot method")

class SimplePolishLoop:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.build_dir = self.project_root / "build"
        self.screenshots_dir = self.project_root / "screenshots" / "polish_iterations"
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)

        self.iteration = 0
        self.max_iterations = 5

    def check_build(self):
        """Check if nova_demo.exe exists"""
        exe_path = self.build_dir / "bin/Debug/nova_demo.exe"
        if not exe_path.exists():
            print(f"ERROR: {exe_path} not found")
            print("Please build the project first")
            return False
        print(f"OK: Found {exe_path}")
        return True

    def launch_demo(self):
        """Launch nova_demo.exe"""
        exe_path = self.build_dir / "bin/Debug/nova_demo.exe"

        print("Launching nova_demo.exe...")
        print("Please navigate to the main menu and position the window nicely")

        # Launch and let it run
        process = subprocess.Popen(
            [str(exe_path)],
            cwd=exe_path.parent
        )

        # Give user time to position window
        print("\nWaiting 5 seconds for you to set up the view...")
        time.sleep(5)

        return process

    def take_screenshot(self):
        """Take screenshot using Windows snipping tool or PIL"""
        screenshot_path = self.screenshots_dir / f"iteration_{self.iteration:03d}.png"

        print(f"\nTaking screenshot {self.iteration}...")

        if PIL_AVAILABLE:
            try:
                screenshot = ImageGrab.grab()
                screenshot.save(screenshot_path)
                print(f"  Saved: {screenshot_path.name}")
                return screenshot_path
            except Exception as e:
                print(f"  PIL screenshot failed: {e}")

        # Fallback: use Windows screenshot
        print("  Press Print Screen, then paste into Paint and save manually")
        print(f"  Save to: {screenshot_path}")
        input("  Press Enter when screenshot is saved...")

        if screenshot_path.exists():
            return screenshot_path
        else:
            print("  Screenshot not found, skipping this iteration")
            return None

    def analyze_screenshot(self, screenshot_path):
        """Analyze screenshot and provide suggestions"""
        if not screenshot_path or not PIL_AVAILABLE:
            # Manual analysis
            print("\n=== MANUAL ANALYSIS ===")
            print("Look at the screenshot and rate these aspects (1-10):")

            try:
                lighting = int(input("  Lighting quality (1-10): "))
                colors = int(input("  Color warmth/appeal (1-10): "))
                composition = int(input("  Scene composition (1-10): "))
            except:
                lighting, colors, composition = 5, 5, 5

            analysis = {
                'lighting': lighting / 10.0,
                'colors': colors / 10.0,
                'composition': composition / 10.0
            }
        else:
            # Automatic analysis
            img = Image.open(screenshot_path)

            # Simple metrics
            pixels = list(img.getdata())

            # Brightness
            brightness = sum(sum(p[:3]) for p in pixels) / (len(pixels) * 3 * 255)

            # Contrast (simple estimate)
            brightnesses = [sum(p[:3])/3 for p in pixels]
            contrast = (max(brightnesses) - min(brightnesses)) / 255

            # Color warmth (R vs B)
            r_avg = sum(p[0] for p in pixels) / len(pixels)
            b_avg = sum(p[2] for p in pixels) / len(pixels)
            warmth = (r_avg - b_avg) / 255

            analysis = {
                'brightness': brightness,
                'contrast': contrast,
                'warmth': warmth,
                'lighting': min(1.0, contrast * 1.5),  # Good lighting = high contrast
                'colors': 0.5 + warmth,  # Warmer is better for fantasy
                'composition': brightness  # Reasonable assumption
            }

        print(f"\n  Analysis:")
        print(f"    Lighting: {analysis.get('lighting', 0.5) * 10:.1f}/10")
        print(f"    Colors: {analysis.get('colors', 0.5) * 10:.1f}/10")
        print(f"    Composition: {analysis.get('composition', 0.5) * 10:.1f}/10")

        return analysis

    def generate_suggestions(self, analysis):
        """Generate improvement suggestions"""
        suggestions = []

        lighting_score = analysis.get('lighting', 0.5)
        color_score = analysis.get('colors', 0.5)

        # Target: Fantasy movie look
        # - Warm golden lighting
        # - High contrast
        # - Rich saturated colors

        if lighting_score < 0.7:
            suggestions.append("Increase directional light intensity for more dramatic shadows")
            suggestions.append("Add rim lighting to highlight hero/buildings")

        if color_score < 0.6:
            suggestions.append("Warm up color temperature (increase red/orange in lighting)")
            suggestions.append("Increase ambient light warmth")

        if analysis.get('brightness', 0.5) < 0.4:
            suggestions.append("Scene too dark - increase ambient lighting")
        elif analysis.get('brightness', 0.5) > 0.7:
            suggestions.append("Scene too bright - reduce light intensity")

        if not suggestions:
            suggestions.append("Scene looks good! Consider adding atmospheric effects (fog, particles)")

        return suggestions

    def display_suggestions(self, suggestions):
        """Display suggestions to user"""
        print(f"\n=== SUGGESTIONS FOR IMPROVEMENT ===")
        for i, suggestion in enumerate(suggestions, 1):
            print(f"  {i}. {suggestion}")

        print(f"\nMake these changes manually in:")
        print(f"  - RTSApplication.cpp (lighting values)")
        print(f"  - Shader code (color grading)")
        print(f"  - Asset JSON files (materials, lights)")

    def wait_for_changes(self):
        """Wait for user to make changes"""
        print(f"\n=== WAITING FOR CHANGES ===")
        print("Make the suggested changes, then rebuild if needed")
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

    def run(self):
        """Main loop"""
        print("=" * 60)
        print(" Simple Polish Loop - Fantasy Movie Quality")
        print("=" * 60)

        if not self.check_build():
            return

        for self.iteration in range(1, self.max_iterations + 1):
            print(f"\n{'='*60}")
            print(f" Iteration {self.iteration}/{self.max_iterations}")
            print(f"{'='*60}")

            # Launch
            process = self.launch_demo()

            try:
                # Screenshot
                screenshot_path = self.take_screenshot()

                if screenshot_path:
                    # Analyze
                    analysis = self.analyze_screenshot(screenshot_path)

                    # Suggest
                    suggestions = self.generate_suggestions(analysis)
                    self.display_suggestions(suggestions)

                    # Check if we're done
                    avg_score = (analysis.get('lighting', 0.5) +
                               analysis.get('colors', 0.5) +
                               analysis.get('composition', 0.5)) / 3

                    if avg_score > 0.8:
                        print(f"\n*** EXCELLENT! Average score: {avg_score*10:.1f}/10 ***")
                        print("Scene has reached fantasy movie quality!")
                        break

            finally:
                # Always close
                self.close_demo(process)

            # Wait for changes if not last iteration
            if self.iteration < self.max_iterations:
                self.wait_for_changes()

        print(f"\n{'='*60}")
        print(" Polish Loop Complete!")
        print(f" Screenshots saved to: {self.screenshots_dir}")
        print(f"{'='*60}")

if __name__ == "__main__":
    project_root = Path(__file__).parent.parent
    loop = SimplePolishLoop(project_root)
    loop.run()
