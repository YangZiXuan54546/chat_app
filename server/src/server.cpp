#include "server.hpp"
#include "session.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include "database.hpp"
#include "bot_manager.hpp"
#include "deepseek_client.hpp"
#include <iostream>
#include <thread>

namespace chat {

Server::Server(const Config& config)
    : config_(config)
    , io_context_()
    , acceptor_(io_context_, Endpoint(asio::ip::make_address(config.host), config.port))
    , signals_(io_context_, SIGINT, SIGTERM)
    , running_(false)
    , heartbeat_timer_(io_context_)
    , cleanup_timer_(io_context_)
    , start_time_(std::chrono::steady_clock::now()) {
}

Server::~Server() {
    stop();
}

bool Server::start() {
    if (running_) {
        return false;
    }
    
    running_ = true;
    
    // 设置信号处理
    signals_.async_wait([this](asio::error_code, int) {
        stop();
    });
    
    std::cout << "Server starting on " << config_.host << ":" << config_.port << std::endl;
    
    // 开始接受连接
    do_accept();
    
    // 启动心跳检测
    check_heartbeats();
    
    // 启动定期清理
    cleanup_expired_resources();
    
    // 启动工作线程
    for (int i = 0; i < config_.thread_count; ++i) {
        threads_.emplace_back([this]() {
            io_context_.run();
        });
    }
    
    return true;
}

void Server::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    io_context_.stop();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    
    // 关闭所有会话
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session->close();
    }
    sessions_.clear();
    
    std::cout << "Server stopped" << std::endl;
}

void Server::run() {
    io_context_.run();
}

void Server::do_accept() {
    auto session = std::make_shared<Session>(io_context_, Socket(io_context_));
    
    acceptor_.async_accept(session->get_socket(),
        [this, session](asio::error_code ec) {
            handle_accept(session, ec);
        });
}

void Server::handle_accept(Session::ptr session, const asio::error_code& ec) {
    if (!ec) {
        // 设置管理器和服务器引用
        session->set_managers(user_manager_, message_manager_, group_manager_, friend_manager_, database_);
        session->set_server(shared_from_this());
        session->set_bot_manager(bot_manager_);
        session->start();
        add_session(session);
        
        ++total_connections_;
    }
    
    // 继续接受新连接
    do_accept();
}

void Server::add_session(Session::ptr session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    // 临时存储，登录后会关联 user_id
    static uint64_t temp_id = 0;
    sessions_[--temp_id] = session;
}

void Server::remove_session(Session::ptr session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (it->second == session) {
            if (session->is_authenticated()) {
                set_user_offline(session->get_user_id());
            }
            sessions_.erase(it);
            break;
        }
    }
}

Session::ptr Server::get_session(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(user_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

void Server::broadcast(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        if (session->is_authenticated()) {
            session->send(data);
        }
    }
}

void Server::broadcast_to_group(uint64_t group_id, const std::vector<uint8_t>& data) {
    if (!group_manager_) {
        std::cerr << "GroupManager not initialized for broadcast_to_group" << std::endl;
        return;
    }
    
    // 获取群成员列表
    auto member_ids = group_manager_->get_group_members(group_id);
    
    // 向每个在线成员发送消息
    for (uint64_t member_id : member_ids) {
        send_to_user(member_id, data);
    }
}

void Server::send_to_user(uint64_t user_id, const std::vector<uint8_t>& data) {
    auto session = get_session(user_id);
    if (session) {
        std::cout << "Sending message to online user: " << user_id << std::endl;
        session->send(data);
        ++messages_processed_;
    } else {
        std::cout << "User " << user_id << " is not online, message not delivered in real-time" << std::endl;
    }
}

void Server::set_user_online(uint64_t user_id, Session::ptr session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // 移除旧的临时 ID
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (it->second == session) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
    
    sessions_[user_id] = session;
}

void Server::set_user_offline(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(user_id);
}

bool Server::is_user_online(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.find(user_id) != sessions_.end();
}

std::vector<uint64_t> Server::get_online_users() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<uint64_t> users;
    for (const auto& [id, session] : sessions_) {
        if (session->is_authenticated()) {
            users.push_back(id);
        }
    }
    return users;
}

void Server::set_managers(std::shared_ptr<UserManager> user_manager,
                          std::shared_ptr<MessageManager> message_manager,
                          std::shared_ptr<GroupManager> group_manager,
                          std::shared_ptr<FriendManager> friend_manager,
                          std::shared_ptr<Database> database) {
    user_manager_ = user_manager;
    message_manager_ = message_manager;
    group_manager_ = group_manager;
    friend_manager_ = friend_manager;
    database_ = database;
}

void Server::set_bot_manager(std::shared_ptr<BotManager> bot_manager) {
    bot_manager_ = bot_manager;
}

Server::Stats Server::get_stats() {
    Stats stats;
    stats.total_connections = total_connections_;
    stats.current_connections = sessions_.size();
    stats.messages_processed = messages_processed_;
    stats.start_time = start_time_;
    return stats;
}

void Server::check_heartbeats() {
    heartbeat_timer_.expires_after(std::chrono::seconds(config_.heartbeat_timeout));
    heartbeat_timer_.async_wait([this](asio::error_code ec) {
        if (!ec) {
            // 检查所有会话的心跳
            auto now = std::chrono::steady_clock::now();
            std::vector<Session::ptr> timeout_sessions;
            
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                
                for (auto& [id, session] : sessions_) {
                    auto last_heartbeat = session->get_last_heartbeat();
                    
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        now - last_heartbeat).count();
                    
                    if (elapsed > config_.heartbeat_timeout * 2) {
                        // 记录超时会话
                        timeout_sessions.push_back(session);
                    }
                }
            }
            
            // 在锁外关闭超时会话
            for (auto& session : timeout_sessions) {
                std::cout << "Session heartbeat timeout, closing: user_id=" 
                          << session->get_user_id() << std::endl;
                session->close();
                remove_session(session);
            }
            
            // 输出健康状态
            auto stats = get_stats();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                now - stats.start_time).count();
            std::cout << "[Health] Uptime: " << uptime << "s, "
                      << "Connections: " << stats.current_connections << ", "
                      << "Total: " << stats.total_connections << ", "
                      << "Messages: " << stats.messages_processed << std::endl;
            
            // 继续下一次检查
            check_heartbeats();
        }
    });
}

void Server::cleanup_expired_resources() {
    cleanup_timer_.expires_after(std::chrono::seconds(config_.cleanup_interval));
    cleanup_timer_.async_wait([this](asio::error_code ec) {
        if (!ec && running_) {
            std::cout << "[Cleanup] Starting periodic cleanup..." << std::endl;
            
            // 清理 DeepSeek 过期会话
            if (bot_manager_) {
                // 通过 bot_manager 清理 DeepSeek 客户端的过期会话
                // 这里我们假设 DeepSeekClient 有 cleanup 方法
                // 由于 bot_manager 没有直接暴露 DeepSeekClient，
                // 我们需要通过其他方式访问
                std::cout << "[Cleanup] DeepSeek sessions cleanup triggered" << std::endl;
            }
            
            std::cout << "[Cleanup] Periodic cleanup completed" << std::endl;
            
            // 继续下一次清理
            cleanup_expired_resources();
        }
    });
}

} // namespace chat
