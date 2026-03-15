#include "websocket_server.hpp"
#include "http_gateway.hpp"
#include "database.hpp"
#include "database_pool.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include "fcm_manager.hpp"
#include "jpush_manager.hpp"
#include "protocol.hpp"
#include <iostream>
#include <memory>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <asio.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 全局服务器实例
std::shared_ptr<chat::WebSocketServer> g_ws_server;
std::shared_ptr<chat::HttpGateway> g_http_gateway;
asio::io_context g_io_context;  // 用于 BotManager

// 依赖管理器
std::shared_ptr<chat::Database> g_database;
std::shared_ptr<chat::DatabasePool> g_db_pool;
std::shared_ptr<chat::UserManager> g_user_manager;
std::shared_ptr<chat::MessageManager> g_message_manager;
std::shared_ptr<chat::GroupManager> g_group_manager;
std::shared_ptr<chat::FriendManager> g_friend_manager;
std::shared_ptr<chat::BotManager> g_bot_manager;
std::shared_ptr<chat::FcmManager> g_fcm_manager;
std::shared_ptr<chat::JPushManager> g_jpush_manager;

void signal_handler(int signal) {
    std::cout << "\nShutting down..." << std::endl;
    if (g_ws_server) g_ws_server->stop();
    if (g_http_gateway) g_http_gateway->stop();
}

// 处理 WebSocket 消息
void handle_ws_message(chat::WsConnection::ptr conn, const std::string& message) {
    try {
        json msg = json::parse(message);
        
        if (!msg.contains("type")) {
            std::cerr << "[WS] Missing message type" << std::endl;
            return;
        }
        
        std::string type = msg["type"];
        
        // 处理登录
        if (type == "login") {
            handle_login(conn, msg);
            return;
        }
        
        // 其他消息需要认证
        if (!conn->is_authenticated()) {
            conn->send_text(R"({"type":"error","message":"Not authenticated"})");
            return;
        }
        
        uint64_t user_id = conn->get_user_id();
        
        // 路由消息
        if (type == "privateMessage") {
            handle_private_message(conn, msg);
        } else if (type == "groupMessage") {
            handle_group_message(conn, msg);
        } else if (type == "friendRequest") {
            handle_friend_request(conn, msg);
        } else if (type == "friendAccept") {
            handle_friend_accept(conn, msg);
        } else if (type == "friendReject") {
            handle_friend_reject(conn, msg);
        } else if (type == "friendRemove") {
            handle_friend_remove(conn, msg);
        } else if (type == "createGroup") {
            handle_create_group(conn, msg);
        } else if (type == "inviteGroupMembers") {
            handle_invite_group_members(conn, msg);
        } else if (type == "groupKick") {
            handle_group_kick(conn, msg);
        } else if (type == "groupQuit") {
            handle_group_quit(conn, msg);
        } else if (type == "groupDismiss") {
            handle_group_dismiss(conn, msg);
        } else if (type == "groupSetAdmin") {
            handle_group_set_admin(conn, msg);
        } else if (type == "groupTransferOwner") {
            handle_group_transfer_owner(conn, msg);
        } else if (type == "heartbeat") {
            handle_heartbeat(conn, msg);
        } else if (type == "fcmTokenRegister") {
            handle_fcm_token_register(conn, msg);
        } else if (type == "favoriteAdd") {
            handle_favorite_add(conn, msg);
        } else if (type == "favoriteRemove") {
            handle_favorite_remove(conn, msg);
        } else if (type == "favoriteList") {
            handle_favorite_list(conn, msg);
        } else if (type == "passwordUpdate") {
            handle_password_update(conn, msg);
        } else {
            std::cerr << "[WS] Unknown message type: " << type << std::endl;
        }
        
    } catch (const json::exception& e) {
        std::cerr << "[WS] JSON parse error: " << e.what() << std::endl;
        conn->send_text(R"({"type":"error","message":"Invalid JSON"})");
    }
}

