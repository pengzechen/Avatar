#!/usr/bin/env python3
#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# You may obtain a copy of the License at:
# https://opensource.org/licenses/MIT
#
# @file add_file_headers.py
# @brief Script to add copyright headers to source files
# @author Avatar Project Team
# @date 2024
#

import os
import sys
import argparse
from pathlib import Path
import re

# File extension to header template mapping
HEADER_TEMPLATES = {
    # C/C++ files
    '.c': 'c_header',
    '.h': 'c_header',
    '.cpp': 'c_header',
    '.hpp': 'c_header',
    '.cc': 'c_header',
    
    # Assembly files
    '.S': 'asm_header',
    '.s': 'asm_header',
    '.asm': 'asm_header',
    
    # Shell scripts
    '.sh': 'shell_header',
    
    # Python files
    '.py': 'python_header',
    
    # Makefiles
    'Makefile': 'makefile_header',
    'makefile': 'makefile_header',
}

def get_c_header(filename, brief=""):
    """Generate C/C++ header"""
    if not brief:
        brief = f"Implementation of {filename}"

    return f"""/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file {filename}
 * @brief {brief}
 * @author Avatar Project Team
 * @date 2024
 */

"""

def get_asm_header(filename, brief=""):
    """Generate Assembly header"""
    if not brief:
        brief = f"Assembly implementation of {filename}"

    return f"""/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file {filename}
 * @brief {brief}
 * @author Avatar Project Team
 * @date 2024
 */

"""

def get_shell_header(filename, brief=""):
    """Generate Shell script header"""
    if not brief:
        brief = f"Shell script: {filename}"

    return f"""#!/bin/bash
#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# See LICENSE file in the project root for full license information.
#
# @file {filename}
# @brief {brief}
# @author Avatar Project Team
# @date 2024
#

"""

def get_python_header(filename, brief=""):
    """Generate Python header"""
    if not brief:
        brief = f"Python script: {filename}"

    return f"""#!/usr/bin/env python3
#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# See LICENSE file in the project root for full license information.
#
# @file {filename}
# @brief {brief}
# @author Avatar Project Team
# @date 2024
#

"""

def get_makefile_header(filename, brief=""):
    """Generate Makefile header"""
    if not brief:
        brief = f"Build configuration: {filename}"

    return f"""#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# See LICENSE file in the project root for full license information.
#
# @file {filename}
# @brief {brief}
# @author Avatar Project Team
# @date 2024
#

"""

def has_copyright_header(content):
    """Check if file already has a copyright header"""
    return "Copyright" in content[:1000] and "Avatar Project" in content[:1000]

def get_header_for_file(filepath, brief=""):
    """Get appropriate header for file based on extension"""
    filename = os.path.basename(filepath)
    ext = Path(filepath).suffix.lower()
    
    if filename in HEADER_TEMPLATES:
        template_type = HEADER_TEMPLATES[filename]
    elif ext in HEADER_TEMPLATES:
        template_type = HEADER_TEMPLATES[ext]
    else:
        return None
    
    if template_type == 'c_header':
        return get_c_header(filename, brief)
    elif template_type == 'asm_header':
        return get_asm_header(filename, brief)
    elif template_type == 'shell_header':
        return get_shell_header(filename, brief)
    elif template_type == 'python_header':
        return get_python_header(filename, brief)
    elif template_type == 'makefile_header':
        return get_makefile_header(filename, brief)
    
    return None

