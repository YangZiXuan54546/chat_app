#!/bin/bash

# Chat App 服务器启动脚本
# 启动 Gateway 服务器 (WebSocket + HTTP Gateway)

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$PROJECT_ROOT/server"
MEDIA_DIR="$SERVER_DIR/media"

# Termux 库路径
TERMUX_LIB="/data/data/com.termux/files/usr/lib/aarch64-linux-android"
export LD_LIBRARY_PATH="$TERMUX_LIB:$LD_LIBRARY_PATH"

# DeepSeek API Key (用于 AI 机器人)
# 设置环境变量 DEEPSEEK_API_KEY 或在此处填写
DEEPSEEK_API_KEY="${DEEPSEEK_API_KEY:-}"

# JPush 极光推送配置 (国内推送)
# 设置环境变量 JPUSH_APP_KEY 和 JPUSH_MASTER_SECRET 或在此处填写
JPUSH_APP_KEY="${JPUSH_APP_KEY:-16d9f5ae7a467d54f3d9f775}"
JPUSH_MASTER_SECRET="${JPUSH_MASTER_SECRET:-f028dc58ec2143b14d59b1d6}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}==================================="
echo "   Chat App 服务器启动脚本 v3.0"
echo "   Gateway 架构 (WebSocket + HTTP)"
echo -e "===================================${NC}"
echo ""

# 创建媒体目录
mkdir -p "$MEDIA_DIR"
echo -e "${GREEN}✓ 媒体目录已创建: $MEDIA_DIR${NC}"

# 启动 Gateway 服务器 (WebSocket 聊天 + HTTP 文件服务)
echo -e "${YELLOW}启动 Gateway 服务器...${NC}"
cd "$SERVER_DIR"
MEDIA_DIR="$MEDIA_DIR" \
JPUSH_APP_KEY="$JPUSH_APP_KEY" \
JPUSH_MASTER_SECRET="$JPUSH_MASTER_SECRET" \
./build/linux/arm64/release/gateway_server --media-dir "$MEDIA_DIR" --deepseek-api-key "$DEEPSEEK_API_KEY" &
CHAT_PID=$!

# 等待服务器启动
sleep 2

echo ""
echo -e "${GREEN}===================================${NC}"
echo -e "${GREEN}服务器已启动:${NC}"
echo -e "  WebSocket: ${YELLOW}ws://localhost:8888/ws${NC}"
echo -e "  HTTP 文件服务: ${YELLOW}http://localhost:8889${NC}"
echo ""
echo -e "  文件上传: POST http://localhost:8889/api/upload"
echo -e "  文件下载: GET  http://localhost:8889/media/{file_id}/{filename}"
echo ""
echo -e "${GREEN}功能状态:${NC}"
echo -e "  ✓ WebSocket 聊天服务"
echo -e "  ✓ HTTP Gateway (文件服务)"
echo -e "  ✓ AI 机器人 (DeepSeek API)"
echo -e "  ✓ JPush 推送 (极光推送)"
echo ""
echo "按 Ctrl+C 停止服务器"
echo -e "${GREEN}===================================${NC}"

# 保存 PID 到文件
echo "$CHAT_PID" > "$SERVER_DIR/chat_server.pid"

# 等待服务器进程结束
wait $CHAT_PID

# 清理
echo ""
echo "服务器已停止"
rm -f "$SERVER_DIR/chat_server.pid"
