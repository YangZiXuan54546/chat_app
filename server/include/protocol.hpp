#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace chat {

using json = nlohmann::json;

// 消息类型枚举
enum class MessageType : uint8_t {
    // 认证相关
    REGISTER = 1,
    REGISTER_RESPONSE = 2,
    LOGIN = 3,
    LOGIN_RESPONSE = 4,
    LOGOUT = 5,
    LOGOUT_RESPONSE = 6,
    
    // 用户相关
    USER_INFO = 10,
    USER_INFO_RESPONSE = 11,
    USER_SEARCH = 12,
    USER_SEARCH_RESPONSE = 13,
    USER_UPDATE = 14,
    USER_UPDATE_RESPONSE = 15,
    
    // 好友相关
    FRIEND_ADD = 20,
    FRIEND_ADD_RESPONSE = 21,
    FRIEND_ACCEPT = 22,
    FRIEND_ACCEPT_RESPONSE = 23,
    FRIEND_REJECT = 24,
    FRIEND_REJECT_RESPONSE = 25,
    FRIEND_REMOVE = 26,
    FRIEND_REMOVE_RESPONSE = 27,
    FRIEND_LIST = 28,
    FRIEND_LIST_RESPONSE = 29,
    FRIEND_REQUESTS = 30,
    FRIEND_REQUESTS_RESPONSE = 31,
    FRIEND_REMARK = 32,
    FRIEND_REMARK_RESPONSE = 33,
    
    // 私聊消息
    PRIVATE_MESSAGE = 40,
    PRIVATE_MESSAGE_RESPONSE = 41,
    PRIVATE_MESSAGE_ACK = 42,
    PRIVATE_HISTORY = 43,
    PRIVATE_HISTORY_RESPONSE = 44,
    
    // 群组相关
    GROUP_CREATE = 50,
    GROUP_CREATE_RESPONSE = 51,
    GROUP_JOIN = 52,
    GROUP_JOIN_RESPONSE = 53,
    GROUP_LEAVE = 54,
    GROUP_LEAVE_RESPONSE = 55,
    GROUP_DISMISS = 56,
    GROUP_DISMISS_RESPONSE = 57,
    GROUP_INFO = 58,
    GROUP_INFO_RESPONSE = 59,
    GROUP_LIST = 60,
    GROUP_LIST_RESPONSE = 61,
    GROUP_MEMBERS = 62,
    GROUP_MEMBERS_RESPONSE = 63,
    GROUP_ADD_MEMBER = 64,
    GROUP_ADD_MEMBER_RESPONSE = 65,
    GROUP_REMOVE_MEMBER = 66,
    GROUP_REMOVE_MEMBER_RESPONSE = 67,
    GROUP_SET_ADMIN = 68,
    GROUP_SET_ADMIN_RESPONSE = 69,
    GROUP_TRANSFER_OWNER = 70,
    GROUP_TRANSFER_OWNER_RESPONSE = 71,
    
    // 群聊消息
    GROUP_MESSAGE = 72,
    GROUP_MESSAGE_RESPONSE = 73,
    GROUP_MESSAGE_ACK = 74,
    GROUP_HISTORY = 75,
    GROUP_HISTORY_RESPONSE = 76,
    
    // 多媒体消息
    MEDIA_UPLOAD = 80,
    MEDIA_UPLOAD_RESPONSE = 81,
    MEDIA_DOWNLOAD = 82,
    MEDIA_DOWNLOAD_RESPONSE = 83,
    
    // 在线状态
    ONLINE_STATUS = 90,
    ONLINE_STATUS_RESPONSE = 91,
    
    // 心跳
    HEARTBEAT = 100,
    HEARTBEAT_RESPONSE = 101,
    
    // 端到端加密
    KEY_UPLOAD = 110,
    KEY_UPLOAD_RESPONSE = 111,
    KEY_REQUEST = 112,
    KEY_RESPONSE = 113,
    ENCRYPTED_MESSAGE = 114,
    ENCRYPTED_MESSAGE_RESPONSE = 115,
    