def process_file(filepath, dry_run=False, force=False, verbose=False):
    """Process a single file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        if verbose:
            print(f"⚠️  Skipping binary file: {filepath}")
        return "binary"

    # Check if already has header
    if has_copyright_header(content) and not force:
        if verbose:
            print(f"✅ Already has header: {filepath}")
        return "has_header"
    
    # Get appropriate header
    header = get_header_for_file(filepath)
    if not header:
        if verbose:
            print(f"❓ No template for: {filepath}")
        return "no_template"
    
    # Handle shebang lines
    new_content = content
    if content.startswith('#!'):
        lines = content.split('\n', 1)
        if len(lines) > 1:
            # Keep shebang, add header after
            if filepath.endswith('.py'):
                new_content = lines[0] + '\n' + header[len('#!/usr/bin/env python3\n'):] + lines[1]
            elif filepath.endswith('.sh'):
                new_content = lines[0] + '\n' + header[len('#!/bin/bash\n'):] + lines[1]
            else:
                new_content = header + content
        else:
            new_content = header + content
    else:
        new_content = header + content
    
    if dry_run:
        print(f"📝 Would add header to: {filepath}")
        return "would_add"

    # Write back
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"✅ Added header to: {filepath}")
    return "added"

def main():
    parser = argparse.ArgumentParser(description='Add copyright headers to Avatar project source files')
    parser.add_argument('paths', nargs='*', default=['.'], help='Paths to process (default: current directory)')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be done without making changes')
    parser.add_argument('--force', action='store_true', help='Overwrite existing headers')
    parser.add_argument('--exclude', action='append', default=[], help='Additional exclude patterns')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')

    args = parser.parse_args()

    print("Avatar Project - Copyright Header Tool")
    print("=" * 40)
    if args.dry_run:
        print("🔍 DRY RUN MODE - No files will be modified")
    print()
    
    # Default exclusions - exclude entire clib and guest directories
    exclude_dirs = [
        'build', '.git', '__pycache__',
        'clib', 'guest'  # Exclude entire clib and guest directories
    ]

    exclude_file_patterns = [
        '*.o', '*.bin', '*.img', '*.gz', '*.pyc',
        '*.so', '*.a', '*.lib', '*.dll'
    ] + args.exclude
    
    processed = 0
    skipped = 0
    already_has_header = 0

    print(f"📁 Excluded directories: {', '.join(exclude_dirs)}")
    print(f"🚫 Excluded file patterns: {', '.join(exclude_file_patterns)}")
    print()

    for path in args.paths:
        if os.path.isfile(path):
            result = process_file(path, args.dry_run, args.force, args.verbose)
            if result == "added" or result == "would_add":
                processed += 1
            elif result == "has_header":
                already_has_header += 1
            elif result in ["binary", "no_template"]:
                skipped += 1
        elif os.path.isdir(path):
            for root, dirs, files in os.walk(path):
                # Skip excluded directories (modify dirs in-place to prevent os.walk from entering them)
                dirs[:] = [d for d in dirs if d not in exclude_dirs]

                # Print current directory being processed
                rel_path = os.path.relpath(root, path)
                if rel_path != '.' and args.verbose:
                    print(f"📂 Processing directory: {rel_path}")

                for file in files:
                    filepath = os.path.join(root, file)

                    # Skip excluded file patterns
                    if any(filepath.endswith(pattern.replace('*', '')) or
                           pattern.replace('*', '') in filepath
                           for pattern in exclude_file_patterns):
                        if args.verbose:
                            print(f"⏭️  Skipping excluded file: {filepath}")
                        skipped += 1
                        continue

                    result = process_file(filepath, args.dry_run, args.force, args.verbose)
                    if result == "added" or result == "would_add":
                        processed += 1
                    elif result == "has_header":
                        already_has_header += 1
                    elif result in ["binary", "no_template"]:
                        skipped += 1

    # Print summary
    print("\n" + "=" * 50)
    print("📊 SUMMARY")
    print("=" * 50)
    if args.dry_run:
        print(f"📝 Files that would be processed: {processed}")
    else:
        print(f"✅ Files processed (headers added): {processed}")
    print(f"✅ Files already with headers: {already_has_header}")
    print(f"⏭️  Files skipped: {skipped}")
    print(f"📁 Total files examined: {processed + already_has_header + skipped}")
    print("=" * 50)

if __name__ == '__main__':
    main()
