#!/bin/bash
# ============================================================
# 前端构建脚本
# 功能：安装依赖并构建前端项目
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FRONTEND_DIR="$PROJECT_DIR/frontend"

echo "=== 前端构建开始 ==="
echo "前端目录: $FRONTEND_DIR"

cd "$FRONTEND_DIR"

echo "1/2 安装依赖..."
npm install --legacy-peer-deps

echo "2/2 构建项目..."
npm run build

echo "=== 前端构建完成 ==="
echo "构建产物: $FRONTEND_DIR/dist/"
ls -lh "$FRONTEND_DIR/dist/" | head -10
