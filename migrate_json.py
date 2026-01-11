#!/usr/bin/env python3
"""
Migrate jsoncpp API calls to nlohmann::json API
"""

import os
import re
import sys
from pathlib import Path

# Mapping of jsoncpp methods to nlohmann::json equivalents
REPLACEMENTS = [
    # Type checking methods
    (r'\.isNull\(\)', '.is_null()'),
    (r'\.isBool\(\)', '.is_boolean()'),
    (r'\.isInt\(\)', '.is_number_integer()'),
    (r'\.isDouble\(\)', '.is_number_float()'),
    (r'\.isString\(\)', '.is_string()'),
    (r'\.isArray\(\)', '.is_array()'),
    (r'\.isObject\(\)', '.is_object()'),

    # Value access methods - these need special handling
    (r'\.asBool\(\)', '.get<bool>()'),
    (r'\.asInt\(\)', '.get<int>()'),
    (r'\.asUInt\(\)', '.get<unsigned int>()'),
    (r'\.asInt64\(\)', '.get<int64_t>()'),
    (r'\.asUInt64\(\)', '.get<uint64_t>()'),
    (r'\.asFloat\(\)', '.get<float>()'),
    (r'\.asDouble\(\)', '.get<double>()'),
    (r'\.asString\(\)', '.get<std::string>()'),
    (r'\.asCString\(\)', '.get<std::string>().c_str()'),

    # Array/Object methods
    (r'\.append\(', '.push_back('),
    (r'\.size\(\)', '.size()'),  # Same but included for completeness
    (r'\.empty\(\)', '.empty()'),
    (r'\.clear\(\)', '.clear()'),

    # Member access
    (r'\.isMember\(([^)]+)\)', r'.contains(\1)'),
    (r'\.removeMember\(([^)]+)\)', r'.erase(\1)'),
    (r'\.getMemberNames\(\)', '.items()'),  # Note: different API, may need manual fix

    # Array access patterns
    (r'\.get\((\w+),\s*([^)]+)\)', r'[value("\1", \2)]'),  # get with default

    # Type enum values
    (r'Json::nullValue', 'nlohmann::json::value_t::null'),
    (r'Json::intValue', 'nlohmann::json::value_t::number_integer'),
    (r'Json::uintValue', 'nlohmann::json::value_t::number_unsigned'),
    (r'Json::realValue', 'nlohmann::json::value_t::number_float'),
    (r'Json::stringValue', 'nlohmann::json::value_t::string'),
    (r'Json::booleanValue', 'nlohmann::json::value_t::boolean'),
    (r'Json::arrayValue', 'nlohmann::json::value_t::array'),
    (r'Json::objectValue', 'nlohmann::json::value_t::object'),

    # Type name
    (r'Json::Value', 'nlohmann::json'),
    (r'Json::Reader', 'nlohmann::json'),  # nlohmann parses differently
    (r'Json::FastWriter', 'nlohmann::json'),  # nlohmann writes differently
    (r'Json::StyledWriter', 'nlohmann::json'),
]

def migrate_file(filepath):
    """Migrate a single file from jsoncpp to nlohmann::json"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        original_content = content

        # Apply all replacements
        for pattern, replacement in REPLACEMENTS:
            content = re.sub(pattern, replacement, content)

        # Special case: Reader.parse() -> nlohmann::json::parse()
        content = re.sub(
            r'Json::Reader\s+(\w+);\s*\1\.parse\(([^,]+),\s*([^)]+)\)',
            r'\3 = nlohmann::json::parse(\2)',
            content
        )

        # Special case: writer.write() -> .dump()
        content = re.sub(r'(\w+)\.write\(([^)]+)\)', r'\2.dump()', content)

        # Only write if changed
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    except Exception as e:
        print(f"Error processing {filepath}: {e}", file=sys.stderr)
        return False

def find_files_with_json(root_dir):
    """Find all C++ files that use jsoncpp"""
    files = []
    for ext in ['*.cpp', '*.hpp', '*.h']:
        for filepath in Path(root_dir).rglob(ext):
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    if 'Json::Value' in content or 'json/json.h' in content:
                        files.append(str(filepath))
            except:
                pass
    return files

def main():
    root_dir = sys.argv[1] if len(sys.argv) > 1 else '.'

    print(f"Searching for files with jsoncpp usage in {root_dir}...")
    files = find_files_with_json(root_dir)

    print(f"Found {len(files)} files to migrate")

    modified_count = 0
    for filepath in files:
        if migrate_file(filepath):
            modified_count += 1
            print(f"✓ {filepath}")

    print(f"\nMigration complete: {modified_count}/{len(files)} files modified")

if __name__ == '__main__':
    main()
