#include "deepseek_client.hpp"
#include <cstdio>
#include <cstdlib>
#include <array>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace chat {

DeepSeekClient::DeepSeekClient(asio::io_context& io_context)
    : io_context_(io_context) {
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
    // 构建 curl 命令
    std::string url = "https://" + host + path;
    
    // 将请求体写入临时文件（避免 shell 转义问题）
    std::string tmp_file = "/tmp/deepseek_request_" + std::to_string(std::rand()) + ".json";
    FILE* f = fopen(tmp_file.c_str(), "w");
    if (f) {
        fwrite(body.c_str(), 1, body.size(), f);
        fclose(f);
    } else {
        return "";
    }
    
    // 构建 curl 命令
    std::string cmd = "curl -s -X POST \"" + url + "\" "
                     "-H \"Content-Type: " + content_type + "\" "
                     "-H \"Authorization: Bearer " + api_key_ + "\" "
                     "-d @" + tmp_file + " "
                     "--connect-timeout 30 "
                     "--max-time 60";
    
    // 执行命令
    std::array<char, 4096> buffer;
    std::string result;
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        // 清理临时文件
        remove(tmp_file.c_str());
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    pclose(pipe);
    
    // 清理临时文件
    remove(tmp_file.c_str());
    
    return result;
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
    // 在 io_context 中异步执行
    asio::post(io_context_, [this, user_message, conversation_id, callback]() {
        auto response = chat_sync(user_message, conversation_id);
        if (callback) {
            callback(response);
        }
    });
}

} // namespace chat
