#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace chat {

// 前向声明
class Database;
class UserManager;
class MessageManager;
class GroupManager;
class FriendManager;
class BotManager;
class FcmManager;
class JPushManager;

// WebSocket 操作码 (RFC 6455)
enum class WsOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

// WebSocket 帧
struct WsFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_len;
    std::vector<uint8_t> masking_key;
    std::vector<uint8_t> payload;
};

// WebSocket 连接
class WsConnection : public std::enable_shared_from_this<WsConnection> {
public:
    using ptr = std::shared_ptr<WsConnection>;
    using MessageHandler = std::function<void(ptr, const std::string&)>;
    
    WsConnection(int socket, const std::string& client_ip);
    ~WsConnection();
    
    // 发送文本消息
    bool send_text(const std::string& message);
    
    // 发送二进制消息
    bool send_binary(const std::vector<uint8_t>& data);
    
    // 发送 Ping
    bool send_ping(const std::string& payload = "");
    
    // 发送 Pong
    bool send_pong(const std::string& payload = "");
    
    // 发送 Close 帧
    bool send_close(uint16_t code = 1000, const std::string& reason = "");
    
    // 关闭连接
    void close();
    
    // 获取原始 socket
    int get_socket() const { return socket_; }
    
    // 用户相关
    uint64_t get_user_id() const { return user_id_; }
    void set_user_id(uint64_t id) { user_id_ = id; }
    
    bool is_authenticated() const { return authenticated_; }
    void set_authenticated(bool auth) { authenticated_ = auth; }
    
    const std::string& get_client_ip() const { return client_ip_; }
    
    // 设置消息处理器
    void set_message_handler(MessageHandler handler) { message_handler_ = handler; }
    
    // 处理接收数据
    bool handle_receive(const uint8_t* data, size_t len);
    
    // 最后活动时间
    std::chrono::steady_clock::time_point last_activity() const { return last_activity_; }
    void update_activity() { last_activity_ = std::chrono::steady_clock::now(); }
    
private:
    // 解析 WebSocket 帧
    bool parse_frame(const uint8_t* data, size_t len, size_t& consumed);
    
    // 处理完整帧
    bool handle_frame(const WsFrame& frame);
    
    // 发送 WebSocket 帧
    bool send_frame(uint8_t opcode, const uint8_t* data, size_t len, bool fin = true);
    
    // 发送原始数据
    bool send_raw(const uint8_t* data, size_t len);
    
private:
    int socket_;
    std::string client_ip_;
    uint64_t user_id_ = 0;
    bool authenticated_ = false;
    std::atomic<bool> closed_{false};
    
    MessageHandler message_handler_;
    
    // 接收缓冲区
    std::vector<uint8_t> recv_buffer_;
    
    // 消息重组
    bool in_fragmented_ = false;
    uint8_t fragment_opcode_ = 0;
    std::vector<uint8_t> fragment_data_;
    
    // 最后活动时间
    std::chrono::steady_clock::time_point last_activity_;
};

// WebSocket 服务器配置
struct WsServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8888;
    size_t max_connections = 1000;
    size_t max_message_size = 10 * 1024 * 1024; // 10MB
    uint32_t heartbeat_interval = 30; // 秒
    uint32_t heartbeat_timeout = 60; // 秒
};

// WebSocket 服务器
class WebSocketServer {
public:
    using MessageHandler = std::function<void(WsConnection::ptr, const std::string&)>;
    using ConnectionHandler = std::function<void(WsConnection::ptr)>;
    
    WebSocketServer(const WsServerConfig& config);
    ~WebSocketServer();
    
    // 启动服务器
    bool start();
    
    // 停止服务器
    void stop();
    
    // 运行事件循环
    void run();
    
    // 设置依赖
    void set_database(std::shared_ptr<Database> db) { database_ = db; }
    void set_user_manager(std::shared_ptr<UserManager> mgr) { user_manager_ = mgr; }
    void set_message_manager(std::shared_ptr<MessageManager> mgr) { message_manager_ = mgr; }
    void set_group_manager(std::shared_ptr<GroupManager> mgr) { group_manager_ = mgr; }
    void set_friend_manager(std::shared_ptr<FriendManager> mgr) { friend_manager_ = mgr; }
    void set_bot_manager(std::shared_ptr<BotManager> mgr) { bot_manager_ = mgr; }
    void set_fcm_manager(std::shared_ptr<FcmManager> mgr) { fcm_manager_ = mgr; }
    void set_jpush_manager(std::shared_ptr<JPushManager> mgr) { jpush_manager_ = mgr; }
    
    // 设置消息处理器
    void set_message_handler(MessageHandler handler) { message_handler_ = handler; }
    void set_connect_handler(ConnectionHandler handler) { connect_handler_ = handler; }
    void set_disconnect_handler(ConnectionHandler handler) { disconnect_handler_ = handler; }
    
    // 广播消息
    void broadcast(const std::string& message);
    void broadcast_to_user(uint64_t user_id, const std::string& message);
    void broadcast_to_users(const std::vector<uint64_t>& user_ids, const std::string& message);
    
    // 获取连接
    WsConnection::ptr get_connection(uint64_t user_id);
    std::vector<WsConnection::ptr> get_all_connections();
    size_t get_connection_count();
    
    // 检查用户在线状态
    bool is_user_online(uint64_t user_id);
    
private:
    // 接受连接线程
    void accept_loop();
    
    // 处理新连接
    void handle_new_connection(int client_socket, const std::string& client_ip);
    
    // 执行 WebSocket 握手
    bool perform_handshake(int socket, const std::string& request);
    
    // 心跳检查线程
    void heartbeat_loop();
    
    // 移除连接
    void remove_connection(WsConnection::ptr conn);
    
    // 计算 Sec-WebSocket-Accept
    static std::string compute_accept_key(const std::string& key);
    
private:
    WsServerConfig config_;
    int listen_socket_ = -1;
    std::atomic<bool> running_{false};
    
    // 线程
    std::thread accept_thread_;
    std::thread heartbeat_thread_;
    
    // 依赖
    std::shared_ptr<Database> database_;
    std::shared_ptr<UserManager> user_manager_;
    std::shared_ptr<MessageManager> message_manager_;
    std::shared_ptr<GroupManager> group_manager_;
    std::shared_ptr<FriendManager> friend_manager_;
    std::shared_ptr<BotManager> bot_manager_;
    std::shared_ptr<FcmManager> fcm_manager_;
    std::shared_ptr<JPushManager> jpush_manager_;
    
    // 处理器
    MessageHandler message_handler_;
    ConnectionHandler connect_handler_;
    ConnectionHandler disconnect_handler_;
    
    // 连接管理
    std::map<int, WsConnection::ptr> connections_; // socket -> connection
    std::map<uint64_t, WsConnection::ptr> user_connections_; // user_id -> connection
    std::mutex conn_mutex_;
};

} // namespace chat

#endif // WEBSOCKET_SERVER_HPP
