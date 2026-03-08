#include "session.hpp"
#include "server.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include "database.hpp"
#include <chrono>
#include <iostream>

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
            
            handle_message(header.type, header.sequence, body);
            
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
        
        // 通知目标用户
        // server_->send_to_user(friend_id, notification);
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
        
        // 转发给接收者
        // server_->send_to_user(receiver_id, Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, message.to_json()));
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
        // server_->broadcast_to_group(group_id, Protocol::serialize(MessageType::GROUP_MESSAGE, 0, message.to_json()));
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
    
    // Base64 解码
    // ... (实际实现需要 Base64 解码)
    
    uint64_t file_id = 0;
    std::string url;
    std::string error;
    
    std::vector<uint8_t> file_data; // 解码后的数据
    // message_manager_->upload_media(user_id_, file_name, file_data, 
    //                                static_cast<MediaType>(media_type_int),
    //                                file_id, url, error);
    
    json response = {
        {"file_id", file_id},
        {"url", url}
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

} // namespace chat
