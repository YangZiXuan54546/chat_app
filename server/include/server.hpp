#ifndef SERVER_HPP
#define SERVER_HPP

#include <memory>
#include <map>
#include <mutex>
#include <functional>
#include <asio.hpp>
#include "protocol.hpp"
#include "session.hpp"

namespace chat {

// 前向声明
class UserManager;
class MessageManager;
class GroupManager;
class FriendManager;
class Database;

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
    
    // 获取在线用户列表
    std::vector<uint64_t> get_online_users();
    
    // 设置管理器
    void set_managers(std::shared_ptr<UserManager> user_manager,
                      std::shared_ptr<MessageManager> message_manager,
                      std::shared_ptr<GroupManager> group_manager,
                      std::shared_ptr<FriendManager> friend_manager,
                      std::shared_ptr<Database> database);
    
private:
    void do_accept();
    void handle_accept(Session::ptr session, const asio::error_code& ec);
    void check_heartbeats();
    
private:
    Config config_;
    IOContext io_context_;
    Acceptor acceptor_;
    SignalSet signals_;
    
    std::map<uint64_t, Session::ptr> sessions_;
    std::mutex sessions_mutex_;
    
    std::atomic<bool> running_;
    std::vector<std::thread> threads_;
    
    asio::steady_timer heartbeat_timer_;
    
    // 管理器
    std::shared_ptr<UserManager> user_manager_;
    std::shared_ptr<MessageManager> message_manager_;
    std::shared_ptr<GroupManager> group_manager_;
    std::shared_ptr<FriendManager> friend_manager_;
    std::shared_ptr<Database> database_;
};

} // namespace chat

#endif // SERVER_HPP