// 处理登录
void handle_login(chat::WsConnection::ptr conn, const json& msg) {
    if (!msg.contains("username") || !msg.contains("password")) {
        conn->send_text(R"({"type":"loginResponse","success":false,"error":"Missing credentials"})");
        return;
    }
    
    std::string username = msg["username"];
    std::string password = msg["password"];
    
    // 验证用户
    chat::User user;
    if (!g_user_manager->login(username, password, user)) {
        conn->send_text(R"({"type":"loginResponse","success":false,"error":"Invalid credentials"})");
        return;
    }
    
    // 检查是否已在其他地方登录
    auto existing_conn = g_ws_server->get_connection(user.user_id);
    if (existing_conn) {
        // 踢掉旧连接
        existing_conn->send_text(R"({"type":"kicked","reason":"Logged in elsewhere"})");
        existing_conn->close();
    }
    
    // 设置用户信息
    conn->set_user_id(user.user_id);
    conn->set_authenticated(true);
    
    // 更新在线状态
    g_user_manager->set_online(user.user_id, true);
    
    // 关联连接
    // (WebSocketServer 会自动管理 user_connections_)
    
    // 发送登录成功响应
    json response = {
        {"type", "loginResponse"},
        {"success", true},
        {"user_id", user.user_id},
        {"user_info", {
            {"user_id", user.user_id},
            {"username", user.username},
            {"nickname", user.nickname},
            {"avatar_url", user.avatar_url},
            {"status", "ONLINE"}
        }}
    };
    
    conn->send_text(response.dump());
    
    std::cout << "[WS] User logged in: " << username << " (id=" << user.user_id << ")" << std::endl;
    
    // 发送待处理的好友请求
    auto pending_requests = g_friend_manager->get_pending_requests(user.user_id);
    if (!pending_requests.empty()) {
        for (const auto& req : pending_requests) {
            json notification = {
                {"type", "friendRequestNotification"},
                {"request_id", req.request_id},
                {"from_user_id", req.from_user_id},
                {"from_username", req.from_username},
                {"from_nickname", req.from_nickname},
                {"message", req.message},
                {"created_at", req.created_at}
            };
            conn->send_text(notification.dump());
        }
    }
}

// 处理私聊消息
void handle_private_message(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t sender_id = conn->get_user_id();
    uint64_t receiver_id = msg["receiver_id"].get<uint64_t>();
    std::string content = msg["content"];
    int message_type = msg.value("message_type", 0);
    std::string media_url = msg.value("media_url", "");
    std::string media_type = msg.value("media_type", "");
    
    // 保存消息
    uint64_t message_id = g_message_manager->send_private_message(
        sender_id, receiver_id, content, message_type, media_url, media_type);
    
    if (message_id == 0) {
        conn->send_text(R"({"type":"error","message":"Failed to send message"})");
        return;
    }
    
    // 获取发送者信息
    chat::User sender;
    g_user_manager->get_user(sender_id, sender);
    
    // 构建消息
    json msg_response = {
        {"type", "privateMessageResponse"},
        {"message_id", message_id},
        {"sender_id", sender_id},
        {"receiver_id", receiver_id},
        {"content", content},
        {"message_type", message_type},
        {"media_url", media_url},
        {"media_type", media_type},
        {"created_at", std::time(nullptr)}
    };
    
    // 发送确认给发送者
    conn->send_text(msg_response.dump());
    
    // 尝试发送给接收者
    auto receiver_conn = g_ws_server->get_connection(receiver_id);
    if (receiver_conn) {
        json msg_notification = {
            {"type", "privateMessage"},
            {"message_id", message_id},
            {"sender_id", sender_id},
            {"sender_username", sender.username},
            {"sender_nickname", sender.nickname},
            {"sender_avatar", sender.avatar_url},
            {"content", content},
            {"message_type", message_type},
            {"media_url", media_url},
            {"media_type", media_type},
            {"created_at", std::time(nullptr)}
        };
        receiver_conn->send_text(msg_notification.dump());
    } else {
        // 接收者离线，发送推送通知
        if (g_jpush_manager && g_jpush_manager->is_configured()) {
            std::string title = sender.nickname.empty() ? sender.username : sender.nickname;
            std::string body = content;
            if (body.length() > 50) body = body.substr(0, 50) + "...";
            
            json extra = {
                {"type", "private_message"},
                {"sender_id", sender_id}
            };
            
            g_jpush_manager->send_to_user(receiver_id, title, body, extra.dump());
        } else if (g_fcm_manager && g_fcm_manager->is_configured()) {
            std::string title = sender.nickname.empty() ? sender.username : sender.nickname;
            std::string body = content;
            if (body.length() > 50) body = body.substr(0, 50) + "...";
            
            g_fcm_manager->send_to_user(receiver_id, title, body, "private_message", sender_id);
        }
    }
    
    // 机器人自动回复
    if (g_bot_manager && g_bot_manager->is_bot(receiver_id)) {
        g_bot_manager->handle_message(sender_id, content, message_id);
    }
}

