#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <mysql/mysql.h>
#include "protocol.hpp"

namespace chat {

class Database {
public:
    struct Config {
        std::string host = "localhost";
        int port = 3306;
        std::string user = "root";
        std::string password;
        std::string database = "chat_app";
        int pool_size = 10;
    };

    Database();
    ~Database();
    
    bool init(const Config& config);
    void close();
    
    // 用户相关
    bool create_user(const std::string& username, const std::string& password, 
                     const std::string& nickname, uint64_t& user_id);
    bool get_user_by_id(uint64_t user_id, UserInfo& user);
    bool get_user_by_username(const std::string& username, UserInfo& user);
    bool verify_user(const std::string& username, const std::string& password, uint64_t& user_id);
    bool update_user(const UserInfo& user);
    bool update_user_password(uint64_t user_id, const std::string& new_password);
    bool update_user_online_status(uint64_t user_id, OnlineStatus status);
    std::vector<UserInfo> search_users(const std::string& keyword, int limit = 20);
    
    // 好友相关
    bool add_friend_request(uint64_t user_id, uint64_t friend_id);
    bool accept_friend_request(uint64_t user_id, uint64_t friend_id);
    bool reject_friend_request(uint64_t user_id, uint64_t friend_id);
    bool remove_friend(uint64_t user_id, uint64_t friend_id);
    bool get_friend_list(uint64_t user_id, std::vector<std::pair<UserInfo, FriendRelation>>& friends);
    bool get_friend_requests(uint64_t user_id, std::vector<std::pair<UserInfo, FriendRelation>>& requests);
    bool is_friend(uint64_t user_id, uint64_t friend_id);
    bool set_friend_remark(uint64_t user_id, uint64_t friend_id, const std::string& remark);
    
    // 私聊消息相关
    bool save_private_message(Message& message);
    bool get_private_history(uint64_t user1_id, uint64_t user2_id, 
                              int64_t before_time, int limit, std::vector<Message>& messages);
    bool update_message_status(uint64_t message_id, MessageStatus status);
    bool mark_messages_read(uint64_t user_id, uint64_t peer_id);
    int get_unread_count(uint64_t user_id, uint64_t peer_id);
    
    // 群组相关
    bool create_group(const std::string& group_name, uint64_t owner_id,
                      const std::string& description, uint64_t& group_id);
    bool get_group_by_id(uint64_t group_id, GroupInfo& group);
    bool update_group(const GroupInfo& group);
    bool dismiss_group(uint64_t group_id);
    bool add_group_member(uint64_t group_id, uint64_t user_id, bool is_admin = false);
    bool remove_group_member(uint64_t group_id, uint64_t user_id);
    bool get_user_groups(uint64_t user_id, std::vector<GroupInfo>& groups);
    bool get_group_members(uint64_t group_id, std::vector<uint64_t>& members);
    bool is_group_member(uint64_t group_id, uint64_t user_id);
    bool is_group_admin(uint64_t group_id, uint64_t user_id);
    bool is_group_owner(uint64_t group_id, uint64_t user_id);
    bool set_group_admin(uint64_t group_id, uint64_t user_id, bool is_admin);
    bool transfer_group_owner(uint64_t group_id, uint64_t old_owner_id, uint64_t new_owner_id);
    
    // 群聊消息相关
    bool save_group_message(Message& message);
    bool get_group_history(uint64_t group_id, int64_t before_time, 
                           int limit, std::vector<Message>& messages);
    
    // 媒体文件相关
    bool save_media_file(uint64_t user_id, const std::string& file_name,
                         const std::string& file_path, MediaType type, 
                         uint64_t& file_id, std::string& url);
    bool get_media_file(uint64_t file_id, std::string& file_path, MediaType& type);
    
    // 机器人聊天记录相关
    bool save_bot_conversation(uint64_t user_id, const std::string& conversation_id,
                               const std::string& role, const std::string& content);
    bool get_bot_conversation(uint64_t user_id, const std::string& conversation_id,
                              std::vector<std::pair<std::string, std::string>>& messages,
                              int limit = 20);
    bool clear_bot_conversation(uint64_t user_id, const std::string& conversation_id);
    int get_bot_conversation_char_count(uint64_t user_id, const std::string& conversation_id);
    bool create_new_bot_session(uint64_t user_id, std::string& new_conversation_id);
    bool get_user_bot_sessions(uint64_t user_id, std::vector<std::string>& session_ids);
    
private:
    MYSQL* connection_;
    std::mutex mutex_;
    Config config_;
    bool connected_;
    
    bool connect();
    void disconnect();
    bool init_tables();
    std::string escape_string(const std::string& str);
    uint64_t get_last_insert_id();
    int64_t get_current_timestamp();
};

} // namespace chat

#endif // DATABASE_HPP
