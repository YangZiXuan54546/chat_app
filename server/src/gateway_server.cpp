#include "gateway_server.hpp"
#include "database.hpp"
#include "user_manager.hpp"
#include "message_manager.hpp"
#include "group_manager.hpp"
#include "friend_manager.hpp"
#include "bot_manager.hpp"
#include "fcm_manager.hpp"
#include "jpush_manager.hpp"
#include "protocol.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <chrono>
#include <regex>
#include <mutex>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace chat {

// ==================== HttpResponse ====================

HttpResponse HttpResponse::json(int status, const std::string& body) {
    HttpResponse resp;
    resp.status_code = status;
    resp.content_type = "application/json";
    resp.body = body;
    resp.headers["Access-Control-Allow-Origin"] = "*";
    resp.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
    resp.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    return resp;
}

HttpResponse HttpResponse::error(int status, const std::string& message) {
    std::ostringstream ss;
    ss << "{\"code\":" << status << ",\"message\":\"" << message << "\"}";
    return json(status, ss.str());
}

HttpResponse HttpResponse::file_data(int status, const std::string& data, const std::string& content_type) {
    HttpResponse resp;
    resp.status_code = status;
    resp.content_type = content_type;
    resp.body = data;
    resp.headers["Access-Control-Allow-Origin"] = "*";
    resp.headers["Cache-Control"] = "max-age=86400";
    return resp;
}

// ==================== WebSocketConnection ====================

WebSocketConnection::WebSocketConnection(struct MHD_Connection* connection, const std::string& key)
    : connection_(connection), websocket_key_(key) {
}

WebSocketConnection::~WebSocketConnection() {
}

bool WebSocketConnection::send(const std::string& message) {
    // WebSocket 文本帧
    // TODO: 实现实际的 WebSocket 发送
    return true;
}

bool WebSocketConnection::send_binary(const std::vector<uint8_t>& data) {
    // WebSocket 二进制帧
    // TODO: 实现实际的 WebSocket 发送
    return true;
}

void WebSocketConnection::close() {
    authenticated_ = false;
}

// ==================== GatewayServer ====================

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config) {
}

GatewayServer::~GatewayServer() {
    stop();
}

bool GatewayServer::start() {
    if (running_) return false;
    
    // 创建媒体目录
    mkdir(config_.media_dir.c_str(), 0755);
    
    // 设置路由
    setup_routes();
    
    // 启动 MHD 守护进程
    daemon_ = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG,
        config_.port,
        nullptr, nullptr,
        &GatewayServer::request_handler, this,
        MHD_OPTION_END);
    
    if (!daemon_) {
        std::cerr << "[Gateway] Failed to start on port " << config_.port << std::endl;
        return false;
    }
    
    running_ = true;
    std::cout << "[Gateway] Started on port " << config_.port << std::endl;
    std::cout << "  - WebSocket: ws://localhost:" << config_.port << "/ws" << std::endl;
    std::cout << "  - File upload: POST http://localhost:" << config_.port << "/api/upload" << std::endl;
    std::cout << "  - File download: GET http://localhost:" << config_.port << "/media/{file_id}/{filename}" << std::endl;
    std::cout << "  - Health check: GET http://localhost:" << config_.port << "/health" << std::endl;
    
    return true;
}

void GatewayServer::stop() {
    if (!running_) return;
    
    running_ = false;
    
    if (daemon_) {
        MHD_stop_daemon(daemon_);
        daemon_ = nullptr;
    }
    
    // 关闭所有 WebSocket 连接
    std::lock_guard<std::mutex> lock(ws_mutex_);
    for (auto& [id, conn] : ws_connections_) {
        conn->close();
    }
    ws_connections_.clear();
    
    std::cout << "[Gateway] Stopped" << std::endl;
}

