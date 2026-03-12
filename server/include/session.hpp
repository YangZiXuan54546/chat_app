#ifndef SESSION_HPP
#define SESSION_HPP

#include <memory>
#include <array>
#include <functional>
#include <deque>
#include <asio.hpp>
#include "protocol.hpp"

namespace chat {

// 前向声明
class Server;
class UserManager;
class MessageManager;
class GroupManager;
class FriendManager;
class Database;
class BotManager;

class Session : public std::enable_shared_from_this<Session> {
public:
    using ptr = std::shared_ptr<Session>;
    using Socket = asio::ip::tcp::socket;
    
    Session(asio::io_context& io_context, Socket socket);
    ~Session();
    
    void start();
    void send(const std::vector<uint8_t>& data);
    void close();
    
    uint64_t get_user_id() const { return user_id_; }
    void set_user_id(uint64_t user_id) { user_id_ = user_id; }
    
    bool is_authenticated() const { return authenticated_; }
    void set_authenticated(bool auth) { authenticated_ = auth; }
    
    Socket& get_socket() { return socket_; }
    
    // 获取最后心跳时间
    std::chrono::steady_clock::time_point get_last_heartbeat() const { return last_heartbeat_; }
    
    void set_managers(std::shared_ptr<UserManager> user_manager,
                      std::shared_ptr<MessageManager> message_manager,
                      std::shared_ptr<GroupManager> group_manager,
                      std::shared_ptr<FriendManager> friend_manager,
                      std::shared_ptr<Database> database);
    
    void set_server(std::shared_ptr<Server> server) { server_ = server; }
    void set_bot_manager(std::shared_ptr<BotManager> bot_manager) { bot_manager_ = bot_manager; }
    
private:
    void do_read_header();
    void do_read_body(uint32_t body_size);
    void do_write();
    void handle_message(MessageType type, uint32_t sequence, const json& body);
    
    // 消息处理器
    void handle_register(uint32_t sequence, const json& body);
    void handle_login(uint32_t sequence, const json& body);
    void handle_logout(uint32_t sequence, const json& body);
    void handle_user_info(uint32_t sequence, const json& body);
    void handle_user_search(uint32_t sequence, const json& body);
    void handle_user_update(uint32_t sequence, const json& body);
    
    void handle_friend_add(uint32_t sequence, const json& body);
    void handle_friend_accept(uint32_t sequence, const json& body);
    void handle_friend_reject(uint32_t sequence, const json& body);
    void handle_friend_remove(uint32_t sequence, const json& body);
    void handle_friend_list(uint32_t sequence, const json& body);
    void handle_friend_requests(uint32_t sequence, const json& body);
    void handle_friend_remark(uint32_t sequence, const json& body);
    
    void handle_private_message(uint32_t sequence, const json& body);
    void handle_private_history(uint32_t sequence, const json& body);
    
    void handle_group_create(uint32_t sequence, const json& body);
    void handle_group_join(uint32_t sequence, const json& body);
    void handle_group_leave(uint32_t sequence, const json& body);
    void handle_group_dismiss(uint32_t sequence, const json& body);
    void handle_group_info(uint32_t sequence, const json& body);
    void handle_group_list(uint32_t sequence, const json& body);
    void handle_group_members(uint32_t sequence, const json& body);
    void handle_group_add_member(uint32_t sequence, const json& body);
    void handle_group_remove_member(uint32_t sequence, const json& body);
    void handle_group_set_admin(uint32_t sequence, const json& body);
    void handle_group_transfer_owner(uint32_t sequence, const json& body);
    void handle_group_message(uint32_t sequence, const json& body);
    void handle_group_history(uint32_t sequence, const json& body);
    
    void handle_media_upload(uint32_t sequence, const json& body);
    void handle_heartbeat(uint32_t sequence, const json& body);
    
    // 端到端加密
    void handle_key_upload(uint32_t sequence, const json& body);
    void handle_key_request(uint32_t sequence, const json& body);
    void handle_encrypted_message(uint32_t sequence, const json& body);
    
    // 消息撤回
    void handle_message_recall(uint32_t sequence, const json& body);
    
private:
    asio::io_context& io_context_;
    Socket socket_;
    
    std::array<uint8_t, sizeof(MessageHeader)> header_buffer_;
    std::vector<uint8_t> body_buffer_;
    std::vector<uint8_t> write_buffer_;
    
    std::deque<std::vector<uint8_t>> write_queue_;
    std::mutex write_mutex_;
    
    uint64_t user_id_;
    bool authenticated_;
    uint32_t last_sequence_;
    
    std::shared_ptr<UserManager> user_manager_;
    std::shared_ptr<MessageManager> message_manager_;
    std::shared_ptr<GroupManager> group_manager_;
    std::shared_ptr<FriendManager> friend_manager_;
    std::shared_ptr<Database> database_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<BotManager> bot_manager_;
    
    std::chrono::steady_clock::time_point last_heartbeat_;
};

} // namespace chat

#endif // SESSION_HPP