    // 消息撤回
    MESSAGE_RECALL = 120,
    MESSAGE_RECALL_RESPONSE = 121,
    
    // 错误
    ERROR = 255
};

// 媒体类型
enum class MediaType : uint8_t {
    TEXT = 0,
    IMAGE = 1,
    AUDIO = 2,
    VIDEO = 3,
    FILE = 4,
    LOCATION = 5
};

// 消息状态
enum class MessageStatus : uint8_t {
    SENDING = 0,
    SENT = 1,
    DELIVERED = 2,
    READ = 3,
    FAILED = 4
};

// 好友状态
enum class FriendStatus : uint8_t {
    PENDING = 0,
    ACCEPTED = 1,
    REJECTED = 2,
    DELETED = 3
};

// 在线状态
enum class OnlineStatus : uint8_t {
    OFFLINE = 0,
    ONLINE = 1,
    BUSY = 2,
    AWAY = 3
};

// 基础消息头
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t length;      // 消息体长度
    MessageType type;     // 消息类型
    uint32_t sequence;    // 序列号
};
#pragma pack(pop)

// 用户信息
struct UserInfo {
    uint64_t user_id;
    std::string username;
    std::string nickname;
    std::string avatar_url;
    std::string signature;
    OnlineStatus online_status;
    int64_t created_at;
    int64_t updated_at;
    
    json to_json() const {
        return {
            {"user_id", user_id},
            {"username", username},
            {"nickname", nickname},
            {"avatar_url", avatar_url},
            {"signature", signature},
            {"online_status", static_cast<int>(online_status)},
            {"created_at", created_at},
            {"updated_at", updated_at}
        };
    }
    
    static UserInfo from_json(const json& j) {
        UserInfo user;
        user.user_id = j.value("user_id", 0ULL);
        user.username = j.value("username", "");
        user.nickname = j.value("nickname", "");
        user.avatar_url = j.value("avatar_url", "");
        user.signature = j.value("signature", "");
        user.online_status = static_cast<OnlineStatus>(j.value("online_status", 0));
        user.created_at = j.value("created_at", 0LL);
        user.updated_at = j.value("updated_at", 0LL);
        return user;
    }
};

// 消息结构
struct Message {
    uint64_t message_id;
    uint64_t sender_id;
    uint64_t receiver_id;      // 私聊时为接收者ID，群聊时为群组ID
    uint64_t group_id;         // 群聊时有效，私聊为0
    MediaType media_type;
    std::string content;
    std::string media_url;
    std::string extra;         // 额外信息（JSON格式）
    MessageStatus status;
    int64_t created_at;
    
    json to_json() const {
        return {
            {"message_id", message_id},
            {"sender_id", sender_id},
            {"receiver_id", receiver_id},
            {"group_id", group_id},
            {"media_type", static_cast<int>(media_type)},
            {"content", content},
            {"media_url", media_url},
            {"extra", extra},
            {"status", static_cast<int>(status)},
            {"created_at", created_at}
        };
    }
    
    static Message from_json(const json& j) {
        Message msg;
        msg.message_id = j.value("message_id", 0ULL);
        msg.sender_id = j.value("sender_id", 0ULL);
        msg.receiver_id = j.value("receiver_id", 0ULL);
        msg.group_id = j.value("group_id", 0ULL);
        msg.media_type = static_cast<MediaType>(j.value("media_type", 0));
        msg.content = j.value("content", "");
        msg.media_url = j.value("media_url", "");
        msg.extra = j.value("extra", "");
        msg.status = static_cast<MessageStatus>(j.value("status", 0));
        msg.created_at = j.value("created_at", 0LL);
        return msg;
    }
};

