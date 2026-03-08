#!/bin/bash

# Chat App 初始化脚本
# 用于设置开发环境和运行基本测试

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$PROJECT_ROOT/server"
CLIENT_DIR="$PROJECT_ROOT/client/chat_app"

echo "==================================="
echo "   Chat App 初始化脚本"
echo "==================================="
echo ""

# 检查依赖
check_dependencies() {
    echo "检查系统依赖..."
    
    # 检查 CMake
    if command -v cmake &> /dev/null; then
        echo "  ✓ CMake: $(cmake --version | head -1)"
    else
        echo "  ✗ CMake 未安装"
    fi
    
    # 检查 Flutter
    if command -v flutter &> /dev/null; then
        echo "  ✓ Flutter: $(flutter --version | head -1)"
    else
        echo "  ✗ Flutter 未安装"
    fi
    
    # 检查 MySQL
    if command -v mysql &> /dev/null; then
        echo "  ✓ MySQL Client: $(mysql --version)"
    else
        echo "  ✗ MySQL Client 未安装"
    fi
    
    echo ""
}

# 创建数据库
create_database() {
    echo "创建 MySQL 数据库..."
    echo "请确保 MySQL 服务已启动，并手动执行以下 SQL:"
    echo ""
    echo "  CREATE DATABASE IF NOT EXISTS chat_app CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
    echo "  CREATE USER IF NOT EXISTS 'chat_user'@'localhost' IDENTIFIED BY 'your_password';"
    echo "  GRANT ALL PRIVILEGES ON chat_app.* TO 'chat_user'@'localhost';"
    echo "  FLUSH PRIVILEGES;"
    echo ""
}

# 构建服务器
build_server() {
    echo "构建 C++ 服务器..."
    
    if [ -d "$SERVER_DIR" ]; then
        mkdir -p "$SERVER_DIR/build"
        cd "$SERVER_DIR/build"
        
        if command -v cmake &> /dev/null; then
            cmake ..
            echo "CMake 配置完成，运行 'make' 进行编译"
        else
            echo "跳过: CMake 未安装"
        fi
    else
        echo "跳过: 服务器目录不存在"
    fi
    
    echo ""
}

# 初始化 Flutter 客户端
init_client() {
    echo "初始化 Flutter 客户端..."
    
    if [ -d "$CLIENT_DIR" ]; then
        cd "$CLIENT_DIR"
        
        if command -v flutter &> /dev/null; then
            flutter pub get
            echo "Flutter 依赖安装完成"
            
            # 运行代码生成器
            echo "运行代码生成器..."
            flutter pub run build_runner build --delete-conflicting-outputs || true
        else
            echo "跳过: Flutter 未安装"
        fi
    else
        echo "跳过: 客户端目录不存在"
    fi
    
    echo ""
}

# 主菜单
show_menu() {
    echo "选择操作:"
    echo "  1) 检查依赖"
    echo "  2) 创建数据库 (显示 SQL)"
    echo "  3) 构建服务器"
    echo "  4) 初始化客户端"
    echo "  5) 全部执行"
    echo "  0) 退出"
    echo ""
    read -p "请输入选项: " choice
    
    case $choice in
        1) check_dependencies ;;
        2) create_database ;;
        3) build_server ;;
        4) init_client ;;
        5) 
            check_dependencies
            create_database
            build_server
            init_client
            ;;
        0) 
            echo "退出"
            exit 0
            ;;
        *) 
            echo "无效选项"
            ;;
    esac
}

# 如果有参数，直接执行
if [ "$1" = "all" ]; then
    check_dependencies
    create_database
    build_server
    init_client
elif [ "$1" = "check" ]; then
    check_dependencies
elif [ "$1" = "server" ]; then
    build_server
elif [ "$1" = "client" ]; then
    init_client
else
    # 交互式菜单
    while true; do
        show_menu
    done
fi

echo "初始化完成！"
