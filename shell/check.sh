#!/bin/bash
# ============================================================
# TakeAwayPlatform 服务状态检查脚本
# 功能: 检查服务是否运行、健康状态、端口占用
# 用法: ./check.sh                # 基础检查
#       ./check.sh --watch        # 持续监控（每5秒刷新）
#       ./check.sh --quick-test   # 快速接口测试
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 从 config.json 读取端口
CONFIG_FILE="$PROJECT_ROOT/config/config.json"
PORT=9090
if [ -f "$CONFIG_FILE" ]; then
    PORT=$(python3 -c "
import json, sys
try:
    with open('$CONFIG_FILE') as f:
        content = f.read()
        lines = [l for l in content.split('\n') if not l.strip().startswith('//') and not l.strip().startswith('/*') and not l.strip().startswith('*')]
        data = json.loads('\n'.join(lines))
        print(data.get('server', {}).get('port', 9090))
except: print(9090)
" 2>/dev/null || echo "9090")
fi

BASE_URL="http://localhost:$PORT"

# ---------- 颜色 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

WATCH_MODE=false
QUICK_TEST=false

for arg in "$@"; do
    case "$arg" in
        --watch|-w)
            WATCH_MODE=true ;;
        --quick-test|-t)
            QUICK_TEST=true ;;
        -h|--help)
            echo "用法: ./check.sh [选项]"
            echo ""
            echo "选项:"
            echo "  -w, --watch     持续监控模式（每5秒刷新）"
            echo "  -t, --quick-test 快速接口测试"
            echo "  -h, --help      显示帮助"
            exit 0
            ;;
    esac
done

# ---------- 检查函数 ----------
check_status() {
    echo "============================================"
    echo "  TakeAwayPlatform 服务状态检查"
    echo "  时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "  端口: $PORT"
    echo "============================================"

    # 1. 进程检查
    echo ""
    echo "【进程状态】"
    PID=$(pgrep -f "TakeAwayPlatform" | head -1)
    if [ -n "$PID" ]; then
        echo -e "  ${GREEN}✓ 进程运行中${NC}  PID: $PID"
        # 显示进程详情
        ps -p "$PID" -o pid,ppid,user,%cpu,%mem,etime,cmd --no-headers 2>/dev/null | while read line; do
            echo "  $line"
        done
    else
        echo -e "  ${RED}✗ 进程未运行${NC}"
    fi

    # 2. 端口检查
    echo ""
    echo "【端口监听】"
    if ss -tlnp 2>/dev/null | grep -q ":$PORT " || netstat -tlnp 2>/dev/null | grep -q ":$PORT "; then
        echo -e "  ${GREEN}✓ 端口 $PORT 正在监听${NC}"
        LISTEN_INFO=$(ss -tlnp 2>/dev/null | grep ":$PORT " || netstat -tlnp 2>/dev/null | grep ":$PORT ")
        echo "  $LISTEN_INFO"
    else
        echo -e "  ${RED}✗ 端口 $PORT 未监听${NC}"
    fi

    # 3. 健康检查
    echo ""
    echo "【健康检查】"
    if command -v curl &>/dev/null; then
        HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "$BASE_URL/health" 2>/dev/null || echo "000")
        if [ "$HTTP_CODE" = "200" ]; then
            BODY=$(curl -s --connect-timeout 3 "$BASE_URL/health" 2>/dev/null)
            echo -e "  ${GREEN}✓ GET /health → HTTP $HTTP_CODE${NC}  响应: $BODY"
        else
            echo -e "  ${RED}✗ GET /health → HTTP $HTTP_CODE${NC}"
        fi

        # 根路径检查
        HTTP_CODE_ROOT=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "$BASE_URL/" 2>/dev/null || echo "000")
        if [ "$HTTP_CODE_ROOT" = "200" ]; then
            echo -e "  ${GREEN}✓ GET /       → HTTP $HTTP_CODE_ROOT${NC}"
        else
            echo -e "  ${YELLOW}⚠ GET /       → HTTP $HTTP_CODE_ROOT${NC}"
        fi
    else
        echo -e "  ${YELLOW}⚠ curl 未安装，无法进行 HTTP 检查${NC}"
    fi

    # 4. 日志尾部
    echo ""
    echo "【最近日志】(/tmp/takeaway_platform.log)"
    if [ -f /tmp/takeaway_platform.log ]; then
        tail -5 /tmp/takeaway_platform.log 2>/dev/null | while read line; do
            echo "  $line"
        done
    else
        echo "  (日志文件不存在)"
    fi

    echo ""
    echo "============================================"
}

