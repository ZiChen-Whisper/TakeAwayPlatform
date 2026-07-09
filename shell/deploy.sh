#!/bin/bash
# ============================================================
# TakeAwayPlatform 部署脚本
# 功能: 编译并部署到 /opt/TakeAwayPlatform/
# 用法: ./deploy.sh               # 编译并部署
#       ./deploy.sh --no-build    # 跳过编译直接部署
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

DEPLOY_ROOT="/opt/TakeAwayPlatform"
DEPLOY_BIN="$DEPLOY_ROOT/bin"
DEPLOY_CONFIG="$DEPLOY_ROOT/config"
DEPLOY_LOGS="$DEPLOY_ROOT/logs"
DEPLOY_LIB="$DEPLOY_ROOT/bin/lib"

SKIP_BUILD=false

# 解析参数
for arg in "$@"; do
    case "$arg" in
        --no-build)
            SKIP_BUILD=true ;;
        -h|--help)
            echo "用法: ./deploy.sh [选项]"
            echo ""
            echo "选项:"
            echo "  --no-build    跳过编译，直接部署"
            echo "  -h, --help    显示帮助"
            exit 0
            ;;
    esac
done

echo "============================================"
echo "  TakeAwayPlatform 部署"
echo "  项目目录: $PROJECT_ROOT"
echo "  部署目录: $DEPLOY_ROOT"
echo "============================================"

# ---------- 编译 ----------
if [ "$SKIP_BUILD" = false ]; then
    echo ""
    echo "[1/4] 编译项目..."
    bash "$SCRIPT_DIR/build.sh"
else
    echo ""
    echo "[1/4] 跳过编译（--no-build）"
fi

EXECUTABLE="$BUILD_DIR/TakeAwayPlatform"
if [ ! -f "$EXECUTABLE" ]; then
    echo "错误: 可执行文件不存在: $EXECUTABLE"
    exit 1
fi

# ---------- 创建部署目录 ----------
echo ""
echo "[2/4] 创建部署目录..."
sudo mkdir -p "$DEPLOY_BIN" "$DEPLOY_CONFIG" "$DEPLOY_LOGS" "$DEPLOY_LIB"

# ---------- 复制可执行文件 ----------
echo ""
echo "[3/4] 复制文件..."

# 停止旧服务（如果在运行）
if pgrep -f "TakeAwayPlatform" > /dev/null 2>&1; then
    echo "检测到正在运行的服务，正在停止..."
    sudo pkill -f "TakeAwayPlatform" 2>/dev/null || true
    sleep 1
fi

# 复制可执行文件
sudo cp "$EXECUTABLE" "$DEPLOY_BIN/"
echo "  ✓ 可执行文件 → $DEPLOY_BIN/TakeAwayPlatform"

# 复制配置文件
sudo cp "$PROJECT_ROOT/config/config.json" "$DEPLOY_CONFIG/"
echo "  ✓ 配置文件   → $DEPLOY_CONFIG/config.json"

# 检查并复制库文件（如果 build 目录下有编译出的库）
if ls "$BUILD_DIR"/*.so 2>/dev/null | head -1 | grep -q .; then
    sudo cp "$BUILD_DIR"/*.so "$DEPLOY_LIB/" 2>/dev/null || true
    echo "  ✓ 动态库     → $DEPLOY_LIB/"
fi

echo ""
echo "[4/4] 部署完成！"
echo ""
echo "============================================"
echo "  部署摘要"
echo "  可执行文件: $DEPLOY_BIN/TakeAwayPlatform"
echo "  配置文件:   $DEPLOY_CONFIG/config.json"
echo "  日志目录:   $DEPLOY_LOGS/"
echo "============================================"
echo ""
echo "启动服务:"
echo "  $DEPLOY_BIN/TakeAwayPlatform $DEPLOY_CONFIG/config.json"
echo ""
echo "或使用项目脚本后台运行:"
echo "  $SCRIPT_DIR/run.sh --no-build --bg $DEPLOY_CONFIG/config.json"
echo ""

# ---------- 部署前端 ----------
echo ""
echo "----------------------------------------"
echo "  前端部署"
echo "----------------------------------------"
if [ -f "$SCRIPT_DIR/frontend-deploy.sh" ]; then
    bash "$SCRIPT_DIR/frontend-deploy.sh"
else
    echo -e "  前端部署脚本不存在，跳过"
fi
