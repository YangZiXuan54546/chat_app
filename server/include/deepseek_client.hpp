#ifndef DEEPSEEK_CLIENT_HPP
#define DEEPSEEK_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <asio.hpp>
#include <openssl/ssl.h>

namespace chat {

/**
 * DeepSeek API 消息结构
 */
struct DeepSeekMessage {
    std::string role;     // "system", "user", "assistant"
    std::string content;
    
    DeepSeekMessage(const std::string& r, const std::string& c) 
        : role(r), content(c) {}
};

/**
 * DeepSeek API 响应
 */
struct DeepSeekResponse {
    bool success = false;
    std::string content;
    std::string error;
    int tokens_used = 0;
};

/**
 * DeepSeek API 客户端
 * 支持异步调用 DeepSeek Chat API
 */
class DeepSeekClient {
public:
    using ResponseCallback = std::function<void(const DeepSeekResponse&)>;
    
    DeepSeekClient(asio::io_context& io_context);
    ~DeepSeekClient();
    
    /**
     * 设置 API Key
     */
    void set_api_key(const std::string& api_key);
    
    /**
     * 设置 API 端点 (默认为 DeepSeek 官方端点)
     */
    void set_api_endpoint(const std::string& endpoint);
    
    /**
     * 设置模型名称 (默认为 deepseek-chat)
     */
    void set_model(const std::string& model);
    
    /**
     * 设置系统提示词
     */
    void set_system_prompt(const std::string& prompt);
    
    /**
     * 发送聊天请求 (异步)
     * @param user_message 用户消息
     * @param conversation_id 对话ID (用于维护上下文)
     * @param callback 响应回调
     */
    void chat(const std::string& user_message, 
              const std::string& conversation_id,
              ResponseCallback callback);
    
    /**
     * 发送聊天请求 (同步)
     */
    DeepSeekResponse chat_sync(const std::string& user_message,
                                const std::string& conversation_id);
    
    /**
     * 清除对话上下文
     */
    void clear_conversation(const std::string& conversation_id);
    
    /**
     * 清除所有对话上下文
     */
    void clear_all_conversations();
    
    /**
     * 检查是否已配置 API Key
     */
    bool is_configured() const { return !api_key_.empty(); }
    
    /**
     * 设置最大上下文消息数
     */
    void set_max_context_messages(int max) { max_context_messages_ = max; }

private:
    /**
     * 构建请求 JSON
     */
    std::string build_request(const std::vector<DeepSeekMessage>& messages);
    
    /**
     * 解析响应 JSON
     */
    DeepSeekResponse parse_response(const std::string& json_response);
    
    /**
     * 执行 HTTP POST 请求 (同步)
     */
    std::string http_post(const std::string& host, const std::string& path,
                          const std::string& body, const std::string& content_type);
    
    /**
     * 获取或创建对话上下文
     */
    std::vector<DeepSeekMessage>& get_conversation(const std::string& conversation_id);
    
private:
    asio::io_context& io_context_;
    std::string api_key_;
    std::string api_endpoint_ = "api.deepseek.com";
    std::string api_path_ = "/v1/chat/completions";
    std::string model_ = "deepseek-chat";
    std::string system_prompt_ = "你是一个友好的聊天助手。请用简洁、自然的方式回复用户的消息。";
    
    int max_context_messages_ = 10;  // 最大上下文消息数
    int max_tokens_ = 1000;          // 最大生成 token 数
    float temperature_ = 0.7f;       // 温度参数
    
    std::mutex conversations_mutex_;
    std::unordered_map<std::string, std::vector<DeepSeekMessage>> conversations_;
};

} // namespace chat

#endif // DEEPSEEK_CLIENT_HPP
