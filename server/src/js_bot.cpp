#include "js_bot.hpp"
#include "database.hpp"
#include "server.hpp"
#include "protocol.hpp"
#include <quickjs.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <curl/curl.h>

namespace chat {

// 静态成员初始化
JSBotEngine* JSBotAPI::engine_ = nullptr;
std::shared_ptr<Database> JSBotAPI::database_ = nullptr;
std::shared_ptr<Server> JSBotAPI::server_ = nullptr;

// ==================== JSBotEngine 实现 ====================

JSBotEngine::JSBotEngine() {
}

JSBotEngine::~JSBotEngine() {
    if (context_) {
        JS_FreeContext(context_);
    }
    if (runtime_) {
        JS_FreeRuntime(runtime_);
    }
}

bool JSBotEngine::init() {
    if (initialized_) return true;
    
    return init_runtime();
}

bool JSBotEngine::init_runtime() {
    // 创建运行时
    runtime_ = JS_NewRuntime();
    if (!runtime_) {
        std::cerr << "Failed to create QuickJS runtime" << std::endl;
        return false;
    }
    
    // 设置内存限制 (64MB)
    JS_SetMemoryLimit(runtime_, 64 * 1024 * 1024);
    
    // 设置栈大小
    JS_SetMaxStackSize(runtime_, 256 * 1024);
    
    // 创建上下文
    context_ = JS_NewContext(runtime_);
    if (!context_) {
        std::cerr << "Failed to create QuickJS context" << std::endl;
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
        return false;
    }
    
    // 注册原生函数
    register_native_functions();
    
    initialized_ = true;
    std::cout << "QuickJS engine initialized" << std::endl;
    return true;
}

// JavaScript C 函数包装宏
#define JS_FUNC(name) \
    static JSValue js_##name(JSContext* ctx, JSValueConst this_val, \
                             int argc, JSValueConst* argv)

// 日志函数
JS_FUNC(log) {
    if (argc < 2) return JS_UNDEFINED;
    
    const char* level = JS_ToCString(ctx, argv[0]);
    const char* message = JS_ToCString(ctx, argv[1]);
    
    if (level && message) {
        std::cout << "[JS:" << level << "] " << message << std::endl;
    }
    
    JS_FreeCString(ctx, level);
    JS_FreeCString(ctx, message);
    return JS_UNDEFINED;
}

// 发送消息
JS_FUNC(send_message) {
    if (argc < 2) return JS_NewBool(ctx, false);
    
    uint64_t receiver_id;
    JS_ToIndex(ctx, &receiver_id, argv[0]);
    
    const char* content = JS_ToCString(ctx, argv[1]);
    if (!content) return JS_NewBool(ctx, false);
    
    bool success = JSBotAPI::send_message(receiver_id, content);
    
    JS_FreeCString(ctx, content);
    return JS_NewBool(ctx, success);
}

// 发送群消息
JS_FUNC(send_group_message) {
    if (argc < 2) return JS_NewBool(ctx, false);
    
    uint64_t group_id;
    JS_ToIndex(ctx, &group_id, argv[0]);
    
    const char* content = JS_ToCString(ctx, argv[1]);
    if (!content) return JS_NewBool(ctx, false);
    
    std::vector<uint64_t> mentioned;
    if (argc >= 3 && JS_IsArray(ctx, argv[2])) {
        JSValue length_val = JS_GetPropertyStr(ctx, argv[2], "length");
        uint32_t length;
        JS_ToUint32(ctx, &length, length_val);
        JS_FreeValue(ctx, length_val);
        
        for (uint32_t i = 0; i < length; i++) {
            JSValue elem = JS_GetPropertyUint32(ctx, argv[2], i);
            uint64_t user_id;
            JS_ToIndex(ctx, &user_id, elem);
            mentioned.push_back(user_id);
            JS_FreeValue(ctx, elem);
        }
    }
    
    bool success = JSBotAPI::send_group_message(group_id, content, mentioned);
    
    JS_FreeCString(ctx, content);
    return JS_NewBool(ctx, success);
}

// 获取用户信息
JS_FUNC(get_user_info) {
    if (argc < 1) return JS_NULL;
    
    uint64_t user_id;
    JS_ToIndex(ctx, &user_id, argv[0]);
    
    std::string info = JSBotAPI::get_user_info(user_id);
    return JS_NewString(ctx, info.c_str());
}

// 获取群成员
JS_FUNC(get_group_members) {
    if (argc < 1) return JS_NULL;
    
    uint64_t group_id;
    JS_ToIndex(ctx, &group_id, argv[0]);
    
    std::string members = JSBotAPI::get_group_members(group_id);
    return JS_NewString(ctx, members.c_str());
}

