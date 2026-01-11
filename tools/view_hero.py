#!/usr/bin/env python3
"""
View Hero Asset - Launch demo and show instructions for manual screenshot
"""

import subprocess
import time
from pathlib import Path

PROJECT_ROOT = Path("H:/Github/Old3DEngine")
HERO_JSON = PROJECT_ROOT / "game/assets/heroes/warrior_hero.json"
DEMO_EXE = PROJECT_ROOT / "build/bin/Debug/nova_demo.exe"
SCREENSHOTS_DIR = PROJECT_ROOT / "screenshots/hero_polish"

def main():
    print("=" * 70)
    print(" Warrior Hero Viewer")
    print("=" * 70)

    # Create screenshots directory
    SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)

    # Check files exist
    if not DEMO_EXE.exists():
        print(f"ERROR: Demo not found at {DEMO_EXE}")
        return

    if not HERO_JSON.exists():
        print(f"ERROR: Hero asset not found at {HERO_JSON}")
        return

    print(f"\nHero asset: {HERO_JSON.name}")
    print(f"Screenshots will be saved to: {SCREENSHOTS_DIR}")

    print("\n" + "=" * 70)
    print(" INSTRUCTIONS")
    print("=" * 70)
    print("\n1. Demo will launch momentarily")
    print("2. The hero should appear in the scene with:")
    print("   - Sword in right hand")
    print("   - Shield in left hand")
    print("   - Red cape flowing behind")
    print("   - Metal helmet and armor")
    print("   - Idle breathing animation (3 second loop)")
    print("\n3. Camera is positioned at [2.5, 1.5, 3.0] looking at the hero")
    print("4. Take a screenshot using:")
    print("   - Windows: Win + PrintScreen (saves to Pictures/Screenshots)")
    print("   - Or: Alt + PrintScreen (copies window to clipboard)")
    print("\n5. If the hero doesn't appear, check that the demo loaded")
    print("   game/assets/heroes/warrior_hero.json correctly")
    print("\n" + "=" * 70)

    # Launch demo
    print(f"\nLaunching demo: {DEMO_EXE}")

    try:
        # Start the demo process
        subprocess.Popen(
            [str(DEMO_EXE)],
            cwd=str(DEMO_EXE.parent),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        print("✓ Demo launched")
        print("\nWaiting for window to appear...")
        time.sleep(2)

        print("\n" + "=" * 70)
        print(" Demo is running - you should see the window now")
        print(" Take your screenshot, then close the demo when finished")
        print("=" * 70)

    except Exception as e:
        print(f"ERROR launching demo: {e}")

if __name__ == "__main__":
    main()
