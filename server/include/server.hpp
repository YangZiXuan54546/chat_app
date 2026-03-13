#ifndef SERVER_HPP
#define SERVER_HPP

#include <memory>
#include <map>
#include <mutex>
#include <functional>
#include <thread>
#include <atomic>
#include <asio.hpp>
#include "protocol.hpp"
#include "session.hpp"
#include "thread_pool.hpp"

namespace chat {

// 前向声明
class UserManager;
class MessageManager;
class GroupManager;
class FriendManager;
class Database;
class BotManager;
class FcmManager;
class JPushManager;

class Server : public std::enable_shared_from_this<Server> {
public:
    using IOContext = asio::io_context;
    using Acceptor = asio::ip::tcp::acceptor;
    using Endpoint = asio::ip::tcp::endpoint;
    using Socket = asio::ip::tcp::socket;
    using SignalSet = asio::signal_set;
    
    struct Config {
        std::string host = "0.0.0.0";
        uint16_t port = 8888;
        int thread_count = 4;
        int heartbeat_timeout = 60; // seconds
        int cleanup_interval = 300;  // seconds - 清理过期会话的间隔
    };
    
    Server(const Config& config);
    ~Server();
    
    bool start();
    void stop();
    void run();
    
    // 会话管理
    void add_session(Session::ptr session);
    void remove_session(Session::ptr session);
    Session::ptr get_session(uint64_t user_id);
    void broadcast(const std::vector<uint8_t>& data);
    void broadcast_to_group(uint64_t group_id, const std::vector<uint8_t>& data);
    void send_to_user(uint64_t user_id, const std::vector<uint8_t>& data);
    
    // 设置用户上线/下线
    void set_user_online(uint64_t user_id, Session::ptr session);
    void set_user_offline(uint64_t user_id);
    bool is_user_online(uint64_t user_id);
    bool is_user_active(uint64_t user_id);  // 检查用户是否活跃（最近有心跳）
    
    // 获取在线用户列表
    std::vector<uint64_t> get_online_users();
    
    // 设置管理器
    void set_managers(std::shared_ptr<UserManager> user_manager,
                      std::shared_ptr<MessageManager> message_manager,
                      std::shared_ptr<GroupManager> group_manager,
                      std::shared_ptr<FriendManager> friend_manager,
                      std::shared_ptr<Database> database);
    
    // 设置机器人管理器
    void set_bot_manager(std::shared_ptr<BotManager> bot_manager);
    
    // 设置 FCM 管理器
    void set_fcm_manager(std::shared_ptr<FcmManager> fcm_manager);
    std::shared_ptr<FcmManager> get_fcm_manager() { return fcm_manager_; }
    
    // 设置 JPush 管理器
    void set_jpush_manager(std::shared_ptr<JPushManager> jpush_manager);
    std::shared_ptr<JPushManager> get_jpush_manager() { return jpush_manager_; }
    
    // 获取 io_context（用于异步操作）
    IOContext& get_io_context() { return io_context_; }
    
    // 获取线程池（用于数据库操作）
    ThreadPool& get_thread_pool() { return *thread_pool_; }
    
    // 获取服务器统计信息
    struct Stats {
        size_t total_connections = 0;
        size_t current_connections = 0;
        size_t messages_processed = 0;
        std::chrono::steady_clock::time_point start_time;
    };
    Stats get_stats();
    
private:
    void do_accept();
    void handle_accept(Session::ptr session, const asio::error_code& ec);
    void check_heartbeats();
    void cleanup_expired_resources();  // 定期清理过期资源
    
private:
    Config config_;
    IOContext io_context_;
    Acceptor acceptor_;
    SignalSet signals_;
    
    // Work guard - 防止 io_context 在没有任务时停止
    std::unique_ptr<asio::executor_work_guard<IOContext::executor_type>> work_guard_;
    
    // 线程池用于处理数据库操作
    std::unique_ptr<ThreadPool> thread_pool_;
    
    std::map<uint64_t, Session::ptr> sessions_;
    std::mutex sessions_mutex_;
    
    std::atomic<bool> running_;
    std::vector<std::thread> threads_;
    
    asio::steady_timer heartbeat_timer_;
    asio::steady_timer cleanup_timer_;  // 清理定时器
    
    // 管理器
    std::shared_ptr<UserManager> user_manager_;
    std::shared_ptr<MessageManager> message_manager_;
    std::shared_ptr<GroupManager> group_manager_;
    std::shared_ptr<FriendManager> friend_manager_;
    std::shared_ptr<Database> database_;
    std::shared_ptr<BotManager> bot_manager_;
    std::shared_ptr<FcmManager> fcm_manager_;
    std::shared_ptr<JPushManager> jpush_manager_;
    
    // 统计信息
    std::atomic<size_t> total_connections_{0};
    std::atomic<size_t> messages_processed_{0};
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace chat

#endif // SERVER_HPP
