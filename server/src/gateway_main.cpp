#include "gateway_server.hpp"
#include "database.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include "fcm_manager.hpp"
#include "jpush_manager.hpp"
#include <iostream>
#include <memory>
#include <csignal>
#include <cstdlib>
#include <asio.hpp>

std::shared_ptr<chat::GatewayServer> g_gateway;

void signal_handler(int signal) {
    if (g_gateway) {
        g_gateway->stop();
    }
}

int main(int argc, char* argv[]) {
    // 数据库配置
    chat::Database::Config db_config;
    db_config.host = "localhost";
    db_config.port = 3306;
    db_config.user = "chat_user";
    db_config.password = "chat_password_2026";
    db_config.database = "chat_app";
    
    // 网关配置
    chat::GatewayConfig gateway_config;
    gateway_config.host = "0.0.0.0";
    gateway_config.port = 8888;
    gateway_config.max_upload_size = 10 * 1024 * 1024; // 10MB
    
    // 机器人配置
    std::string deepseek_api_key;
    std::string bot_username = "deepseek_bot";
    std::string bot_nickname = "AI 助手";
    bool bot_enabled = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--db-host" && i + 1 < argc) {
            db_config.host = argv[++i];
        } else if (arg == "--db-port" && i + 1 < argc) {
            db_config.port = std::stoi(argv[++i]);
        } else if (arg == "--db-user" && i + 1 < argc) {
            db_config.user = argv[++i];
        } else if (arg == "--db-password" && i + 1 < argc) {
            db_config.password = argv[++i];
        } else if (arg == "--db-name" && i + 1 < argc) {
            db_config.database = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            gateway_config.port = std::stoi(argv[++i]);
        } else if (arg == "--media-dir" && i + 1 < argc) {
            gateway_config.media_dir = argv[++i];
        } else if (arg == "--deepseek-api-key" && i + 1 < argc) {
            deepseek_api_key = argv[++i];
            bot_enabled = true;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>          Gateway port (default: 8888)\n"
                      << "  --media-dir <dir>      Media files directory (default: ./media)\n"
                      << "  --db-host <host>       Database host (default: localhost)\n"
                      << "  --db-port <port>       Database port (default: 3306)\n"
                      << "  --db-user <user>       Database user\n"
                      << "  --db-password <pass>   Database password\n"
                      << "  --db-name <name>       Database name\n"
                      << "  --deepseek-api-key <key>  DeepSeek API key for AI bot\n"
                      << "  --help                 Show this help message\n";
            return 0;
        }
    }
    
    // 从环境变量获取媒体目录
    const char* env_media_dir = std::getenv("MEDIA_DIR");
    if (env_media_dir) {
        gateway_config.media_dir = env_media_dir;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  Chat App Gateway Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 初始化数据库
    auto database = std::make_shared<chat::Database>();
    if (!database->init(db_config)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }
    std::cout << "[OK] Database connected" << std::endl;
    
    // 初始化管理器
    auto user_manager = std::make_shared<chat::UserManager>(database);
    auto message_manager = std::make_shared<chat::MessageManager>(database);
    auto group_manager = std::make_shared<chat::GroupManager>(database);
    auto friend_manager = std::make_shared<chat::FriendManager>(database);
    
    // 创建网关服务器
    g_gateway = std::make_shared<chat::GatewayServer>(gateway_config);
    
    // 设置依赖
    g_gateway->set_database(database);
    g_gateway->set_user_manager(user_manager);
    g_gateway->set_message_manager(message_manager);
    g_gateway->set_group_manager(group_manager);
    g_gateway->set_friend_manager(friend_manager);
    
    // 初始化 FCM
    auto fcm_manager = std::make_shared<chat::FcmManager>(database);
    fcm_manager->set_config("chatapp-ae10f", "config/firebase-service-account.json");
    if (fcm_manager->is_configured()) {
        g_gateway->set_fcm_manager(fcm_manager);
        std::cout << "[OK] FCM Push initialized" << std::endl;
    } else {
        std::cout << "[--] FCM not configured" << std::endl;
    }
    
    // 初始化 JPush
    auto jpush_manager = std::make_shared<chat::JPushManager>(database);
    const char* jpush_app_key = std::getenv("JPUSH_APP_KEY");
    const char* jpush_master_secret = std::getenv("JPUSH_MASTER_SECRET");
    if (jpush_app_key && jpush_master_secret) {
        jpush_manager->set_config(jpush_app_key, jpush_master_secret);
        if (jpush_manager->is_configured()) {
            g_gateway->set_jpush_manager(jpush_manager);
            std::cout << "[OK] JPush initialized" << std::endl;
        }
    } else {
        std::cout << "[--] JPush not configured" << std::endl;
    }
    
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 启动网关
    if (!g_gateway->start()) {
        std::cerr << "Failed to start gateway server" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Gateway server running. Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;
    
    // 运行
    g_gateway->run();
    
    // 清理
    database->close();
    
    std::cout << "Server stopped." << std::endl;
    return 0;
}
