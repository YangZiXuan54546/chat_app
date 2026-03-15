#include "websocket_server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace chat {

// ==================== WsConnection ====================

WsConnection::WsConnection(int socket, const std::string& client_ip)
    : socket_(socket), client_ip_(client_ip) {
    last_activity_ = std::chrono::steady_clock::now();
    recv_buffer_.reserve(65536); // 64KB 初始缓冲区
}

WsConnection::~WsConnection() {
    close();
}

bool WsConnection::send_text(const std::string& message) {
    return send_frame(static_cast<uint8_t>(WsOpcode::TEXT),
                      reinterpret_cast<const uint8_t*>(message.data()),
                      message.size());
}

bool WsConnection::send_binary(const std::vector<uint8_t>& data) {
    return send_frame(static_cast<uint8_t>(WsOpcode::BINARY),
                      data.data(), data.size());
}

bool WsConnection::send_ping(const std::string& payload) {
    return send_frame(static_cast<uint8_t>(WsOpcode::PING),
                      reinterpret_cast<const uint8_t*>(payload.data()),
                      payload.size());
}

bool WsConnection::send_pong(const std::string& payload) {
    return send_frame(static_cast<uint8_t>(WsOpcode::PONG),
                      reinterpret_cast<const uint8_t*>(payload.data()),
                      payload.size());
}

bool WsConnection::send_close(uint16_t code, const std::string& reason) {
    std::vector<uint8_t> payload(2 + reason.size());
    payload[0] = static_cast<uint8_t>(code >> 8);
    payload[1] = static_cast<uint8_t>(code & 0xFF);
    std::memcpy(payload.data() + 2, reason.data(), reason.size());
    
    bool result = send_frame(static_cast<uint8_t>(WsOpcode::CLOSE),
                             payload.data(), payload.size());
    
    // 发送 Close 帧后关闭连接
    close();
    return result;
}

void WsConnection::close() {
    bool expected = false;
    if (closed_.compare_exchange_strong(expected, true)) {
        if (socket_ >= 0) {
            ::shutdown(socket_, SHUT_RDWR);
            ::close(socket_);
            socket_ = -1;
        }
    }
}

bool WsConnection::handle_receive(const uint8_t* data, size_t len) {
    if (closed_) return false;
    
    // 追加到接收缓冲区
    recv_buffer_.insert(recv_buffer_.end(), data, data + len);
    
    update_activity();
    
    // 尝试解析帧
    while (!recv_buffer_.empty()) {
        size_t consumed = 0;
        if (!parse_frame(recv_buffer_.data(), recv_buffer_.size(), consumed)) {
            return false; // 解析错误
        }
        
        if (consumed == 0) {
            break; // 需要更多数据
        }
        
        // 移除已处理的数据
        recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + consumed);
    }
    
    return true;
}

bool WsConnection::parse_frame(const uint8_t* data, size_t len, size_t& consumed) {
    consumed = 0;
    
    // 最小帧头: 2 字节
    if (len < 2) {
        return true; // 需要更多数据
    }
    
    WsFrame frame;
    frame.fin = (data[0] & 0x80) != 0;
    frame.opcode = data[0] & 0x0F;
    frame.mask = (data[1] & 0x80) != 0;
    
    // 解析长度
    uint8_t len_byte = data[1] & 0x7F;
    size_t header_len = 2;
    
    if (len_byte == 126) {
        if (len < 4) return true;
        frame.payload_len = (static_cast<uint64_t>(data[2]) << 8) | data[3];
        header_len = 4;
    } else if (len_byte == 127) {
        if (len < 10) return true;
        frame.payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            frame.payload_len = (frame.payload_len << 8) | data[2 + i];
        }
        header_len = 10;
    } else {
        frame.payload_len = len_byte;
    }
    
    // 检查消息大小限制
    if (frame.payload_len > 10 * 1024 * 1024) { // 10MB
        std::cerr << "[WsConn] Message too large: " << frame.payload_len << std::endl;
        return false;
    }
    
    // 解析掩码
    if (frame.mask) {
        if (len < header_len + 4) return true;
        frame.masking_key.assign(data + header_len, data + header_len + 4);
        header_len += 4;
    }
    
    // 检查是否有完整的负载
    if (len < header_len + frame.payload_len) {
        return true; // 需要更多数据
    }
    
    // 提取负载
    const uint8_t* payload_start = data + header_len;
    if (frame.mask) {
        // 解掩码
        frame.payload.resize(frame.payload_len);
        for (uint64_t i = 0; i < frame.payload_len; ++i) {
            frame.payload[i] = payload_start[i] ^ frame.masking_key[i % 4];
        }
    } else {
        frame.payload.assign(payload_start, payload_start + frame.payload_len);
    }
    
    consumed = header_len + frame.payload_len;
    
    return handle_frame(frame);
}

