# Shell 脚本使用指南

本目录包含 TakeAwayPlatform 项目的 11 个运维脚本，按功能分为四类。

---

## 快速开始

```bash
# 第一次使用：初始化数据库 → 一键启动
./init_db.sh
./start.sh
```

---

## 一、服务管理（最常用）

### `start.sh` — 一键启动

同时启动后端服务和前端开发服务器。

```bash
./start.sh                        # 启动全部（后端 + 前端开发模式）
./start.sh --backend-only         # 仅启动后端
./start.sh --frontend-only        # 仅启动前端
./start.sh --no-build             # 跳过编译
./start.sh --prod                 # 生产模式（构建前端产物，用 preview 服务）
```

启动后访问：
- 前端开发：`http://localhost:5173`
- 后端 API：`http://localhost:9090`

### `stop.sh` — 停止服务

```bash
./stop.sh                         # 优雅停止（SIGTERM）
./stop.sh -f                      # 强制停止（SIGKILL）
./stop.sh --cleanup               # 清理全部残留进程（含 Vite）
```

> `stop.sh` 会同时停止后端进程和 Vite 前端开发服务器。

### `restart.sh` — 重启

```bash
./restart.sh                      # 停止 → 编译 → 后台启动
./restart.sh --no-build           # 跳过编译
```

### `check.sh` — 状态检查

```bash
./check.sh                        # 一次性检查（进程/端口/健康/前端）
./check.sh -w                     # 持续监控，每 5 秒刷新
./check.sh -t                     # 快速接口测试（4 个核心 API）
```

---

## 二、编译与运行

### `build.sh` — 编译后端

```bash
./build.sh                        # Release 构建（全线程）
./build.sh debug                  # Debug 构建
./build.sh -j4                    # 4 线程并行编译
```

### `run.sh` — 运行后端

```bash
./run.sh                          # 自动编译（如有变更）→ 前台运行
./run.sh --no-build               # 跳过编译
./run.sh --bg                     # 后台运行
./run.sh custom.json              # 使用自定义配置文件
```

---

## 三、部署与初始化

### `deploy.sh` — 部署到生产目录

将编译产物部署到 `/opt/TakeAwayPlatform/`，并自动部署前端构建产物。

```bash
./deploy.sh                       # 编译 → 部署（后端 + 前端）
./deploy.sh --no-build            # 跳过编译
```

### `init_db.sh` — 初始化数据库

```bash
./init_db.sh                      # 使用 config.json 配置
./init_db.sh --force              # 强制重建（删除旧库）
./init_db.sh -u root -p 123      # 手动指定密码
```

### `clean.sh` — 清理

```bash
./clean.sh                        # 清理编译产物（保留 CMake 缓存）
./clean.sh --all                  # 彻底清理（build 目录 + 日志）
./clean.sh --logs                 # 只清理日志
```

---

## 四、前端专用

### `frontend-build.sh` — 构建前端

```bash
./frontend-build.sh               # npm install → npm run build
```

### `frontend-deploy.sh` — 部署前端

```bash
./frontend-deploy.sh              # dist/ → /opt/TakeAwayPlatform/web/
```

> `deploy.sh` 会自动调用此脚本，通常无需手动执行。

---

## 典型工作流

### 日常开发

```bash
./start.sh                        # 一键启动，开始编码
# ... 修改代码 ...
./restart.sh                      # 重启后端（自动检测源码变更并编译）
./check.sh -w                     # 另一个终端监控状态
```

### 首次部署

```bash
./init_db.sh --force              # 初始化数据库
./deploy.sh                       # 编译并部署到 /opt/
```

### 生产发布

```bash
./build.sh                        # 编译后端
./frontend-build.sh               # 构建前端
./deploy.sh --no-build            # 部署（跳过重复编译）
./start.sh --prod                 # 生产模式启动
```

### 故障排查

```bash
./check.sh -t                     # 快速 API 测试
./check.sh                        # 完整状态检查
tail -f /tmp/takeaway_platform.log  # 查看后端日志
tail -f /tmp/frontend.log         # 查看前端日志
```

---

## 端口说明

| 服务 | 端口 | 说明 |
|------|------|------|
| 后端 API | `9090` | config.json 中配置 |
| 前端开发 | `5173` | Vite 默认，自动代理 API 到 9090 |
| 前端预览 | `4173` | `--prod` 模式下的构建预览 |
| MySQL | `3306` | config.json 中配置 |
