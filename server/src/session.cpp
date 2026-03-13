#include "session.hpp"
#include "server.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include "database.hpp"
#include "bot_manager.hpp"
#include "fcm_manager.hpp"
#include <chrono>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <ctime>
#include <iomanip>
#include <cstdlib>

namespace chat {

Session::Session(asio::io_context& io_context, Socket socket)
    : io_context_(io_context)
    , socket_(std::move(socket))
    , user_id_(0)
    , authenticated_(false)
    , last_sequence_(0)
    , last_heartbeat_(std::chrono::steady_clock::now()) {
}

Session::~Session() {
    close();
}

void Session::start() {
    do_read_header();
}

void Session::send(const std::vector<uint8_t>& data) {
    bool write_in_progress = false;
    
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_queue_.empty();
        write_queue_.push_back(data);
    }
    
    if (!write_in_progress) {
        do_write();
    }
}

void Session::close() {
    asio::error_code ec;
    socket_.close(ec);
}

void Session::set_managers(std::shared_ptr<UserManager> user_manager,
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

void Session::do_read_header() {
    auto self = shared_from_this();
    
    asio::async_read(socket_, asio::buffer(header_buffer_),
        [this, self](asio::error_code ec, std::size_t /*length*/) {
            if (ec) {
                // 连接断开
                return;
            }
            
            MessageHeader header;
            if (!Protocol::parse_header(header_buffer_.data(), 
                                        header_buffer_.size(), header)) {
                // 协议错误
                close();
                return;
            }
            
            last_sequence_ = header.sequence;
            do_read_body(header.length);
        });
}

void Session::do_read_body(uint32_t body_size) {
    auto self = shared_from_this();
    
    body_buffer_.resize(body_size);
    
    asio::async_read(socket_, asio::buffer(body_buffer_),
        [this, self, body_size](asio::error_code ec, std::size_t /*length*/) {
            if (ec) {
                return;
            }
            
            MessageHeader header;
            Protocol::parse_header(header_buffer_.data(), header_buffer_.size(), header);
            json body = Protocol::parse_body(body_buffer_.data(), body_size);
            
            // 在线程池中处理消息，避免阻塞 io_context
            auto self = shared_from_this();
            auto server = server_;
            if (server) {
                // 获取线程池并提交任务
                asio::post(io_context_, [this, self, header, body]() {
                    handle_message(header.type, header.sequence, body);
                });
            } else {
                handle_message(header.type, header.sequence, body);
            }
            
            // 继续读取下一条消息
            do_read_header();
        });
}

void Session::do_write() {
    auto self = shared_from_this();
    
    std::vector<uint8_t> data;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.empty()) {
            return;
        }
        data = std::move(write_queue_.front());
        write_queue_.pop_front();
    }
    
    asio::async_write(socket_, asio::buffer(data),
        [this, self](asio::error_code ec, std::size_t /*length*/) {
            if (ec) {
                close();
                return;
            }
            
            // 继续发送队列中的消息
            do_write();
        });
}

