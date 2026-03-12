#include "fcm_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

using json = nlohmann::json;

namespace chat {

// CURL 回调函数
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Base64 URL 编码
static std::string base64_url_encode(const std::string& input) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, input.c_str(), input.length());
    BIO_flush(bio);
    
    BUF_MEM* buffer;
    BIO_get_mem_ptr(bio, &buffer);
    std::string result(buffer->data, buffer->length);
    BIO_free_all(bio);
    
    // 转换为 URL 安全格式
    std::replace(result.begin(), result.end(), '+', '-');
    std::replace(result.begin(), result.end(), '/', '_');
    result.erase(std::remove(result.begin(), result.end(), '='), result.end());
    
    return result;
}

FcmManager::FcmManager(std::shared_ptr<Database> database)
    : database_(database) {
    // 初始化 OpenSSL
    static bool openssl_initialized = false;
    if (!openssl_initialized) {
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();
        openssl_initialized = true;
    }
}

FcmManager::~FcmManager() {
}

void FcmManager::set_config(const std::string& project_id, 
                            const std::string& service_account_key_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    project_id_ = project_id;
    service_account_key_path_ = service_account_key_path;
    
    if (!service_account_key_path.empty()) {
        if (load_service_account_key(service_account_key_path)) {
            std::cout << "FCM configured for project: " << project_id_ 
                      << " with service account: " << client_email_ << std::endl;
        } else {
            std::cerr << "Failed to load service account key" << std::endl;
        }
    }
}

bool FcmManager::load_service_account_key(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open service account key file: " << path << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    try {
        json key = json::parse(buffer.str());
        client_email_ = key.value("client_email", "");
        private_key_ = key.value("private_key", "");
        token_uri_ = key.value("token_uri", "https://oauth2.googleapis.com/token");
        
        // 处理私钥中的转义字符
        size_t pos = 0;
        while ((pos = private_key_.find("\\n", pos)) != std::string::npos) {
            private_key_.replace(pos, 2, "\n");
            pos += 1;
        }
        
        return !client_email_.empty() && !private_key_.empty();
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse service account key: " << e.what() << std::endl;
        return false;
    }
}

bool FcmManager::register_token(uint64_t user_id, const std::string& fcm_token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "Registering FCM token for user " << user_id << ": " 
              << fcm_token.substr(0, 20) << "..." << std::endl;
    
    // 更新内存缓存
    token_cache_[user_id] = fcm_token;
    
    // 保存到数据库
    if (database_) {
        return database_->save_fcm_token(user_id, fcm_token);
    }
    
    return true;
}

std::string FcmManager::get_token(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 先检查内存缓存
    auto it = token_cache_.find(user_id);
    if (it != token_cache_.end()) {
        return it->second;
    }
    
    // 从数据库获取
    if (database_) {
        std::string token = database_->get_fcm_token(user_id);
        if (!token.empty()) {
            token_cache_[user_id] = token;
        }
        return token;
    }
    
    return "";
}

void FcmManager::remove_token(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    token_cache_.erase(user_id);
    
    if (database_) {
        database_->remove_fcm_token(user_id);
    }
    
    std::cout << "Removed FCM token for user " << user_id << std::endl;
}

std::string FcmManager::get_access_token() {
    // 检查缓存的 token 是否仍然有效
    auto now = std::chrono::system_clock::now();
    auto now_ts = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
    
    if (!cached_access_token_.empty() && token_expiry_time_ > now_ts + 60) {
        return cached_access_token_;
    }
    
    if (private_key_.empty() || client_email_.empty()) {
        std::cerr << "Service account not configured for FCM" << std::endl;
        return "";
    }
    
    // 创建 JWT
    try {
        // JWT Header
        json header = {
            {"alg", "RS256"},
            {"typ", "JWT"}
        };
        
        // JWT Payload
        json payload = {
            {"iss", client_email_},
            {"scope", "https://www.googleapis.com/auth/firebase.messaging"},
            {"aud", token_uri_},
            {"iat", now_ts},
            {"exp", now_ts + 3600}
        };
        
        std::string header_b64 = base64_url_encode(header.dump());
        std::string payload_b64 = base64_url_encode(payload.dump());
        std::string sign_input = header_b64 + "." + payload_b64;
        
        // 使用私钥签名
        BIO* key_bio = BIO_new_mem_buf(private_key_.c_str(), -1);
        EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
        BIO_free(key_bio);
        
        if (!pkey) {
            std::cerr << "Failed to read private key for JWT signing" << std::endl;
            return "";
        }
        
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        EVP_DigestSignInit(md_ctx, nullptr, EVP_sha256(), nullptr, pkey);
        EVP_DigestSignUpdate(md_ctx, sign_input.c_str(), sign_input.length());
        
        size_t sig_len = 0;
        EVP_DigestSignFinal(md_ctx, nullptr, &sig_len);
        
        std::vector<unsigned char> signature(sig_len);
        EVP_DigestSignFinal(md_ctx, signature.data(), &sig_len);
        
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(pkey);
        
        std::string signature_b64 = base64_url_encode(
            std::string(reinterpret_cast<char*>(signature.data()), sig_len)
        );
        
        std::string jwt = sign_input + "." + signature_b64;
        
        // 使用 JWT 获取 Access Token
        CURL* curl = curl_easy_init();
        if (!curl) {
            return "";
        }
        
        std::string response_data;
        std::string post_data = "grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer&assertion=" + jwt;
        
        curl_easy_setopt(curl, CURLOPT_URL, token_uri_.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "Failed to get OAuth2 access token: " << curl_easy_strerror(res) << std::endl;
            return "";
        }
        
        json response = json::parse(response_data);
        cached_access_token_ = response.value("access_token", "");
        token_expiry_time_ = now_ts + response.value("expires_in", 3600);
        
        if (!cached_access_token_.empty()) {
            std::cout << "Obtained OAuth2 access token for FCM" << std::endl;
        }
        
        return cached_access_token_;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to create JWT: " << e.what() << std::endl;
        return "";
    }
}