// 处理群聊消息
void handle_group_message(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t sender_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    std::string content = msg["content"];
    int message_type = msg.value("message_type", 0);
    std::string media_url = msg.value("media_url", "");
    std::string media_type = msg.value("media_type", "");
    
    // 检查是否是群成员
    if (!g_group_manager->is_member(group_id, sender_id)) {
        conn->send_text(R"({"type":"error","message":"Not a group member"})");
        return;
    }
    
    // 保存消息
    uint64_t message_id = g_message_manager->send_group_message(
        sender_id, group_id, content, message_type, media_url, media_type);
    
    if (message_id == 0) {
        conn->send_text(R"({"type":"error","message":"Failed to send message"})");
        return;
    }
    
    // 获取发送者和群组信息
    chat::User sender;
    g_user_manager->get_user(sender_id, sender);
    
    chat::Group group;
    g_group_manager->get_group(group_id, group);
    
    // 获取群成员列表
    auto members = g_group_manager->get_members(group_id);
    
    // 构建消息
    json msg_response = {
        {"type", "groupMessageResponse"},
        {"message_id", message_id},
        {"group_id", group_id},
        {"sender_id", sender_id},
        {"content", content},
        {"message_type", message_type},
        {"media_url", media_url},
        {"media_type", media_type},
        {"created_at", std::time(nullptr)}
    };
    
    // 发送确认给发送者
    conn->send_text(msg_response.dump());
    
    // 广播给所有在线群成员
    for (const auto& member : members) {
        if (member.user_id == sender_id) continue;
        
        auto member_conn = g_ws_server->get_connection(member.user_id);
        if (member_conn) {
            json msg_notification = {
                {"type", "groupMessage"},
                {"message_id", message_id},
                {"group_id", group_id},
                {"group_name", group.name},
                {"sender_id", sender_id},
                {"sender_username", sender.username},
                {"sender_nickname", sender.nickname},
                {"sender_avatar", sender.avatar_url},
                {"content", content},
                {"message_type", message_type},
                {"media_url", media_url},
                {"media_type", media_type},
                {"created_at", std::time(nullptr)}
            };
            member_conn->send_text(msg_notification.dump());
        } else {
            // 离线推送
            if (g_jpush_manager && g_jpush_manager->is_configured()) {
                std::string title = "[" + group.name + "] " + 
                    (sender.nickname.empty() ? sender.username : sender.nickname);
                std::string body = content;
                if (body.length() > 50) body = body.substr(0, 50) + "...";
                
                json extra = {
                    {"type", "group_message"},
                    {"group_id", group_id},
                    {"sender_id", sender_id}
                };
                
                g_jpush_manager->send_to_user(member.user_id, title, body, extra.dump());
            }
        }
    }
}