void GatewayServer::run() {
    // MHD 使用自己的线程，主线程等待
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void GatewayServer::setup_routes() {
    // CORS 预检
    register_route("OPTIONS", "*", [](const HttpRequest& req) {
        return HttpResponse::json(200, "{}");
    });
    
    // 健康检查
    register_route("GET", "/health", [this](const HttpRequest& req) {
        return handle_health_check(req);
    });
    
    // 文件上传
    register_route("POST", "/api/upload", [this](const HttpRequest& req) {
        return handle_api_upload(req);
    });
    
    // 文件下载
    register_route("GET", "/media", [this](const HttpRequest& req) {
        return handle_media_download(req);
    });
    
    // WebSocket 路由
    register_ws_route("/ws", [this](WebSocketConnection::ptr conn, const std::string& msg) {
        handle_ws_message(conn, msg);
    });
}

void GatewayServer::register_route(const std::string& method, const std::string& path, HttpHandler handler) {
    routes_[method + ":" + path] = handler;
}

void GatewayServer::register_ws_route(const std::string& path, WsHandler handler) {
    ws_routes_[path] = handler;
}

MHD_Result GatewayServer::request_handler(
    void* cls,
    struct MHD_Connection* connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** con_cls) {
    
    auto* server = static_cast<GatewayServer*>(cls);
    
    // 首次调用，初始化连接状态
    if (*con_cls == nullptr) {
        auto* request = new HttpRequest();
        request->method = method;
        request->path = url;
        request->connection = connection;
        *con_cls = request;
        return MHD_YES;
    }
    
    auto* request = static_cast<HttpRequest*>(*con_cls);
    
    // 处理 POST 数据
    if (strcmp(method, "POST") == 0 && *upload_data_size > 0) {
        request->body.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    // 检查 WebSocket 升级请求
    const char* upgrade = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Upgrade");
    const char* ws_key = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Sec-WebSocket-Key");
    
    if (upgrade && strcmp(upgrade, "websocket") == 0 && ws_key) {
        // WebSocket 升级
        std::cout << "[Gateway] WebSocket upgrade request: " << url << std::endl;
        return server->handle_websocket_upgrade(connection, *request);
    }
    
    // 收集请求头
    auto header_iterator = [](void* cls, enum MHD_ValueKind, const char* key, const char* value) -> MHD_Result {
        auto* req = static_cast<HttpRequest*>(cls);
        req->headers[key] = value;
        if (strcmp(key, "Content-Type") == 0) req->content_type = value;
        if (strcmp(key, "Authorization") == 0) req->authorization = value;
        return MHD_YES;
    };
    MHD_get_connection_values(connection, MHD_HEADER_KIND, header_iterator, request);
    
    // 收集查询参数
    auto param_iterator = [](void* cls, enum MHD_ValueKind, const char* key, const char* value) -> MHD_Result {
        auto* req = static_cast<HttpRequest*>(cls);
        if (key && value) req->params[key] = value;
        return MHD_YES;
    };
    MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, param_iterator, request);
    
    // 处理请求
    HttpResponse response = server->handle_request(*request);
    
    // 构建响应
    struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
        response.body.size(),
        const_cast<char*>(response.body.c_str()),
        MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(mhd_response, "Content-Type", response.content_type.c_str());
    for (const auto& [key, value] : response.headers) {
        MHD_add_response_header(mhd_response, key.c_str(), value.c_str());
    }
    
    MHD_Result ret = MHD_queue_response(connection, response.status_code, mhd_response);
    MHD_destroy_response(mhd_response);
    
    delete request;
    *con_cls = nullptr;
    
    return ret;
}

MHD_Result GatewayServer::handle_websocket_upgrade(
    struct MHD_Connection* connection,
    const HttpRequest& request) {
    
    const char* ws_key = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Sec-WebSocket-Key");
    if (!ws_key) {
        return MHD_NO;
    }
    
    // 计算 Sec-WebSocket-Accept
    std::string key_with_guid = std::string(ws_key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(key_with_guid.c_str()), key_with_guid.size(), hash);
    
    // Base64 编码
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    
    BUF_MEM* buffer;
    BIO_get_mem_ptr(bio, &buffer);
    std::string accept_key(buffer->data, buffer->length);
    BIO_free_all(bio);
    
    // 发送握手响应
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n"
             << "\r\n";
    
    std::string response_str = response.str();
    
    // 注意: MHD 不直接支持 WebSocket，这里需要特殊处理
    // 实际应用中应该使用专门的 WebSocket 库或者在 MHD 外部处理
    
    struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
        response_str.size(),
        const_cast<char*>(response_str.c_str()),
        MHD_RESPMEM_MUST_COPY);
    
    MHD_add_response_header(mhd_response, "Upgrade", "websocket");
    MHD_add_response_header(mhd_response, "Connection", "Upgrade");
    MHD_add_response_header(mhd_response, "Sec-WebSocket-Accept", accept_key.c_str());
    
    MHD_Result ret = MHD_queue_response(connection, 101, mhd_response);
    MHD_destroy_response(mhd_response);
    
    std::cout << "[Gateway] WebSocket handshake completed" << std::endl;
    
    return ret;
}

HttpResponse GatewayServer::handle_request(const HttpRequest& request) {
    // 查找匹配的路由
    HttpHandler handler = find_handler(request.method, request.path);
    
    if (handler) {
        return handler(request);
    }
    
    return HttpResponse::error(404, "Not Found");
}