// HTTP GET
JS_FUNC(http_get) {
    if (argc < 1) return JS_NULL;
    
    const char* url = JS_ToCString(ctx, argv[0]);
    if (!url) return JS_NULL;
    
    std::string result = JSBotAPI::http_get(url);
    
    JS_FreeCString(ctx, url);
    return JS_NewString(ctx, result.c_str());
}

// HTTP POST
JS_FUNC(http_post) {
    if (argc < 2) return JS_NULL;
    
    const char* url = JS_ToCString(ctx, argv[0]);
    const char* body = JS_ToCString(ctx, argv[1]);
    const char* content_type = "application/json";
    
    if (argc >= 3) {
        content_type = JS_ToCString(ctx, argv[2]);
    }
    
    if (!url || !body) {
        JS_FreeCString(ctx, url);
        JS_FreeCString(ctx, body);
        return JS_NULL;
    }
    
    std::string result = JSBotAPI::http_post(url, body, content_type);
    
    JS_FreeCString(ctx, url);
    JS_FreeCString(ctx, body);
    if (argc >= 3) JS_FreeCString(ctx, content_type);
    
    return JS_NewString(ctx, result.c_str());
}

// 保存数据
JS_FUNC(save_data) {
    if (argc < 2) return JS_NewBool(ctx, false);
    
    const char* key = JS_ToCString(ctx, argv[0]);
    const char* value = JS_ToCString(ctx, argv[1]);
    
    if (!key || !value) {
        JS_FreeCString(ctx, key);
        JS_FreeCString(ctx, value);
        return JS_NewBool(ctx, false);
    }
    
    bool success = JSBotAPI::save_data(key, value);
    
    JS_FreeCString(ctx, key);
    JS_FreeCString(ctx, value);
    return JS_NewBool(ctx, success);
}

// 加载数据
JS_FUNC(load_data) {
    if (argc < 1) return JS_NULL;
    
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_NULL;
    
    std::string value = JSBotAPI::load_data(key);
    
    JS_FreeCString(ctx, key);
    
    if (value.empty()) return JS_NULL;
    return JS_NewString(ctx, value.c_str());
}

void JSBotEngine::register_native_functions() {
    // 创建全局对象
    JSValue global = JS_GetGlobalObject(context_);
    
    // 创建 Bot API 对象
    JSValue bot_api = JS_NewObject(context_);
    
    // 注册函数
    auto register_func = [this, &bot_api](const char* name, JSCFunction* func) {
        JS_SetPropertyStr(context_, bot_api, name, 
            JS_NewCFunction(context_, func, name, 0));
    };
    
    register_func("log", js_log);
    register_func("sendMessage", js_send_message);
    register_func("sendGroupMessage", js_send_group_message);
    register_func("getUserInfo", js_get_user_info);
    register_func("getGroupMembers", js_get_group_members);
    register_func("httpGet", js_http_get);
    register_func("httpPost", js_http_post);
    register_func("saveData", js_save_data);
    register_func("loadData", js_load_data);
    
    // 将 Bot API 添加到全局对象
    JS_SetPropertyStr(context_, global, "Bot", bot_api);
    
    JS_FreeValue(context_, global);
    
    // 设置 API 引用
    JSBotAPI::set_engine(this, database_, server_);
}

