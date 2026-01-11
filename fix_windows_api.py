#!/usr/bin/env python3
"""
Fix Windows platform API issues
"""

import os
import re
import sys
from pathlib import Path

def fix_windows_api(filepath):
    """Fix Windows API issues in platform files"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        original_content = content

        # Check if this is a Windows platform file
        if 'Windows' not in str(filepath):
            return False

        # Fix IsWindows10OrGreater - add include if needed
        if 'IsWindows10OrGreater' in content:
            if '#include <VersionHelpers.h>' not in content:
                # Find windows.h include and add VersionHelpers.h after it
                content = re.sub(
                    r'(#include <[Ww]indows\.h>)',
                    r'\1\n#include <VersionHelpers.h>',
                    content
                )

        # Only write if changed
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    except Exception as e:
        print(f"Error processing {filepath}: {e}", file=sys.stderr)
        return False

def main():
    root_dir = sys.argv[1] if len(sys.argv) > 1 else '.'

    print(f"Searching for Windows platform files in {root_dir}...")

    modified_count = 0
    for ext in ['*.cpp', '*.hpp', '*.h']:
        for filepath in Path(root_dir).rglob(ext):
            if fix_windows_api(str(filepath)):
                modified_count += 1
                print(f"✓ {filepath}")

    print(f"\nWindows API fix complete: {modified_count} files modified")

if __name__ == '__main__':
    main()
