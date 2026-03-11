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
    
    // 获取或创建用户当前会话
    std::string conversation_id = get_user_session_id(sender_id);
    
    // 检查会话是否超过字数限制
    if (is_session_over_limit(sender_id, conversation_id)) {
        // 发送提示消息
        if (server_) {
            Message limit_message;
            limit_message.sender_id = config_.bot_user_id;
            limit_message.receiver_id = sender_id;
            limit_message.content = "当前会话已超过字数限制（" + std::to_string(config_.max_char_count) + 
                                    " 字）。请发送 /new 开始新会话，或发送 /sessions 查看历史会话。";
            limit_message.media_type = MediaType::TEXT;
            limit_message.created_at = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            database_->save_private_message(limit_message);
            server_->send_to_user(sender_id,
                Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, limit_message.to_json()));
        }
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            processing_messages_.erase(message_id);
        }
        return;
    }
    
    // 检查是否是命令
    if (content == "/new") {
        // 创建新会话
        std::string new_session = create_new_session(sender_id);
        
        if (server_) {
            Message reply;
            reply.sender_id = config_.bot_user_id;
            reply.receiver_id = sender_id;
            reply.content = "已创建新会话。当前会话ID: " + new_session;
            reply.media_type = MediaType::TEXT;
            reply.created_at = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            database_->save_private_message(reply);
            server_->send_to_user(sender_id,
                Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, reply.to_json()));
        }
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            processing_messages_.erase(message_id);
        }
        return;
    }
    
    if (content == "/sessions") {
        // 列出所有会话
        auto sessions = get_user_sessions(sender_id);
        
        std::string reply_content = "您的历史会话:\n";
        for (const auto& session : sessions) {
            int char_count = database_->get_bot_conversation_char_count(sender_id, session);
            reply_content += "- " + session + " (" + std::to_string(char_count) + " 字)\n";
        }
        
        if (sessions.empty()) {
            reply_content = "暂无历史会话。";
        }
        
        if (server_) {
            Message reply;
            reply.sender_id = config_.bot_user_id;
            reply.receiver_id = sender_id;
            reply.content = reply_content;
            reply.media_type = MediaType::TEXT;
            reply.created_at = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            database_->save_private_message(reply);
            server_->send_to_user(sender_id,
                Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, reply.to_json()));
        }
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            processing_messages_.erase(message_id);
        }
        return;
    }
    
    // 保存用户消息到数据库
    database_->save_bot_conversation(sender_id, conversation_id, "user", content);
    
    // 加载历史对话
    std::vector<std::pair<std::string, std::string>> history;
    database_->get_bot_conversation(sender_id, conversation_id, history, 20);
    
    // 设置对话上下文
    deepseek_client_->clear_conversation(conversation_id);
    for (const auto& msg : history) {
        // 这里我们不手动添加历史，让 DeepSeekClient 内部管理
    }
    
    // 调用 DeepSeek API
    deepseek_client_->chat(content, conversation_id, 
        [this, sender_id, message_id, conversation_id](const DeepSeekResponse& response) {
            // 处理完成后移除标记
            {
                std::lock_guard<std::mutex> lock(mutex_);
                processing_messages_.erase(message_id);
            }
            
            if (!response.success) {
                std::cerr << "DeepSeek API error: " << response.error << std::endl;
                
                // 发送错误消息
                if (server_) {
                    Message error_msg;
                    error_msg.sender_id = config_.bot_user_id;
                    error_msg.receiver_id = sender_id;
                    error_msg.content = "抱歉，AI 服务暂时不可用，请稍后再试。";
                    error_msg.media_type = MediaType::TEXT;
                    error_msg.created_at = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    database_->save_private_message(error_msg);
                    server_->send_to_user(sender_id,
                        Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, error_msg.to_json()));
                }
                return;
            }
            
            std::cout << "Bot response: " << response.content << std::endl;
            
            // 保存助手回复到数据库
            database_->save_bot_conversation(sender_id, conversation_id, "assistant", response.content);
            
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
            
            // 检查是否接近字数限制
            int current_count = database_->get_bot_conversation_char_count(sender_id, conversation_id);
            if (current_count > config_.max_char_count * 0.9) {
                // 发送警告
                if (server_) {
                    Message warning;
                    warning.sender_id = config_.bot_user_id;
                    warning.receiver_id = sender_id;
                    warning.content = "提示: 当前会话字数已接近限制（" + std::to_string(current_count) + 
                                     "/" + std::to_string(config_.max_char_count) + 
                                     "）。发送 /new 可创建新会话。";
                    warning.media_type = MediaType::TEXT;
                    warning.created_at = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count();
                    
                    database_->save_private_message(warning);
                    server_->send_to_user(sender_id,
                        Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, warning.to_json()));
                }
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

std::string BotManager::get_user_session_id(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = user_sessions_.find(user_id);
    if (it != user_sessions_.end()) {
        return it->second;
    }
    
    // 创建默认会话
    std::string session_id = "user_" + std::to_string(user_id) + "_default";
    user_sessions_[user_id] = session_id;
    return session_id;
}

void BotManager::set_user_session_id(uint64_t user_id, const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_sessions_[user_id] = session_id;
}

std::string BotManager::create_new_session(uint64_t user_id) {
    std::string new_session_id;
    database_->create_new_bot_session(user_id, new_session_id);
    
    std::lock_guard<std::mutex> lock(mutex_);
    user_sessions_[user_id] = new_session_id;
    
    return new_session_id;
}

std::vector<std::string> BotManager::get_user_sessions(uint64_t user_id) {
    std::vector<std::string> sessions;
    database_->get_user_bot_sessions(user_id, sessions);
    return sessions;
}

bool BotManager::is_session_over_limit(uint64_t user_id, const std::string& session_id) {
    int char_count = database_->get_bot_conversation_char_count(user_id, session_id);
    return char_count >= config_.max_char_count;
}

} // namespace chat
