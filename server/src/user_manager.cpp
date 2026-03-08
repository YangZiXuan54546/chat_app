#include "user_manager.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

namespace chat {

UserManager::UserManager(std::shared_ptr<Database> database)
    : database_(database) {
}

std::string UserManager::hash_password(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), 
           password.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool UserManager::verify_password(const std::string& password, const std::string& hash) {
    return hash_password(password) == hash;
}

bool UserManager::register_user(const std::string& username, const std::string& password,
                                const std::string& nickname, uint64_t& user_id,
                                std::string& error) {
    // 检查用户名是否已存在
    UserInfo existing_user;
    if (database_->get_user_by_username(username, existing_user)) {
        error = "Username already exists";
        return false;
    }
    
    // 哈希密码
    std::string hashed_password = hash_password(password);
    
    // 创建用户
    if (!database_->create_user(username, hashed_password, nickname, user_id)) {
        error = "Failed to create user";
        return false;
    }
    
    return true;
}

bool UserManager::login(const std::string& username, const std::string& password,
                        uint64_t& user_id, UserInfo& user_info, std::string& error) {
    // 获取用户信息
    if (!database_->get_user_by_username(username, user_info)) {
        error = "User not found";
        return false;
    }
    
    // 验证密码
    std::string hashed_password = hash_password(password);
    if (!database_->verify_user(username, hashed_password, user_id)) {
        error = "Invalid password";
        return false;
    }
    
    return true;
}

bool UserManager::get_user_info(uint64_t user_id, UserInfo& user) {
    return database_->get_user_by_id(user_id, user);
}

bool UserManager::update_user_info(const UserInfo& user) {
    return database_->update_user(user);
}

bool UserManager::update_password(uint64_t user_id, const std::string& old_password,
                                  const std::string& new_password, std::string& error) {
    UserInfo user;
    if (!database_->get_user_by_id(user_id, user)) {
        error = "User not found";
        return false;
    }
    
    // 验证旧密码
    std::string hashed_old = hash_password(old_password);
    uint64_t temp_id;
    if (!database_->verify_user(user.username, hashed_old, temp_id)) {
        error = "Invalid old password";
        return false;
    }
    
    // 更新密码
    std::string hashed_new = hash_password(new_password);
    return database_->update_user_password(user_id, hashed_new);
}

std::vector<UserInfo> UserManager::search_users(const std::string& keyword, int limit) {
    return database_->search_users(keyword, limit);
}

bool UserManager::set_online_status(uint64_t user_id, OnlineStatus status) {
    return database_->update_user_online_status(user_id, status);
}

} // namespace chat