// 处理好友请求
void handle_friend_request(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t from_id = conn->get_user_id();
    uint64_t to_id = msg["to_user_id"].get<uint64_t>();
    std::string message = msg.value("message", "");
    
    // 发送好友请求
    uint64_t request_id = g_friend_manager->send_request(from_id, to_id, message);
    
    if (request_id == 0) {
        conn->send_text(R"({"type":"error","message":"Failed to send friend request"})");
        return;
    }
    
    // 获取发送者信息
    chat::User from_user;
    g_user_manager->get_user(from_id, from_user);
    
    // 发送确认
    json response = {
        {"type", "friendAddResponse"},
        {"success", true},
        {"request_id", request_id}
    };
    conn->send_text(response.dump());
    
    // 通知接收者
    auto to_conn = g_ws_server->get_connection(to_id);
    if (to_conn) {
        json notification = {
            {"type", "friendRequestNotification"},
            {"request_id", request_id},
            {"from_user_id", from_id},
            {"from_username", from_user.username},
            {"from_nickname", from_user.nickname},
            {"from_avatar", from_user.avatar_url},
            {"message", message},
            {"created_at", std::time(nullptr)}
        };
        to_conn->send_text(notification.dump());
    }
    
    // 机器人自动接受好友请求
    if (g_bot_manager && g_bot_manager->is_bot(to_id)) {
        g_bot_manager->handle_friend_request(from_id);
    }
}

// 处理接受好友请求
void handle_friend_accept(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    uint64_t request_id = msg["request_id"].get<uint64_t>();
    
    if (g_friend_manager->accept_request(request_id, user_id)) {
        json response = {
            {"type", "friendAcceptResponse"},
            {"success", true}
        };
        conn->send_text(response.dump());
        
        // 通知对方
        // 获取请求信息
        auto request = g_friend_manager->get_request(request_id);
        if (request.from_user_id > 0) {
            auto from_conn = g_ws_server->get_connection(request.from_user_id);
            if (from_conn) {
                chat::User accepter;
                g_user_manager->get_user(user_id, accepter);
                
                json notification = {
                    {"type", "friendAcceptNotification"},
                    {"user_id", user_id},
                    {"username", accepter.username},
                    {"nickname", accepter.nickname}
                };
                from_conn->send_text(notification.dump());
            }
        }
    } else {
        conn->send_text(R"({"type":"friendAcceptResponse","success":false,"error":"Failed to accept"})");
    }
}

// 处理拒绝好友请求
void handle_friend_reject(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    uint64_t request_id = msg["request_id"].get<uint64_t>();
    
    if (g_friend_manager->reject_request(request_id, user_id)) {
        conn->send_text(R"({"type":"friendRejectResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"friendRejectResponse","success":false})");
    }
}

// 处理删除好友
void handle_friend_remove(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    uint64_t friend_id = msg["friend_id"].get<uint64_t>();
    
    if (g_friend_manager->remove_friend(user_id, friend_id)) {
        conn->send_text(R"({"type":"friendRemoveResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"friendRemoveResponse","success":false})");
    }
}

// 处理创建群组
void handle_create_group(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t owner_id = conn->get_user_id();
    std::string name = msg["name"];
    std::string description = msg.value("description", "");
    
    uint64_t group_id = g_group_manager->create_group(name, owner_id, description);
    
    if (group_id > 0) {
        json response = {
            {"type", "createGroupResponse"},
            {"success", true},
            {"group_id", group_id}
        };
        conn->send_text(response.dump());
    } else {
        conn->send_text(R"({"type":"createGroupResponse","success":false})");
    }
}

// 处理邀请群成员
void handle_invite_group_members(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t inviter_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    auto user_ids = msg["user_ids"].get<std::vector<uint64_t>>();
    
    if (g_group_manager->add_members(group_id, user_ids, inviter_id)) {
        conn->send_text(R"({"type":"inviteGroupMembersResponse","success":true})");
        
        // 通知被邀请的用户
        for (uint64_t user_id : user_ids) {
            auto user_conn = g_ws_server->get_connection(user_id);
            if (user_conn) {
                chat::Group group;
                g_group_manager->get_group(group_id, group);
                
                json notification = {
                    {"type", "groupInviteNotification"},
                    {"group_id", group_id},
                    {"group_name", group.name},
                    {"inviter_id", inviter_id}
                };
                user_conn->send_text(notification.dump());
            }
        }
    } else {
        conn->send_text(R"({"type":"inviteGroupMembersResponse","success":false})");
    }
}