// 群组信息
struct GroupInfo {
    uint64_t group_id;
    std::string group_name;
    std::string avatar_url;
    std::string description;
    uint64_t owner_id;
    std::vector<uint64_t> admins;
    std::vector<uint64_t> members;
    int64_t created_at;
    int64_t updated_at;
    
    json to_json() const {
        return {
            {"group_id", group_id},
            {"group_name", group_name},
            {"avatar_url", avatar_url},
            {"description", description},
            {"owner_id", owner_id},
            {"admins", admins},
            {"members", members},
            {"created_at", created_at},
            {"updated_at", updated_at}
        };
    }
    
    static GroupInfo from_json(const json& j) {
        GroupInfo group;
        group.group_id = j.value("group_id", 0ULL);
        group.group_name = j.value("group_name", "");
        group.avatar_url = j.value("avatar_url", "");
        group.description = j.value("description", "");
        group.owner_id = j.value("owner_id", 0ULL);
        if (j.contains("admins")) {
            for (auto& a : j["admins"]) {
                group.admins.push_back(a.get<uint64_t>());
            }
        }
        if (j.contains("members")) {
            for (auto& m : j["members"]) {
                group.members.push_back(m.get<uint64_t>());
            }
        }
        group.created_at = j.value("created_at", 0LL);
        group.updated_at = j.value("updated_at", 0LL);
        return group;
    }
};

// 好友关系
struct FriendRelation {
    uint64_t user_id;
    uint64_t friend_id;
    std::string remark;
    FriendStatus status;
    int64_t created_at;
    int64_t updated_at;
    
    json to_json() const {
        return {
            {"user_id", user_id},
            {"friend_id", friend_id},
            {"remark", remark},
            {"status", static_cast<int>(status)},
            {"created_at", created_at},
            {"updated_at", updated_at}
        };
    }
    
    static FriendRelation from_json(const json& j) {
        FriendRelation rel;
        rel.user_id = j.value("user_id", 0ULL);
        rel.friend_id = j.value("friend_id", 0ULL);
        rel.remark = j.value("remark", "");
        rel.status = static_cast<FriendStatus>(j.value("status", 0));
        rel.created_at = j.value("created_at", 0LL);
        rel.updated_at = j.value("updated_at", 0LL);
        return rel;
    }
};

// 协议工具函数
class Protocol {
public:
    // 序列化消息
    static std::vector<uint8_t> serialize(MessageType type, uint32_t sequence, const json& body) {
        std::string body_str = body.dump();
        std::vector<uint8_t> buffer(sizeof(MessageHeader) + body_str.size());
        
        MessageHeader header;
        header.length = static_cast<uint32_t>(body_str.size());
        header.type = type;
        header.sequence = sequence;
        
        memcpy(buffer.data(), &header, sizeof(MessageHeader));
        memcpy(buffer.data() + sizeof(MessageHeader), body_str.data(), body_str.size());
        
        return buffer;
    }
    
    // 解析消息头
    static bool parse_header(const uint8_t* data, size_t size, MessageHeader& header) {
        if (size < sizeof(MessageHeader)) {
            return false;
        }
        memcpy(&header, data, sizeof(MessageHeader));
        return true;
    }
    
    // 解析消息体
    static json parse_body(const uint8_t* data, size_t size) {
        if (size == 0) {
            return json::object();
        }
        try {
            return json::parse(std::string(reinterpret_cast<const char*>(data), size));
        } catch (const json::exception& e) {
            return json::object();
        }
    }
    
    // 创建错误响应
    static std::vector<uint8_t> create_error(uint32_t sequence, int code, const std::string& message) {
        json body = {
            {"code", code},
            {"message", message}
        };
        return serialize(MessageType::ERROR, sequence, body);
    }
    
    // 创建成功响应
    static std::vector<uint8_t> create_response(MessageType type, uint32_t sequence, const json& data) {
        json body = {
            {"code", 0},
            {"message", "success"},
            {"data", data}
        };
        return serialize(type, sequence, body);
    }
};

} // namespace chat

#endif // PROTOCOL_HPP
