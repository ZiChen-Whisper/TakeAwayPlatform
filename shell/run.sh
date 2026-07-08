#!/bin/bash
# ============================================================
# TakeAwayPlatform 运行脚本
# 功能: 自动构建(如需要)并在前台启动服务
# 用法: ./run.sh              # 使用项目 config/config.json
#       ./run.sh --no-build   # 跳过编译，直接运行
#       ./run.sh --bg         # 后台运行
#       ./run.sh custom.json  # 使用自定义配置文件
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CONFIG_FILE="$PROJECT_ROOT/config/config.json"

SKIP_BUILD=false
RUN_BG=false

# 解析参数
for arg in "$@"; do
    case "$arg" in
        --no-build)
            SKIP_BUILD=true
            ;;
        --bg)
            RUN_BG=true
            ;;
        *.json)
            CONFIG_FILE="$arg"
            # 如果是相对路径，转为绝对路径
            if [[ "$CONFIG_FILE" != /* ]]; then
                CONFIG_FILE="$(pwd)/$CONFIG_FILE"
            fi
            ;;
        -h|--help)
            echo "用法: ./run.sh [选项] [配置文件.json]"
            echo ""
            echo "选项:"
            echo "  --no-build    跳过编译，直接运行"
            echo "  --bg          后台运行"
            echo "  -h, --help    显示帮助"
            echo ""
            echo "示例:"
            echo "  ./run.sh                           # 编译并前台运行"
            echo "  ./run.sh --bg                      # 编译并后台运行"
            echo "  ./run.sh --no-build custom.json    # 不编译，使用自定义配置"
            exit 0
            ;;
    esac
done

echo "============================================"
echo "  TakeAwayPlatform 服务启动"
echo "  项目目录: $PROJECT_ROOT"
echo "  配置文件: $CONFIG_FILE"
echo "============================================"

# 检查配置文件
if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件不存在: $CONFIG_FILE"
    exit 1
fi

# 自动构建
if [ "$SKIP_BUILD" = false ]; then
    EXECUTABLE="$BUILD_DIR/TakeAwayPlatform"

    # 判断是否需要编译：可执行文件不存在 或 源码比可执行文件更新
    NEED_BUILD=false
    if [ ! -f "$EXECUTABLE" ]; then
        echo "可执行文件不存在，需要编译..."
        NEED_BUILD=true
    else
        # 检查 src/ 下是否有比可执行文件更新的源码
        if find "$PROJECT_ROOT/src" -name "*.cpp" -newer "$EXECUTABLE" 2>/dev/null | head -1 | grep -q .; then
            echo "检测到源码变更，需要重新编译..."
            NEED_BUILD=true
        fi
    fi

    if [ "$NEED_BUILD" = true ]; then
        echo ""
        bash "$SCRIPT_DIR/build.sh"
        if [ $? -ne 0 ]; then
            echo "构建失败，退出"
            exit 1
        fi
    else
        echo "可执行文件已是最新，跳过编译"
    fi
else
    echo "跳过编译（--no-build）"
fi

# 确保可执行文件存在
EXECUTABLE="$BUILD_DIR/TakeAwayPlatform"
if [ ! -f "$EXECUTABLE" ]; then
    echo "错误: 可执行文件不存在: $EXECUTABLE"
    echo "请先运行 ./build.sh 编译项目"
    exit 1
fi

# 赋予执行权限
chmod +x "$EXECUTABLE"

echo ""
echo "启动服务..."

if [ "$RUN_BG" = true ]; then
    # 后台运行
    LOG_DIR="/opt/TakeAwayPlatform/logs"
    sudo mkdir -p "$LOG_DIR"
    
    nohup "$EXECUTABLE" "$CONFIG_FILE" > /tmp/takeaway_platform.log 2>&1 &
    PID=$!
    echo "服务已在后台启动，PID: $PID"
    echo "日志文件: /tmp/takeaway_platform.log"
    echo ""
    echo "查看日志: tail -f /tmp/takeaway_platform.log"
    echo "停止服务: kill $PID"
    echo "检查状态: curl http://localhost:9090/health"

    # 等待一下检查是否启动成功
    sleep 2
    if kill -0 $PID 2>/dev/null; then
        echo ""
        echo "✓ 服务启动成功"
        # 快速健康检查
        if command -v curl &>/dev/null; then
            HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:9090/health 2>/dev/null || echo "000")
            if [ "$HTTP_CODE" = "200" ]; then
                echo "✓ 健康检查通过 (HTTP $HTTP_CODE)"
            else
                echo "⚠ 健康检查未通过 (HTTP $HTTP_CODE)，请稍后再试"
            fi
        fi
    else
        echo "✗ 服务启动失败，请查看日志: tail /tmp/takeaway_platform.log"
        exit 1
    fi
else
    # 前台运行
    echo "正在前台运行，按 Ctrl+C 停止..."
    echo ""
    "$EXECUTABLE" "$CONFIG_FILE"
fi
