#include "server.hpp"
#include "database.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include <iostream>
#include <memory>
#include <csignal>

std::shared_ptr<chat::Server> g_server;

void signal_handler(int signal) {
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // 配置
    chat::Database::Config db_config;
    db_config.host = "localhost";
    db_config.port = 3306;
    db_config.user = "chat_user";
    db_config.password = "chat_password_2026";
    db_config.database = "chat_app";
    
    chat::Server::Config server_config;
    server_config.host = "0.0.0.0";
    server_config.port = 8888;
    server_config.thread_count = 4;
    server_config.heartbeat_timeout = 60;
    
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
            server_config.port = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            server_config.thread_count = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --db-host <host>       Database host (default: localhost)\n"
                      << "  --db-port <port>       Database port (default: 3306)\n"
                      << "  --db-user <user>       Database user (default: root)\n"
                      << "  --db-password <pass>   Database password\n"
                      << "  --db-name <name>       Database name (default: chat_app)\n"
                      << "  --port <port>          Server port (default: 8888)\n"
                      << "  --threads <count>      Thread count (default: 4)\n"
                      << "  --help                 Show this help message\n";
            return 0;
        }
    }
    
    // 初始化数据库
    auto database = std::make_shared<chat::Database>();
    if (!database->init(db_config)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }
    std::cout << "Database connected successfully" << std::endl;
    
    // 初始化管理器
    auto user_manager = std::make_shared<chat::UserManager>(database);
    auto message_manager = std::make_shared<chat::MessageManager>(database);
    auto group_manager = std::make_shared<chat::GroupManager>(database);
    auto friend_manager = std::make_shared<chat::FriendManager>(database);
    
    // 创建服务器
    g_server = std::make_shared<chat::Server>(server_config);
    
    // 设置服务器的管理器
    g_server->set_managers(user_manager, message_manager, group_manager, friend_manager, database);
    
    // 设置管理器
    message_manager->set_server(g_server);
    group_manager->set_server(g_server);
    friend_manager->set_server(g_server);
    
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 启动服务器
    if (!g_server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "Chat server started on port " << server_config.port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // 运行服务器
    g_server->run();
    
    // 清理
    database->close();
    
    return 0;
}