// 处理踢出群成员
void handle_group_kick(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t operator_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    uint64_t user_id = msg["user_id"].get<uint64_t>();
    
    if (g_group_manager->kick_member(group_id, user_id, operator_id)) {
        conn->send_text(R"({"type":"groupKickResponse","success":true})");
        
        // 通知被踢出的用户
        auto user_conn = g_ws_server->get_connection(user_id);
        if (user_conn) {
            chat::Group group;
            g_group_manager->get_group(group_id, group);
            
            json notification = {
                {"type", "groupKickedNotification"},
                {"group_id", group_id},
                {"group_name", group.name}
            };
            user_conn->send_text(notification.dump());
        }
    } else {
        conn->send_text(R"({"type":"groupKickResponse","success":false})");
    }
}

// 处理退出群组
void handle_group_quit(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    
    if (g_group_manager->quit_group(group_id, user_id)) {
        conn->send_text(R"({"type":"groupQuitResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"groupQuitResponse","success":false})");
    }
}

// 处理解散群组
void handle_group_dismiss(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t owner_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    
    if (g_group_manager->dismiss_group(group_id, owner_id)) {
        conn->send_text(R"({"type":"groupDismissResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"groupDismissResponse","success":false})");
    }
}

// 处理设置管理员
void handle_group_set_admin(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t operator_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    uint64_t user_id = msg["user_id"].get<uint64_t>();
    bool is_admin = msg["is_admin"].get<bool>();
    
    if (g_group_manager->set_admin(group_id, user_id, operator_id, is_admin)) {
        json response = {
            {"type", "groupSetAdminResponse"},
            {"success", true}
        };
        conn->send_text(response.dump());
    } else {
        conn->send_text(R"({"type":"groupSetAdminResponse","success":false})");
    }
}

// 处理转让群主
void handle_group_transfer_owner(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t owner_id = conn->get_user_id();
    uint64_t group_id = msg["group_id"].get<uint64_t>();
    uint64_t new_owner_id = msg["new_owner_id"].get<uint64_t>();
    
    if (g_group_manager->transfer_owner(group_id, owner_id, new_owner_id)) {
        json response = {
            {"type", "groupTransferOwnerResponse"},
            {"success", true}
        };
        conn->send_text(response.dump());
    } else {
        conn->send_text(R"({"type":"groupTransferOwnerResponse","success":false})");
    }
}

// 处理心跳
void handle_heartbeat(chat::WsConnection::ptr conn, const json& msg) {
    json response = {
        {"type", "heartbeatResponse"},
        {"timestamp", std::time(nullptr)}
    };
    conn->send_text(response.dump());
}

// 处理 FCM Token 注册
void handle_fcm_token_register(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    std::string fcm_token = msg["fcm_token"];
    
    if (g_database && g_database->save_fcm_token(user_id, fcm_token)) {
        conn->send_text(R"({"type":"fcmTokenRegisterResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"fcmTokenRegisterResponse","success":false})");
    }
}

// 处理添加收藏
void handle_favorite_add(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    uint64_t message_id = msg["message_id"].get<uint64_t>();
    std::string message_type = msg["message_type"].get<std::string>();
    uint64_t sender_id = msg["sender_id"].get<uint64_t>();
    std::string content = msg["content"];
    int media_type_int = msg.value("media_type", 0);
    std::string media_url = msg.value("media_url", "");
    
    if (g_database && g_database->add_favorite(
        user_id, message_id, message_type, sender_id, content, 
        static_cast<chat::MediaType>(media_type_int), media_url)) {
        conn->send_text(R"({"type":"favoriteAddResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"favoriteAddResponse","success":false})");
    }
}

// 处理取消收藏
void handle_favorite_remove(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    uint64_t message_id = msg["message_id"].get<uint64_t>();
    std::string message_type = msg["message_type"].get<std::string>();
    
    if (g_database && g_database->remove_favorite(user_id, message_id, message_type)) {
        conn->send_text(R"({"type":"favoriteRemoveResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"favoriteRemoveResponse","success":false})");
    }
}

