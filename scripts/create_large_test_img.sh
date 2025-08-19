#!/bin/bash

# 创建更大的test.img文件以容纳所有guest文件

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
IMG_SIZE="256M"  # 256MB应该足够了
IMG_NAME="host.img"

main() {
    log_info "创建新的 $IMG_NAME ($IMG_SIZE)..."
    
    # 备份现有的host.img
    if [[ -f "$IMG_NAME" ]]; then
        log_info "备份现有的 $IMG_NAME 到 ${IMG_NAME}.backup"
        cp "$IMG_NAME" "${IMG_NAME}.backup"
    fi
    
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