bool WsConnection::handle_frame(const WsFrame& frame) {
    uint8_t opcode = frame.opcode;
    
    // 处理控制帧
    if (opcode >= 0x8) {
        switch (opcode) {
            case static_cast<uint8_t>(WsOpcode::PING):
                // 响应 Pong
                send_pong(std::string(frame.payload.begin(), frame.payload.end()));
                return true;
                
            case static_cast<uint8_t>(WsOpcode::PONG):
                // 忽略 Pong
                return true;
                
            case static_cast<uint8_t>(WsOpcode::CLOSE):
                // 对方请求关闭
                if (!closed_) {
                    send_close(1000, "Normal closure");
                }
                return false;
                
            default:
                std::cerr << "[WsConn] Unknown control opcode: " << (int)opcode << std::endl;
                return false;
        }
    }
    
    // 处理数据帧
    if (opcode == static_cast<uint8_t>(WsOpcode::CONTINUATION)) {
        if (!in_fragmented_) {
            std::cerr << "[WsConn] Unexpected continuation frame" << std::endl;
            return false;
        }
        
        fragment_data_.insert(fragment_data_.end(),
                              frame.payload.begin(), frame.payload.end());
        
        if (frame.fin) {
            in_fragmented_ = false;
            
            // 完整消息
            std::string message(fragment_data_.begin(), fragment_data_.end());
            fragment_data_.clear();
            
            if (message_handler_) {
                message_handler_(shared_from_this(), message);
            }
        }
    } else {
        // 新消息
        if (in_fragmented_) {
            std::cerr << "[WsConn] Expected continuation, got opcode " << (int)opcode << std::endl;
            return false;
        }
        
        if (!frame.fin) {
            // 分片消息开始
            in_fragmented_ = true;
            fragment_opcode_ = opcode;
            fragment_data_ = frame.payload;
        } else {
            // 完整的单帧消息
            std::string message(frame.payload.begin(), frame.payload.end());
            if (message_handler_) {
                message_handler_(shared_from_this(), message);
            }
        }
    }
    
    return true;
}

bool WsConnection::send_frame(uint8_t opcode, const uint8_t* data, size_t len, bool fin) {
    if (closed_) return false;
    
    std::vector<uint8_t> frame;
    frame.reserve(len + 10);
    
    // 第一个字节: FIN + RSV + Opcode
    uint8_t first = opcode & 0x0F;
    if (fin) first |= 0x80;
    frame.push_back(first);
    
    // 第二个字节: MASK + Length (服务器不掩码)
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }
    
    // 添加负载
    if (len > 0 && data) {
        frame.insert(frame.end(), data, data + len);
    }
    
    return send_raw(frame.data(), frame.size());
}

bool WsConnection::send_raw(const uint8_t* data, size_t len) {
    if (closed_) return false;
    
    ssize_t sent = ::send(socket_, data, len, MSG_NOSIGNAL);
    if (sent < 0 || static_cast<size_t>(sent) != len) {
        std::cerr << "[WsConn] Send failed: " << strerror(errno) << std::endl;
        close();
        return false;
    }
    
    return true;
}

// ==================== WebSocketServer ====================

