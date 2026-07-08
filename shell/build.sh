#!/bin/bash
# ============================================================
# TakeAwayPlatform 构建脚本
# 用法: ./build.sh [debug|release] [-j<N>]
# 示例: ./build.sh              # 默认 Release 构建
#       ./build.sh debug        # Debug 构建
#       ./build.sh -j4          # 4 线程并行编译
# ============================================================
set -e  # 遇到错误立即退出

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# 默认参数
BUILD_TYPE="Release"
MAKE_FLAGS="-j$(nproc)"

# 解析参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        debug|Debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        release|Release)
            BUILD_TYPE="Release"
            shift
            ;;
        -j*)
            MAKE_FLAGS="$1"
            shift
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: ./build.sh [debug|release] [-j<N>]"
            exit 1
            ;;
    esac
done

echo "============================================"
echo "  TakeAwayPlatform 项目构建"
echo "  项目目录: $PROJECT_ROOT"
echo "  构建目录: $BUILD_DIR"
echo "  构建类型: $BUILD_TYPE"
echo "  编译线程: $MAKE_FLAGS"
echo "============================================"

# 创建 build 目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "[1/3] 运行 CMake 配置..."
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="/opt/TakeAwayPlatform"

echo ""
echo "[2/3] 编译项目..."
make $MAKE_FLAGS

echo ""
echo "[3/3] 构建完成！"
echo "可执行文件: $BUILD_DIR/TakeAwayPlatform"
echo ""
echo "接下来可以运行:"
echo "  cd $BUILD_DIR && ./TakeAwayPlatform $PROJECT_ROOT/config/config.json"
echo "或使用运行脚本:"
echo "  $SCRIPT_DIR/run.sh"
