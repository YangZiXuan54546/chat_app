#ifndef MESSAGE_MANAGER_HPP
#define MESSAGE_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "protocol.hpp"
#include "database.hpp"

namespace chat {

class Session;
class Server;

class MessageManager {
public:
    MessageManager(std::shared_ptr<Database> database);
    
    // 发送私聊消息
    bool send_private_message(uint64_t sender_id, uint64_t receiver_id,
                              const std::string& content, MediaType media_type,
                              const std::string& media_url, const std::string& extra,
                              Message& message, std::string& error);
    
    // 获取私聊历史消息
    std::vector<Message> get_private_history(uint64_t user1_id, uint64_t user2_id,
                                              int64_t before_time, int limit);
    
    // 标记消息已读
    bool mark_messages_read(uint64_t user_id, uint64_t peer_id);
    
    // 获取未读消息数
    int get_unread_count(uint64_t user_id, uint64_t peer_id);
    
    // 发送群聊消息
    bool send_group_message(uint64_t sender_id, uint64_t group_id,
                            const std::string& content, MediaType media_type,
                            const std::string& media_url, const std::string& extra,
                            Message& message, std::string& error);
    
    // 获取群聊历史消息
    std::vector<Message> get_group_history(uint64_t group_id,
                                           int64_t before_time, int limit);
    
    // 上传媒体文件
    bool upload_media(uint64_t user_id, const std::string& file_name,
                      const std::vector<uint8_t>& file_data, MediaType type,
                      uint64_t& file_id, std::string& url, std::string& error);
    
    // 下载媒体文件
    bool download_media(uint64_t file_id, std::vector<uint8_t>& file_data,
                        MediaType& type, std::string& error);
    
    void set_server(std::shared_ptr<Server> server) { server_ = server; }
    
private:
    std::shared_ptr<Database> database_;
    std::shared_ptr<Server> server_;
    std::string media_dir_ = "./media";
    
    bool ensure_media_dir();
    std::string get_media_path(uint64_t file_id, const std::string& ext);
};

} // namespace chat

#endif // MESSAGE_MANAGER_HPP
