#!/usr/bin/env python3
"""Replace menu sections in StandaloneEditor.cpp"""

def replace_menu_section(content, start_marker, end_marker, replacement_file):
    """Replace a section of code between markers"""

    # Find the start and end positions
    start_pos = content.find(start_marker)
    if start_pos == -1:
        print(f"Warning: Could not find start marker")
        return content

    # Find the matching EndMenu after the start
    end_search_start = start_pos + len(start_marker)
    end_pos = content.find(end_marker, end_search_start)
    if end_pos == -1:
        print(f"Warning: Could not find end marker")
        return content

    # Include the EndMenu line
    end_pos = content.find('\n', end_pos) + 1

    # Read the replacement
    with open(replacement_file, 'r', encoding='utf-8') as f:
        replacement = f.read()

    # Replace
    return content[:start_pos] + replacement + content[end_pos:]

# Read the source file
print("Reading StandaloneEditor.cpp...")
with open('StandaloneEditor.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Backup
print("Creating backup...")
with open('StandaloneEditor.cpp.before_menu_replace', 'w', encoding='utf-8') as f:
    f.write(content)

# Replace File menu
print("Replacing File menu...")
content = replace_menu_section(
    content,
    '        if (ImGui::BeginMenu("File")) {',
    '            ImGui::EndMenu();',
    'file_menu_replacement.cpp'
)

# Replace Edit menu
print("Replacing Edit menu...")
content = replace_menu_section(
    content,
    '        if (ImGui::BeginMenu("Edit")) {',
    '            ImGui::EndMenu();',
    'edit_menu_replacement.cpp'
)

# Write the result
print("Writing patched file...")
with open('StandaloneEditor.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Done! Menus replaced successfully.")
print("Backup saved to: StandaloneEditor.cpp.before_menu_replace")
