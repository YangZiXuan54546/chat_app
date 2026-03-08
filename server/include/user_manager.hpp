#ifndef USER_MANAGER_HPP
#define USER_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include "protocol.hpp"
#include "database.hpp"

namespace chat {

class Session;

class UserManager {
public:
    UserManager(std::shared_ptr<Database> database);
    
    // 用户注册
    bool register_user(const std::string& username, const std::string& password,
                       const std::string& nickname, uint64_t& user_id, std::string& error);
    
    // 用户登录
    bool login(const std::string& username, const std::string& password,
               uint64_t& user_id, UserInfo& user_info, std::string& error);
    
    // 获取用户信息
    bool get_user_info(uint64_t user_id, UserInfo& user);
    
    // 更新用户信息
    bool update_user_info(const UserInfo& user);
    
    // 更新密码
    bool update_password(uint64_t user_id, const std::string& old_password,
                         const std::string& new_password, std::string& error);
    
    // 搜索用户
    std::vector<UserInfo> search_users(const std::string& keyword, int limit = 20);
    
    // 设置在线状态
    bool set_online_status(uint64_t user_id, OnlineStatus status);
    
private:
    std::shared_ptr<Database> database_;
    
    // 简单的密码哈希（实际应用中应使用更强的加密）
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);
};

} // namespace chat

#endif // USER_MANAGER_HPP
