#!/usr/bin/env python3
#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# See LICENSE file in the project root for full license information.
#
# @file avatar_symbols.py
# @brief Python script: avatar_symbols.py
# @author Avatar Project Team
# @date 2024
#

"""
Symbol Table Analyzer for Avatar OS
自动从 build/dis.txt 中读取 SYMBOL TABLE 并进行分析
"""

import os
import sys
import re
from typing import List, Dict, Tuple, Optional

def read_symbol_table_from_file(file_path: str) -> List[str]:
    """
    从 dis.txt 文件中读取 SYMBOL TABLE 部分
    按照用户逻辑：从 "SYMBOL TABLE:" 的下一行开始，到 "Disassembly of section .text:" 的上一行

    Args:
        file_path: dis.txt 文件路径

    Returns:
        符号表行的列表
    """
    if not os.path.exists(file_path):
        print(f"错误: 文件 {file_path} 不存在")
        return []

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # 查找 "SYMBOL TABLE:" 开始位置
        start_idx = -1
        for i, line in enumerate(lines):
            if line.strip() == 'SYMBOL TABLE:':
                start_idx = i + 1  # 从下一行开始读取符号
                break

        if start_idx == -1:
            print("错误: 在文件中未找到 'SYMBOL TABLE:' 标记")
            return []

        # 查找 "Disassembly of section .text:" 结束位置
        end_idx = -1
        for i in range(start_idx, len(lines)):
            line = lines[i].strip()
            if line.startswith('Disassembly of section .text:'):
                end_idx = i  # 到这一行的上一行
                break

        if end_idx == -1:
            print("警告: 未找到 'Disassembly of section .text:' 标记，读取到文件末尾")
            end_idx = len(lines)

        # 提取符号表行
        symbol_lines = []
        for i in range(start_idx, end_idx):
            line = lines[i].strip()
            if line:  # 只跳过空行
                symbol_lines.append(line)

        print(f"成功读取 {len(symbol_lines)} 行符号数据")
        return symbol_lines

    except Exception as e:
        print(f"错误: 读取文件时出现异常: {e}")
        return []

