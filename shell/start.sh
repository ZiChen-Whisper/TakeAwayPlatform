#!/bin/bash
# ============================================================
# TakeAwayPlatform 一键启动脚本
# 功能: 同时启动后端服务和前端开发服务器
# 用法: ./start.sh                    # 启动全部（后端+前端）
#       ./start.sh --backend-only     # 仅启动后端
#       ./start.sh --frontend-only    # 仅启动前端
#       ./start.sh --no-build         # 跳过编译
#       ./start.sh --prod             # 生产模式（构建前端）
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
FRONTEND_DIR="$PROJECT_ROOT/frontend"
CONFIG_FILE="$PROJECT_ROOT/config/config.json"

# ---------- 颜色输出 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# ---------- 默认选项 ----------
START_BACKEND=true
START_FRONTEND=true
SKIP_BUILD=false
PROD_MODE=false

# ---------- 解析参数 ----------
for arg in "$@"; do
    case "$arg" in
        --backend-only)
            START_FRONTEND=false ;;
        --frontend-only)
            START_BACKEND=false ;;
        --no-build)
            SKIP_BUILD=true ;;
        --prod|--production)
            PROD_MODE=true ;;
        -h|--help)
            echo "用法: ./start.sh [选项]"
            echo ""
            echo "选项:"
            echo "  --backend-only      仅启动后端服务"
            echo "  --frontend-only     仅启动前端开发服务器"
            echo "  --no-build          跳过编译"
            echo "  --prod, --production 生产模式（构建前端产物）"
            echo "  -h, --help          显示帮助"
            echo ""
            echo "示例:"
            echo "  ./start.sh                           # 启动全部（开发模式）"
            echo "  ./start.sh --backend-only            # 仅启动后端"
            echo "  ./start.sh --frontend-only           # 仅启动前端"
            echo "  ./start.sh --prod                    # 生产模式"
            echo "  ./start.sh --no-build --backend-only # 不编译直接启动后端"
            exit 0
            ;;
        *)
            echo -e "${RED}未知参数: $arg${NC}"
            echo "使用 -h 查看帮助"
            exit 1
            ;;
    esac
done

echo "============================================"
echo -e "  ${BOLD}TakeAwayPlatform 一键启动${NC}"
echo "  时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ---------- 环境检查 ----------
echo -e "${BLUE}[检查] 运行环境...${NC}"

# 检查是否已有服务在运行
EXISTING_PID=$(pgrep -f "TakeAwayPlatform" 2>/dev/null | head -1)
if [ -n "$EXISTING_PID" ]; then
    echo -e "  ${YELLOW}⚠ 检测到后端已在运行 (PID: $EXISTING_PID)${NC}"
    echo -e "  如需重启请先执行: ${BOLD}./stop.sh${NC} 然后重新运行本脚本"
    echo ""
fi

# ---------- 后端 ----------
if [ "$START_BACKEND" = true ]; then
    echo "----------------------------------------"
    echo -e "${BOLD}[1] 启动后端服务${NC}"
    echo "----------------------------------------"

    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}错误: 配置文件不存在: $CONFIG_FILE${NC}"
        exit 1
    fi

    # 编译
    if [ "$SKIP_BUILD" = false ]; then
        EXECUTABLE="$BUILD_DIR/TakeAwayPlatform"
        NEED_BUILD=false
        if [ ! -f "$EXECUTABLE" ]; then
            echo "  可执行文件不存在，需要编译..."
            NEED_BUILD=true
        elif find "$PROJECT_ROOT/src" -name "*.cpp" -newer "$EXECUTABLE" 2>/dev/null | head -1 | grep -q .; then
            echo "  检测到源码变更，需要重新编译..."
            NEED_BUILD=true
        fi

        if [ "$NEED_BUILD" = true ]; then
            bash "$SCRIPT_DIR/build.sh"
            if [ $? -ne 0 ]; then
                echo -e "${RED}构建失败，退出${NC}"
                exit 1
            fi
        else
            echo "  ✓ 可执行文件已是最新，跳过编译"
        fi
    else
        echo "  跳过编译（--no-build）"
    fi

    # 启动后端
    bash "$SCRIPT_DIR/run.sh" --no-build --bg
    if [ $? -ne 0 ]; then
        echo -e "${RED}后端启动失败${NC}"
        exit 1
    fi
    echo ""
    echo -e "  ${GREEN}✓ 后端服务已启动${NC}"
    echo -e "  API 地址: ${BOLD}http://localhost:9090${NC}"
    echo -e "  健康检查: ${BOLD}http://localhost:9090/health${NC}"
