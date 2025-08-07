#!/bin/bash
# ============================================================================
# Avatar Hypervisor Build Script
# ============================================================================

set -e

# 默认配置
CONFIG=${CONFIG:-debug}
PLATFORM=${PLATFORM:-qemu-virt}
JOBS=${JOBS:-$(nproc)}
VERBOSE=${VERBOSE:-0}

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# 显示帮助
show_help() {
    cat << EOF
Avatar Hypervisor Build Script

Usage: $0 [OPTIONS] [TARGET]

OPTIONS:
    -c, --config CONFIG     Build configuration (debug|release|profile)
    -p, --platform PLATFORM Target platform (qemu-virt|qemu-raspi3|hardware)
    -j, --jobs JOBS         Number of parallel jobs (default: $(nproc))
    -v, --verbose           Verbose output
    -h, --help              Show this help

TARGETS:
    all                     Build kernel binary (default)
    clean                   Clean build files
    run                     Build and run in QEMU
    debug                   Build and run in QEMU debug mode
    test                    Run tests
    analyze                 Run static analysis
    format                  Format source code

EXAMPLES:
    $0                      # Build with default settings
    $0 -c release run       # Build release and run
    $0 -p qemu-raspi3 -j 8  # Build for Raspberry Pi 3 with 8 jobs
    $0 clean all            # Clean and rebuild
EOF
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--config)
            CONFIG="$2"
            shift 2
            ;;
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            TARGET="$1"
            shift
            ;;
    esac
done

# 默认目标
TARGET=${TARGET:-all}

# 验证配置
case $CONFIG in
    debug|release|profile)
        ;;
    *)
        log_error "Invalid config: $CONFIG"
        exit 1
        ;;
esac

case $PLATFORM in
    qemu-virt|qemu-raspi3|hardware)
        ;;
    *)
        log_error "Invalid platform: $PLATFORM"
        exit 1
        ;;
esac

# 检查工具链
check_toolchain() {
    local prefix="aarch64-linux-musl-"
    if ! command -v "${prefix}gcc" &> /dev/null; then
        log_error "Toolchain not found: ${prefix}gcc"
        log_info "Please install aarch64 cross-compilation toolchain"
        exit 1
    fi
    log_info "Toolchain: $(${prefix}gcc --version | head -n1)"
}

# 显示构建信息
show_build_info() {
    log_info "Build Configuration:"
    echo "  Config:   $CONFIG"
    echo "  Platform: $PLATFORM"
    echo "  Jobs:     $JOBS"
    echo "  Target:   $TARGET"
    echo "  Verbose:  $VERBOSE"
    echo ""
}

# 主构建函数
main() {
    log_info "Avatar Hypervisor Build System"
    echo ""
    
    check_toolchain
    show_build_info
    
    # 设置环境变量
    export CONFIG PLATFORM VERBOSE
    
    # 执行构建
    log_info "Starting build..."
    if make -j$JOBS $TARGET; then
        log_success "Build completed successfully!"
    else
        log_error "Build failed!"
        exit 1
    fi
}

# 运行主函数
main "$@"
