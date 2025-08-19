#!/usr/bin/env python3
"""
Avatar VMM Dependency Checker
检查源文件之间的依赖关系，生成依赖图
"""

import os
import re
import sys
import argparse
from pathlib import Path
from collections import defaultdict, deque

class DependencyAnalyzer:
    def __init__(self, src_dirs, include_dirs):
        self.src_dirs = src_dirs
        self.include_dirs = include_dirs
        self.dependencies = defaultdict(set)
        self.reverse_deps = defaultdict(set)
        self.files = {}
        
    def find_source_files(self):
        """查找所有源文件"""
        for src_dir in self.src_dirs:
            if not os.path.exists(src_dir):
                continue

            for root, dirs, files in os.walk(src_dir):
                # 跳过clib目录和guest子目录（除了需要的文件）
                if 'clib' in root or any(subdir in root for subdir in ['guest/linux', 'guest/nimbos', 'guest/testos']):
                    continue

                for file in files:
                    if file.endswith(('.c', '.h', '.S')):
                        full_path = os.path.join(root, file)
                        rel_path = os.path.relpath(full_path)

                        # 对于guest目录，只包含指定的文件
                        if 'guest' in root:
                            if file in ['test_guest.S', 'guest_manifests.c', 'guest_manifest.h']:
                                self.files[file] = rel_path
                        # 对于app目录，排除syscall.S
                        elif 'app' in root and file == 'syscall.S':
                            continue
                        else:
                            self.files[file] = rel_path
                        
    def parse_includes(self, file_path):
        """解析文件中的#include指令"""
        includes = set()
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    line = line.strip()
                    # 匹配 #include "file.h" 和 #include <file.h>
                    match = re.match(r'#include\s*[<"]([^>"]+)[>"]', line)
                    if match:
                        includes.add(match.group(1))
        except Exception as e:
            print(f"Warning: Could not parse {file_path}: {e}")
        return includes
        
    def resolve_include(self, include_name):
        """解析include文件的实际路径"""
        # 首先检查是否是相对路径
        if include_name in self.files:
            return self.files[include_name]
            
        # 检查include目录
        for include_dir in self.include_dirs:
            full_path = os.path.join(include_dir, include_name)
            if os.path.exists(full_path):
                return os.path.relpath(full_path)
                
        # 检查系统头文件（跳过）
        if include_name.startswith(('sys/', 'linux/', 'asm/')):
            return None
            
        return None
        
    def build_dependency_graph(self):
        """构建依赖关系图"""
        self.find_source_files()
        
        for file_name, file_path in self.files.items():
            if not file_path.endswith(('.c', '.S')):
                continue
                
            includes = self.parse_includes(file_path)
            for include in includes:
                resolved = self.resolve_include(include)
                if resolved and resolved != file_path:
                    self.dependencies[file_path].add(resolved)
                    self.reverse_deps[resolved].add(file_path)
                    
    def find_circular_dependencies(self):
        """查找循环依赖"""
        def dfs(node, path, visited, rec_stack):
            visited.add(node)
            rec_stack.add(node)
            path.append(node)

            for neighbor in self.dependencies[node]:
                if neighbor not in visited:
                    cycle = dfs(neighbor, path, visited, rec_stack)
                    if cycle:
                        return cycle
                elif neighbor in rec_stack:
                    # 找到循环
                    cycle_start = path.index(neighbor)
                    return path[cycle_start:] + [neighbor]

            path.pop()
            rec_stack.remove(node)
            return None

        visited = set()
        # 创建节点列表的副本以避免运行时修改
        nodes = list(self.dependencies.keys())
        for node in nodes:
            if node not in visited:
                cycle = dfs(node, [], visited, set())
                if cycle:
                    return cycle
        return None
        
    def get_build_order(self):
        """获取构建顺序（拓扑排序）"""
        in_degree = defaultdict(int)
        
        # 计算入度
        for node in self.dependencies:
            if node not in in_degree:
                in_degree[node] = 0
            for dep in self.dependencies[node]:
                in_degree[dep] += 1
                
        # 拓扑排序
        queue = deque([node for node, degree in in_degree.items() if degree == 0])
        result = []
        
        while queue:
            node = queue.popleft()
            result.append(node)
            
            for neighbor in self.dependencies[node]:
                in_degree[neighbor] -= 1
                if in_degree[neighbor] == 0:
                    queue.append(neighbor)
                    
        return result
        
    def generate_dot_graph(self, output_file):
        """生成Graphviz DOT格式的依赖图"""
        with open(output_file, 'w') as f:
            f.write("digraph dependencies {\n")
            f.write("  rankdir=TB;\n")
            f.write("  node [shape=box];\n")
            
            # 节点
            for file_path in self.files.values():
                label = os.path.basename(file_path)
                color = "lightblue" if file_path.endswith('.h') else "lightgreen"
                f.write(f'  "{file_path}" [label="{label}" fillcolor="{color}" style=filled];\n')
                
            # 边
            for source, deps in self.dependencies.items():
                for dep in deps:
                    f.write(f'  "{source}" -> "{dep}";\n')
                    
            f.write("}\n")
            
    def print_statistics(self):
        """打印统计信息"""
        print(f"Source files: {len([f for f in self.files.values() if f.endswith(('.c', '.S'))])}")
        print(f"Header files: {len([f for f in self.files.values() if f.endswith('.h')])}")
        print(f"Dependencies: {sum(len(deps) for deps in self.dependencies.values())}")
        
        # 查找最复杂的文件
        max_deps = max((len(deps), file) for file, deps in self.dependencies.items())
        print(f"Most dependencies: {max_deps[1]} ({max_deps[0]} deps)")
        
        max_reverse = max((len(deps), file) for file, deps in self.reverse_deps.items())
        print(f"Most dependents: {max_reverse[1]} ({max_reverse[0]} dependents)")

def main():
    parser = argparse.ArgumentParser(description='Analyze Avatar VMM dependencies')
    parser.add_argument('--dot', help='Generate DOT graph file')
    parser.add_argument('--check-cycles', action='store_true', help='Check for circular dependencies')
    parser.add_argument('--build-order', action='store_true', help='Show build order')
    args = parser.parse_args()
    
    # 配置目录
    src_dirs = ['.', 'boot', 'exception', 'io', 'mem', 'timer', 'task',
                'process', 'spinlock', 'vmm', 'lib', 'fs', 'syscall', 'guest']
    include_dirs = ['include', 'guest']
    
    analyzer = DependencyAnalyzer(src_dirs, include_dirs)
    analyzer.build_dependency_graph()
    
    print("Avatar VMM Dependency Analysis")
    print("=" * 40)
    analyzer.print_statistics()
    
    if args.check_cycles:
        print("\nChecking for circular dependencies...")
        cycle = analyzer.find_circular_dependencies()
        if cycle:
            print(f"Circular dependency found: {' -> '.join(cycle)}")
        else:
            print("No circular dependencies found.")
            
    if args.build_order:
        print("\nBuild order:")
        order = analyzer.get_build_order()
        for i, file in enumerate(order, 1):
            print(f"{i:3d}. {file}")
            
    if args.dot:
        analyzer.generate_dot_graph(args.dot)
        print(f"\nDOT graph saved to {args.dot}")
        print("Generate image with: dot -Tpng deps.dot -o deps.png")

if __name__ == '__main__':
    main()