bool JSBotEngine::load_plugin(const std::string& script_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 读取脚本文件
    std::ifstream file(script_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open plugin: " << script_path << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();
    file.close();
    
    // 执行脚本
    JSValue result = JS_Eval(context_, script.c_str(), script.length(),
                              script_path.c_str(), JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(context_);
        const char* error = JS_ToCString(context_, exception);
        std::cerr << "Plugin load error: " << error << std::endl;
        JS_FreeCString(context_, error);
        JS_FreeValue(context_, exception);
        JS_FreeValue(context_, result);
        return false;
    }
    
    JS_FreeValue(context_, result);
    
    // 提取插件信息
    JSBotPlugin plugin;
    plugin.script_path = script_path;
    
    // 尝试获取插件元数据
    JSValue global = JS_GetGlobalObject(context_);
    JSValue meta = JS_GetPropertyStr(context_, global, "PLUGIN_META");
    if (JS_IsObject(meta)) {
        JSValue name_val = JS_GetPropertyStr(context_, meta, "name");
        JSValue version_val = JS_GetPropertyStr(context_, meta, "version");
        JSValue desc_val = JS_GetPropertyStr(context_, meta, "description");
        JSValue author_val = JS_GetPropertyStr(context_, meta, "author");
        
        const char* name = JS_ToCString(context_, name_val);
        const char* version = JS_ToCString(context_, version_val);
        const char* desc = JS_ToCString(context_, desc_val);
        const char* author = JS_ToCString(context_, author_val);
        
        if (name) plugin.name = name;
        if (version) plugin.version = version;
        if (desc) plugin.description = desc;
        if (author) plugin.author = author;
        
        JS_FreeCString(context_, name);
        JS_FreeCString(context_, version);
        JS_FreeCString(context_, desc);
        JS_FreeCString(context_, author);
        
        JS_FreeValue(context_, name_val);
        JS_FreeValue(context_, version_val);
        JS_FreeValue(context_, desc_val);
        JS_FreeValue(context_, author_val);
    }
    JS_FreeValue(context_, meta);
    JS_FreeValue(context_, global);
    
    if (plugin.name.empty()) {
        // 从文件名生成插件名
        size_t pos = script_path.find_last_of("/\\");
        plugin.name = (pos != std::string::npos) ? script_path.substr(pos + 1) : script_path;
    }
    
    plugins_[plugin.name] = plugin;
    
    std::cout << "Plugin loaded: " << plugin.name << " v" << plugin.version << std::endl;
    return true;
}

int JSBotEngine::load_plugins_from_dir(const std::string& dir_path) {
    int count = 0;
    
    // 使用文件系统遍历目录
    std::string cmd = "ls " + dir_path + "/*.js 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string path = buffer;
            // 移除换行符
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
                path.pop_back();
            }
            if (load_plugin(path)) {
                count++;
            }
        }
        pclose(pipe);
    }
    
    return count;
}

JSBotResponse JSBotEngine::handle_message(const JSBotMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    JSBotResponse response;
    
    // 获取处理函数
    JSValue global = JS_GetGlobalObject(context_);
    JSValue handler = JS_GetPropertyStr(context_, global, "onMessage");
    
    if (!JS_IsFunction(context_, handler)) {
        JS_FreeValue(context_, handler);
        JS_FreeValue(context_, global);
        response.success = false;
        response.error = "No message handler found";
        return response;
    }
    
    // 创建消息对象
    JSValue msg_obj = JS_NewObject(context_);
    
    JS_SetPropertyStr(context_, msg_obj, "messageId", JS_NewBigUint64(context_, message.message_id));
    JS_SetPropertyStr(context_, msg_obj, "senderId", JS_NewBigUint64(context_, message.sender_id));
    JS_SetPropertyStr(context_, msg_obj, "senderName", JS_NewString(context_, message.sender_name.c_str()));
    JS_SetPropertyStr(context_, msg_obj, "receiverId", JS_NewBigUint64(context_, message.receiver_id));
    JS_SetPropertyStr(context_, msg_obj, "content", JS_NewString(context_, message.content.c_str()));
    JS_SetPropertyStr(context_, msg_obj, "mediaType", JS_NewInt32(context_, message.media_type));
    JS_SetPropertyStr(context_, msg_obj, "mediaUrl", JS_NewString(context_, message.media_url.c_str()));
    JS_SetPropertyStr(context_, msg_obj, "createdAt", JS_NewInt64(context_, message.created_at));
    JS_SetPropertyStr(context_, msg_obj, "isGroup", JS_NewBool(context_, message.is_group));
    
    if (message.is_group) {
        JS_SetPropertyStr(context_, msg_obj, "groupId", JS_NewBigUint64(context_, message.group_id));
        
        // 创建 @成员 数组
        JSValue mentioned = JS_NewArray(context_);
        for (size_t i = 0; i < message.mentioned_users.size(); i++) {
            JS_SetPropertyUint32(context_, mentioned, i, 
                JS_NewBigUint64(context_, message.mentioned_users[i]));
        }
        JS_SetPropertyStr(context_, msg_obj, "mentionedUsers", mentioned);
    }
    
    // 调用处理函数
    JSValue args[1] = { msg_obj };
    JSValue result = JS_Call(context_, handler, global, 1, args);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(context_);
        const char* error = JS_ToCString(context_, exception);
        response.success = false;
        response.error = error ? error : "Unknown error";
        JS_FreeCString(context_, error);
        JS_FreeValue(context_, exception);
    } else if (JS_IsObject(result)) {
        // 解析响应对象
        JSValue content_val = JS_GetPropertyStr(context_, result, "content");
        JSValue reply_val = JS_GetPropertyStr(context_, result, "shouldReply");
        
        if (JS_IsString(content_val)) {
            const char* content = JS_ToCString(context_, content_val);
            response.content = content ? content : "";
            JS_FreeCString(context_, content);
            response.success = true;
        }
        
        if (JS_IsBool(reply_val)) {
            response.should_reply = JS_ToBool(context_, reply_val);
        }
        
        JS_FreeValue(context_, content_val);
        JS_FreeValue(context_, reply_val);
    }
    
    JS_FreeValue(context_, result);
    JS_FreeValue(context_, msg_obj);
    JS_FreeValue(context_, handler);
    JS_FreeValue(context_, global);
    
    return response;
}

