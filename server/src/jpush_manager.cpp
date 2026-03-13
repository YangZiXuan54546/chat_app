#include "jpush_manager.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace chat {

// Base64 编码
static std::string base64_encode(const std::string& input) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.c_str(), static_cast<int>(input.length()));
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    
    return result;
}

// CURL 回调函数
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

JPushManager::JPushManager(std::shared_ptr<Database> database)
    : database_(database) {
}

JPushManager::~JPushManager() {
}

void JPushManager::set_config(const std::string& app_key, const std::string& master_secret) {
    app_key_ = app_key;
    master_secret_ = master_secret;
    std::cout << "JPush configured with app_key: " << app_key_.substr(0, 8) << "..." << std::endl;
}

bool JPushManager::register_registration_id(uint64_t user_id, const std::string& registration_id) {
    std::cout << "Registering JPush Registration ID for user " << user_id 
              << ": " << registration_id.substr(0, 20) << "..." << std::endl;
    
    // 更新缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        registration_id_cache_[user_id] = registration_id;
    }
    
    // 保存到数据库
    if (database_) {
        return database_->save_fcm_token(user_id, registration_id);
    }
    
    return true;
}

std::string JPushManager::get_registration_id(uint64_t user_id) {
    // 先查缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = registration_id_cache_.find(user_id);
        if (it != registration_id_cache_.end()) {
            return it->second;
        }
    }
    
    // 查数据库
    if (database_) {
        std::string registration_id = database_->get_fcm_token(user_id);
        if (!registration_id.empty()) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            registration_id_cache_[user_id] = registration_id;
        }
        return registration_id;
    }
    
    return "";
}

void JPushManager::remove_registration_id(uint64_t user_id) {
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        registration_id_cache_.erase(user_id);
    }
    
    if (database_) {
        database_->remove_fcm_token(user_id);
    }
    
    std::cout << "Removed JPush Registration ID for user " << user_id << std::endl;
}

std::string JPushManager::get_auth_token() {
    // JPush 使用 Basic Auth: base64(appKey:masterSecret)
    std::string credentials = app_key_ + ":" + master_secret_;
    return base64_encode(credentials);
}

bool JPushManager::send_jpush_request(const std::string& registration_id,
                                      const std::string& title,
                                      const std::string& body,
                                      const std::map<std::string, std::string>& data) {
    if (!is_configured()) {
        std::cerr << "JPush not configured" << std::endl;
        return false;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }
    
    // 构建请求 JSON
    std::ostringstream json_stream;
    json_stream << "{";
    json_stream << "\"platform\":\"android\",";
    json_stream << "\"audience\":{\"registration_id\":[\"" << registration_id << "\"]},";
    json_stream << "\"notification\":{";
    json_stream << "\"android\":{";
    json_stream << "\"alert\":\"" << body << "\",";
    json_stream << "\"title\":\"" << title << "\",";
    json_stream << "\"extras\":{";
    
    // 添加额外数据
    bool first = true;
    for (const auto& [key, value] : data) {
        if (!first) json_stream << ",";
        json_stream << "\"" << key << "\":\"" << value << "\"";
        first = false;
    }
    
    json_stream << "}}},";  // 关闭 extras, android, notification
    
    // 添加 options
    json_stream << "\"options\":{";
    json_stream << "\"time_to_live\":86400,";  // 1天
    json_stream << "\"apns_production\":false";  // 开发环境
    json_stream << "}";
    json_stream << "}";
    
    std::string json_body = json_stream.str();
    
    // 设置请求
    struct curl_slist* headers = nullptr;
    std::string auth_header = "Authorization: Basic " + get_auth_token();
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.jpush.cn/v3/push");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_body.length());
    
    // 响应
    std::string response_string;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    
    // 超时设置
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // 发送请求
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "JPush request failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    std::cout << "JPush notification sent successfully to: " 
              << registration_id.substr(0, 20) << "..." << std::endl;
    std::cout << "Response: " << response_string << std::endl;
    
    return true;
}

bool JPushManager::send_notification(uint64_t user_id,
                                    const std::string& title,
                                    const std::string& body,
                                    const std::map<std::string, std::string>& data) {
    if (!is_configured()) {
        std::cout << "JPush not configured, skipping notification for user " << user_id << std::endl;
        return false;
    }
    
    std::string registration_id = get_registration_id(user_id);
    if (registration_id.empty()) {
        std::cout << "No JPush Registration ID for user " << user_id << std::endl;
        return false;
    }
    
    return send_jpush_request(registration_id, title, body, data);
}

void JPushManager::send_message_notification(uint64_t receiver_id,
                                            uint64_t sender_id,
                                            const std::string& sender_name,
                                            const std::string& message_content,
                                            bool is_group,
                                            uint64_t group_id) {
    std::string title = sender_name;
    std::string body = message_content;
    
    // 截断消息内容
    if (body.length() > 50) {
        body = body.substr(0, 50) + "...";
    }
    
    std::map<std::string, std::string> data;
    data["type"] = is_group ? "group_message" : "private_message";
    data["sender_id"] = std::to_string(sender_id);
    data["sender_name"] = sender_name;
    
    if (is_group) {
        data["group_id"] = std::to_string(group_id);
    }
    
    send_notification(receiver_id, title, body, data);
}

void JPushManager::send_group_message_notification(uint64_t receiver_id,
                                                   uint64_t group_id,
                                                   const std::string& group_name,
                                                   uint64_t sender_id,
                                                   const std::string& sender_name,
                                                   const std::string& message_content) {
    std::string title = group_name;
    std::string body = sender_name + ": " + message_content;
    
    // 截断消息内容
    if (body.length() > 50) {
        body = body.substr(0, 50) + "...";
    }
    
    std::map<std::string, std::string> data;
    data["type"] = "group_message";
    data["group_id"] = std::to_string(group_id);
    data["group_name"] = group_name;
    data["sender_id"] = std::to_string(sender_id);
    data["sender_name"] = sender_name;
    
    send_notification(receiver_id, title, body, data);
}

} // namespace chat
