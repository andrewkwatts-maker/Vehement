#!/usr/bin/env python3
"""
Editor UI Link Checker and Behavior Analyzer

This script analyzes the Nova3D Editor codebase to find:
1. Broken UI links (menu items, buttons that don't call functions)
2. Unimplemented behavior (stub functions, TODO comments)
3. Missing integrations (asset types without editors)
4. Incomplete features (dialogs that don't save, etc.)
"""

import os
import re
import json
from pathlib import Path
from typing import List, Dict, Set, Tuple

class EditorAnalyzer:
    def __init__(self, root_dir: str):
        self.root_dir = Path(root_dir)
        self.examples_dir = self.root_dir / "examples"
        self.engine_dir = self.root_dir / "engine"

        self.issues = {
            "broken_ui_links": [],
            "unimplemented_functions": [],
            "missing_integrations": [],
            "incomplete_features": [],
            "todo_items": []
        }

    def analyze(self):
        """Run all analysis passes"""
        print("=== Nova3D Editor Analysis ===\n")

        self.find_ui_links()
        self.find_unimplemented_functions()
        self.find_missing_integrations()
        self.find_todo_items()

        self.print_report()
        self.save_report()

    def find_ui_links(self):
        """Find ImGui menu items and buttons that may not be wired up"""
        print("[1/4] Analyzing UI links...")

        patterns = [
            (r'ImGui::MenuItem\("([^"]+)"[^)]*\)\s*{\s*(?://|/\*)', "Menu item with only comment"),
            (r'ImGui::Button\("([^"]+)"\)\s*{\s*(?://|/\*)', "Button with only comment"),
            (r'ImGui::MenuItem\("([^"]+)"[^)]*\)\s*{\s*}', "Empty menu item handler"),
            (r'ImGui::Button\("([^"]+)"\)\s*{\s*}', "Empty button handler"),
            (r'ImGui::MenuItem\("([^"]+)"[^)]*\)\s*{\s*spdlog::warn\("([^"]*)not.*implemented', "Menu item explicitly not implemented"),
        ]

        for cpp_file in self.examples_dir.glob("**/*.cpp"):
            content = cpp_file.read_text(encoding='utf-8', errors='ignore')

            for pattern, issue_type in patterns:
                matches = re.finditer(pattern, content, re.IGNORECASE)
                for match in matches:
                    self.issues["broken_ui_links"].append({
                        "file": str(cpp_file.relative_to(self.root_dir)),
                        "type": issue_type,
                        "text": match.group(1) if match.groups() else match.group(0),
                        "line": content[:match.start()].count('\n') + 1
                    })

    def find_unimplemented_functions(self):
        """Find function stubs and unimplemented methods"""
        print("[2/4] Finding unimplemented functions...")

        patterns = [
            (r'void\s+(\w+)\([^)]*\)\s*{\s*spdlog::(?:warn|error)\("([^"]*?)(?:not|Not|NOT).*?(?:implement|Implement)', "Function with 'not implemented' log"),
            (r'void\s+(\w+)\([^)]*\)\s*{\s*//\s*TODO[:\s]+(.+?)$', "Function with TODO comment"),
            (r'void\s+(\w+)\([^)]*\)\s*{\s*return;\s*}', "Empty void function"),
            (r'\w+\s+(\w+)\([^)]*\)\s*{\s*return\s+\w+\(\);\s*}', "Function returning default value"),
        ]

        for cpp_file in self.examples_dir.glob("**/*.cpp"):
            content = cpp_file.read_text(encoding='utf-8', errors='ignore')

            for pattern, issue_type in patterns:
                matches = re.finditer(pattern, content, re.MULTILINE)
                for match in matches:
                    self.issues["unimplemented_functions"].append({
                        "file": str(cpp_file.relative_to(self.root_dir)),
                        "function": match.group(1),
                        "type": issue_type,
                        "line": content[:match.start()].count('\n') + 1,
                        "details": match.group(2) if len(match.groups()) > 1 else ""
                    })

    def find_missing_integrations(self):
        """Find asset types that lack editor integration"""
        print("[3/4] Checking asset type integrations...")

        # Read asset types from AssetConfig.hpp
        asset_config = self.engine_dir / "game" / "AssetConfig.hpp"
        if asset_config.exists():
            content = asset_config.read_text()

            # Extract AssetType enum
            enum_match = re.search(r'enum class AssetType\s*{([^}]+)}', content, re.DOTALL)
            if enum_match:
                asset_types = re.findall(r'(\w+)\s*,?\s*//.*$', enum_match.group(1), re.MULTILINE)
                asset_types = [t for t in asset_types if t and t != 'Asset']

                # Check which types have editors
                for asset_type in asset_types:
                    # Look for dedicated editor files
                    editor_file = self.examples_dir / f"{asset_type}Editor.cpp"

                    # Look for handling in AssetEditor or StandaloneEditor
                    has_integration = False
                    for editor_cpp in self.examples_dir.glob("*Editor.cpp"):
                        editor_content = editor_cpp.read_text(encoding='utf-8', errors='ignore')
                        if asset_type in editor_content or asset_type.lower() in editor_content:
                            has_integration = True
                            break

                    if not has_integration and not editor_file.exists():
                        self.issues["missing_integrations"].append({
                            "asset_type": asset_type,
                            "issue": f"No editor found for {asset_type}",
                            "suggestion": f"Create {asset_type}Editor or add handling to JSONEditor"
                        })

    def find_todo_items(self):
        """Find all TODO, FIXME, HACK comments in editor code"""
        print("[4/4] Collecting TODO items...")

        patterns = [
            r'//\s*TODO[:\s]+(.+?)$',
            r'//\s*FIXME[:\s]+(.+?)$',
            r'//\s*HACK[:\s]+(.+?)$',
            r'//\s*XXX[:\s]+(.+?)$',
            r'/\*\s*TODO[:\s]+(.+?)\*/',
        ]

        for cpp_file in self.examples_dir.glob("**/*.[ch]pp"):
            content = cpp_file.read_text(encoding='utf-8', errors='ignore')

            for pattern in patterns:
                matches = re.finditer(pattern, content, re.MULTILINE | re.IGNORECASE)
                for match in matches:
                    self.issues["todo_items"].append({
                        "file": str(cpp_file.relative_to(self.root_dir)),
                        "line": content[:match.start()].count('\n') + 1,
                        "text": match.group(1).strip()
                    })

    def print_report(self):
        """Print analysis report to console"""
        print("\n" + "="*80)
        print("ANALYSIS REPORT")
        print("="*80)

        print(f"\n[1] Broken UI Links: {len(self.issues['broken_ui_links'])} found")
        for item in self.issues["broken_ui_links"][:10]:  # Show first 10
            print(f"  - {item['file']}:{item['line']} - {item['type']}: '{item['text']}'")
        if len(self.issues["broken_ui_links"]) > 10:
            print(f"  ... and {len(self.issues['broken_ui_links']) - 10} more")

        print(f"\n[2] Unimplemented Functions: {len(self.issues['unimplemented_functions'])} found")
        for item in self.issues["unimplemented_functions"][:10]:
            print(f"  - {item['file']}:{item['line']} - {item['function']}() - {item['type']}")
        if len(self.issues["unimplemented_functions"]) > 10:
            print(f"  ... and {len(self.issues['unimplemented_functions']) - 10} more")

        print(f"\n[3] Missing Integrations: {len(self.issues['missing_integrations'])} found")
        for item in self.issues["missing_integrations"]:
            print(f"  - {item['asset_type']}: {item['issue']}")
            print(f"    Suggestion: {item['suggestion']}")

        print(f"\n[4] TODO Items: {len(self.issues['todo_items'])} found")
        for item in self.issues["todo_items"][:10]:
            print(f"  - {item['file']}:{item['line']} - {item['text']}")
        if len(self.issues["todo_items"]) > 10:
            print(f"  ... and {len(self.issues['todo_items']) - 10} more")

        print("\n" + "="*80)
        print(f"Total Issues: {sum(len(v) for v in self.issues.values())}")
        print("="*80)

    def save_report(self):
        """Save detailed report to JSON"""
        report_path = self.root_dir / "editor_analysis_report.json"
        with open(report_path, 'w') as f:
            json.dump(self.issues, f, indent=2)
        print(f"\nDetailed report saved to: {report_path}")

        # Also create a prioritized action list
        action_list = []

        # Priority 1: Broken UI links (user-facing)
        for item in self.issues["broken_ui_links"]:
            action_list.append({
                "priority": 1,
                "category": "UI Link",
                "action": f"Implement handler for '{item['text']}'",
                "file": item['file'],
                "line": item['line']
            })

        # Priority 2: Missing integrations (core features)
        for item in self.issues["missing_integrations"]:
            action_list.append({
                "priority": 2,
                "category": "Integration",
                "action": f"Add editor support for {item['asset_type']}",
                "suggestion": item['suggestion']
            })

        # Priority 3: Unimplemented functions
        for item in self.issues["unimplemented_functions"]:
            action_list.append({
                "priority": 3,
                "category": "Implementation",
                "action": f"Implement {item['function']}()",
                "file": item['file'],
                "line": item['line']
            })

        action_path = self.root_dir / "editor_action_list.json"
        with open(action_path, 'w') as f:
            json.dump(sorted(action_list, key=lambda x: x['priority']), f, indent=2)
        print(f"Action list saved to: {action_path}")

if __name__ == "__main__":
    analyzer = EditorAnalyzer("H:/Github/Old3DEngine")
    analyzer.analyze()
