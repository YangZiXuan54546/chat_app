#ifndef FCM_MANAGER_HPP
#define FCM_MANAGER_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include "database.hpp"

namespace chat {

/**
 * FCM 推送管理器
 * 管理 FCM Token 和发送推送通知
 */
class FcmManager {
public:
    FcmManager(std::shared_ptr<Database> database);
    ~FcmManager();
    
    /**
     * 设置 Firebase 项目配置
     * @param project_id Firebase 项目 ID
     * @param server_key Firebase Server Key (已弃用，推荐使用服务账号)
     * @param service_account_key 服务账号 JSON 密钥文件路径
     */
    void set_config(const std::string& project_id, 
                    const std::string& service_account_key_path);
    
    /**
     * 注册用户的 FCM Token
     * @param user_id 用户 ID
     * @param fcm_token FCM Token
     * @return 是否成功
     */
    bool register_token(uint64_t user_id, const std::string& fcm_token);
    
    /**
     * 获取用户的 FCM Token
     * @param user_id 用户 ID
     * @return FCM Token (如果不存在返回空字符串)
     */
    std::string get_token(uint64_t user_id);
    
    /**
     * 删除用户的 FCM Token
     * @param user_id 用户 ID
     */
    void remove_token(uint64_t user_id);
    
    /**
     * 发送推送通知给指定用户
     * @param user_id 目标用户 ID
     * @param title 通知标题
     * @param body 通知内容
     * @param data 额外数据
     * @return 是否成功
     */
    bool send_notification(uint64_t user_id, 
                          const std::string& title, 
                          const std::string& body,
                          const std::map<std::string, std::string>& data = {});
    
    /**
     * 发送消息通知
     * @param receiver_id 接收者 ID
     * @param sender_name 发送者名称
     * @param message_content 消息内容
     * @param message_id 消息 ID
     * @param sender_id 发送者 ID
     */
    void send_message_notification(uint64_t receiver_id,
                                   const std::string& sender_name,
                                   const std::string& message_content,
                                   uint64_t message_id,
                                   uint64_t sender_id);
    
    /**
     * 发送群消息通知
     * @param receiver_id 接收者 ID
     * @param group_name 群组名称
     * @param sender_name 发送者名称
     * @param message_content 消息内容
     * @param message_id 消息 ID
     * @param group_id 群组 ID
     * @param sender_id 发送者 ID
     */
    void send_group_message_notification(uint64_t receiver_id,
                                          const std::string& group_name,
                                          const std::string& sender_name,
                                          const std::string& message_content,
                                          uint64_t message_id,
                                          uint64_t group_id,
                                          uint64_t sender_id);
    
    /**
     * 检查是否已配置
     */
    bool is_configured() const { return !project_id_.empty(); }

private:
    /**
     * 获取 OAuth 2.0 Access Token
     */
    std::string get_access_token();
    
    /**
     * 发送 HTTP 请求到 FCM API
     */
    bool send_fcm_request(const std::string& fcm_token,
                         const std::string& title,
                         const std::string& body,
                         const std::map<std::string, std::string>& data);
    
    /**
     * 解析服务账号密钥文件
     */
    bool load_service_account_key(const std::string& path);

private:
    std::shared_ptr<Database> database_;
    std::mutex mutex_;
    
    // Firebase 配置
    std::string project_id_;
    std::string service_account_key_path_;
    
    // 服务账号信息
    std::string client_email_;
    std::string private_key_;
    std::string token_uri_;
    
    // Access Token 缓存
    std::string cached_access_token_;
    int64_t token_expiry_time_ = 0;
    
    // 内存缓存
    std::unordered_map<uint64_t, std::string> token_cache_;
};

} // namespace chat

#endif // FCM_MANAGER_HPP