WebSocketServer::WebSocketServer(const WsServerConfig& config)
    : config_(config) {
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    if (running_) return false;
    
    // 创建监听 socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        std::cerr << "[WsServer] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置 SO_REUSEADDR
    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
    
    if (bind(listen_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[WsServer] Failed to bind: " << strerror(errno) << std::endl;
        ::close(listen_socket_);
        listen_socket_ = -1;
        return false;
    }
    
    // 开始监听
    if (listen(listen_socket_, 128) < 0) {
        std::cerr << "[WsServer] Failed to listen: " << strerror(errno) << std::endl;
        ::close(listen_socket_);
        listen_socket_ = -1;
        return false;
    }
    
    running_ = true;
    
    // 启动接受连接线程
    accept_thread_ = std::thread(&WebSocketServer::accept_loop, this);
    
    // 启动心跳检查线程
    heartbeat_thread_ = std::thread(&WebSocketServer::heartbeat_loop, this);
    
    std::cout << "[WsServer] Started on port " << config_.port << std::endl;
    std::cout << "  - WebSocket: ws://localhost:" << config_.port << "/ws" << std::endl;
    
    return true;
}

void WebSocketServer::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // 关闭监听 socket
    if (listen_socket_ >= 0) {
        ::close(listen_socket_);
        listen_socket_ = -1;
    }
    
    // 等待线程结束
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    
    // 关闭所有连接
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        for (auto& [sock, conn] : connections_) {
            conn->close();
        }
        connections_.clear();
        user_connections_.clear();
    }
    
    std::cout << "[WsServer] Stopped" << std::endl;
}

void WebSocketServer::run() {
    // 主线程等待
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void WebSocketServer::accept_loop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(listen_socket_,
                                   (struct sockaddr*)&client_addr,
                                   &addr_len);
        
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "[WsServer] Accept failed: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // 获取客户端 IP
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        
        // 设置非阻塞
        int flags = fcntl(client_socket, F_GETFL, 0);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
        
        // 设置 TCP_NODELAY
        int nodelay = 1;
        setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        
        // 处理新连接（在后台线程中）
        std::thread(&WebSocketServer::handle_new_connection,
                    this, client_socket, std::string(client_ip)).detach();
    }
}

void WebSocketServer::handle_new_connection(int client_socket, const std::string& client_ip) {
    // 读取 HTTP 升级请求
    char buffer[4096];
    std::string request;
    
    // 等待数据（使用 poll）
    struct pollfd pfd;
    pfd.fd = client_socket;
    pfd.events = POLLIN;
    
    // 5秒超时
    int ret = poll(&pfd, 1, 5000);
    if (ret <= 0) {
        std::cerr << "[WsServer] Timeout waiting for handshake from " << client_ip << std::endl;
        ::close(client_socket);
        return;
    }
    
    ssize_t n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        std::cerr << "[WsServer] Failed to read handshake from " << client_ip << std::endl;
        ::close(client_socket);
        return;
    }
    
    buffer[n] = '\0';
    request = buffer;
    
    // 检查是否是 WebSocket 升级请求
    if (request.find("Upgrade: websocket") == std::string::npos &&
        request.find("upgrade: websocket") == std::string::npos) {
        std::cerr << "[WsServer] Not a WebSocket upgrade request from " << client_ip << std::endl;
        ::close(client_socket);
        return;
    }
    
    // 执行握手
    if (!perform_handshake(client_socket, request)) {
        std::cerr << "[WsServer] Handshake failed for " << client_ip << std::endl;
        ::close(client_socket);
        return;
    }
    
    // 设置为阻塞模式进行消息处理
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags & ~O_NONBLOCK);
    
    // 创建连接对象
    auto conn = std::make_shared<WsConnection>(client_socket, client_ip);
    conn->set_message_handler([this](WsConnection::ptr c, const std::string& msg) {
        if (message_handler_) {
            message_handler_(c, msg);
        }
    });
    
    // 添加到连接表
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        connections_[client_socket] = conn;
    }
    
    std::cout << "[WsServer] New WebSocket connection from " << client_ip << std::endl;
    
    // 调用连接处理器
    if (connect_handler_) {
        connect_handler_(conn);
    }
    
    // 消息循环
    char recv_buffer[65536];
    while (running_ && !conn->closed_) {
        struct pollfd pfd;
        pfd.fd = client_socket;
        pfd.events = POLLIN;
        
        // 1秒超时，允许检查 running_
        ret = poll(&pfd, 1, 1000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (ret == 0) continue; // 超时
        
        if (pfd.revents & (POLLERR | POLLHUP)) {
            break;
        }
        
        if (pfd.revents & POLLIN) {
            n = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0);
            if (n <= 0) {
                break;
            }
            
            if (!conn->handle_receive(reinterpret_cast<uint8_t*>(recv_buffer), n)) {
                break;
            }
        }
    }
    
    // 清理
    remove_connection(conn);
    
    // 调用断开处理器
    if (disconnect_handler_) {
        disconnect_handler_(conn);
    }
    
    std::cout << "[WsServer] Connection closed from " << client_ip << std::endl;
}