void Session::handle_message(MessageType type, uint32_t sequence, const json& body) {
    last_heartbeat_ = std::chrono::steady_clock::now();
    
    switch (type) {
        // 认证相关
        case MessageType::REGISTER:
            handle_register(sequence, body);
            break;
        case MessageType::LOGIN:
            handle_login(sequence, body);
            break;
        case MessageType::LOGOUT:
            handle_logout(sequence, body);
            break;
            
        // 用户相关
        case MessageType::USER_INFO:
            handle_user_info(sequence, body);
            break;
        case MessageType::USER_SEARCH:
            handle_user_search(sequence, body);
            break;
        case MessageType::USER_UPDATE:
            handle_user_update(sequence, body);
            break;
            
        // 好友相关
        case MessageType::FRIEND_ADD:
            handle_friend_add(sequence, body);
            break;
        case MessageType::FRIEND_ACCEPT:
            handle_friend_accept(sequence, body);
            break;
        case MessageType::FRIEND_REJECT:
            handle_friend_reject(sequence, body);
            break;
        case MessageType::FRIEND_REMOVE:
            handle_friend_remove(sequence, body);
            break;
        case MessageType::FRIEND_LIST:
            handle_friend_list(sequence, body);
            break;
        case MessageType::FRIEND_REQUESTS:
            handle_friend_requests(sequence, body);
            break;
        case MessageType::FRIEND_REMARK:
            handle_friend_remark(sequence, body);
            break;
            
        // 私聊消息
        case MessageType::PRIVATE_MESSAGE:
            handle_private_message(sequence, body);
            break;
        case MessageType::PRIVATE_HISTORY:
            handle_private_history(sequence, body);
            break;
            
        // 群组相关
        case MessageType::GROUP_CREATE:
            handle_group_create(sequence, body);
            break;
        case MessageType::GROUP_JOIN:
            handle_group_join(sequence, body);
            break;
        case MessageType::GROUP_LEAVE:
            handle_group_leave(sequence, body);
            break;
        case MessageType::GROUP_DISMISS:
            handle_group_dismiss(sequence, body);
            break;
        case MessageType::GROUP_INFO:
            handle_group_info(sequence, body);
            break;
        case MessageType::GROUP_LIST:
            handle_group_list(sequence, body);
            break;
        case MessageType::GROUP_MEMBERS:
            handle_group_members(sequence, body);
            break;
        case MessageType::GROUP_ADD_MEMBER:
            handle_group_add_member(sequence, body);
            break;
        case MessageType::GROUP_REMOVE_MEMBER:
            handle_group_remove_member(sequence, body);
            break;
        case MessageType::GROUP_SET_ADMIN:
            handle_group_set_admin(sequence, body);
            break;
        case MessageType::GROUP_TRANSFER_OWNER:
            handle_group_transfer_owner(sequence, body);
            break;
            
        // 群聊消息
        case MessageType::GROUP_MESSAGE:
            handle_group_message(sequence, body);
            break;
        case MessageType::GROUP_HISTORY:
            handle_group_history(sequence, body);
            break;
            
        // 媒体上传
        case MessageType::MEDIA_UPLOAD:
            handle_media_upload(sequence, body);
            break;
            
        // 心跳
        case MessageType::HEARTBEAT:
            handle_heartbeat(sequence, body);
            break;
            
        // 端到端加密
        case MessageType::KEY_UPLOAD:
            handle_key_upload(sequence, body);
            break;
        case MessageType::KEY_REQUEST:
            handle_key_request(sequence, body);
            break;
        case MessageType::ENCRYPTED_MESSAGE:
            handle_encrypted_message(sequence, body);
            break;
            
        // 消息撤回
        case MessageType::MESSAGE_RECALL:
            handle_message_recall(sequence, body);
            break;
            
        // FCM Token 注册
        case MessageType::FCM_TOKEN_REGISTER:
            handle_fcm_token_register(sequence, body);
            break;
            
        default:
            send(Protocol::create_error(sequence, 400, "Unknown message type"));
            break;
    }
}

// ==================== 认证相关处理器 ====================

