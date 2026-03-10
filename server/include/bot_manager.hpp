#ifndef BOT_MANAGER_HPP
#define BOT_MANAGER_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "deepseek_client.hpp"
#include "database.hpp"
#include "server.hpp"

namespace chat {

// 前向声明
class Server;

/**
 * 机器人配置
 */
struct BotConfig {
    uint64_t bot_user_id = 0;           // 机器人用户ID
    std::string bot_username;            // 机器人用户名
    std::string bot_nickname;            // 机器人昵称
    std::string api_key;                 // DeepSeek API Key
    std::string model;                   // 使用的模型
    std::string system_prompt;           // 系统提示词
    bool auto_accept_friend = true;      // 自动接受好友请求
    bool enabled = true;                 // 是否启用
};

/**
 * 机器人管理器
 * 管理 AI 机器人用户，处理自动回复
 */
class BotManager {
public:
    BotManager(asio::io_context& io_context, 
               std::shared_ptr<Database> database);
    ~BotManager();
    
    /**
     * 初始化机器人
     * @param config 机器人配置
     * @return 是否成功
     */
    bool init(const BotConfig& config);
    
    /**
     * 设置服务器引用
     */
    void set_server(std::shared_ptr<Server> server) { server_ = server; }
    
    /**
     * 检查用户ID是否是机器人
     */
    bool is_bot(uint64_t user_id) const;
    
    /**
     * 获取机器人用户ID
     */
    uint64_t get_bot_user_id() const { return config_.bot_user_id; }
    
    /**
     * 处理发给机器人的消息
     * @param sender_id 发送者ID
     * @param content 消息内容
     * @param message_id 原始消息ID
     */
    void handle_bot_message(uint64_t sender_id, const std::string& content, uint64_t message_id);
    
    /**
     * 处理好友请求（自动接受）
     * @param from_user_id 请求者ID
     * @return 是否接受
     */
    bool handle_friend_request(uint64_t from_user_id);
    
    /**
     * 更新 API Key
     */
    void set_api_key(const std::string& api_key);
    
    /**
     * 启用/禁用机器人
     */
    void set_enabled(bool enabled) { config_.enabled = enabled; }
    
    /**
     * 检查机器人是否启用
     */
    bool is_enabled() const { return config_.enabled && deepseek_client_->is_configured(); }
    
    /**
     * 获取机器人配置
     */
    const BotConfig& get_config() const { return config_; }

private:
    /**
     * 创建机器人用户账户
     */
    bool create_bot_user();
    
    /**
     * 生成对话ID（基于用户ID）
     */
    std::string get_conversation_id(uint64_t user_id) const;

private:
    asio::io_context& io_context_;
    std::shared_ptr<Database> database_;
    std::shared_ptr<Server> server_;
    std::shared_ptr<DeepSeekClient> deepseek_client_;
    
    BotConfig config_;
    mutable std::mutex mutex_;
    
    // 记录正在处理的消息，避免重复处理
    std::unordered_set<uint64_t> processing_messages_;
};

} // namespace chat

#endif // BOT_MANAGER_HPP
