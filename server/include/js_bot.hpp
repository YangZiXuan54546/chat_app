#ifndef JS_BOT_HPP
#define JS_BOT_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>

// QuickJS 前向声明
struct JSContext;
struct JSRuntime;

namespace chat {

// 前向声明
class Database;
class Server;

/**
 * JavaScript 机器人消息
 */
struct JSBotMessage {
    uint64_t message_id;
    uint64_t sender_id;
    std::string sender_name;
    uint64_t receiver_id;
    std::string content;
    int media_type;
    std::string media_url;
    int64_t created_at;
    
    // @成员 相关
    bool is_group;
    uint64_t group_id;
    std::vector<uint64_t> mentioned_users;  // 被@的用户ID列表
};

/**
 * JavaScript 机器人响应
 */
struct JSBotResponse {
    bool success = false;
    std::string content;
    std::string error;
    bool should_reply = true;  // 是否回复消息
};

/**
 * JavaScript 机器人插件信息
 */
struct JSBotPlugin {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string script_path;
    bool enabled = true;
};

/**
 * QuickJS 机器人管理器
 * 允许使用 JavaScript 编写机器人插件
 */
class JSBotEngine {
public:
    using MessageHandler = std::function<JSBotResponse(const JSBotMessage&)>;
    
    JSBotEngine();
    ~JSBotEngine();
    
    /**
     * 初始化引擎
     */
    bool init();
    
    /**
     * 加载 JavaScript 插件
     * @param script_path 脚本路径
     * @return 是否成功
     */
    bool load_plugin(const std::string& script_path);
    
    /**
     * 加载插件目录下的所有插件
     */
    int load_plugins_from_dir(const std::string& dir_path);
    
    /**
     * 重新加载插件
     */
    bool reload_plugin(const std::string& plugin_name);
    
    /**
     * 卸载插件
     */
    bool unload_plugin(const std::string& plugin_name);
    
    /**
     * 获取已加载的插件列表
     */
    std::vector<JSBotPlugin> get_loaded_plugins() const;
    
    /**
     * 处理消息
     * @param message 收到的消息
     * @return 响应内容
     */
    JSBotResponse handle_message(const JSBotMessage& message);
    
    /**
     * 设置数据库引用
     */
    void set_database(std::shared_ptr<Database> db) { database_ = db; }
    
    /**
     * 设置服务器引用
     */
    void set_server(std::shared_ptr<Server> server) { server_ = server; }
    
    /**
     * 执行 JavaScript 代码
     */
    std::string eval_js(const std::string& code);
    
    /**
     * 调用 JavaScript 函数
     */
    bool call_js_function(const std::string& func_name, const std::string& json_args);

private:
    /**
     * 初始化 JavaScript 运行时
     */
    bool init_runtime();
    
    /**
     * 注册 C++ 函数到 JavaScript
     */
    void register_native_functions();
    
    /**
     * 创建消息对象
     */
    void* create_js_message_object(const JSBotMessage& message);
    
    /**
     * 解析响应对象
     */
    JSBotResponse parse_js_response(void* obj);

private:
    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
    std::shared_ptr<Database> database_;
    std::shared_ptr<Server> server_;
    
    std::unordered_map<std::string, JSBotPlugin> plugins_;
    mutable std::mutex mutex_;
    
    bool initialized_ = false;
};

/**
 * JavaScript 机器人 API 工具类
 * 提供给 JS 调用的原生函数
 */
class JSBotAPI {
public:
    /**
     * 发送消息
     */
    static bool send_message(uint64_t receiver_id, const std::string& content);
    
    /**
     * 发送群消息
     */
    static bool send_group_message(uint64_t group_id, const std::string& content, 
                                    const std::vector<uint64_t>& mentioned_users = {});
    
    /**
     * 获取用户信息
     */
    static std::string get_user_info(uint64_t user_id);
    
    /**
     * 获取群成员列表
     */
    static std::string get_group_members(uint64_t group_id);
    
    /**
     * HTTP GET 请求
     */
    static std::string http_get(const std::string& url);
    
    /**
     * HTTP POST 请求
     */
    static std::string http_post(const std::string& url, const std::string& body, 
                                  const std::string& content_type = "application/json");
    
    /**
     * 记录日志
     */
    static void log(const std::string& level, const std::string& message);
    
    /**
     * 保存数据
     */
    static bool save_data(const std::string& key, const std::string& value);
    
    /**
     * 加载数据
     */
    static std::string load_data(const std::string& key);
    
    /**
     * 设置引擎引用
     */
    static void set_engine(JSBotEngine* engine, std::shared_ptr<Database> db, std::shared_ptr<Server> server);
    
private:
    static JSBotEngine* engine_;
    static std::shared_ptr<Database> database_;
    static std::shared_ptr<Server> server_;
};

} // namespace chat

#endif // JS_BOT_HPP