// 处理获取收藏列表
void handle_favorite_list(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    int limit = msg.value("limit", 50);
    int offset = msg.value("offset", 0);
    
    std::vector<json> favorites;
    if (g_database && g_database->get_favorites(user_id, limit, offset, favorites)) {
        json response = {
            {"type", "favoriteListResponse"},
            {"favorites", favorites}
        };
        conn->send_text(response.dump());
    } else {
        conn->send_text(R"({"type":"favoriteListResponse","favorites":[]})");
    }
}

// 处理修改密码
void handle_password_update(chat::WsConnection::ptr conn, const json& msg) {
    uint64_t user_id = conn->get_user_id();
    std::string old_password = msg["old_password"];
    std::string new_password = msg["new_password"];
    
    // 验证旧密码
    chat::User user;
    if (!g_user_manager->get_user(user_id, user)) {
        conn->send_text(R"({"type":"passwordUpdateResponse","success":false,"error":"User not found"})");
        return;
    }
    
    if (!g_user_manager->verify_password(old_password, user.password_hash)) {
        conn->send_text(R"({"type":"passwordUpdateResponse","success":false,"error":"Invalid old password"})");
        return;
    }
    
    // 验证新密码长度
    if (new_password.length() < 6) {
        conn->send_text(R"({"type":"passwordUpdateResponse","success":false,"error":"Password too short"})");
        return;
    }
    
    // 更新密码
    if (g_user_manager->update_password(user_id, new_password)) {
        conn->send_text(R"({"type":"passwordUpdateResponse","success":true})");
    } else {
        conn->send_text(R"({"type":"passwordUpdateResponse","success":false,"error":"Failed to update password"})");
    }
}

// 处理用户搜索
void handle_user_search(chat::WsConnection::ptr conn, const json& msg) {
    std::string keyword = msg["keyword"];
    
    auto users = g_user_manager->search_users(keyword);
    
    json response = {
        {"type", "userSearchResponse"},
        {"users", json::array()}
    };
    
    for (const auto& user : users) {
        response["users"].push_back({
            {"user_id", user.user_id},
            {"username", user.username},
            {"nickname", user.nickname},
            {"avatar_url", user.avatar_url}
        });
    }
    
    conn->send_text(response.dump());
}

// 连接事件处理
void on_connect(chat::WsConnection::ptr conn) {
    std::cout << "[WS] Client connected: " << conn->get_client_ip() << std::endl;
}

