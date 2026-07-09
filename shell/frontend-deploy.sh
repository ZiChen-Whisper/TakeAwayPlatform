#!/bin/bash
# ============================================================
# 前端部署脚本
# 功能：将构建产物部署到生产目录
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FRONTEND_DIST="$PROJECT_DIR/frontend/dist"
DEPLOY_DIR="/opt/TakeAwayPlatform/web"

echo "=== 前端部署开始 ==="
echo "源目录: $FRONTEND_DIST"
echo "目标目录: $DEPLOY_DIR"

if [ ! -d "$FRONTEND_DIST" ]; then
    echo "错误: 前端构建产物不存在，请先运行 frontend-build.sh"
    exit 1
fi

sudo mkdir -p "$DEPLOY_DIR"
sudo cp -r "$FRONTEND_DIST"/* "$DEPLOY_DIR"/
sudo chmod -R 755 "$DEPLOY_DIR"

echo "=== 前端部署完成 ==="
echo "访问地址: http://localhost:9090/web/"
ls -lh "$DEPLOY_DIR/"
