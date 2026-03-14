#ifndef DATABASE_POOL_HPP
#define DATABASE_POOL_HPP

// 支持 MariaDB (Termux/Android) 和 MySQL
#if defined(__ANDROID__) || defined(__TERMUX__)
#include <mariadb/mysql.h>
#include <mariadb/errmsg.h>
#else
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <iostream>
#include <thread>

namespace chat {

/**
 * 数据库连接配置
 */
struct DatabaseConfig {
    std::string host = "localhost";
    uint16_t port = 3306;
    std::string user = "root";
    std::string password = "";
    std::string database = "chat_app";
    std::string charset = "utf8mb4";
    unsigned int connect_timeout = 10;
    unsigned int read_timeout = 10;
    unsigned int write_timeout = 10;
};

/**
 * 数据库连接包装
 * 封装 MYSQL 连接，提供 RAII 管理
 */
class DatabaseConnection {
public:
    DatabaseConnection();
    ~DatabaseConnection();
    
    // 禁止拷贝
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
    
    // 允许移动
    DatabaseConnection(DatabaseConnection&& other) noexcept;
    DatabaseConnection& operator=(DatabaseConnection&& other) noexcept;
    
    /**
     * 连接到数据库
     */
    bool connect(const DatabaseConfig& config);
    
    /**
     * 检查连接是否有效
     */
    bool is_valid() const;
    
    /**
     * 检查连接是否在使用中
     */
    bool is_in_use() const { return in_use_; }
    
    /**
     * 设置使用状态
     */
    void set_in_use(bool in_use) { in_use_ = in_use; }
    
    /**
     * 获取最后使用时间
     */
    std::chrono::steady_clock::time_point get_last_used() const { return last_used_; }
    
    /**
     * 更新最后使用时间
     */
    void touch() { last_used_ = std::chrono::steady_clock::now(); }
    
    /**
     * 获取原始 MYSQL 连接
     */
    MYSQL* get() { return connection_; }
    const MYSQL* get() const { return connection_; }
    
    /**
     * 执行查询
     */
    bool query(const std::string& sql);
    
    /**
     * 获取结果集
     */
    MYSQL_RES* store_result();
    
    /**
     * 获取影响行数
     */
    uint64_t affected_rows();
    
    /**
     * 获取最后插入ID
     */
    uint64_t insert_id();
    
    /**
     * 获取错误信息
     */
    std::string error() const;
    
    /**
     * 获取错误码
     */
    unsigned int error_code() const;
    
    /**
     * 转义字符串
     */
    std::string escape_string(const std::string& str);
    
    /**
     * Ping 连接（检查是否存活）
     */
    bool ping();
    
private:
    MYSQL* connection_;
    bool in_use_;
    std::chrono::steady_clock::time_point last_used_;
    DatabaseConfig config_;
};

/**
 * 数据库连接池
 * 线程安全的连接池，支持自动重连和连接回收
 */
class DatabasePool {
public:
    /**
     * 连接池配置
     */
    struct PoolConfig {
        size_t min_connections = 5;     // 最小连接数
        size_t max_connections = 20;    // 最大连接数
        size_t max_idle_time = 300;     // 最大空闲时间（秒）
        size_t connection_timeout = 5;  // 获取连接超时（秒）
        size_t health_check_interval = 60;  // 健康检查间隔（秒）
    };
    
    explicit DatabasePool(const DatabaseConfig& db_config, 
                          const PoolConfig& pool_config);
    ~DatabasePool();
    
    // 禁止拷贝和移动
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;
    DatabasePool(DatabasePool&&) = delete;
    DatabasePool& operator=(DatabasePool&&) = delete;
    
    /**
     * 初始化连接池
     */
    bool init();
    
    /**
     * 获取连接（阻塞，直到获取成功或超时）
     * @return 连接指针，如果超时返回 nullptr
     */
    std::shared_ptr<DatabaseConnection> acquire();
    
    /**
     * 尝试获取连接（非阻塞）
     * @return 连接指针，如果没有可用连接返回 nullptr
     */
    std::shared_ptr<DatabaseConnection> try_acquire();
    
    /**
     * 释放连接回连接池
     */
    void release(std::shared_ptr<DatabaseConnection> conn);
    
    /**
     * 获取当前连接数
     */
    size_t size() const;
    
    /**
     * 获取当前空闲连接数
     */
    size_t idle_size() const;
    
    /**
     * 获取当前使用中连接数
     */
    size_t used_size() const;
    
    /**
     * 获取统计信息
     */
    struct Stats {
        size_t total_connections;
        size_t idle_connections;
        size_t used_connections;
        size_t total_acquires;
        size_t total_releases;
        size_t acquire_timeouts;
        size_t connection_errors;
    };
    Stats get_stats() const;
    
    /**
     * 关闭连接池
     */
    void shutdown();
    
    /**
     * 检查连接池是否已初始化
     */
    bool is_initialized() const { return initialized_; }

private:
    /**
     * 创建新连接
     */
    std::shared_ptr<DatabaseConnection> create_connection();
    
    /**
     * 检查并清理空闲连接
     */
    void cleanup_idle_connections();
    
    /**
     * 健康检查线程
     */
    void health_check_loop();
    
    /**
     * 连接包装器（用于自动释放）
     */
    struct ConnectionWrapper {
        std::shared_ptr<DatabaseConnection> conn;
        DatabasePool* pool;
        
        ~ConnectionWrapper() {
            if (conn && pool) {
                pool->release(conn);
            }
        }
    };

private:
    DatabaseConfig db_config_;
    PoolConfig pool_config_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    std::queue<std::shared_ptr<DatabaseConnection>> idle_connections_;
    std::vector<std::shared_ptr<DatabaseConnection>> all_connections_;
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_{false};
    
    // 统计信息
    std::atomic<size_t> total_acquires_{0};
    std::atomic<size_t> total_releases_{0};
    std::atomic<size_t> acquire_timeouts_{0};
    std::atomic<size_t> connection_errors_{0};
    
    // 健康检查线程
    std::thread health_check_thread_;
};

/**
 * RAII 风格的连接守卫
 * 自动获取和释放连接
 */
class ConnectionGuard {
public:
    explicit ConnectionGuard(DatabasePool& pool);
    ~ConnectionGuard();
    
    // 禁止拷贝
    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;
    
    // 允许移动
    ConnectionGuard(ConnectionGuard&& other) noexcept;
    ConnectionGuard& operator=(ConnectionGuard&& other) noexcept;
    
    /**
     * 获取连接
     */
    DatabaseConnection* get();
    DatabaseConnection* operator->() { return get(); }
    
    /**
     * 检查连接是否有效
     */
    explicit operator bool() const { return connection_ != nullptr; }
    
    /**
     * 释放连接
     */
    void release();

private:
    DatabasePool& pool_;
    std::shared_ptr<DatabaseConnection> connection_;
};

} // namespace chat

#endif // DATABASE_POOL_HPP