# ---------- 前端检查 ----------
check_frontend() {
    echo ""
    echo "【前端状态】"
    VITE_PID=$(pgrep -f "vite" 2>/dev/null | head -1)
    if [ -n "$VITE_PID" ]; then
        echo -e "  ${GREEN}✓ Vite 开发服务器运行中${NC}  PID: $VITE_PID"

        if command -v curl &>/dev/null; then
            HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "http://localhost:5173/" 2>/dev/null || echo "000")
            if [ "$HTTP_CODE" = "200" ]; then
                echo -e "  ${GREEN}✓ 前端页面可访问${NC} (HTTP $HTTP_CODE)"
            else
                echo -e "  ${YELLOW}⚠ 前端页面响应异常${NC} (HTTP $HTTP_CODE)"
            fi

            # 检查代理是否工作
            HTTP_CODE_API=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "http://localhost:5173/health" 2>/dev/null || echo "000")
            if [ "$HTTP_CODE_API" = "200" ]; then
                echo -e "  ${GREEN}✓ API 代理正常${NC} (Vite → 后端)"
            else
                echo -e "  ${YELLOW}⚠ API 代理异常${NC} (HTTP $HTTP_CODE_API, 后端可能未启动)"
            fi
        fi
    else
        echo -e "  ${YELLOW}○ Vite 开发服务器未运行${NC}"
    fi

    # 检查前端构建产物
    FRONTEND_DIST="$PROJECT_ROOT/frontend/dist"
    if [ -f "$FRONTEND_DIST/index.html" ]; then
        echo -e "  ${GREEN}✓ 前端构建产物存在${NC} ($FRONTEND_DIST)"
    else
        echo -e "  ${YELLOW}○ 前端构建产物不存在${NC} (运行 frontend-build.sh 生成)"
    fi
}

# ---------- 快速接口测试 ----------
quick_test() {
    echo "============================================"
    echo "  TakeAwayPlatform 快速接口测试"
    echo "============================================"
    echo ""

    # 测试健康检查
    echo -n "  [1] GET /health ............... "
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "$BASE_URL/health" 2>/dev/null || echo "000")
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}PASS${NC} (HTTP $HTTP_CODE)"
    else
        echo -e "${RED}FAIL${NC} (HTTP $HTTP_CODE)"
    fi

    # 测试根路径
    echo -n "  [2] GET / .................... "
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "$BASE_URL/" 2>/dev/null || echo "000")
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}PASS${NC} (HTTP $HTTP_CODE)"
    else
        echo -e "${RED}FAIL${NC} (HTTP $HTTP_CODE)"
    fi

    # 测试注册接口（无鉴权访问）
    echo -n "  [3] POST /api/user/register .. "
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 -X POST "$BASE_URL/api/user/register" -H "Content-Type: application/json" -d '{"username":"__test__","password":"test"}' 2>/dev/null || echo "000")
    if [ "$HTTP_CODE" != "000" ]; then
        echo -e "${GREEN}PASS${NC} (HTTP $HTTP_CODE - 接口可达)"
    else
        echo -e "${RED}FAIL${NC} (无法连接)"
    fi

    # 测试未鉴权访问
    echo -n "  [4] GET /api/user/profile .... "
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "$BASE_URL/api/user/profile" 2>/dev/null || echo "000")
    if [ "$HTTP_CODE" = "401" ]; then
        echo -e "${GREEN}PASS${NC} (HTTP 401 - 鉴权正常)"
    elif [ "$HTTP_CODE" != "000" ]; then
        echo -e "${YELLOW}WARN${NC} (HTTP $HTTP_CODE)"
    else
        echo -e "${RED}FAIL${NC} (无法连接)"
    fi

    echo ""
    echo "============================================"
}

# ---------- 主逻辑 ----------
if [ "$QUICK_TEST" = true ]; then
    quick_test
elif [ "$WATCH_MODE" = true ]; then
    echo "进入监控模式，按 Ctrl+C 退出..."
    while true; do
        clear
        check_status
        check_frontend
        echo ""
        echo "每5秒刷新一次..."
        sleep 5
    done
else
    check_status
    check_frontend
fi