// 路由匹配 - 返回 HttpHandler 或 nullptr
HttpHandler GatewayServer::find_handler(const std::string& method, const std::string& path) {
    // 精确匹配
    std::string key = method + ":" + path;
    auto it = routes_.find(key);
    if (it != routes_.end()) {
        return it->second;
    }
    
    // 前缀匹配
    for (const auto& [k, v] : routes_) {
        size_t colon = k.find(':');
        if (colon == std::string::npos) continue;
        
        std::string r_method = k.substr(0, colon);
        std::string r_path = k.substr(colon + 1);
        
        if (r_method != method) continue;
        if (r_path == "*") return v;
        if (r_path.back() == '*' && path.find(r_path.substr(0, r_path.size() - 1)) == 0) {
            return v;
        }
    }
    
    return nullptr;
}

HttpResponse GatewayServer::handle_health_check(const HttpRequest& request) {
    std::ostringstream ss;
    ss << "{"
       << "\"status\":\"ok\","
       << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::system_clock::now().time_since_epoch()
       ).count()
       << "}";
    return HttpResponse::json(200, ss.str());
}

HttpResponse GatewayServer::handle_api_upload(const HttpRequest& request) {
    // 解析 Authorization
    uint64_t user_id = 0;
    if (!request.authorization.empty()) {
        // 格式: Bearer {user_id}:{token}
        if (request.authorization.find("Bearer ") == 0) {
            std::string token = request.authorization.substr(7);
            size_t colon = token.find(':');
            if (colon != std::string::npos) {
                user_id = std::stoull(token.substr(0, colon));
            }
        }
    }
    
    if (user_id == 0) {
        return HttpResponse::error(401, "Unauthorized");
    }
    
    // 解析 multipart/form-data
    std::string boundary;
    if (request.content_type.find("multipart/form-data") != std::string::npos) {
        size_t pos = request.content_type.find("boundary=");
        if (pos != std::string::npos) {
            boundary = request.content_type.substr(pos + 9);
            // 移除可能的引号
            if (!boundary.empty() && boundary[0] == '"') {
                boundary = boundary.substr(1, boundary.size() - 2);
            }
        }
    }
    
    if (boundary.empty()) {
        return HttpResponse::error(400, "Invalid content type, expected multipart/form-data");
    }
    
    // 解析 multipart 数据
    std::string file_data;
    std::string file_name;
    int media_type = 1; // 默认图片
    
    std::string delimiter = "--" + boundary;
    size_t pos = 0;
    size_t end = request.body.find(delimiter + "--");
    
    while (pos < end) {
        size_t part_start = request.body.find(delimiter, pos);
        if (part_start == std::string::npos) break;
        
        part_start += delimiter.size();
        if (request.body[part_start] == '\r') part_start += 2;
        
        size_t part_end = request.body.find(delimiter, part_start);
        if (part_end == std::string::npos) part_end = end;
        
        std::string part = request.body.substr(part_start, part_end - part_start);
        
        // 解析 part headers
        size_t header_end = part.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string headers = part.substr(0, header_end);
            std::string content = part.substr(header_end + 4);
            
            // 移除尾部的 \r\n
            if (!content.empty() && content.back() == '\n') {
                content = content.substr(0, content.size() - 2);
            }
            
            // 检查 Content-Disposition
            if (headers.find("name=\"file\"") != std::string::npos) {
                file_data = content;
                // 提取文件名
                size_t fn_pos = headers.find("filename=\"");
                if (fn_pos != std::string::npos) {
                    fn_pos += 10;
                    size_t fn_end = headers.find("\"", fn_pos);
                    if (fn_end != std::string::npos) {
                        file_name = headers.substr(fn_pos, fn_end - fn_pos);
                    }
                }
            } else if (headers.find("name=\"media_type\"") != std::string::npos) {
                media_type = std::stoi(content);
            }
        }
        
        pos = part_end;
    }
    
    if (file_data.empty() || file_name.empty()) {
        return HttpResponse::error(400, "Missing file or filename");
    }
    
    // 创建日期目录
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_info = std::localtime(&now_time);
    char date_path[32];
    std::strftime(date_path, sizeof(date_path), "%Y/%m/%d", tm_info);
    
    std::string full_dir = config_.media_dir + "/" + date_path;
    std::string cmd = "mkdir -p " + full_dir;
    system(cmd.c_str());
    
    // 生成唯一文件名
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::string extension;
    size_t dot_pos = file_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = file_name.substr(dot_pos);
    }
    
    std::ostringstream unique_name;
    unique_name << user_id << "_" << timestamp << extension;
    
    std::string file_path = full_dir + "/" + unique_name.str();
    
    // 写入文件
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file) {
        return HttpResponse::error(500, "Failed to save file");
    }
    out_file.write(file_data.data(), file_data.size());
    out_file.close();
    
    // 保存到数据库
    uint64_t file_id = 0;
    std::string url;
    
    if (database_) {
        // 简化：使用时间戳作为 file_id
        file_id = static_cast<uint64_t>(timestamp);
        
        std::ostringstream url_stream;
        url_stream << "/media/" << file_id << "/" << unique_name.str();
        url = url_stream.str();
        
        // TODO: 实际保存到 media_files 表
    }
    
    std::cout << "[Gateway] File uploaded: " << unique_name.str() 
              << " (" << file_data.size() << " bytes) for user " << user_id << std::endl;
    
    // 返回响应
    std::ostringstream response_json;
    response_json << "{"
                  << "\"code\":0,"
                  << "\"data\":{"
                  << "\"file_id\":" << file_id << ","
                  << "\"url\":\"" << url << "\","
                  << "\"file_name\":\"" << file_name << "\","
                  << "\"file_size\":" << file_data.size() << ","
                  << "\"media_type\":" << media_type
                  << "}}";
    
    return HttpResponse::json(200, response_json.str());
}