bool FcmManager::send_fcm_request(const std::string& fcm_token,
                                   const std::string& title,
                                   const std::string& body,
                                   const std::map<std::string, std::string>& data) {
    std::string access_token = get_access_token();
    if (access_token.empty()) {
        std::cerr << "No access token available for FCM" << std::endl;
        return false;
    }
    
    // 构建请求体 (FCM HTTP v1 API)
    json message = {
        {"message", {
            {"token", fcm_token},
            {"notification", {
                {"title", title},
                {"body", body}
            }},
            {"android", {
                {"priority", "high"},
                {"notification", {
                    {"channel_id", "messages"},
                    {"priority", "high"},
                    {"sound", "default"},
                    {"icon", "@mipmap/ic_launcher"}
                }}
            }},
            {"apns", {
                {"payload", {
                    {"aps", {
                        {"alert", {
                            {"title", title},
                            {"body", body}
                        }},
                        {"sound", "default"},
                        {"badge", 1}
                    }}
                }}
            }}
        }}
    };
    
    // 添加自定义数据
    if (!data.empty()) {
        json data_json;
        for (const auto& [key, value] : data) {
            data_json[key] = value;
        }
        message["message"]["data"] = data_json;
    }
    
    std::string request_body = message.dump();
    
    // 发送请求
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string url = "https://fcm.googleapis.com/v1/projects/" + project_id_ + "/messages:send";
    std::string response_data;
    
    struct curl_slist* headers = nullptr;
    std::string auth_header = "Authorization: Bearer " + access_token;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || http_code >= 400) {
        std::cerr << "FCM request failed: " << curl_easy_strerror(res) 
                  << ", HTTP " << http_code << std::endl;
        std::cerr << "Response: " << response_data << std::endl;
        return false;
    }
    
    std::cout << "FCM notification sent successfully to token: " 
              << fcm_token.substr(0, 20) << "..." << std::endl;
    return true;
}

bool FcmManager::send_notification(uint64_t user_id,
                                   const std::string& title,
                                   const std::string& body,
                                   const std::map<std::string, std::string>& data) {
    if (!is_configured()) {
        std::cout << "FCM not configured, skipping notification for user " << user_id << std::endl;
        return false;
    }
    
    std::string fcm_token = get_token(user_id);
    if (fcm_token.empty()) {
        std::cout << "No FCM token registered for user " << user_id << std::endl;
        return false;
    }
    
    return send_fcm_request(fcm_token, title, body, data);
}

void FcmManager::send_message_notification(uint64_t receiver_id,
                                           const std::string& sender_name,
                                           const std::string& message_content,
                                           uint64_t message_id,
                                           uint64_t sender_id) {
    // 截断消息内容用于通知显示
    std::string content = message_content;
    if (content.length() > 100) {
        content = content.substr(0, 97) + "...";
    }
    
    std::map<std::string, std::string> data = {
        {"type", "private_message"},
        {"message_id", std::to_string(message_id)},
        {"sender_id", std::to_string(sender_id)},
        {"sender_name", sender_name}
    };
    
    send_notification(receiver_id, sender_name, content, data);
}

void FcmManager::send_group_message_notification(uint64_t receiver_id,
                                                  const std::string& group_name,
                                                  const std::string& sender_name,
                                                  const std::string& message_content,
                                                  uint64_t message_id,
                                                  uint64_t group_id,
                                                  uint64_t sender_id) {
    std::string title = group_name + " - " + sender_name;
    
    std::string content = message_content;
    if (content.length() > 100) {
        content = content.substr(0, 97) + "...";
    }
    
    std::map<std::string, std::string> data = {
        {"type", "group_message"},
        {"message_id", std::to_string(message_id)},
        {"group_id", std::to_string(group_id)},
        {"sender_id", std::to_string(sender_id)},
        {"sender_name", sender_name},
        {"group_name", group_name}
    };
    
    send_notification(receiver_id, title, content, data);
}

} // namespace chat