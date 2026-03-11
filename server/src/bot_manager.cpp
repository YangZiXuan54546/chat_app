#include "bot_manager.hpp"
#include "user_manager.hpp"
#include "friend_manager.hpp"
#include "message_manager.hpp"
#include "protocol.hpp"
#include <iostream>
#include <chrono>
#include <sstream>

namespace chat {

BotManager::BotManager(asio::io_context& io_context,
                       std::shared_ptr<Database> database)
    : io_context_(io_context)
    , database_(database)
    , deepseek_client_(std::make_shared<DeepSeekClient>(io_context)) {
}

BotManager::~BotManager() {
}

bool BotManager::init(const BotConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // 配置 DeepSeek 客户端
    if (!config_.api_key.empty()) {
        deepseek_client_->set_api_key(config_.api_key);
    }
    
    if (!config_.model.empty()) {
        deepseek_client_->set_model(config_.model);
    }
    
    if (!config_.system_prompt.empty()) {
        deepseek_client_->set_system_prompt(config_.system_prompt);
    }
    
    // 如果机器人用户ID为0，尝试创建或查找机器人用户
    if (config_.bot_user_id == 0) {
        if (!create_bot_user()) {
            std::cerr << "Failed to create bot user" << std::endl;
            return false;
        }
    } else {
        // 验证机器人用户是否存在
        UserInfo user;
        if (!database_->get_user_by_id(config_.bot_user_id, user)) {
            std::cerr << "Bot user not found: " << config_.bot_user_id << std::endl;
            return false;
        }
    }
    
    std::cout << "Bot initialized: user_id=" << config_.bot_user_id 
              << ", username=" << config_.bot_username << std::endl;
    
    return true;
}

bool BotManager::create_bot_user() {
    // 检查是否已存在机器人用户
    if (!config_.bot_username.empty()) {
        UserInfo existing_user;
        if (database_->get_user_by_username(config_.bot_username, existing_user)) {
            config_.bot_user_id = existing_user.user_id;
            std::cout << "Found existing bot user: " << config_.bot_user_id << std::endl;
            return true;
        }
    }
    
    // 创建新的机器人用户
    std::string username = config_.bot_username.empty() ? "deepseek_bot" : config_.bot_username;
    std::string nickname = config_.bot_nickname.empty() ? "AI 助手" : config_.bot_nickname;
    std::string password = "bot_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    uint64_t user_id = 0;
    
    if (database_->create_user(username, password, nickname, user_id)) {
        config_.bot_user_id = user_id;
        config_.bot_username = username;
        config_.bot_nickname = nickname;
        
        std::cout << "Created bot user: " << user_id << std::endl;
        return true;
    }
    
    std::cerr << "Failed to create bot user" << std::endl;
    return false;
}

bool BotManager::is_bot(uint64_t user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return user_id == config_.bot_user_id;
}

std::string BotManager::get_conversation_id(uint64_t user_id) const {
    return "user_" + std::to_string(user_id);
}

void BotManager::handle_bot_message(uint64_t sender_id, const std::string& content, uint64_t message_id) {
    if (!is_enabled()) {
        std::cout << "Bot is disabled, skipping message" << std::endl;
        return;
    }
    
    // 检查是否已在处理中
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (processing_messages_.count(message_id) > 0) {
            return;
        }
        processing_messages_.insert(message_id);
    }
    
    std::cout << "Bot handling message from user " << sender_id << ": " << content << std::endl;
    
    // 获取对话ID
    std::string conversation_id = get_conversation_id(sender_id);
    
    // 调用 DeepSeek API
    deepseek_client_->chat(content, conversation_id, 
        [this, sender_id, message_id](const DeepSeekResponse& response) {
            // 处理完成后移除标记
            {
                std::lock_guard<std::mutex> lock(mutex_);
                processing_messages_.erase(message_id);
            }
            
            if (!response.success) {
                std::cerr << "DeepSeek API error: " << response.error << std::endl;
                return;
            }
            
            std::cout << "Bot response: " << response.content << std::endl;
            
            // 发送回复消息
            if (server_) {
                // 构建消息
                Message reply_message;
                reply_message.sender_id = config_.bot_user_id;
                reply_message.receiver_id = sender_id;
                reply_message.content = response.content;
                reply_message.media_type = MediaType::TEXT;
                reply_message.created_at = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                
                // 保存消息到数据库
                database_->save_private_message(reply_message);
                
                // 发送给用户
                server_->send_to_user(sender_id,
                    Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, reply_message.to_json()));
            }
        });
}

bool BotManager::handle_friend_request(uint64_t from_user_id) {
    if (!config_.auto_accept_friend) {
        return false;
    }
    
    // 接受好友请求
    // accept_friend_request(acceptor, requester): 更新 requester->acceptor 的请求状态并添加反向关系
    // from_user_id 是请求者，bot 是接受者
    if (database_->accept_friend_request(config_.bot_user_id, from_user_id)) {
        std::cout << "Bot accepted friend request from user " << from_user_id << std::endl;
        
        // 通知用户好友请求已被接受
        if (server_) {
            // 构建通知消息
            json notification = {
                {"friend_id", config_.bot_user_id},
                {"friend_username", config_.bot_username},
                {"friend_nickname", config_.bot_nickname},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()}
            };
            
            // 发送好友请求接受通知给用户
            server_->send_to_user(from_user_id,
                Protocol::serialize(MessageType::FRIEND_ACCEPT, 0, notification));
            
            std::cout << "Sent friend accept notification to user " << from_user_id << std::endl;
        }
        
        return true;
    }
    
    std::cerr << "Failed to accept friend request" << std::endl;
    return false;
}

void BotManager::set_api_key(const std::string& api_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.api_key = api_key;
    deepseek_client_->set_api_key(api_key);
}

} // namespace chat