HttpResponse GatewayServer::handle_media_download(const HttpRequest& request) {
    // 解析路径: /media/{file_id}/{filename} 或 /media/{filename}
    std::string path = request.path;
    
    // 移除 /media/ 前缀
    if (path.find("/media/") == 0) {
        path = path.substr(7);
    } else if (path[0] == '/') {
        path = path.substr(1);
    }
    
    // 提取文件名
    std::string filename;
    size_t slash_pos = path.find('/');
    if (slash_pos != std::string::npos) {
        filename = path.substr(slash_pos + 1);
    } else {
        filename = path;
    }
    
    if (filename.empty()) {
        return HttpResponse::error(400, "Invalid file path");
    }
    
    // 递归查找文件
    std::string found_path;
    std::function<bool(const std::string&)> find_file = [&](const std::string& dir) -> bool {
        DIR* dp = opendir(dir.c_str());
        if (!dp) return false;
        
        struct dirent* entry;
        while ((entry = readdir(dp)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            std::string full_path = dir + "/" + entry->d_name;
            
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    if (find_file(full_path)) {
                        closedir(dp);
                        return true;
                    }
                } else if (strcmp(entry->d_name, filename.c_str()) == 0) {
                    found_path = full_path;
                    closedir(dp);
                    return true;
                }
            }
        }
        closedir(dp);
        return false;
    };
    
    if (!find_file(config_.media_dir)) {
        return HttpResponse::error(404, "File not found");
    }
    
    // 读取文件
    std::ifstream file(found_path, std::ios::binary);
    if (!file) {
        return HttpResponse::error(500, "Failed to read file");
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    file.close();
    
    // 确定 Content-Type
    std::string content_type = "application/octet-stream";
    if (filename.find(".jpg") != std::string::npos || filename.find(".jpeg") != std::string::npos) {
        content_type = "image/jpeg";
    } else if (filename.find(".png") != std::string::npos) {
        content_type = "image/png";
    } else if (filename.find(".gif") != std::string::npos) {
        content_type = "image/gif";
    } else if (filename.find(".webp") != std::string::npos) {
        content_type = "image/webp";
    } else if (filename.find(".pdf") != std::string::npos) {
        content_type = "application/pdf";
    }
    
    std::cout << "[Gateway] File downloaded: " << filename << std::endl;
    
    return HttpResponse::file_data(200, content.str(), content_type);
}

void GatewayServer::handle_ws_message(WebSocketConnection::ptr conn, const std::string& message) {
    // TODO: 实现 WebSocket 消息处理
    // 解析 JSON 消息，调用相应的管理器处理
    std::cout << "[Gateway] WebSocket message: " << message << std::endl;
}

void GatewayServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    for (auto& [id, conn] : ws_connections_) {
        conn->send(message);
    }
}

void GatewayServer::broadcast_to_user(uint64_t user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    if (ws_connections_.count(user_id)) {
        ws_connections_[user_id]->send(message);
    }
}

std::vector<WebSocketConnection::ptr> GatewayServer::get_connections() {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    std::vector<WebSocketConnection::ptr> result;
    for (auto& [id, conn] : ws_connections_) {
        result.push_back(conn);
    }
    return result;
}

WebSocketConnection::ptr GatewayServer::get_connection(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    if (ws_connections_.count(user_id)) {
        return ws_connections_[user_id];
    }
    return nullptr;
}

} // namespace chat
