#!/bin/bash
#
# Copyright (c) 2024 Avatar Project
#
# Licensed under the MIT License.
# See LICENSE file in the project root for full license information.
#
# @file create_host_img.sh
# @brief Shell script: create_host_img.sh
# @author Avatar Project Team
# @date 2024
#


# 创建host.img文件以容纳所有guest文件

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 配置
IMG_SIZE="256M"
IMG_NAME="host.img"
FORCE_YES=false

# 解析命令行参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -y|--yes)
                FORCE_YES=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# 显示帮助信息
show_help() {
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -y, --yes     自动确认所有提示，删除现有文件而不备份"
    echo "  -h, --help    显示此帮助信息"
    echo ""
    echo "描述:"
    echo "  创建 $IMG_SIZE 大小的 $IMG_NAME 镜像文件"
    echo "  如果文件已存在，默认会询问用户是删除还是备份"
    echo "  使用 -y 参数可以自动删除现有文件，跳过所有交互"
}

# 用户交互函数
ask_user_choice() {
    local prompt="$1"
    local default_choice="$2"  # 可选的默认选择
    local choice

    # 如果启用了强制确认模式，直接返回默认选择
    if [[ "$FORCE_YES" == true ]]; then
        if [[ "$default_choice" == "y" ]]; then
            return 0
        else
            return 1
        fi
    fi

    while true; do
        echo -e "${YELLOW}[QUESTION]${NC} $prompt (y/n): "
        read -r choice
        case "$choice" in
            [Yy]|[Yy][Ee][Ss])
                return 0
                ;;
            [Nn]|[Nn][Oo])
                return 1
                ;;
            *)
                log_warning "请输入 y 或 n"
                ;;
        esac
    done
}

# 处理现有镜像文件
handle_existing_image() {
    if [[ ! -f "$IMG_NAME" ]]; then
        log_info "未发现现有的 $IMG_NAME，将创建新镜像"
        return 0
    fi

    log_warning "发现现有的 $IMG_NAME 文件"

    if [[ "$FORCE_YES" == true ]]; then
        log_info "使用 -y 参数，自动删除现有的 $IMG_NAME..."
        rm -f "$IMG_NAME"
        log_success "已删除现有的 $IMG_NAME"
        return 0
    fi

    if ask_user_choice "是否删除现有的 $IMG_NAME 文件？" "y"; then
        log_info "删除现有的 $IMG_NAME..."
        rm -f "$IMG_NAME"
        log_success "已删除现有的 $IMG_NAME"
    else
        log_info "备份现有的 $IMG_NAME 到 ${IMG_NAME}.backup"

        # 如果备份文件已存在，询问是否覆盖
        if [[ -f "${IMG_NAME}.backup" ]]; then
            if ask_user_choice "备份文件 ${IMG_NAME}.backup 已存在，是否覆盖？" "y"; then
                cp "$IMG_NAME" "${IMG_NAME}.backup"
                log_success "已覆盖备份文件 ${IMG_NAME}.backup"
            else
                # 创建带时间戳的备份文件
                local timestamp=$(date +"%Y%m%d_%H%M%S")
                local backup_name="${IMG_NAME}.backup_${timestamp}"
                cp "$IMG_NAME" "$backup_name"
                log_success "已备份到 $backup_name"
            fi
        else
            cp "$IMG_NAME" "${IMG_NAME}.backup"
            log_success "已备份到 ${IMG_NAME}.backup"
        fi

        # 删除原文件以便创建新的
        rm -f "$IMG_NAME"
    fi
}

main() {
    # 解析命令行参数
    parse_args "$@"

    log_info "开始创建 $IMG_NAME ($IMG_SIZE)..."

    # 处理现有镜像文件
    handle_existing_image

    # 创建新的镜像文件
    log_info "创建 $IMG_SIZE 的镜像文件..."
    dd if=/dev/zero of="$IMG_NAME" bs=1M count=256 status=progress

    # 格式化为FAT32
    log_info "格式化为FAT32文件系统..."
    mkfs.fat -F 32 -n "AVATAR_FS" "$IMG_NAME"

    # 验证
    log_info "验证新创建的镜像..."
    mdir -i "$IMG_NAME" ::/ || {
        log_error "镜像验证失败"
        exit 1
    }

    log_success "新的 $IMG_NAME 创建完成！"
    log_info "镜像大小: $IMG_SIZE"
    log_info "文件系统: FAT32"
    log_info "卷标: AVATAR_FS"

    # 显示可用空间
    log_info "可用空间信息："
    mdir -i "$IMG_NAME" ::/
}

main "$@"
