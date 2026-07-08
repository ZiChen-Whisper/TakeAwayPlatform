#!/bin/bash
# ============================================================
# TakeAwayPlatform 重启脚本
# 功能: 停止旧服务 → 重新编译(可选) → 后台启动
# 用法: ./restart.sh              # 停止+编译+启动
#       ./restart.sh --no-build   # 停止+启动（不编译）
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_BUILD=false

for arg in "$@"; do
    case "$arg" in
        --no-build)
            SKIP_BUILD=true ;;
        -h|--help)
            echo "用法: ./restart.sh [选项]"
            echo ""
            echo "选项:"
            echo "  --no-build    跳过编译，直接启动"
            echo "  -h, --help    显示帮助"
            exit 0
            ;;
    esac
done

echo "============================================"
echo "  TakeAwayPlatform 重启"
echo "============================================"

# ---------- 停止旧服务 ----------
echo ""
echo "[1/3] 停止旧服务..."
STOPPED=false
if pgrep -f "TakeAwayPlatform" > /dev/null 2>&1; then
    pkill -f "TakeAwayPlatform" 2>/dev/null || true
    STOPPED=true
    
    # 等待进程完全退出（最多5秒）
    for i in $(seq 1 10); do
        if ! pgrep -f "TakeAwayPlatform" > /dev/null 2>&1; then
            break
        fi
        sleep 0.5
    done
    
    if pgrep -f "TakeAwayPlatform" > /dev/null 2>&1; then
        echo "⚠ 进程未响应，强制杀死..."
        pkill -9 -f "TakeAwayPlatform" 2>/dev/null || true
        sleep 1
    fi
    echo "✓ 旧服务已停止"
else
    echo "  没有运行中的服务"
fi

# ---------- 编译 ----------
if [ "$SKIP_BUILD" = false ]; then
    echo ""
    echo "[2/3] 编译项目..."
    bash "$SCRIPT_DIR/build.sh"
else
    echo ""
    echo "[2/3] 跳过编译（--no-build）"
fi

# ---------- 启动 ----------
echo ""
echo "[3/3] 后台启动服务..."
bash "$SCRIPT_DIR/run.sh" --no-build --bg
