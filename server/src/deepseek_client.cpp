#include "deepseek_client.hpp"
#include <iostream>
#include <thread>
#include <future>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

using json = nlohmann::json;

namespace chat {

// CURL 回调函数 - 接收响应数据
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

DeepSeekClient::DeepSeekClient(asio::io_context& io_context)
    : io_context_(io_context) {
    // 初始化 libcurl (全局初始化，只需调用一次)
    static bool curl_initialized = false;
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }
}

DeepSeekClient::~DeepSeekClient() {
}

void DeepSeekClient::set_api_key(const std::string& api_key) {
    api_key_ = api_key;
}

void DeepSeekClient::set_api_endpoint(const std::string& endpoint) {
    api_endpoint_ = endpoint;
}

void DeepSeekClient::set_model(const std::string& model) {
    model_ = model;
}

void DeepSeekClient::set_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
}

std::vector<DeepSeekMessage>& DeepSeekClient::get_conversation(const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(conversations_mutex_);
    
    auto it = conversations_.find(conversation_id);
    if (it == conversations_.end()) {
        // 创建新对话，添加系统提示
        auto& conv = conversations_[conversation_id];
        if (!system_prompt_.empty()) {
            conv.emplace_back("system", system_prompt_);
        }
        return conv;
    }
    return it->second;
}

void DeepSeekClient::clear_conversation(const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(conversations_mutex_);
    conversations_.erase(conversation_id);
}

void DeepSeekClient::clear_all_conversations() {
    std::lock_guard<std::mutex> lock(conversations_mutex_);
    conversations_.clear();
}

std::string DeepSeekClient::build_request(const std::vector<DeepSeekMessage>& messages) {
    json request_json = {
        {"model", model_},
        {"messages", json::array()},
        {"max_tokens", max_tokens_},
        {"temperature", temperature_},
        {"stream", false}
    };
    
    for (const auto& msg : messages) {
        request_json["messages"].push_back({
            {"role", msg.role},
            {"content", msg.content}
        });
    }
    
    return request_json.dump();
}

DeepSeekResponse DeepSeekClient::parse_response(const std::string& json_response) {
    DeepSeekResponse response;
    
    try {
        json resp = json::parse(json_response);
        
        if (resp.contains("error")) {
            response.success = false;
            response.error = resp["error"].value("message", "Unknown error");
            return response;
        }
        
        if (resp.contains("choices") && !resp["choices"].empty()) {
            response.success = true;
            response.content = resp["choices"][0]["message"].value("content", "");
            
            if (resp.contains("usage")) {
                response.tokens_used = resp["usage"].value("total_tokens", 0);
            }
        } else {
            response.success = false;
            response.error = "Empty response from API";
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = std::string("JSON parse error: ") + e.what();
    }
    
    return response;
}

std::string DeepSeekClient::http_post(const std::string& host, const std::string& path,
                                       const std::string& body, const std::string& content_type) {
    std::string response_data;
    
    // 创建 CURL 句柄
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return "";
    }
    
    // 构建完整 URL
    std::string url = "https://" + host + path;
    
    // 设置请求头
    struct curl_slist* headers = nullptr;
    std::string auth_header = "Authorization: Bearer " + api_key_;
    std::string content_header = "Content-Type: " + content_type;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, content_header.c_str());
    
    // 配置 CURL 选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    // SSL 配置
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // 超时配置 - 减少超时时间避免阻塞服务器
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    
    // 检查错误
    if (res != CURLE_OK) {
        std::cerr << "CURL request failed: " << curl_easy_strerror(res) << std::endl;
        response_data.clear();
    }
    
    // 获取 HTTP 状态码
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        std::cerr << "HTTP error: " << http_code << std::endl;
        std::cerr << "Response: " << response_data << std::endl;
    }
    
    // 清理
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return response_data;
}

DeepSeekResponse DeepSeekClient::chat_sync(const std::string& user_message,
                                            const std::string& conversation_id) {
    DeepSeekResponse response;
    
    if (!is_configured()) {
        response.success = false;
        response.error = "API key not configured";
        return response;
    }
    
    // 获取对话上下文
    auto& conversation = get_conversation(conversation_id);
    
    // 添加用户消息
    conversation.emplace_back("user", user_message);
    
    // 构建请求
    std::string request_body = build_request(conversation);
    
    // 发送请求
    std::string response_body = http_post(api_endpoint_, api_path_, request_body, "application/json");
    
    if (response_body.empty()) {
        response.success = false;
        response.error = "Failed to send request to DeepSeek API";
        // 移除失败的用户消息
        conversation.pop_back();
        return response;
    }
    
    // 解析响应
    response = parse_response(response_body);
    
    if (response.success) {
        // 添加助手回复到对话上下文
        conversation.emplace_back("assistant", response.content);
        
        // 限制上下文长度
        while (conversation.size() > static_cast<size_t>(max_context_messages_ + 1)) {
            // 保留系统提示（第一条）
            conversation.erase(conversation.begin() + 1);
        }
    } else {
        // 移除失败的用户消息
        conversation.pop_back();
    }
    
    return response;
}

void DeepSeekClient::chat(const std::string& user_message,
                          const std::string& conversation_id,
                          ResponseCallback callback) {
    // 使用独立线程执行 HTTP 请求，避免阻塞 io_context 工作线程
    std::thread([this, user_message, conversation_id, callback]() {
        auto response = chat_sync(user_message, conversation_id);
        // 回调需要在 io_context 线程中执行
        asio::post(io_context_, [callback, response]() {
            if (callback) {
                callback(response);
            }
        });
    }).detach();
}

} // namespace chat
