#!/usr/bin/env python3
"""
Batch render all hero assets using the CPU SDF renderer
"""

import subprocess
import sys
from pathlib import Path
import json

HEROES_DIR = Path("H:/Github/Old3DEngine/game/assets/heroes")
OUTPUT_DIR = Path("H:/Github/Old3DEngine/screenshots/asset_icons/heroes")
RENDERER = Path("H:/Github/Old3DEngine/tools/render_sdf_cpu.py")

def main():
    # Ensure output directory exists
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Find all hero JSON files
    hero_files = sorted(HEROES_DIR.glob("*.json"))

    print("=" * 70)
    print(f"BATCH RENDERING {len(hero_files)} HERO ASSETS")
    print("=" * 70)
    print()

    success_count = 0
    error_count = 0

    for i, hero_file in enumerate(hero_files, 1):
        # Load to get name
        try:
            with open(hero_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            name = data.get('name', hero_file.stem)
        except:
            name = hero_file.stem

        output_file = OUTPUT_DIR / f"{hero_file.stem}.png"

        print(f"[{i}/{len(hero_files)}] {name}...")
        print(f"  Input:  {hero_file}")
        print(f"  Output: {output_file}")

        try:
            result = subprocess.run(
                [sys.executable, str(RENDERER), str(hero_file), str(output_file), "--width", "512", "--height", "512"],
                capture_output=True,
                text=True,
                timeout=300
            )

            if result.returncode == 0:
                print(f"  Status: SUCCESS")
                success_count += 1
            else:
                print(f"  Status: FAILED")
                print(f"  Error: {result.stderr[:200]}")
                error_count += 1

        except subprocess.TimeoutExpired:
            print(f"  Status: TIMEOUT")
            error_count += 1
        except Exception as e:
            print(f"  Status: ERROR - {e}")
            error_count += 1

        print()

    print("=" * 70)
    print("BATCH RENDERING COMPLETE")
    print("=" * 70)
    print(f"Total:   {len(hero_files)}")
    print(f"Success: {success_count}")
    print(f"Failed:  {error_count}")
    print(f"\nOutput directory: {OUTPUT_DIR}")
    print("=" * 70)

    return 0 if error_count == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
