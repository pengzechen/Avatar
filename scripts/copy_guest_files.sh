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
AVAILABLE_GUESTS=()

# 显示帮助信息
show_help() {
    echo "Avatar OS Guest Files Setup Script"
    echo "=================================="
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help    显示此帮助信息并退出"
    echo ""
    echo "描述:"
    echo "  此脚本用于将 guest 操作系统文件拷贝到 host.img 文件系统中。"
    echo "  脚本会自动检测可用的 guest 文件，并只处理完整的 guest。"
    echo ""
    echo "支持的 Guest 操作系统:"
    echo "  • Linux Guest"
    echo "    - 需要文件: linux.bin + linux.dtb + initrd.gz"
    echo "    - 路径: guest/linux/"
    echo "    - 说明: 必须三个文件都存在才会被处理"
    echo ""
    echo "  • NimbOS Guest"
    echo "    - 需要文件: nimbos.bin"
    echo "    - 路径: guest/nimbos/kernel/target/aarch64/debug/"
    echo "    - 说明: 只需要 bin 文件即可"
    echo ""
    echo "  • TestOS Guest"
    echo "    - 需要文件: kernel.bin"
    echo "    - 路径: guest/testos/build/"
    echo "    - 说明: 只需要 bin 文件即可"
    echo ""
    echo "工作流程:"
    echo "  1. 检查 host.img 文件是否存在"
    echo "  2. 检查各个 guest 的文件完整性"
    echo "  3. 至少需要一个完整的 guest，否则退出"
    echo "  4. 挂载 host.img 到临时目录"
    echo "  5. 创建 /guests/ 目录结构"
    echo "  6. 拷贝可用的 guest 文件"
    echo "  7. 验证拷贝结果并卸载"
    echo ""
    echo "注意事项:"
    echo "  • 需要 sudo 权限来挂载文件系统"
    echo "  • 如果 host.img 中已有文件，会被直接覆盖"
    echo "  • 脚本会自动清理临时挂载点"
    echo "  • 支持任意组合的 guest（1个、2个或3个）"
    echo ""
    echo "示例:"
    echo "  $0              # 处理所有可用的 guest 文件"
    echo "  $0 -h           # 显示此帮助信息"
    echo ""
}

# 解析命令行参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                log_info "使用 $0 -h 查看帮助信息"
                exit 1
                ;;
        esac
    done
}

# 检查单个guest的文件完整性
check_guest_files() {
    local guest_name="$1"
    local guest_path="$2"
    local required_files=("${@:3}")

    local missing_files=()
    local all_exist=true

    for file in "${required_files[@]}"; do
        if [[ ! -f "$file" ]]; then
            missing_files+=("$file")
            all_exist=false
        fi
    done

    if [[ "$all_exist" == true ]]; then
        log_success "✓ $guest_name guest 文件完整"
        return 0
    else
        log_warning "✗ $guest_name guest 文件不完整，缺少："
        for file in "${missing_files[@]}"; do
            log_warning "    - $file"
        done
        return 1
    fi
}

# 检查源文件是否存在
check_source_files() {
    log_info "检查源文件..."

    local available_guests=()

    # 检查 Linux guest (需要三个文件)
    if check_guest_files "Linux" "guest/linux" \
        "guest/linux/linux.bin" \
        "guest/linux/linux.dtb" \
        "guest/linux/initrd.gz"; then
        available_guests+=("linux")
    fi

    # 检查 NimbOS guest (只需要bin文件)
    if check_guest_files "NimbOS" "guest/nimbos" \
        "guest/nimbos/kernel/target/aarch64/debug/nimbos.bin"; then
        available_guests+=("nimbos")
    fi

    # 检查 TestOS guest (只需要bin文件)
    if check_guest_files "TestOS" "guest/testos" \
        "guest/testos/build/kernel.bin"; then
        available_guests+=("testos")
    fi

    # 检查是否至少有一个guest可用
    if [[ ${#available_guests[@]} -eq 0 ]]; then
        log_error "没有找到任何可用的guest文件！"
        log_error "至少需要以下其中一个guest："
        log_error "  - Linux: 需要 linux.bin + linux.dtb + initrd.gz"
        log_error "  - NimbOS: 需要 nimbos.bin"
        log_error "  - TestOS: 需要 kernel.bin"
        exit 1
    fi

    log_success "找到 ${#available_guests[@]} 个可用的guest: ${available_guests[*]}"

    # 将可用的guest列表存储到全局变量
    AVAILABLE_GUESTS=("${available_guests[@]}")
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

    # 只为可用的guest创建目录
    for guest in "${AVAILABLE_GUESTS[@]}"; do
        log_info "创建 $guest guest 目录..."
        sudo mkdir -p "$MOUNT_POINT/guests/$guest"
    done

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

# 根据可用的guest拷贝文件
copy_guest_files() {
    log_info "开始拷贝可用的guest文件..."

    for guest in "${AVAILABLE_GUESTS[@]}"; do
        case "$guest" in
            "linux")
                copy_linux_files
                ;;
            "nimbos")
                copy_nimbos_files
                ;;
            "testos")
                copy_testos_files
                ;;
            *)
                log_warning "未知的guest类型: $guest"
                ;;
        esac
    done

    log_success "所有可用guest文件拷贝完成"
}

# 验证拷贝结果
verify_copy() {
    log_info "验证拷贝结果..."

    log_info "挂载点中的文件列表："
    log_info "=== /guests/ ==="
    ls -la "$MOUNT_POINT/guests/" || true

    # 只验证可用的guest目录
    for guest in "${AVAILABLE_GUESTS[@]}"; do
        log_info "=== /guests/$guest/ ==="
        ls -la "$MOUNT_POINT/guests/$guest/" || true
    done

    # 计算总大小
    log_info "=== 磁盘使用情况 ==="
    df -h "$MOUNT_POINT" || true

    log_success "验证完成"
}

# 主函数
main() {
    # 解析命令行参数
    parse_args "$@"

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
    copy_guest_files

    # 同步文件系统
    log_info "同步文件系统..."
    sync

    # 验证结果
    verify_copy

    log_success "Guest 文件复制完成！"
}

# 运行主函数
main "$@"