fi

# ---------- 前端 ----------
if [ "$START_FRONTEND" = true ]; then
    echo ""
    echo "----------------------------------------"
    echo -e "${BOLD}[2] 启动前端服务${NC}"
    echo "----------------------------------------"

    if [ ! -d "$FRONTEND_DIR" ]; then
        echo -e "${RED}错误: 前端目录不存在: $FRONTEND_DIR${NC}"
        exit 1
    fi

    cd "$FRONTEND_DIR"

    if [ "$PROD_MODE" = true ]; then
        # 生产模式：构建 + 预览
        echo "  生产模式：构建前端..."
        bash "$SCRIPT_DIR/frontend-build.sh"
        echo ""
        echo -e "  ${GREEN}✓ 前端构建完成${NC}"
        echo -e "  构建产物: ${BOLD}$FRONTEND_DIR/dist/${NC}"
        echo ""
        echo -e "  启动预览服务器..."
        npx vite preview --host 0.0.0.0 --port 4173 > /tmp/frontend.log 2>&1 &
        FRONTEND_PID=$!
        echo ""
        echo -e "  ${GREEN}✓ 前端预览服务已启动${NC}"
        echo -e "  预览地址: ${BOLD}http://localhost:4173${NC}"
        echo -e "  日志文件: /tmp/frontend.log"
    else
        # 开发模式
        # 检查 node_modules
        if [ ! -d "node_modules" ]; then
            echo "  安装依赖..."
            npm install --legacy-peer-deps
        fi

        # 安装依赖（如果需要）
        if [ ! -d "node_modules/react-router-dom" ]; then
            echo "  安装缺失依赖..."
            npm install --legacy-peer-deps
        fi

        echo "  启动 Vite 开发服务器..."
        npx vite --host 0.0.0.0 > /tmp/frontend.log 2>&1 &
        FRONTEND_PID=$!

        # 等待启动
        sleep 3
        if kill -0 $FRONTEND_PID 2>/dev/null; then
            echo -e "  ${GREEN}✓ 前端开发服务器已启动${NC}"
            echo -e "  开发地址: ${BOLD}http://localhost:5173${NC}"
        else
            echo -e "  ${RED}✗ 前端启动失败，查看日志: tail /tmp/frontend.log${NC}"
        fi
        echo -e "  日志文件: /tmp/frontend.log"
    fi
fi

# ---------- 汇总 ----------
echo ""
echo "============================================"
echo -e "  ${GREEN}${BOLD}✓ 启动完成！${NC}"
echo "============================================"

if [ "$START_BACKEND" = true ]; then
    echo -e "  后端 API:   ${BOLD}http://localhost:9090${NC}"
    echo -e "  健康检查:   ${BOLD}http://localhost:9090/health${NC}"
fi

if [ "$START_FRONTEND" = true ]; then
    if [ "$PROD_MODE" = true ]; then
        echo -e "  前端预览:   ${BOLD}http://localhost:4173${NC}"
    else
        echo -e "  前端开发:   ${BOLD}http://localhost:5173${NC}"
    fi
fi

echo ""
echo -e "  停止服务:   ${BOLD}./stop.sh${NC}"
echo -e "  检查状态:   ${BOLD}./check.sh${NC}"
echo -e "  查看日志:   ${BOLD}tail -f /tmp/takeaway_platform.log${NC}"
echo "============================================"
echo ""
echo -e "${BLUE}实时日志（按 Ctrl+C 停止查看，服务继续后台运行）:${NC}"
echo ""
tail -f /tmp/takeaway_platform.log /tmp/frontend.log 2>/dev/null