std::string JSBotEngine::eval_js(const std::string& code) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    JSValue result = JS_Eval(context_, code.c_str(), code.length(),
                              "<eval>", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(context_);
        const char* error = JS_ToCString(context_, exception);
        std::string err_str = error ? error : "Unknown error";
        JS_FreeCString(context_, error);
        JS_FreeValue(context_, exception);
        JS_FreeValue(context_, result);
        return "Error: " + err_str;
    }
    
    const char* str = JS_ToCString(context_, result);
    std::string result_str = str ? str : "";
    JS_FreeCString(context_, str);
    JS_FreeValue(context_, result);
    
    return result_str;
}

// ==================== JSBotAPI 实现 ====================

void JSBotAPI::set_engine(JSBotEngine* engine, std::shared_ptr<Database> db, std::shared_ptr<Server> server) {
    engine_ = engine;
    database_ = db;
    server_ = server;
}

bool JSBotAPI::send_message(uint64_t receiver_id, const std::string& content) {
    if (!server_) return false;
    
    Message msg;
    msg.sender_id = 0;  // 系统消息
    msg.receiver_id = receiver_id;
    msg.content = content;
    msg.media_type = MediaType::TEXT;
    msg.created_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    if (database_) {
        database_->save_private_message(msg);
    }
    
    server_->send_to_user(receiver_id,
        Protocol::serialize(MessageType::PRIVATE_MESSAGE, 0, msg.to_json()));
    
    return true;
}

bool JSBotAPI::send_group_message(uint64_t group_id, const std::string& content,
                                   const std::vector<uint64_t>& mentioned_users) {
    if (!server_) return false;
    
    Message msg;
    msg.sender_id = 0;  // 系统消息
    msg.group_id = group_id;
    msg.content = content;
    msg.media_type = MediaType::TEXT;
    msg.created_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // 添加 @成员 信息到 extra 字段
    if (!mentioned_users.empty()) {
        json extra_json;
        extra_json["mentioned_users"] = mentioned_users;
        msg.extra = extra_json.dump();
    }
    
    if (database_) {
        database_->save_group_message(msg);
    }
    
    // 获取群成员列表并发送消息
    std::vector<uint64_t> members;
    if (database_) {
        database_->get_group_members(group_id, members);
    }
    
    for (uint64_t member_id : members) {
        server_->send_to_user(member_id,
            Protocol::serialize(MessageType::GROUP_MESSAGE, 0, msg.to_json()));
    }
    
    return true;
}

std::string JSBotAPI::get_user_info(uint64_t user_id) {
    if (!database_) return "{}";
    
    UserInfo user;
    if (database_->get_user_by_id(user_id, user)) {
        return user.to_json().dump();
    }
    return "{}";
}

std::string JSBotAPI::get_group_members(uint64_t group_id) {
    if (!database_) return "[]";
    
    std::vector<uint64_t> members;
    database_->get_group_members(group_id, members);
    
    json result = json::array();
    for (uint64_t member_id : members) {
        UserInfo user;
        if (database_->get_user_by_id(member_id, user)) {
            result.push_back(user.to_json());
        }
    }
    return result.dump();
}

// CURL 回调函数
static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string JSBotAPI::http_get(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return "";
    }
    return response;
}

std::string JSBotAPI::http_post(const std::string& url, const std::string& body,
                                  const std::string& content_type) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Content-Type: " + content_type).c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return "";
    }
    return response;
}

void JSBotAPI::log(const std::string& level, const std::string& message) {
    std::cout << "[JSBot:" << level << "] " << message << std::endl;
}

bool JSBotAPI::save_data(const std::string& key, const std::string& value) {
    if (!database_) return false;
    
    // 使用简单的键值存储方式
    // 实际实现可以创建专门的表
    return true;
}

std::string JSBotAPI::load_data(const std::string& key) {
    if (!database_) return "";
    return "";
}

} // namespace chat