bool WebSocketServer::perform_handshake(int socket, const std::string& request) {
    // 提取 Sec-WebSocket-Key
    std::string ws_key;
    size_t key_pos = request.find("Sec-WebSocket-Key:");
    if (key_pos == std::string::npos) {
        key_pos = request.find("Sec-WebSocket-key:");
    }
    
    if (key_pos != std::string::npos) {
        size_t start = request.find(':', key_pos) + 1;
        while (start < request.size() && (request[start] == ' ' || request[start] == '\t')) {
            ++start;
        }
        size_t end = request.find('\r', start);
        if (end == std::string::npos) {
            end = request.find('\n', start);
        }
        ws_key = request.substr(start, end - start);
    }
    
    if (ws_key.empty()) {
        return false;
    }
    
    // 计算 Sec-WebSocket-Accept
    std::string accept_key = compute_accept_key(ws_key);
    
    // 发送握手响应
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n"
             << "\r\n";
    
    std::string response_str = response.str();
    ssize_t sent = ::send(socket, response_str.c_str(), response_str.size(), 0);
    
    return sent == static_cast<ssize_t>(response_str.size());
}

void WebSocketServer::heartbeat_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.heartbeat_interval));
        
        auto now = std::chrono::steady_clock::now();
        std::vector<WsConnection::ptr> to_remove;
        
        {
            std::lock_guard<std::mutex> lock(conn_mutex_);
            for (auto& [sock, conn] : connections_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - conn->last_activity()).count();
                
                if (elapsed > config_.heartbeat_timeout) {
                    std::cerr << "[WsServer] Connection timeout for user " << conn->get_user_id() << std::endl;
                    to_remove.push_back(conn);
                } else if (elapsed > config_.heartbeat_interval) {
                    // 发送 Ping
                    conn->send_ping();
                }
            }
        }
        
        // 移除超时连接
        for (auto& conn : to_remove) {
            conn->close();
            remove_connection(conn);
        }
    }
}

void WebSocketServer::remove_connection(WsConnection::ptr conn) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    
    connections_.erase(conn->get_socket());
    
    uint64_t user_id = conn->get_user_id();
    if (user_id > 0) {
        // 只有当连接对象相同时才移除
        auto it = user_connections_.find(user_id);
        if (it != user_connections_.end() && it->second == conn) {
            user_connections_.erase(it);
        }
    }
}

std::string WebSocketServer::compute_accept_key(const std::string& key) {
    std::string key_with_guid = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(key_with_guid.c_str()),
         key_with_guid.size(), hash);
    
    // Base64 编码
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    
    BUF_MEM* buffer;
    BIO_get_mem_ptr(bio, &buffer);
    std::string result(buffer->data, buffer->length);
    BIO_free_all(bio);
    
    return result;
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    for (auto& [sock, conn] : connections_) {
        conn->send_text(message);
    }
}

void WebSocketServer::broadcast_to_user(uint64_t user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    auto it = user_connections_.find(user_id);
    if (it != user_connections_.end()) {
        it->second->send_text(message);
    }
}

void WebSocketServer::broadcast_to_users(const std::vector<uint64_t>& user_ids, const std::string& message) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    for (uint64_t user_id : user_ids) {
        auto it = user_connections_.find(user_id);
        if (it != user_connections_.end()) {
            it->second->send_text(message);
        }
    }
}

WsConnection::ptr WebSocketServer::get_connection(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    auto it = user_connections_.find(user_id);
    if (it != user_connections_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<WsConnection::ptr> WebSocketServer::get_all_connections() {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    std::vector<WsConnection::ptr> result;
    result.reserve(connections_.size());
    for (auto& [sock, conn] : connections_) {
        result.push_back(conn);
    }
    return result;
}

size_t WebSocketServer::get_connection_count() {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    return connections_.size();
}

bool WebSocketServer::is_user_online(uint64_t user_id) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    return user_connections_.count(user_id) > 0;
}

} // namespace chat