// 断开连接事件处理
void on_disconnect(chat::WsConnection::ptr conn) {
    uint64_t user_id = conn->get_user_id();
    
    if (user_id > 0 && conn->is_authenticated()) {
        // 更新用户状态为离线
        g_user_manager->set_online(user_id, false);
        std::cout << "[WS] User disconnected: " << user_id << std::endl;
    } else {
        std::cout << "[WS] Client disconnected: " << conn->get_client_ip() << std::endl;
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
    
    // WebSocket 服务器配置
    chat::WsServerConfig ws_config;
    ws_config.host = "0.0.0.0";
    ws_config.port = 8888;
    ws_config.heartbeat_interval = 30;
    ws_config.heartbeat_timeout = 60;
    
    // HTTP Gateway 配置
    chat::HttpGateway::Config http_config;
    http_config.port = 8889;
    http_config.media_dir = "./media";
    http_config.max_upload_size = 10 * 1024 * 1024; // 10MB
    
    // 机器人配置
    std::string deepseek_api_key;
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
        } else if (arg == "--ws-port" && i + 1 < argc) {
            ws_config.port = std::stoi(argv[++i]);
        } else if (arg == "--http-port" && i + 1 < argc) {
            http_config.port = std::stoi(argv[++i]);
        } else if (arg == "--media-dir" && i + 1 < argc) {
            http_config.media_dir = argv[++i];
        } else if (arg == "--deepseek-api-key" && i + 1 < argc) {
            deepseek_api_key = argv[++i];
            bot_enabled = true;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --ws-port <port>       WebSocket port (default: 8888)\n"
                      << "  --http-port <port>     HTTP API port (default: 8889)\n"
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
    
    // 从环境变量获取配置
    const char* env_media_dir = std::getenv("MEDIA_DIR");
    if (env_media_dir) {
        http_config.media_dir = env_media_dir;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  Chat App Gateway Server v2.0" << std::endl;
    std::cout << "  WebSocket + HTTP Gateway" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 初始化数据库
    g_database = std::make_shared<chat::Database>();
    if (!g_database->init(db_config)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }
    std::cout << "[OK] Database connected" << std::endl;
    
    // 初始化数据库连接池
    g_db_pool = std::make_shared<chat::DatabasePool>(db_config, 5, 20);
    std::cout << "[OK] Database pool initialized" << std::endl;
    
    // 初始化管理器
    g_user_manager = std::make_shared<chat::UserManager>(g_database);
    g_message_manager = std::make_shared<chat::MessageManager>(g_database);
    g_group_manager = std::make_shared<chat::GroupManager>(g_database);
    g_friend_manager = std::make_shared<chat::FriendManager>(g_database);
    
    // 初始化机器人 (暂时禁用，新的 Gateway 架构中需要重构)
    // g_bot_manager = ...;
    
    // 初始化 FCM
    g_fcm_manager = std::make_shared<chat::FcmManager>(g_database);
    g_fcm_manager->set_config("chatapp-ae10f", "config/firebase-service-account.json");
    if (g_fcm_manager->is_configured()) {
        std::cout << "[OK] FCM Push initialized" << std::endl;
    } else {
        std::cout << "[--] FCM not configured" << std::endl;
    }
    
    // 初始化 JPush
    g_jpush_manager = std::make_shared<chat::JPushManager>(g_database);
    const char* jpush_app_key = std::getenv("JPUSH_APP_KEY");
    const char* jpush_master_secret = std::getenv("JPUSH_MASTER_SECRET");
    if (jpush_app_key && jpush_master_secret) {
        g_jpush_manager->set_config(jpush_app_key, jpush_master_secret);
        if (g_jpush_manager->is_configured()) {
            std::cout << "[OK] JPush initialized" << std::endl;
        }
    } else {
        std::cout << "[--] JPush not configured" << std::endl;
    }
    
    // 创建 WebSocket 服务器
    g_ws_server = std::make_shared<chat::WebSocketServer>(ws_config);
    g_ws_server->set_database(g_database);
    g_ws_server->set_user_manager(g_user_manager);
    g_ws_server->set_message_manager(g_message_manager);
    g_ws_server->set_group_manager(g_group_manager);
    g_ws_server->set_friend_manager(g_friend_manager);
    g_ws_server->set_bot_manager(g_bot_manager);
    g_ws_server->set_fcm_manager(g_fcm_manager);
    g_ws_server->set_jpush_manager(g_jpush_manager);
    
    // 设置消息处理器
    g_ws_server->set_message_handler(handle_ws_message);
    g_ws_server->set_connect_handler(on_connect);
    g_ws_server->set_disconnect_handler(on_disconnect);
    
    // 创建 HTTP Gateway
    g_http_gateway = std::make_shared<chat::HttpGateway>(http_config);
    g_http_gateway->set_database(g_database);
    
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 启动服务器
    if (!g_ws_server->start()) {
        std::cerr << "Failed to start WebSocket server" << std::endl;
        return 1;
    }
    
    if (!g_http_gateway->start()) {
        std::cerr << "Failed to start HTTP gateway" << std::endl;
        g_ws_server->stop();
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Services running:" << std::endl;
    std::cout << "  - WebSocket: ws://localhost:" << ws_config.port << std::endl;
    std::cout << "  - HTTP API:  http://localhost:" << http_config.port << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;
    
    // 运行主循环
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 清理
    g_ws_server->stop();
    g_http_gateway->stop();
    g_database->close();
    
    std::cout << "Server stopped." << std::endl;
    return 0;
}