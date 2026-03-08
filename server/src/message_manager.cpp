#include "message_manager.hpp"
#include "server.hpp"
#include <fstream>
#include <sys/stat.h>

namespace chat {

MessageManager::MessageManager(std::shared_ptr<Database> database)
    : database_(database) {
    ensure_media_dir();
}

bool MessageManager::ensure_media_dir() {
    return mkdir(media_dir_.c_str(), 0755) == 0 || errno == EEXIST;
}

std::string MessageManager::get_media_path(uint64_t file_id, const std::string& ext) {
    return media_dir_ + "/" + std::to_string(file_id) + ext;
}

bool MessageManager::send_private_message(uint64_t sender_id, uint64_t receiver_id,
                                          const std::string& content, MediaType media_type,
                                          const std::string& media_url, const std::string& extra,
                                          Message& message, std::string& error) {
    message.sender_id = sender_id;
    message.receiver_id = receiver_id;
    message.group_id = 0;
    message.content = content;
    message.media_type = media_type;
    message.media_url = media_url;
    message.extra = extra;
    message.status = MessageStatus::SENT;
    
    if (!database_->save_private_message(message)) {
        error = "Failed to save message";
        return false;
    }
    
    return true;
}

std::vector<Message> MessageManager::get_private_history(uint64_t user1_id, uint64_t user2_id,
                                                         int64_t before_time, int limit) {
    std::vector<Message> messages;
    database_->get_private_history(user1_id, user2_id, before_time, limit, messages);
    return messages;
}

bool MessageManager::mark_messages_read(uint64_t user_id, uint64_t peer_id) {
    return database_->mark_messages_read(user_id, peer_id);
}

int MessageManager::get_unread_count(uint64_t user_id, uint64_t peer_id) {
    return database_->get_unread_count(user_id, peer_id);
}

bool MessageManager::send_group_message(uint64_t sender_id, uint64_t group_id,
                                        const std::string& content, MediaType media_type,
                                        const std::string& media_url, const std::string& extra,
                                        Message& message, std::string& error) {
    // 检查是否是群成员
    if (!database_->is_group_member(group_id, sender_id)) {
        error = "Not a group member";
        return false;
    }
    
    message.sender_id = sender_id;
    message.group_id = group_id;
    message.receiver_id = group_id;
    message.content = content;
    message.media_type = media_type;
    message.media_url = media_url;
    message.extra = extra;
    message.status = MessageStatus::SENT;
    
    if (!database_->save_group_message(message)) {
        error = "Failed to save message";
        return false;
    }
    
    return true;
}

std::vector<Message> MessageManager::get_group_history(uint64_t group_id,
                                                       int64_t before_time, int limit) {
    std::vector<Message> messages;
    database_->get_group_history(group_id, before_time, limit, messages);
    return messages;
}

bool MessageManager::upload_media(uint64_t user_id, const std::string& file_name,
                                  const std::vector<uint8_t>& file_data, MediaType type,
                                  uint64_t& file_id, std::string& url, std::string& error) {
    ensure_media_dir();
    
    // 保存文件信息到数据库
    std::string temp_path = media_dir_ + "/temp_" + std::to_string(user_id);
    
    // 写入文件
    std::ofstream file(temp_path, std::ios::binary);
    if (!file) {
        error = "Failed to create file";
        return false;
    }
    file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    file.close();
    
    if (!database_->save_media_file(user_id, file_name, temp_path, type, file_id, url)) {
        error = "Failed to save media info";
        return false;
    }
    
    // 重命名文件
    std::string new_path = media_dir_ + "/" + std::to_string(file_id);
    rename(temp_path.c_str(), new_path.c_str());
    
    return true;
}

bool MessageManager::download_media(uint64_t file_id, std::vector<uint8_t>& file_data,
                                    MediaType& type, std::string& error) {
    std::string file_path;
    if (!database_->get_media_file(file_id, file_path, type)) {
        error = "File not found";
        return false;
    }
    
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "Failed to open file";
        return false;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    file_data.resize(size);
    file.read(reinterpret_cast<char*>(file_data.data()), size);
    
    return true;
}

} // namespace chat
