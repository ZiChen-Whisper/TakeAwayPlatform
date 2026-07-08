#!/bin/bash
# ============================================================
# TakeAwayPlatform 清理脚本
# 功能: 清理构建产物、日志文件
# 用法: ./clean.sh              # 清理 build 目录
#       ./clean.sh --all        # 清理 build + 日志 + 临时文件
#       ./clean.sh --logs       # 只清理日志
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

CLEAN_ALL=false
CLEAN_LOGS=false

for arg in "$@"; do
    case "$arg" in
        --all)
            CLEAN_ALL=true ;;
        --logs)
            CLEAN_LOGS=true ;;
        -h|--help)
            echo "用法: ./clean.sh [选项]"
            echo ""
            echo "选项:"
            echo "  (无参数)      清理 build 目录（保留构建缓存）"
            echo "  --all         彻底清理（build + 日志 + CMake 缓存）"
            echo "  --logs        只清理日志文件"
            echo "  -h, --help    显示帮助"
            exit 0
            ;;
    esac
done

echo "============================================"
echo "  TakeAwayPlatform 清理"
echo "============================================"

# 停止服务
if pgrep -f "TakeAwayPlatform" > /dev/null 2>&1; then
    echo "检测到正在运行的服务，正在停止..."
    pkill -f "TakeAwayPlatform" 2>/dev/null || true
    sleep 1
    echo "✓ 服务已停止"
fi

if [ "$CLEAN_LOGS" = true ]; then
    echo ""
    echo "清理日志文件..."
    rm -f /tmp/takeaway_platform.log
    if [ -d "/opt/TakeAwayPlatform/logs" ]; then
        sudo rm -f /opt/TakeAwayPlatform/logs/*.log 2>/dev/null || true
    fi
    echo "✓ 日志已清理"
    exit 0
fi

if [ "$CLEAN_ALL" = true ]; then
    echo ""
    echo "彻底清理 build 目录（包括 CMake 缓存）..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo "✓ build/ 目录已删除"
    fi
    
    # 清理日志
    rm -f /tmp/takeaway_platform.log
    if [ -d "/opt/TakeAwayPlatform/logs" ]; then
        sudo rm -f /opt/TakeAwayPlatform/logs/*.log 2>/dev/null || true
    fi
    
    # 清理 core dump
    rm -f "$PROJECT_ROOT"/core* 2>/dev/null || true
    
    echo "✓ 彻底清理完成"
    echo ""
    echo "重新编译:"
    echo "  $SCRIPT_DIR/build.sh"
else
    echo ""
    echo "清理 build 目录（保留 CMake 缓存）..."
    if [ -d "$BUILD_DIR" ]; then
        # 只删除编译产物，保留 CMake 缓存
        find "$BUILD_DIR" -name "*.o" -delete 2>/dev/null || true
        find "$BUILD_DIR" -name "*.a" -delete 2>/dev/null || true
        rm -f "$BUILD_DIR/TakeAwayPlatform" 2>/dev/null || true
        echo "✓ 编译产物已清理（CMake 缓存已保留）"
    else
        echo "build/ 目录不存在，无需清理"
    fi
fi
