#!/bin/bash

# Avatar OS Guest Files Setup Script
# 根据guest_manifests.c中的配置，将guest文件拷贝到host.img文件系统中

set -e  # 遇到错误立即退出

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

# 检查必要的工具
check_dependencies() {
    log_info "检查依赖工具..."

    # 检查是否有sudo权限
    if ! sudo -n true 2>/dev/null; then
        log_error "需要sudo权限来挂载文件系统"
        log_info "请确保当前用户有sudo权限，或者运行: sudo ./scripts/setup_guest_files.sh"
        exit 1
    fi

    log_success "依赖工具检查完成"
}

# 全局变量
MOUNT_POINT="/tmp/avatar_guest_mount_$$"
IMG_MOUNTED=false

# 检查源文件是否存在
check_source_files() {
    log_info "检查源文件..."
    
    local missing_files=()
    
    # Linux guest files
    if [[ ! -f "guest/linux/linux.bin" ]]; then
        missing_files+=("guest/linux/linux.bin")
    fi
    if [[ ! -f "guest/linux/linux.dtb" ]]; then
        missing_files+=("guest/linux/linux.dtb")
    fi
    if [[ ! -f "guest/linux/initrd.gz" ]]; then
        missing_files+=("guest/linux/initrd.gz")
    fi
    
    # NimbOS guest files
    if [[ ! -f "guest/nimbos/kernel/target/aarch64/debug/nimbos.bin" ]]; then
        missing_files+=("guest/nimbos/kernel/target/aarch64/debug/nimbos.bin")
    fi

    # TestOS guest files
    if [[ ! -f "guest/testos/build/kernel.bin" ]]; then
        missing_files+=("guest/testos/build/kernel.bin")
    fi
    
    if [[ ${#missing_files[@]} -gt 0 ]]; then
        log_warning "以下文件不存在，将跳过："
        for file in "${missing_files[@]}"; do
            log_warning "  - $file"
        done
    else
        log_success "所有源文件检查完成"
    fi
}

# 检查host.img是否存在
check_test_img() {
    if [[ ! -f "host.img" ]]; then
        log_error "host.img 文件不存在"
        log_info "请先创建 host.img 文件，或运行相应的构建命令"
        exit 1
    fi
    log_success "host.img 文件存在"
}

# 挂载镜像文件
mount_image() {
    log_info "挂载 host.img 到 $MOUNT_POINT..."

    # 创建挂载点
    sudo mkdir -p "$MOUNT_POINT"

    # 挂载镜像
    if sudo mount -o loop,rw host.img "$MOUNT_POINT"; then
        IMG_MOUNTED=true
        log_success "镜像挂载成功"
    else
        log_error "镜像挂载失败"
        exit 1
    fi
}

# 卸载镜像文件
unmount_image() {
    if [[ "$IMG_MOUNTED" == "true" ]]; then
        log_info "卸载镜像文件..."
        if sudo umount "$MOUNT_POINT" 2>/dev/null; then
            log_success "镜像卸载成功"
        else
            log_warning "镜像卸载失败，可能已经卸载"
        fi

        # 删除挂载点
        sudo rmdir "$MOUNT_POINT" 2>/dev/null || true
        IMG_MOUNTED=false
    fi
}

# 清理函数（在脚本退出时调用）
cleanup() {
    log_info "清理资源..."
    unmount_image
}

# 设置退出时的清理
trap cleanup EXIT

# 创建目录结构
create_directories() {
    log_info "创建目录结构..."

    # 创建主目录
    sudo mkdir -p "$MOUNT_POINT/guests"

    # 创建各个guest的子目录
    sudo mkdir -p "$MOUNT_POINT/guests/linux"
    sudo mkdir -p "$MOUNT_POINT/guests/nimbos"
    sudo mkdir -p "$MOUNT_POINT/guests/testos"

    log_success "目录结构创建完成"
}

# 拷贝文件的通用函数
copy_file() {
    local src_file="$1"
    local dst_path="$2"
    local file_desc="$3"

    if [[ -f "$src_file" ]]; then
        log_info "拷贝 $file_desc: $src_file -> $dst_path"

        # 获取文件大小用于进度显示
        local file_size=$(stat -c%s "$src_file")
        local file_size_mb=$((file_size / 1024 / 1024))

        if [[ $file_size_mb -gt 0 ]]; then
            log_info "文件大小: ${file_size_mb} MB"
        fi

        # 使用cp命令拷贝，并显示进度
        if sudo cp "$src_file" "$dst_path"; then
            log_success "✓ $file_desc 拷贝成功"

            # 验证文件大小
            local dst_size=$(stat -c%s "$dst_path")
            if [[ "$file_size" -eq "$dst_size" ]]; then
                log_success "✓ 文件大小验证通过 ($file_size bytes)"
            else
                log_error "✗ 文件大小不匹配: 源文件 $file_size bytes, 目标文件 $dst_size bytes"
                return 1
            fi
        else
            log_error "✗ $file_desc 拷贝失败"
            return 1
        fi
    else
        log_warning "跳过不存在的文件: $src_file"
    fi
}

# 拷贝Linux guest文件
copy_linux_files() {
    log_info "拷贝 Linux Guest 文件..."

    copy_file "guest/linux/linux.bin" "$MOUNT_POINT/guests/linux/linux.bin" "Linux内核"
    copy_file "guest/linux/linux.dtb" "$MOUNT_POINT/guests/linux/linux.dtb" "Linux设备树"
    copy_file "guest/linux/initrd.gz" "$MOUNT_POINT/guests/linux/initrd.gz" "Linux初始化文件系统"

    log_success "Linux Guest 文件处理完成"
}

# 拷贝NimbOS guest文件
copy_nimbos_files() {
    log_info "拷贝 NimbOS Guest 文件..."

    copy_file "guest/nimbos/kernel/target/aarch64/debug/nimbos.bin" "$MOUNT_POINT/guests/nimbos/nimbos.bin" "NimbOS内核"

    log_success "NimbOS Guest 文件处理完成"
}

# 拷贝TestOS guest文件
copy_testos_files() {
    log_info "拷贝 TestOS Guest 文件..."

    copy_file "guest/testos/build/kernel.bin" "$MOUNT_POINT/guests/testos/testos.bin" "TestOS内核"

    log_success "TestOS Guest 文件处理完成"
}

# 验证拷贝结果
verify_copy() {
    log_info "验证拷贝结果..."

    log_info "挂载点中的文件列表："
    log_info "=== /guests/ ==="
    ls -la "$MOUNT_POINT/guests/" || true

    log_info "=== /guests/linux/ ==="
    ls -la "$MOUNT_POINT/guests/linux/" || true

    log_info "=== /guests/nimbos/ ==="
    ls -la "$MOUNT_POINT/guests/nimbos/" || true

    log_info "=== /guests/testos/ ==="
    ls -la "$MOUNT_POINT/guests/testos/" || true

    # 计算总大小
    log_info "=== 磁盘使用情况 ==="
    df -h "$MOUNT_POINT" || true

    log_success "验证完成"
}

# 主函数
main() {
    log_info "开始设置 Avatar OS Guest 文件..."
    log_info "工作目录: $(pwd)"
    log_info "挂载点: $MOUNT_POINT"

    # 检查依赖
    check_dependencies

    # 检查文件
    check_test_img
    check_source_files

    # 挂载镜像
    mount_image

    # 创建目录结构
    create_directories

    # 拷贝文件
    copy_linux_files
    copy_nimbos_files
    copy_testos_files

    # 同步文件系统
    log_info "同步文件系统..."
    sync

    # 验证结果
    verify_copy

    log_success "Guest 文件设置完成！"
    log_info "现在可以使用新的guest管理系统了"

    # 卸载将在cleanup函数中自动执行
}

# 运行主函数
main "$@"
