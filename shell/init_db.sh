#!/bin/bash
# ============================================================
# TakeAwayPlatform 数据库初始化脚本
# 功能: 创建数据库并执行建表语句
# 用法: ./init_db.sh                    # 使用 config.json 中的配置
#       ./init_db.sh -u root -p 123    # 手动指定用户名密码
#       ./init_db.sh --force           # 强制重建（删除旧库）
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SQL_FILE="$PROJECT_ROOT/docs/database_init.sql"
CONFIG_FILE="$PROJECT_ROOT/config/config.json"

FORCE_RECREATE=false

# ---------- 颜色输出 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ---------- 从 config.json 读取配置 ----------
parse_json() {
    # 简单解析 JSON（不依赖 jq）
    local key="$1"
    python3 -c "
import json
with open('$CONFIG_FILE') as f:
    content = f.read()
    # 去掉注释
    lines = [l for l in content.split('\n') if not l.strip().startswith('//') and not l.strip().startswith('/*') and not l.strip().startswith('*')]
    data = json.loads('\n'.join(lines))
    print(data$key)
" 2>/dev/null || echo ""
}

if [ -f "$CONFIG_FILE" ]; then
    DB_HOST=$(parse_json "['database']['host']")
    DB_PORT=$(parse_json "['database']['port']")
    DB_USER=$(parse_json "['database']['user']")
    DB_PASS=$(parse_json "['database']['password']")
    DB_NAME=$(parse_json "['database']['name']")
else
    DB_HOST="127.0.0.1"
    DB_PORT="3306"
    DB_USER="root"
    DB_PASS=""
    DB_NAME="TakeAwayDatabase"
fi

# ---------- 解析命令行参数 ----------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--host)
            DB_HOST="$2"; shift 2 ;;
        -P|--port)
            DB_PORT="$2"; shift 2 ;;
        -u|--user)
            DB_USER="$2"; shift 2 ;;
        -p|--password)
            DB_PASS="$2"; shift 2 ;;
        --force)
            FORCE_RECREATE=true; shift ;;
        -h|--help)
            echo "用法: ./init_db.sh [选项]"
            echo ""
            echo "选项:"
            echo "  -h, --host HOST      MySQL 主机 (默认: 127.0.0.1)"
            echo "  -P, --port PORT      MySQL 端口 (默认: 3306)"
            echo "  -u, --user USER      MySQL 用户名 (默认: root)"
            echo "  -p, --password PASS  MySQL 密码"
            echo "  --force              强制重建数据库（删除旧库）"
            echo ""
            echo "示例:"
            echo "  ./init_db.sh                           # 使用 config.json 配置"
            echo "  ./init_db.sh -u root -p 123 --force    # 强制重建"
            exit 0
            ;;
        *)
            echo "未知参数: $1"
            exit 1
            ;;
    esac
done

# ---------- 检查 SQL 文件 ----------
if [ ! -f "$SQL_FILE" ]; then
    echo -e "${RED}错误: SQL 文件不存在: $SQL_FILE${NC}"
    exit 1
fi

# ---------- 构建 MySQL 命令 ----------
MYSQL_CMD="mysql -h $DB_HOST -P $DB_PORT -u $DB_USER"
if [ -n "$DB_PASS" ]; then
    MYSQL_CMD="$MYSQL_CMD -p$DB_PASS"
fi

echo "============================================"
echo "  TakeAwayPlatform 数据库初始化"
echo "  主机: $DB_HOST:$DB_PORT"
echo "  用户: $DB_USER"
echo "  数据库: $DB_NAME"
echo "  SQL 文件: $SQL_FILE"
echo "============================================"

# ---------- 检查 MySQL 连接 ----------
echo ""
echo "[1/3] 检查 MySQL 连接..."
if ! $MYSQL_CMD -e "SELECT 1" &>/dev/null; then
    echo -e "${RED}错误: 无法连接到 MySQL，请检查主机/端口/用户名/密码${NC}"
    echo "尝试连接命令: $MYSQL_CMD"
    exit 1
fi
echo -e "${GREEN}✓ MySQL 连接成功${NC}"

# ---------- 强制重建 ----------
if [ "$FORCE_RECREATE" = true ]; then
    echo ""
    echo -e "${YELLOW}[2/3] 强制重建: 删除旧数据库 $DB_NAME...${NC}"
    $MYSQL_CMD -e "DROP DATABASE IF EXISTS \`$DB_NAME\`;"
    echo -e "${GREEN}✓ 旧数据库已删除${NC}"
fi

# ---------- 执行建表脚本 ----------
echo ""
echo "[3/3] 执行建表脚本..."
if $MYSQL_CMD < "$SQL_FILE"; then
    echo -e "${GREEN}✓ 数据库初始化成功！${NC}"
else
    echo -e "${RED}错误: 建表脚本执行失败${NC}"
    exit 1
fi

# ---------- 验证结果 ----------
echo ""
echo "============================================"
echo "  验证数据库表:"
$MYSQL_CMD -e "USE \`$DB_NAME\`; SHOW TABLES;" 2>/dev/null
echo "============================================"
echo ""
echo -e "${GREEN}数据库初始化完成！${NC}"
echo ""
echo "接下来可以启动服务:"
echo "  $SCRIPT_DIR/run.sh"
