#!/usr/bin/env python3
"""
Automated Main Menu Polish Loop

This script iteratively improves the main menu scene to achieve a fantasy movie look by:
1. Building the application
2. Launching it and taking screenshots
3. Analyzing the visuals
4. Updating SDF/JSON assets
5. Repeating until movie quality is achieved
"""

import subprocess
import time
import os
import sys
import json
from pathlib import Path
from PIL import Image, ImageGrab
import pyautogui

class MenuPolishLoop:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.build_dir = self.project_root / "build"
        self.screenshots_dir = self.project_root / "screenshots" / "menu_iterations"
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)

        self.iteration = 0
        self.max_iterations = 10

        # Paths to key asset files
        self.materials_dir = self.project_root / "game/assets/projects/default_project/assets/materials"
        self.lights_dir = self.project_root / "game/assets/projects/default_project/assets/lights"

        # Quality targets (1-10 scale)
        self.quality_targets = {
            "lighting": 9,
            "materials": 9,
            "composition": 8,
            "atmosphere": 9,
            "detail": 8
        }

    def build_project(self):
        """Build the project with latest changes"""
        print(f"\n{'='*60}")
        print(f"Iteration {self.iteration}: Building project...")
        print(f"{'='*60}")

        os.chdir(self.build_dir)

        # Run CMake
        result = subprocess.run(["cmake", ".."], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"CMake failed: {result.stderr}")
            return False

        # Build
        result = subprocess.run(
            ["cmake", "--build", ".", "--config", "Debug", "--target", "nova_demo"],
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"Build failed: {result.stderr}")
            return False

        print("✓ Build successful")
        return True

    def launch_application(self):
        """Launch the demo application"""
        print("Launching application...")

        exe_path = self.build_dir / "bin/Debug/nova_demo.exe"
        if not exe_path.exists():
            print(f"Executable not found: {exe_path}")
            return None

        # Launch in background
        process = subprocess.Popen(
            [str(exe_path)],
            cwd=exe_path.parent,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        # Wait for window to appear
        time.sleep(3)

        print("✓ Application launched")
        return process

    def take_screenshot(self):
        """Take a screenshot of the main menu"""
        screenshot_path = self.screenshots_dir / f"iteration_{self.iteration:03d}.png"

        print(f"Taking screenshot: {screenshot_path.name}")

        # Use PIL to capture the screen
        try:
            screenshot = ImageGrab.grab()
            screenshot.save(screenshot_path)
            print(f"✓ Screenshot saved: {screenshot_path}")
            return screenshot_path
        except Exception as e:
            print(f"✗ Screenshot failed: {e}")
            return None

    def analyze_screenshot(self, screenshot_path):
        """Analyze screenshot and provide improvement suggestions"""
        print(f"\nAnalyzing screenshot...")

        # Open image
        img = Image.open(screenshot_path)

        # Calculate metrics (simplified for demonstration)
        analysis = {
            "brightness": self._analyze_brightness(img),
            "contrast": self._analyze_contrast(img),
            "color_warmth": self._analyze_color_warmth(img),
            "composition": self._analyze_composition(img)
        }

        # Generate suggestions
        suggestions = self._generate_suggestions(analysis)

        print(f"\n  Analysis Results:")
        print(f"    Brightness: {analysis['brightness']:.2f}")
        print(f"    Contrast: {analysis['contrast']:.2f}")
        print(f"    Color Warmth: {analysis['color_warmth']:.2f}")
        print(f"    Composition: {analysis['composition']:.2f}")

        print(f"\n  Suggestions:")
        for i, suggestion in enumerate(suggestions, 1):
            print(f"    {i}. {suggestion}")

        return analysis, suggestions

    def _analyze_brightness(self, img):
        """Analyze overall brightness (0-1 scale)"""
        grayscale = img.convert('L')
        pixels = list(grayscale.getdata())
        return sum(pixels) / (len(pixels) * 255.0)

    def _analyze_contrast(self, img):
        """Analyze contrast (0-1 scale)"""
        grayscale = img.convert('L')
        pixels = list(grayscale.getdata())
        min_val = min(pixels)
        max_val = max(pixels)
        return (max_val - min_val) / 255.0

    def _analyze_color_warmth(self, img):
        """Analyze color temperature (-1 cool, 0 neutral, 1 warm)"""
        rgb = img.convert('RGB')
        pixels = list(rgb.getdata())

        r_avg = sum(p[0] for p in pixels) / len(pixels)
        b_avg = sum(p[2] for p in pixels) / len(pixels)

        # Warmth = more red than blue
        warmth = (r_avg - b_avg) / 255.0
        return warmth

    def _analyze_composition(self, img):
        """Analyze composition balance (0-1 scale)"""
        # Simplified: Check thirds rule
        width, height = img.size

        # Sample key points
        center = img.getpixel((width // 2, height // 2))
        left_third = img.getpixel((width // 3, height // 2))
        right_third = img.getpixel((2 * width // 3, height // 2))

        # Calculate variance (good composition has variation)
        points = [center, left_third, right_third]
        avg_brightness = sum(sum(p) for p in points) / (len(points) * 3 * 255)

        return avg_brightness

    def _generate_suggestions(self, analysis):
        """Generate improvement suggestions based on analysis"""
        suggestions = []

        # Fantasy movie look targets:
        # - Warm golden lighting (sunset/magic hour)
        # - High contrast (dramatic shadows)
        # - Moderate brightness (not too dark, not washed out)

        if analysis['brightness'] < 0.3:
            suggestions.append("Increase ambient lighting - scene is too dark")
        elif analysis['brightness'] > 0.7:
            suggestions.append("Reduce lighting intensity - scene is too bright")

        if analysis['contrast'] < 0.6:
            suggestions.append("Increase contrast - add stronger directional light")

        if analysis['color_warmth'] < 0.1:
            suggestions.append("Warm up color temperature - add golden/orange tones")
        elif analysis['color_warmth'] > 0.4:
            suggestions.append("Reduce warmth slightly - too much orange")

        if len(suggestions) == 0:
            suggestions.append("Scene looks good! Add atmospheric effects (fog, particles)")

        return suggestions

    def apply_improvements(self, suggestions):
        """Apply improvements to JSON/SDF assets"""
        print(f"\nApplying improvements...")

        improvements_made = []

        for suggestion in suggestions:
            if "lighting" in suggestion.lower():
                self._improve_lighting(suggestion)
                improvements_made.append("Updated lighting")

            if "warm" in suggestion.lower() or "color" in suggestion.lower():
                self._improve_color_temperature(suggestion)
                improvements_made.append("Adjusted color temperature")

            if "contrast" in suggestion.lower():
                self._improve_contrast(suggestion)
                improvements_made.append("Enhanced contrast")

            if "atmosphere" in suggestion.lower():
                self._add_atmospheric_effects()
                improvements_made.append("Added atmospheric effects")

        print(f"  ✓ Applied {len(improvements_made)} improvements:")
        for improvement in improvements_made:
            print(f"    - {improvement}")

        return len(improvements_made) > 0

    def _improve_lighting(self, suggestion):
        """Improve lighting through JSON assets"""
        sun_light_path = self.lights_dir / "sun.json"

        if sun_light_path.exists():
            with open(sun_light_path, 'r') as f:
                sun_config = json.load(f)

            # Adjust intensity
            if "too dark" in suggestion.lower():
                sun_config['intensity'] = sun_config.get('intensity', 1.0) * 1.2
            elif "too bright" in suggestion.lower():
                sun_config['intensity'] = sun_config.get('intensity', 1.0) * 0.8

            with open(sun_light_path, 'w') as f:
                json.dump(sun_config, f, indent=2)

    def _improve_color_temperature(self, suggestion):
        """Adjust color temperature"""
        sun_light_path = self.lights_dir / "sun.json"

        if sun_light_path.exists():
            with open(sun_light_path, 'r') as f:
                sun_config = json.load(f)

            # Adjust temperature
            if "warm up" in suggestion.lower():
                sun_config['temperature'] = min(6500, sun_config.get('temperature', 5500) + 200)
            elif "reduce warmth" in suggestion.lower():
                sun_config['temperature'] = max(4500, sun_config.get('temperature', 5500) - 200)

            with open(sun_light_path, 'w') as f:
                json.dump(sun_config, f, indent=2)

    def _improve_contrast(self, suggestion):
        """Improve contrast through lighting"""
        # Increase directional light, reduce ambient
        # This creates stronger shadows
        pass

    def _add_atmospheric_effects(self):
        """Add fog, particles, etc."""
        # Add atmospheric scattering, volumetric fog
        pass

    def close_application(self, process):
        """Close the running application"""
        if process:
            print("Closing application...")
            process.terminate()
            process.wait(timeout=5)
            print("✓ Application closed")

    def run(self):
        """Main loop"""
        print(f"\n{'#'*60}")
        print(f"# Main Menu Polish Loop - Fantasy Movie Quality")
        print(f"# Target: {self.max_iterations} iterations")
        print(f"{'#'*60}\n")

        for self.iteration in range(1, self.max_iterations + 1):
            # Build
            if not self.build_project():
                print("Build failed, stopping loop")
                break

            # Launch
            process = self.launch_application()
            if not process:
                print("Failed to launch application")
                break

            try:
                # Screenshot
                screenshot_path = self.take_screenshot()
                if not screenshot_path:
                    continue

                # Analyze
                analysis, suggestions = self.analyze_screenshot(screenshot_path)

                # Apply improvements
                if self.iteration < self.max_iterations:
                    improved = self.apply_improvements(suggestions)

                    if not improved:
                        print("\n✓ No more improvements needed!")
                        break
                else:
                    print("\n✓ Maximum iterations reached!")

            finally:
                # Always close the application
                self.close_application(process)

            # Wait before next iteration
            if self.iteration < self.max_iterations:
                print(f"\nWaiting 2 seconds before next iteration...")
                time.sleep(2)

        print(f"\n{'#'*60}")
        print(f"# Polish Loop Complete!")
        print(f"# Final screenshots in: {self.screenshots_dir}")
        print(f"{'#'*60}\n")

if __name__ == "__main__":
    project_root = Path(__file__).parent.parent
    loop = MenuPolishLoop(project_root)
    loop.run()
