#ifndef JPUSH_MANAGER_HPP
#define JPUSH_MANAGER_HPP

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <functional>
#include "database.hpp"

namespace chat {

/**
 * 极光推送管理器
 * 支持国内各厂商推送通道 (小米/华为/OPPO/vivo等)
 */
class JPushManager {
public:
    JPushManager(std::shared_ptr<Database> database);
    ~JPushManager();
    
    /// 配置 JPush
    void set_config(const std::string& app_key, const std::string& master_secret);
    
    /// 检查是否已配置
    bool is_configured() const { return !app_key_.empty() && !master_secret_.empty(); }
    
    /// 注册 Registration ID
    bool register_registration_id(uint64_t user_id, const std::string& registration_id);
    
    /// 获取用户的 Registration ID
    std::string get_registration_id(uint64_t user_id);
    
    /// 移除 Registration ID
    void remove_registration_id(uint64_t user_id);
    
    /// 发送推送通知
    bool send_notification(uint64_t user_id, 
                          const std::string& title, 
                          const std::string& body,
                          const std::map<std::string, std::string>& data = {});
    
    /// 发送消息通知
    void send_message_notification(uint64_t receiver_id,
                                   uint64_t sender_id,
                                   const std::string& sender_name,
                                   const std::string& message_content,
                                   bool is_group = false,
                                   uint64_t group_id = 0);
    
    /// 发送群消息通知
    void send_group_message_notification(uint64_t receiver_id,
                                         uint64_t group_id,
                                         const std::string& group_name,
                                         uint64_t sender_id,
                                         const std::string& sender_name,
                                         const std::string& message_content);

private:
    /// 发送 JPush API 请求
    bool send_jpush_request(const std::string& registration_id,
                           const std::string& title,
                           const std::string& body,
                           const std::map<std::string, std::string>& data);
    
    /// 获取 JPush Auth Token (Base64 encoding of appKey:masterSecret)
    std::string get_auth_token();
    
    std::shared_ptr<Database> database_;
    std::string app_key_;
    std::string master_secret_;
    
    // Token 缓存
    std::map<uint64_t, std::string> registration_id_cache_;
    std::mutex cache_mutex_;
};

} // namespace chat

#endif // JPUSH_MANAGER_HPP