class SymbolAnalyzer:
    """符号表分析器"""

    def __init__(self, symbol_lines: List[str]):
        self.symbol_lines = symbol_lines
        self.symbols = self.parse_symbols()

    def parse_symbols(self) -> List[Dict]:
        """
        解析符号表数据
        每行格式：
        6部分: address flags type section size name
        5部分: address flags section size name (没有type字段)

        例如:
        0000000040080000 l    d  .text	0000000000000000 .text
        0000000040080148 l       .text	0000000000000000 from_el3_to_el1
        """
        symbols = []

        for line_num, line in enumerate(self.symbol_lines, 1):
            try:
                # 使用制表符和空格分割
                # 先按制表符分割，再处理空格
                parts = re.split(r'\s+', line.strip())

                if len(parts) < 5:
                    continue

                # 验证地址格式 (16位十六进制)
                address_str = parts[0]
                if not re.match(r'^[0-9a-fA-F]{16}$', address_str):
                    continue

                address = int(address_str, 16)
                flags = parts[1]

                # 判断是5部分还是6部分格式
                if len(parts) == 5:
                    # 5部分格式: address flags section size name
                    section = parts[2]
                    size_str = parts[3]
                    name = parts[4]
                    symbol_type = ""
                elif len(parts) >= 6:
                    # 6部分格式: address flags type section size name
                    symbol_type = parts[2]
                    section = parts[3]
                    size_str = parts[4]
                    name = ' '.join(parts[5:])  # name可能包含空格
                else:
                    continue

                # 解析大小
                if size_str == '*ABS*':
                    size = 0
                else:
                    try:
                        size = int(size_str, 16)
                    except ValueError:
                        size = 0

                symbol = {
                    'address': address,
                    'address_str': address_str,
                    'flags': flags,
                    'type': symbol_type,
                    'section': section,
                    'size': size,
                    'size_str': size_str,
                    'name': name,
                    'line_num': line_num
                }
                symbols.append(symbol)

            except (ValueError, IndexError) as e:
                print(f"警告: 第{line_num}行解析失败: {line[:60]}...")
                continue

        print(f"成功解析 {len(symbols)} 个符号")
        return symbols

    def sort_by_address(self) -> List[Dict]:
        """按地址排序符号"""
        return sorted(self.symbols, key=lambda s: s['address'])

    def filter_by_section(self, section: str) -> List[Dict]:
        """按段过滤符号"""
        return [s for s in self.symbols if section in s['section']]

    def filter_by_flags(self, flag: str) -> List[Dict]:
        """按标志过滤符号"""
        return [s for s in self.symbols if flag in s['flags']]

    def filter_by_type(self, symbol_type: str) -> List[Dict]:
        """按类型过滤符号 (d=调试段, F=函数, O=对象等)"""
        return [s for s in self.symbols if symbol_type in s['type']]

    def search_symbols(self, pattern: str) -> List[Dict]:
        """搜索符号名"""
        regex = re.compile(pattern, re.IGNORECASE)
        return [s for s in self.symbols if regex.search(s['name'])]

    def get_functions(self) -> List[Dict]:
        """获取所有函数符号 (标志包含F)"""
        return self.filter_by_flags('F')

    def get_objects(self) -> List[Dict]:
        """获取所有对象符号 (标志包含O)"""
        return self.filter_by_flags('O')

    def get_global_symbols(self) -> List[Dict]:
        """获取全局符号 (标志包含g)"""
        return self.filter_by_flags('g')

    def get_local_symbols(self) -> List[Dict]:
        """获取本地符号 (标志包含l)"""
        return self.filter_by_flags('l')

    def analyze_memory_layout(self) -> Dict:
        """分析内存布局"""
        sections = {}
        for symbol in self.symbols:
            section = symbol['section']
            if section not in sections:
                sections[section] = {
                    'min_addr': symbol['address'],
                    'max_addr': symbol['address'],
                    'count': 0,
                    'symbol_size_sum': 0  # 符号大小总和
                }

            sections[section]['min_addr'] = min(sections[section]['min_addr'], symbol['address'])
            sections[section]['max_addr'] = max(sections[section]['max_addr'], symbol['address'])
            sections[section]['count'] += 1
            sections[section]['symbol_size_sum'] += symbol['size']

        # 计算每个段的实际大小（地址范围）
        for section, info in sections.items():
            if info['min_addr'] == info['max_addr']:
                # 如果最小和最大地址相同，使用符号大小总和
                info['actual_size'] = info['symbol_size_sum']
            else:
                # 使用地址范围计算实际大小
                info['actual_size'] = info['max_addr'] - info['min_addr']

        return sections

    def print_section_analysis(self):
        """Print section analysis results"""
        try:
            print("=" * 90)
            print("Avatar OS Symbol Table Section Analysis")
            print("=" * 90)

            print(f"\nTotal symbols: {len(self.symbols)}")

            # Section statistics
            sections = self.analyze_memory_layout()
            print(f"\nSection Statistics:")

            # Table header
            print(f"{'Section':<15} | {'Count':>8} | {'Start Address':>18} | {'End Address':>18} | {'Size':>12}")
            print("-" * 90)

            # Sort by actual size (largest first)
            sorted_sections = sorted(sections.items(), key=lambda x: x[1]['actual_size'], reverse=True)

            for section, info in sorted_sections:
                # Format size display
                size = info['actual_size']
                if size >= 1024 * 1024:
                    size_str = f"{size / (1024 * 1024):.1f} MB"
                elif size >= 1024:
                    size_str = f"{size / 1024:.1f} KB"
                else:
                    size_str = f"{size} bytes"

                print(f"{section:<15} | {info['count']:8} | "
                      f"0x{info['min_addr']:016x} | 0x{info['max_addr']:016x} | "
                      f"{size_str:>12}")

        except BrokenPipeError:
            # Gracefully handle broken pipe
            pass

    def print_section_symbols(self, section_name: str, sort_by: str = "address"):
        """Print symbols in a specific section"""
        # Filter symbols by section
        section_symbols = self.filter_by_section(section_name)

        if not section_symbols:
            try:
                print(f"No symbols found in section '{section_name}'")
            except BrokenPipeError:
                pass
            return

        try:
            print("=" * 100)
            print(f"Symbols in Section: {section_name}")
            if sort_by != "address":
                print(f"Sorted by: {sort_by}")
            print("=" * 100)
            print(f"\nFound {len(section_symbols)} symbols in section '{section_name}'")

            # Sort symbols based on sort_by parameter
            if sort_by == "name":
                sorted_symbols = sorted(section_symbols, key=lambda s: s['name'].lower())
            elif sort_by == "size":
                sorted_symbols = sorted(section_symbols, key=lambda s: s['size'], reverse=True)
            elif sort_by == "flags":
                sorted_symbols = sorted(section_symbols, key=lambda s: s['flags'])
            elif sort_by == "type":
                sorted_symbols = sorted(section_symbols, key=lambda s: s['type'])
            else:  # default: address
                sorted_symbols = sorted(section_symbols, key=lambda s: s['address'])

            # Table header
            print(f"\n{'Address':<18} | {'Flags':<8} | {'Type':<6} | {'Size':<12} | {'Symbol Name'}")
            print("-" * 100)

            for symbol in sorted_symbols:
                # Format size display
                if symbol['size'] > 0:
                    if symbol['size'] >= 1024:
                        size_str = f"{symbol['size'] / 1024:.1f} KB"
                    else:
                        size_str = f"{symbol['size']} bytes"
                else:
                    size_str = symbol['size_str']

                print(f"0x{symbol['address']:016x} | {symbol['flags']:<8} | {symbol['type']:<6} | "
                      f"{size_str:<12} | {symbol['name']}")

        except BrokenPipeError:
            # Gracefully handle broken pipe (e.g., when output is piped to head/less)
            pass

    def list_available_sections(self):
        """List all available sections"""
        sections = set(symbol['section'] for symbol in self.symbols)
        sections = sorted(sections)

        try:
            print("Available sections:")
            for i, section in enumerate(sections, 1):
                count = len(self.filter_by_section(section))
                print(f"  {i:2}. {section:<20} ({count} symbols)")
        except BrokenPipeError:
            # Gracefully handle broken pipe
            pass

        return sections

