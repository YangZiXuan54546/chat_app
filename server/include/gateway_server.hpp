#ifndef GATEWAY_SERVER_HPP
#define GATEWAY_SERVER_HPP

#include <string>
#include <memory>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <microhttpd.h>

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

// HTTP 请求结构
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::string content_type;
    std::string authorization;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    struct MHD_Connection* connection;
};

// HTTP 响应结构
struct HttpResponse {
    int status_code;
    std::string content_type;
    std::string body;
    std::map<std::string, std::string> headers;
    
    static HttpResponse json(int status, const std::string& body);
    static HttpResponse error(int status, const std::string& message);
    static HttpResponse file_data(int status, const std::string& data, const std::string& content_type);
};

// WebSocket 连接
class WebSocketConnection {
public:
    using ptr = std::shared_ptr<WebSocketConnection>;
    
    WebSocketConnection(struct MHD_Connection* connection, const std::string& key);
    ~WebSocketConnection();
    
    bool send(const std::string& message);
    bool send_binary(const std::vector<uint8_t>& data);
    void close();
    
    uint64_t get_user_id() const { return user_id_; }
    void set_user_id(uint64_t id) { user_id_ = id; }
    
    bool is_authenticated() const { return authenticated_; }
    void set_authenticated(bool auth) { authenticated_ = auth; }
    
private:
    struct MHD_Connection* connection_;
    std::string websocket_key_;
    uint64_t user_id_ = 0;
    bool authenticated_ = false;
};

// 路由处理器类型
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;
using WsHandler = std::function<void(WebSocketConnection::ptr, const std::string&)>;

// 网关服务器配置
struct GatewayConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8888;
    std::string media_dir = "./media";
    size_t max_upload_size = 10 * 1024 * 1024; // 10MB
};

// 网关服务器
class GatewayServer {
public:
    GatewayServer(const GatewayConfig& config);
    ~GatewayServer();
    
    bool start();
    void stop();
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
    
    // 路由注册
    void register_route(const std::string& method, const std::string& path, HttpHandler handler);
    void register_ws_route(const std::string& path, WsHandler handler);
    
    // WebSocket 广播
    void broadcast(const std::string& message);
    void broadcast_to_user(uint64_t user_id, const std::string& message);
    
    // 获取在线 WebSocket 连接
    std::vector<WebSocketConnection::ptr> get_connections();
    WebSocketConnection::ptr get_connection(uint64_t user_id);
    
private:
    // MHD 请求处理器
    static MHD_Result request_handler(
        void* cls,
        struct MHD_Connection* connection,
        const char* url,
        const char* method,
        const char* version,
        const char* upload_data,
        size_t* upload_data_size,
        void** con_cls);
    
    // WebSocket 升级处理
    MHD_Result handle_websocket_upgrade(
        struct MHD_Connection* connection,
        const HttpRequest& request);
    
    // HTTP 请求处理
    HttpResponse handle_request(const HttpRequest& request);
    
    // 路由匹配
    HttpHandler find_handler(const std::string& method, const std::string& path);
    
    // 内部处理
    void setup_routes();
    HttpResponse handle_api_upload(const HttpRequest& request);
    HttpResponse handle_media_download(const HttpRequest& request);
    HttpResponse handle_health_check(const HttpRequest& request);
    void handle_ws_message(WebSocketConnection::ptr conn, const std::string& message);
    
private:
    GatewayConfig config_;
    struct MHD_Daemon* daemon_ = nullptr;
    std::atomic<bool> running_{false};
    
    // 依赖
    std::shared_ptr<Database> database_;
    std::shared_ptr<UserManager> user_manager_;
    std::shared_ptr<MessageManager> message_manager_;
    std::shared_ptr<GroupManager> group_manager_;
    std::shared_ptr<FriendManager> friend_manager_;
    std::shared_ptr<BotManager> bot_manager_;
    std::shared_ptr<FcmManager> fcm_manager_;
    std::shared_ptr<JPushManager> jpush_manager_;
    
    // 路由表
    std::map<std::string, HttpHandler> routes_; // key: "METHOD:PATH"
    std::map<std::string, WsHandler> ws_routes_;
    
    // WebSocket 连接管理
    std::map<uint64_t, WebSocketConnection::ptr> ws_connections_;
    std::mutex ws_mutex_;
};

} // namespace chat

#endif // GATEWAY_SERVER_HPP
