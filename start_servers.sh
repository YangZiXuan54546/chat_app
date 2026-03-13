#!/bin/bash

# Chat App 服务器启动脚本
# 启动主服务器和媒体文件服务器

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$PROJECT_ROOT/server"
MEDIA_DIR="$SERVER_DIR/media"

# DeepSeek API Key (用于 AI 机器人)
# 设置环境变量 DEEPSEEK_API_KEY 或在此处填写
DEEPSEEK_API_KEY="${DEEPSEEK_API_KEY:-}"

# JPush 极光推送配置 (国内推送)
# 设置环境变量 JPUSH_APP_KEY 和 JPUSH_MASTER_SECRET 或在此处填写
JPUSH_APP_KEY="${JPUSH_APP_KEY:-}"
JPUSH_MASTER_SECRET="${JPUSH_MASTER_SECRET:-}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}==================================="
echo "   Chat App 服务器启动脚本"
echo -e "===================================${NC}"
echo ""

# 创建媒体目录
mkdir -p "$MEDIA_DIR"
echo -e "${GREEN}✓ 媒体目录已创建: $MEDIA_DIR${NC}"

# 启动媒体文件服务器 (C++ 版本)
echo -e "${YELLOW}启动媒体文件服务器 (端口 8889)...${NC}"
cd "$SERVER_DIR/build"
./media_server -d "$MEDIA_DIR" &
MEDIA_PID=$!
echo -e "${GREEN}✓ 媒体服务器已启动 (PID: $MEDIA_PID)${NC}"

# 等待媒体服务器启动
sleep 1

# 启动主聊天服务器 (带 DeepSeek API 和 JPush 推送)
echo -e "${YELLOW}启动聊天服务器 (端口 8888)...${NC}"
cd "$SERVER_DIR"
MEDIA_DIR="$MEDIA_DIR" \
JPUSH_APP_KEY="$JPUSH_APP_KEY" \
JPUSH_MASTER_SECRET="$JPUSH_MASTER_SECRET" \
./build/chat_server --deepseek-api-key "$DEEPSEEK_API_KEY" &
CHAT_PID=$!
echo -e "${GREEN}✓ 聊天服务器已启动 (PID: $CHAT_PID)${NC}"
echo -e "${GREEN}✓ AI 机器人已启用 (DeepSeek API)${NC}"
echo -e "${GREEN}✓ JPush 推送已启用 (极光推送)${NC}"

echo ""
echo -e "${GREEN}===================================${NC}"
echo -e "${GREEN}所有服务器已启动:${NC}"
echo -e "  聊天服务器: ${YELLOW}localhost:8888${NC}"
echo -e "  媒体服务器: ${YELLOW}localhost:8889${NC}"
echo ""
echo "按 Ctrl+C 停止所有服务器"
echo -e "${GREEN}===================================${NC}"

# 保存 PID 到文件
echo "$MEDIA_PID" > "$SERVER_DIR/media_server.pid"
echo "$CHAT_PID" > "$SERVER_DIR/chat_server.pid"

# 等待任意子进程结束
wait

# 清理
echo ""
echo "停止服务器..."
kill $MEDIA_PID 2>/dev/null
kill $CHAT_PID 2>/dev/null
echo "服务器已停止"