def main():
    """Main function - Section analysis and filtering"""
    import argparse

    parser = argparse.ArgumentParser(description='Avatar OS Symbol Table Analysis Tool')
    parser.add_argument('--file', '-f', default='build/dis.txt',
                       help='Path to dis.txt file (default: build/dis.txt)')
    parser.add_argument('--section', type=str,
                       help='Filter symbols by section (e.g., .text, .data, .bss)')
    parser.add_argument('--sort', '-s', type=str, choices=['address', 'name', 'size', 'flags', 'type'],
                       default='address', help='Sort symbols by: address (default), name, size, flags, or type')
    parser.add_argument('--list-sections', '-l', action='store_true',
                       help='List all available sections')

    args = parser.parse_args()

    # Read symbol table data
    if args.file != 'build/dis.txt':
        dis_file_path = args.file
    else:
        dis_file_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'build', 'dis.txt')

    symbol_lines = read_symbol_table_from_file(dis_file_path)
    if not symbol_lines:
        print(f"Cannot read symbol table data from {dis_file_path}")
        sys.exit(1)

    analyzer = SymbolAnalyzer(symbol_lines)

    # Execute based on arguments
    if args.list_sections:
        analyzer.list_available_sections()
    elif args.section:
        analyzer.print_section_symbols(args.section, args.sort)
    else:
        # Default: show section analysis
        analyzer.print_section_analysis()

if __name__ == '__main__':
    main()