void Session::handle_register(uint32_t sequence, const json& body) {
    std::string username = body.value("username", "");
    std::string password = body.value("password", "");
    std::string nickname = body.value("nickname", "");
    
    if (username.empty() || password.empty()) {
        send(Protocol::create_error(sequence, 400, "Username and password are required"));
        return;
    }
    
    if (nickname.empty()) {
        nickname = username;
    }
    
    uint64_t user_id = 0;
    std::string error;
    
    if (user_manager_->register_user(username, password, nickname, user_id, error)) {
        json response = {
            {"user_id", user_id},
            {"username", username},
            {"nickname", nickname}
        };
        send(Protocol::create_response(MessageType::REGISTER_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_login(uint32_t sequence, const json& body) {
    std::string username = body.value("username", "");
    std::string password = body.value("password", "");
    
    if (username.empty() || password.empty()) {
        send(Protocol::create_error(sequence, 400, "Username and password are required"));
        return;
    }
    
    uint64_t user_id = 0;
    UserInfo user_info;
    std::string error;
    
    if (user_manager_->login(username, password, user_id, user_info, error)) {
        set_user_id(user_id);
        set_authenticated(true);
        
        user_manager_->set_online_status(user_id, OnlineStatus::ONLINE);
        
        // 关联用户会话，以便消息转发
        if (server_) {
            server_->set_user_online(user_id, shared_from_this());
        }
        
        json response = {
            {"user_id", user_id},
            {"user_info", user_info.to_json()}
        };
        send(Protocol::create_response(MessageType::LOGIN_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 401, error));
    }
}

void Session::handle_logout(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    user_manager_->set_online_status(user_id_, OnlineStatus::OFFLINE);
    set_authenticated(false);
    
    json response = {{"success", true}};
    send(Protocol::create_response(MessageType::LOGOUT_RESPONSE, sequence, response));
    
    close();
}

// ==================== 用户相关处理器 ====================

void Session::handle_user_info(uint32_t sequence, const json& body) {
    uint64_t target_user_id = body.value("user_id", 0);
    
    if (target_user_id == 0) {
        if (!is_authenticated()) {
            send(Protocol::create_error(sequence, 401, "Not authenticated"));
            return;
        }
        target_user_id = user_id_;
    }
    
    UserInfo user;
    if (database_->get_user_by_id(target_user_id, user)) {
        send(Protocol::create_response(MessageType::USER_INFO_RESPONSE, sequence, user.to_json()));
    } else {
        send(Protocol::create_error(sequence, 404, "User not found"));
    }
}

void Session::handle_user_search(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    std::string keyword = body.value("keyword", "");
    int limit = body.value("limit", 20);
    
    auto users = user_manager_->search_users(keyword, limit);
    
    json::array_t users_json;
    for (const auto& user : users) {
        users_json.push_back(user.to_json());
    }
    
    json response = {{"users", users_json}};
    send(Protocol::create_response(MessageType::USER_SEARCH_RESPONSE, sequence, response));
}

void Session::handle_user_update(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    UserInfo user;
    database_->get_user_by_id(user_id_, user);
    
    if (body.contains("nickname")) {
        user.nickname = body["nickname"].get<std::string>();
    }
    if (body.contains("avatar_url")) {
        user.avatar_url = body["avatar_url"].get<std::string>();
    }
    if (body.contains("signature")) {
        user.signature = body["signature"].get<std::string>();
    }
    
    if (user_manager_->update_user_info(user)) {
        send(Protocol::create_response(MessageType::USER_UPDATE_RESPONSE, sequence, user.to_json()));
    } else {
        send(Protocol::create_error(sequence, 500, "Failed to update user info"));
    }
}

// ==================== 好友相关处理器 ====================

void Session::handle_friend_add(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t friend_id = body.value("friend_id", 0);
    if (friend_id == 0) {
        send(Protocol::create_error(sequence, 400, "Friend ID is required"));
        return;
    }
    
    std::string error;
    if (friend_manager_->add_friend_request(user_id_, friend_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::FRIEND_ADD_RESPONSE, sequence, response));
        
        // 检查是否添加机器人好友
        if (bot_manager_ && bot_manager_->is_bot(friend_id)) {
            // 机器人自动接受好友请求
            bot_manager_->handle_friend_request(user_id_);
        }
        
        // 通知目标用户（如果在线）
        if (server_) {
            // 获取发送者信息
            UserInfo sender_info;
            database_->get_user_by_id(user_id_, sender_info);
            
            // 构建好友请求通知
            json notification = {
                {"from_user_id", user_id_},
                {"from_username", sender_info.username},
                {"from_nickname", sender_info.nickname},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()}
            };
            
            server_->send_to_user(friend_id, 
                Protocol::serialize(MessageType::FRIEND_ADD, 0, notification));
        }
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_friend_accept(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t friend_id = body.value("friend_id", 0);
    if (friend_id == 0) {
        send(Protocol::create_error(sequence, 400, "Friend ID is required"));
        return;
    }
    
    std::string error;
    if (friend_manager_->accept_friend_request(user_id_, friend_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::FRIEND_ACCEPT_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_friend_reject(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t friend_id = body.value("friend_id", 0);
    if (friend_id == 0) {
        send(Protocol::create_error(sequence, 400, "Friend ID is required"));
        return;
    }
    
    std::string error;
    if (friend_manager_->reject_friend_request(user_id_, friend_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::FRIEND_REJECT_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_friend_remove(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t friend_id = body.value("friend_id", 0);
    if (friend_id == 0) {
        send(Protocol::create_error(sequence, 400, "Friend ID is required"));
        return;
    }
    
    std::string error;
    if (friend_manager_->remove_friend(user_id_, friend_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::FRIEND_REMOVE_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_friend_list(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    auto friends = friend_manager_->get_friend_list(user_id_);
    
    json::array_t friends_json;
    for (const auto& [user, relation] : friends) {
        friends_json.push_back({
            {"user", user.to_json()},
            {"relation", relation.to_json()}
        });
    }
    
    json response = {{"friends", friends_json}};
    send(Protocol::create_response(MessageType::FRIEND_LIST_RESPONSE, sequence, response));
}

void Session::handle_friend_requests(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    auto requests = friend_manager_->get_friend_requests(user_id_);
    
    json::array_t requests_json;
    for (const auto& [user, relation] : requests) {
        requests_json.push_back({
            {"user", user.to_json()},
            {"relation", relation.to_json()}
        });
    }
    
    json response = {{"requests", requests_json}};
    send(Protocol::create_response(MessageType::FRIEND_REQUESTS_RESPONSE, sequence, response));
}

void Session::handle_friend_remark(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t friend_id = body.value("friend_id", 0);
    std::string remark = body.value("remark", "");
    
    if (friend_id == 0) {
        send(Protocol::create_error(sequence, 400, "Friend ID is required"));
        return;
    }
    
    std::string error;
    if (friend_manager_->set_friend_remark(user_id_, friend_id, remark, error)) {
        json response = {{"success", true}, {"friend_id", friend_id}, {"remark", remark}};
        send(Protocol::create_response(MessageType::FRIEND_REMARK_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

// ==================== 私聊消息处理器 ====================

void Session::handle_private_message(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t receiver_id = body.value("receiver_id", 0);
    std::string content = body.value("content", "");
    int media_type_int = body.value("media_type", 0);
    std::string media_url = body.value("media_url", "");
    std::string extra = body.value("extra", "");
    
    if (receiver_id == 0) {
        send(Protocol::create_error(sequence, 400, "Receiver ID is required"));
        return;
    }
    
    Message message;
    std::string error;
    
    if (message_manager_->send_private_message(user_id_, receiver_id, content,
                                               static_cast<MediaType>(media_type_int),
                                               media_url, extra, message, error)) {
        json response = message.to_json();
        send(Protocol::create_response(MessageType::PRIVATE_MESSAGE_RESPONSE, sequence, response));
        
        // 转发给接收者（如果在线）
        if (server_) {
            server_->send_to_user(receiver_id, 
                Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, message.to_json()));
        }
        
        // 检查是否发送给机器人
        if (bot_manager_ && bot_manager_->is_bot(receiver_id)) {
            if (bot_manager_->is_enabled()) {
                bot_manager_->handle_bot_message(user_id_, content, message.message_id);
            }
        }
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_private_history(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t peer_id = body.value("peer_id", 0);
    int64_t before_time = body.value("before_time", 0);
    int limit = body.value("limit", 50);
    
    if (peer_id == 0) {
        send(Protocol::create_error(sequence, 400, "Peer ID is required"));
        return;
    }
    
    auto messages = message_manager_->get_private_history(user_id_, peer_id, before_time, limit);
    
    json::array_t messages_json;
    for (const auto& msg : messages) {
        messages_json.push_back(msg.to_json());
    }
    
    json response = {{"messages", messages_json}};
    send(Protocol::create_response(MessageType::PRIVATE_HISTORY_RESPONSE, sequence, response));
}

// ==================== 群组相关处理器 ====================

void Session::handle_group_create(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    std::string group_name = body.value("group_name", "");
    std::string description = body.value("description", "");
    
    if (group_name.empty()) {
        send(Protocol::create_error(sequence, 400, "Group name is required"));
        return;
    }
    
    uint64_t group_id = 0;
    std::string error;
    
    if (group_manager_->create_group(user_id_, group_name, description, group_id, error)) {
        json response = {
            {"group_id", group_id},
            {"group_name", group_name}
        };
        send(Protocol::create_response(MessageType::GROUP_CREATE_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_join(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    std::string error;
    if (group_manager_->join_group(group_id, user_id_, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::GROUP_JOIN_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_leave(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    std::string error;
    if (group_manager_->leave_group(group_id, user_id_, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::GROUP_LEAVE_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_dismiss(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    std::string error;
    if (group_manager_->dismiss_group(user_id_, group_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::GROUP_DISMISS_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_info(uint32_t sequence, const json& body) {
    uint64_t group_id = body.value("group_id", 0);
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    GroupInfo group;
    if (group_manager_->get_group_info(group_id, group)) {
        send(Protocol::create_response(MessageType::GROUP_INFO_RESPONSE, sequence, group.to_json()));
    } else {
        send(Protocol::create_error(sequence, 404, "Group not found"));
    }
}

void Session::handle_group_list(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    auto groups = group_manager_->get_user_groups(user_id_);
    
    json::array_t groups_json;
    for (const auto& group : groups) {
        groups_json.push_back(group.to_json());
    }
    
    json response = {{"groups", groups_json}};
    send(Protocol::create_response(MessageType::GROUP_LIST_RESPONSE, sequence, response));
}

void Session::handle_group_members(uint32_t sequence, const json& body) {
    uint64_t group_id = body.value("group_id", 0);
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    auto member_ids = group_manager_->get_group_members(group_id);
    
    json::array_t members_json;
    for (uint64_t member_id : member_ids) {
        UserInfo user;
        if (database_->get_user_by_id(member_id, user)) {
            json member_info = {
                {"user_id", user.user_id},
                {"nickname", user.nickname},
                {"avatar_url", user.avatar_url},
                {"online_status", static_cast<int>(user.online_status)},
                {"is_owner", group_manager_->is_owner(group_id, member_id)},
                {"is_admin", group_manager_->is_admin(group_id, member_id)}
            };
            members_json.push_back(member_info);
        }
    }
    
    json response = {{"members", members_json}};
    send(Protocol::create_response(MessageType::GROUP_MEMBERS_RESPONSE, sequence, response));
}

void Session::handle_group_add_member(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    uint64_t user_id = body.value("user_id", 0);
    
    if (group_id == 0 || user_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID and User ID are required"));
        return;
    }
    
    std::string error;
    if (group_manager_->join_group(group_id, user_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::GROUP_ADD_MEMBER_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_remove_member(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    uint64_t user_id = body.value("user_id", 0);
    
    if (group_id == 0 || user_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID and User ID are required"));
        return;
    }
    
    std::string error;
    if (group_manager_->remove_member(user_id_, group_id, user_id, error)) {
        json response = {{"success", true}};
        send(Protocol::create_response(MessageType::GROUP_REMOVE_MEMBER_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_set_admin(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    uint64_t user_id = body.value("user_id", 0);
    bool is_admin = body.value("is_admin", true);
    
    if (group_id == 0 || user_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID and User ID are required"));
        return;
    }
    
    std::string error;
    if (group_manager_->set_admin(user_id_, group_id, user_id, is_admin, error)) {
        json response = {{"success", true}, {"user_id", user_id}, {"is_admin", is_admin}};
        send(Protocol::create_response(MessageType::GROUP_SET_ADMIN_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_transfer_owner(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    uint64_t new_owner_id = body.value("new_owner_id", 0);
    
    if (group_id == 0 || new_owner_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID and new owner ID are required"));
        return;
    }
    
    std::string error;
    if (group_manager_->transfer_owner(user_id_, group_id, new_owner_id, error)) {
        json response = {{"success", true}, {"new_owner_id", new_owner_id}};
        send(Protocol::create_response(MessageType::GROUP_TRANSFER_OWNER_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_message(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    std::string content = body.value("content", "");
    int media_type_int = body.value("media_type", 0);
    std::string media_url = body.value("media_url", "");
    std::string extra = body.value("extra", "");
    
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    Message message;
    std::string error;
    
    if (message_manager_->send_group_message(user_id_, group_id, content,
                                             static_cast<MediaType>(media_type_int),
                                             media_url, extra, message, error)) {
        json response = message.to_json();
        send(Protocol::create_response(MessageType::GROUP_MESSAGE_RESPONSE, sequence, response));
        
        // 广播给群成员
        server_->broadcast_to_group(group_id, Protocol::serialize(MessageType::GROUP_MESSAGE, 0, message.to_json()));
    } else {
        send(Protocol::create_error(sequence, 500, error));
    }
}

void Session::handle_group_history(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t group_id = body.value("group_id", 0);
    int64_t before_time = body.value("before_time", 0);
    int limit = body.value("limit", 50);
    
    if (group_id == 0) {
        send(Protocol::create_error(sequence, 400, "Group ID is required"));
        return;
    }
    
    auto messages = message_manager_->get_group_history(group_id, before_time, limit);
    
    json::array_t messages_json;
    for (const auto& msg : messages) {
        messages_json.push_back(msg.to_json());
    }
    
    json response = {{"messages", messages_json}};
    send(Protocol::create_response(MessageType::GROUP_HISTORY_RESPONSE, sequence, response));
}

// ==================== 媒体上传处理器 ====================

// Base64 解码函数
static std::vector<uint8_t> base64_decode(const std::string& encoded) {
    // Base64 解码表
    static const int decode_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    
    std::vector<uint8_t> decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : encoded) {
        if (decode_table[c] == -1) break;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

void Session::handle_media_upload(uint32_t sequence, const json& body) {
    if (!is_authenticated()) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    std::string file_name = body.value("file_name", "");
    std::string file_data_base64 = body.value("file_data", "");
    int media_type_int = body.value("media_type", 0);
    
    if (file_name.empty() || file_data_base64.empty()) {
        send(Protocol::create_error(sequence, 400, "File name and data are required"));
        return;
    }
    
    // 检查文件大小 (Base64 编码后约为原文件的 4/3 倍，限制 10MB 原始数据)
    const size_t max_file_size = 10 * 1024 * 1024;
    if (file_data_base64.size() > max_file_size * 4 / 3) {
        send(Protocol::create_error(sequence, 400, "File size exceeds limit (10MB)"));
        return;
    }
    
    // Base64 解码
    std::vector<uint8_t> file_data = base64_decode(file_data_base64);
    
    if (file_data.empty()) {
        send(Protocol::create_error(sequence, 400, "Invalid base64 data"));
        return;
    }
    
    // 创建媒体文件目录
    // 从环境变量获取媒体目录，默认为 ../media（相对于 build 目录）
    const char* env_media_dir = std::getenv("MEDIA_DIR");
    std::string media_dir = env_media_dir ? env_media_dir : "../media";
    mkdir(media_dir.c_str(), 0755);
    
    // 生成年月子目录
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_info = std::localtime(&now_time);
    char date_path[32];
    std::strftime(date_path, sizeof(date_path), "%Y/%m/%d", tm_info);
    
    std::string full_dir = media_dir + "/" + date_path;
    std::string cmd = "mkdir -p " + full_dir;
    system(cmd.c_str());
    
    // 生成唯一文件名
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    // 获取文件扩展名
    std::string extension;
    size_t dot_pos = file_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = file_name.substr(dot_pos);
    }
    
    std::ostringstream unique_name;
    unique_name << user_id_ << "_" << timestamp << extension;
    
    std::string file_path = full_dir + "/" + unique_name.str();
    
    // 写入文件
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file) {
        send(Protocol::create_error(sequence, 500, "Failed to save file"));
        return;
    }
    out_file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    out_file.close();
    
    // 保存到数据库
    uint64_t file_id = 0;
    std::string url;
    
    if (!database_->save_media_file(user_id_, unique_name.str(), file_path,
                                     static_cast<MediaType>(media_type_int),
                                     file_id, url)) {
        // 删除已保存的文件
        std::remove(file_path.c_str());
        send(Protocol::create_error(sequence, 500, "Failed to save file info"));
        return;
    }
    
    // 构建完整的 URL
    std::ostringstream url_stream;
    url_stream << "http://localhost:8889/media/" << file_id << "/" << unique_name.str();
    url = url_stream.str();
    
    json response = {
        {"file_id", file_id},
        {"url", url},
        {"file_name", file_name},
        {"file_size", file_data.size()},
        {"media_type", media_type_int}
    };
    send(Protocol::create_response(MessageType::MEDIA_UPLOAD_RESPONSE, sequence, response));
}

// ==================== 心跳处理器 ====================

void Session::handle_heartbeat(uint32_t sequence, const json& body) {
    json response = {
        {"server_time", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()}
    };
    send(Protocol::create_response(MessageType::HEARTBEAT_RESPONSE, sequence, response));
}

// ==================== 端到端加密处理器 ====================

void Session::handle_key_upload(uint32_t sequence, const json& body) {
    // 检查是否已认证
    if (!authenticated_) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    std::string public_key = body.value("public_key", "");
    if (public_key.empty()) {
        send(Protocol::create_error(sequence, 400, "Public key is required"));
        return;
    }
    
    // 保存公钥
    if (!database_->save_user_public_key(user_id_, public_key)) {
        send(Protocol::create_error(sequence, 500, "Failed to save public key"));
        return;
    }
    
    json response = {
        {"user_id", user_id_},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()}
    };
    send(Protocol::create_response(MessageType::KEY_UPLOAD_RESPONSE, sequence, response));
}

void Session::handle_key_request(uint32_t sequence, const json& body) {
    // 检查是否已认证
    if (!authenticated_) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t target_user_id = body.value("user_id", 0ULL);
    if (target_user_id == 0) {
        send(Protocol::create_error(sequence, 400, "User ID is required"));
        return;
    }
    
    // 获取目标用户的公钥
    std::string public_key;
    if (!database_->get_user_public_key(target_user_id, public_key)) {
        send(Protocol::create_error(sequence, 404, "Public key not found"));
        return;
    }
    
    json response = {
        {"user_id", target_user_id},
        {"public_key", public_key}
    };
    send(Protocol::create_response(MessageType::KEY_RESPONSE, sequence, response));
}

void Session::handle_encrypted_message(uint32_t sequence, const json& body) {
    // 检查是否已认证
    if (!authenticated_) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t receiver_id = body.value("receiver_id", 0ULL);
    std::string encrypted_key = body.value("encrypted_key", "");
    std::string encrypted_content = body.value("encrypted_content", "");
    std::string iv = body.value("iv", "");
    int media_type = body.value("media_type", 0);
    std::string media_url = body.value("media_url", "");
    
    if (receiver_id == 0 || encrypted_key.empty() || encrypted_content.empty()) {
        send(Protocol::create_error(sequence, 400, "Invalid encrypted message"));
        return;
    }
    
    // 构建加密消息
    Message message;
    message.sender_id = user_id_;
    message.receiver_id = receiver_id;
    message.group_id = 0;
    message.media_type = static_cast<MediaType>(media_type);
    message.media_url = media_url;
    message.status = MessageStatus::SENT;
    message.created_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // 将加密数据存储在 extra 字段中
    json extra = {
        {"encrypted", true},
        {"encrypted_key", encrypted_key},
        {"encrypted_content", encrypted_content},
        {"iv", iv}
    };
    message.extra = extra.dump();
    message.content = "[加密消息]";
    
    // 保存消息
    database_->save_private_message(message);
    
    // 发送响应给发送者
    json response = message.to_json();
    send(Protocol::create_response(MessageType::ENCRYPTED_MESSAGE_RESPONSE, sequence, response));
    
    // 转发给接收者
    if (server_) {
        server_->send_to_user(receiver_id,
            Protocol::serialize(MessageType::ENCRYPTED_MESSAGE, sequence, message.to_json()));
    }
}

void Session::handle_message_recall(uint32_t sequence, const json& body) {
    if (!authenticated_) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    uint64_t message_id = body.value("message_id", 0ULL);
    bool is_group = body.value("is_group", false);
    uint64_t group_id = body.value("group_id", 0ULL);
    
    if (message_id == 0) {
        send(Protocol::create_error(sequence, 400, "message_id is required"));
        return;
    }
    
    bool success = false;
    json response;
    
    if (is_group) {
        // 群消息撤回
        success = database_->recall_group_message(message_id, user_id_);
        if (success) {
            // 通知群成员
            std::vector<uint64_t> members;
            database_->get_group_members(group_id, members);
            
            json recall_notification = {
                {"message_id", message_id},
                {"recalled_by", user_id_},
                {"is_group", true},
                {"group_id", group_id},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()}
            };
            
            for (uint64_t member_id : members) {
                if (server_) {
                    server_->send_to_user(member_id,
                        Protocol::serialize(MessageType::MESSAGE_RECALL, sequence, recall_notification));
                }
            }
        }
    } else {
        // 私聊消息撤回
        success = database_->recall_private_message(message_id, user_id_);
        if (success) {
            // 获取消息的接收者
            uint64_t sender_id, msg_group_id;
            bool msg_is_group;
            if (database_->get_message_sender(message_id, sender_id, msg_is_group, msg_group_id)) {
                // 通知接收者
                json recall_notification = {
                    {"message_id", message_id},
                    {"recalled_by", user_id_},
                    {"is_group", false},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count()}
                };
                
                // 查找接收者（私聊时是对方）
                // 需要查询数据库获取 receiver_id
                // 这里简化处理，通知除了自己以外的相关用户
                if (server_) {
                    // 获取对方ID（从消息记录中）
                    // 发送给撤回者自己和其他相关用户
                    server_->broadcast(
                        Protocol::serialize(MessageType::MESSAGE_RECALL, sequence, recall_notification));
                }
            }
        }
    }
    
    if (success) {
        response = {
            {"message_id", message_id},
            {"success", true}
        };
    } else {
        response = {
            {"message_id", message_id},
            {"success", false},
            {"error", "Failed to recall message (not found, no permission, or time expired)"}
        };
    }
    
    send(Protocol::create_response(MessageType::MESSAGE_RECALL_RESPONSE, sequence, response));
}

// ==================== FCM Token 注册处理器 ====================

void Session::handle_fcm_token_register(uint32_t sequence, const json& body) {
    if (!authenticated_) {
        send(Protocol::create_error(sequence, 401, "Not authenticated"));
        return;
    }
    
    std::string fcm_token = body.value("fcm_token", "");
    if (fcm_token.empty()) {
        send(Protocol::create_error(sequence, 400, "FCM token is required"));
        return;
    }
    
    // 获取 FCM 管理器
    if (!server_) {
        send(Protocol::create_error(sequence, 500, "Server not available"));
        return;
    }
    
    auto fcm_manager = server_->get_fcm_manager();
    if (!fcm_manager) {
        send(Protocol::create_error(sequence, 500, "FCM not configured"));
        return;
    }
    
    // 注册 FCM Token
    if (fcm_manager->register_token(user_id_, fcm_token)) {
        json response = {
            {"success", true},
            {"user_id", user_id_}
        };
        send(Protocol::create_response(MessageType::FCM_TOKEN_REGISTER_RESPONSE, sequence, response));
    } else {
        send(Protocol::create_error(sequence, 500, "Failed to register FCM token"));
    }
}

} // namespace chat
