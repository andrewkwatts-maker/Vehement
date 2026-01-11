#!/usr/bin/env python3
"""
Fix missing OpenGL constant definitions by ensuring proper headers are included
"""

import os
import re
import sys
from pathlib import Path

def fix_opengl_includes(filepath):
    """Add missing OpenGL headers if GL constants are used"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        original_content = content

        # Check if file uses GL constants but doesn't include glad
        has_gl_constants = re.search(r'\bGL_[A-Z_]+\b', content)
        has_glad_include = '#include <glad/glad.h>' in content or '#include "glad/glad.h"' in content

        if has_gl_constants and not has_glad_include:
            # Find the first #include and add glad before it
            lines = content.split('\n')
            for i, line in enumerate(lines):
                if line.strip().startswith('#include'):
                    # Insert glad include before first include
                    lines.insert(i, '#include <glad/glad.h>')
                    content = '\n'.join(lines)
                    break

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

    print(f"Searching for files with GL constants in {root_dir}...")

    modified_count = 0
    for ext in ['*.cpp', '*.hpp', '*.h']:
        for filepath in Path(root_dir).rglob(ext):
            if fix_opengl_includes(str(filepath)):
                modified_count += 1
                print(f"✓ {filepath}")

    print(f"\nOpenGL header fix complete: {modified_count} files modified")

if __name__ == '__main__':
    main()
