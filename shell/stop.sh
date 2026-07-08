#!/bin/bash
# ============================================================
# TakeAwayPlatform 停止脚本
# 功能: 停止所有运行中的 TakeAwayPlatform 进程（含残留）
# 用法: ./stop.sh                # 优雅停止所有进程
#       ./stop.sh -f             # 强制杀死（SIGKILL）
#       ./stop.sh --cleanup      # 清理所有残留（含 timeout/bash 包装进程）
# ============================================================

FORCE=false
CLEANUP=false

for arg in "$@"; do
    case "$arg" in
        --force|-f)
            FORCE=true ;;
        --cleanup|-c)
            CLEANUP=true
            FORCE=true ;;
        -h|--help)
            echo "用法: ./stop.sh [选项]"
            echo ""
            echo "选项:"
            echo "  -f, --force     强制杀死（SIGKILL）"
            echo "  -c, --cleanup   清理所有残留（含 timeout/bash 包装进程）"
            echo "  -h, --help      显示帮助"
            echo ""
            echo "示例:"
            echo "  ./stop.sh                # 优雅停止"
            echo "  ./stop.sh -f             # 强制停止"
            echo "  ./stop.sh --cleanup      # 清理全部残留（推荐）"
            exit 0
            ;;
    esac
done

echo "============================================"
echo "  TakeAwayPlatform 停止服务"
echo "============================================"

# 收集所有相关 PID
PIDS=$(pgrep -f "TakeAwayPlatform" 2>/dev/null)

# cleanup 模式：还把 timeout、bash -c 等包装进程也纳入
if [ "$CLEANUP" = true ]; then
    # 找出 TakeAwayPlatform 的父进程（timeout / bash -c）
    for pid in $PIDS; do
        PPID=$(ps -o ppid= -p "$pid" 2>/dev/null | tr -d ' ')
        if [ -n "$PPID" ] && [ "$PPID" != "1" ]; then
            PNAME=$(ps -o comm= -p "$PPID" 2>/dev/null)
            case "$PNAME" in
                timeout|bash|sh)
                    PIDS="$PIDS $PPID" ;;
            esac
        fi
    done
    # 去重
    PIDS=$(echo "$PIDS" | tr ' ' '\n' | sort -un | tr '\n' ' ')
fi

if [ -z "$PIDS" ] || [ "$PIDS" = " " ]; then
    echo ""
    echo "没有运行中的 TakeAwayPlatform 进程"
    exit 0
fi

# 统计
COUNT=$(echo "$PIDS" | wc -w)
echo ""
echo "找到 $COUNT 个相关进程:"
ps -o pid,ppid,comm,etime -p $(echo "$PIDS" | tr ' ' ',') --no-headers 2>/dev/null | while read line; do
    echo "  $line"
done

if [ "$FORCE" = true ]; then
    echo ""
    echo "强制杀死所有进程..."
    kill -9 $PIDS 2>/dev/null || sudo kill -9 $PIDS 2>/dev/null
    sleep 1
else
    echo ""
    echo "发送 SIGTERM 信号..."
    kill $PIDS 2>/dev/null || sudo kill $PIDS 2>/dev/null

    # 等待退出（最多 10 秒）
    for i in $(seq 1 20); do
        REMAIN=$(pgrep -f "TakeAwayPlatform" 2>/dev/null)
        if [ -z "$REMAIN" ]; then
            break
        fi
        sleep 0.5
    done

    # 仍有残留则强制
    REMAIN=$(pgrep -f "TakeAwayPlatform" 2>/dev/null)
    if [ -n "$REMAIN" ]; then
        echo "⚠ 部分进程未响应，强制杀死..."
        kill -9 $REMAIN 2>/dev/null || sudo kill -9 $REMAIN 2>/dev/null
        sleep 1
    fi
fi

# 最终确认
REMAIN=$(pgrep -f "TakeAwayPlatform" 2>/dev/null)
if [ -z "$REMAIN" ]; then
    echo "✓ 全部清理完毕"
else
    echo "⚠ 以下进程仍在运行，请手动处理:"
    ps -o pid,ppid,comm -p $(echo "$REMAIN" | tr '\n' ',') --no-headers 2>/dev/null
fi
