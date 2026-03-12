#include "database.hpp"
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>

namespace chat {

Database::Database() 
    : connection_(nullptr)
    , connected_(false) {
}

Database::~Database() {
    close();
}

bool Database::init(const Config& config) {
    config_ = config;
    
    std::cerr << "Initializing MySQL library..." << std::endl;
    
    // 初始化 MySQL
    if (mysql_library_init(0, nullptr, nullptr) != 0) {
        std::cerr << "mysql_library_init failed" << std::endl;
        return false;
    }
    
    std::cerr << "Creating MySQL connection..." << std::endl;
    connection_ = mysql_init(nullptr);
    if (!connection_) {
        std::cerr << "mysql_init failed" << std::endl;
        return false;
    }
    
    // 设置连接选项
    bool reconnect = true;
    mysql_options(connection_, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(connection_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    
    // 设置超时
    unsigned int timeout = 10;
    mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(connection_, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(connection_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    
    std::cerr << "Connecting to database..." << std::endl;
    return connect();
}

bool Database::connect() {
    if (!connection_) {
        return false;
    }
    
    MYSQL* conn = mysql_real_connect(
        connection_,
        config_.host.c_str(),
        config_.user.c_str(),
        config_.password.c_str(),
        config_.database.c_str(),
        config_.port,
        nullptr,
        CLIENT_MULTI_STATEMENTS
    );
    
    if (!conn) {
        std::cerr << "MySQL connection error: " << mysql_error(connection_) << std::endl;
        std::cerr << "Error code: " << mysql_errno(connection_) << std::endl;
        std::cerr << "Connecting to: " << config_.host << ":" << config_.port 
                  << " database: " << config_.database << " user: " << config_.user << std::endl;
        std::cerr << std::flush;
        return false;
    }
    
    connected_ = true;
    return init_tables();
}

void Database::disconnect() {
    if (connection_) {
        mysql_close(connection_);
        connection_ = nullptr;
    }
    connected_ = false;
}

void Database::close() {
    disconnect();
    mysql_library_end();
}

bool Database::init_tables() {
    std::cerr << "Initializing database tables..." << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 用户表
    const char* create_users = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(64) NOT NULL UNIQUE,
            password VARCHAR(128) NOT NULL,
            nickname VARCHAR(64) NOT NULL,
            avatar_url VARCHAR(512) DEFAULT '',
            signature VARCHAR(256) DEFAULT '',
            online_status TINYINT UNSIGNED DEFAULT 0,
            created_at BIGINT NOT NULL,
            updated_at BIGINT NOT NULL,
            INDEX idx_username (username),
            INDEX idx_online_status (online_status)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 好友关系表
    const char* create_friends = R"(
        CREATE TABLE IF NOT EXISTS friends (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            user_id BIGINT UNSIGNED NOT NULL,
            friend_id BIGINT UNSIGNED NOT NULL,
            remark VARCHAR(64) DEFAULT '',
            status TINYINT UNSIGNED DEFAULT 0,
            created_at BIGINT NOT NULL,
            updated_at BIGINT NOT NULL,
            UNIQUE KEY uk_user_friend (user_id, friend_id),
            INDEX idx_friend_id (friend_id),
            INDEX idx_status (status),
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE,
            FOREIGN KEY (friend_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 群组表
    const char* create_groups = R"(
        CREATE TABLE IF NOT EXISTS `groups` (
            group_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            group_name VARCHAR(128) NOT NULL,
            avatar_url VARCHAR(512) DEFAULT '',
            description VARCHAR(512) DEFAULT '',
            owner_id BIGINT UNSIGNED NOT NULL,
            created_at BIGINT NOT NULL,
            updated_at BIGINT NOT NULL,
            INDEX idx_owner_id (owner_id),
            FOREIGN KEY (owner_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 群成员表
    const char* create_group_members = R"(
        CREATE TABLE IF NOT EXISTS group_members (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            group_id BIGINT UNSIGNED NOT NULL,
            user_id BIGINT UNSIGNED NOT NULL,
            is_admin TINYINT DEFAULT 0,
            joined_at BIGINT NOT NULL,
            UNIQUE KEY uk_group_user (group_id, user_id),
            INDEX idx_user_id (user_id),
            FOREIGN KEY (group_id) REFERENCES `groups`(group_id) ON DELETE CASCADE,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 私聊消息表
    const char* create_private_messages = R"(
        CREATE TABLE IF NOT EXISTS private_messages (
            message_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            sender_id BIGINT UNSIGNED NOT NULL,
            receiver_id BIGINT UNSIGNED NOT NULL,
            media_type TINYINT UNSIGNED DEFAULT 0,
            content TEXT,
            media_url VARCHAR(512) DEFAULT '',
            extra TEXT,
            status TINYINT UNSIGNED DEFAULT 0,
            created_at BIGINT NOT NULL,
            INDEX idx_sender (sender_id),
            INDEX idx_receiver (receiver_id),
            INDEX idx_created_at (created_at),
            FOREIGN KEY (sender_id) REFERENCES users(user_id) ON DELETE CASCADE,
            FOREIGN KEY (receiver_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 群聊消息表
    const char* create_group_messages = R"(
        CREATE TABLE IF NOT EXISTS group_messages (
            message_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            group_id BIGINT UNSIGNED NOT NULL,
            sender_id BIGINT UNSIGNED NOT NULL,
            media_type TINYINT UNSIGNED DEFAULT 0,
            content TEXT,
            media_url VARCHAR(512) DEFAULT '',
            extra TEXT,
            status TINYINT UNSIGNED DEFAULT 0,
            created_at BIGINT NOT NULL,
            INDEX idx_group_id (group_id),
            INDEX idx_sender (sender_id),
            INDEX idx_created_at (created_at),
            FOREIGN KEY (group_id) REFERENCES `groups`(group_id) ON DELETE CASCADE,
            FOREIGN KEY (sender_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 媒体文件表
    const char* create_media_files = R"(
        CREATE TABLE IF NOT EXISTS media_files (
            file_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            user_id BIGINT UNSIGNED NOT NULL,
            file_name VARCHAR(256) NOT NULL,
            file_path VARCHAR(512) NOT NULL,
            file_size BIGINT UNSIGNED NOT NULL,
            media_type TINYINT UNSIGNED DEFAULT 0,
            created_at BIGINT NOT NULL,
            INDEX idx_user_id (user_id),
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 机器人聊天记录表
    const char* create_bot_conversations = R"(
        CREATE TABLE IF NOT EXISTS bot_conversations (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            user_id BIGINT UNSIGNED NOT NULL,
            conversation_id VARCHAR(64) NOT NULL,
            role VARCHAR(16) NOT NULL,
            content TEXT NOT NULL,
            char_count INT UNSIGNED DEFAULT 0,
            created_at BIGINT NOT NULL,
            INDEX idx_user_id (user_id),
            INDEX idx_conversation_id (conversation_id),
            INDEX idx_created_at (created_at),
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    // 用户公钥表（端到端加密）
    const char* create_user_keys = R"(
        CREATE TABLE IF NOT EXISTS user_keys (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            user_id BIGINT UNSIGNED NOT NULL UNIQUE,
            public_key TEXT NOT NULL,
            key_version INT UNSIGNED DEFAULT 1,
            created_at BIGINT NOT NULL,
            updated_at BIGINT NOT NULL,
            INDEX idx_user_id (user_id),
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    if (mysql_query(connection_, create_users) != 0) {
        std::cerr << "Failed to create users table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created users table" << std::endl;
    
    if (mysql_query(connection_, create_friends) != 0) {
        std::cerr << "Failed to create friends table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created friends table" << std::endl;
    
    if (mysql_query(connection_, create_groups) != 0) {
        std::cerr << "Failed to create groups table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created groups table" << std::endl;
    
    if (mysql_query(connection_, create_group_members) != 0) {
        std::cerr << "Failed to create group_members table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created group_members table" << std::endl;
    
    if (mysql_query(connection_, create_private_messages) != 0) {
        std::cerr << "Failed to create private_messages table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created private_messages table" << std::endl;
    
    if (mysql_query(connection_, create_group_messages) != 0) {
        std::cerr << "Failed to create group_messages table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created group_messages table" << std::endl;
    
    if (mysql_query(connection_, create_media_files) != 0) {
        std::cerr << "Failed to create media_files table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created media_files table" << std::endl;
    
    if (mysql_query(connection_, create_bot_conversations) != 0) {
        std::cerr << "Failed to create bot_conversations table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created bot_conversations table" << std::endl;
    
    if (mysql_query(connection_, create_user_keys) != 0) {
        std::cerr << "Failed to create user_keys table: " << mysql_error(connection_) << std::endl;
        return false;
    }
    std::cerr << "Created user_keys table" << std::endl;
    
    return true;
}

std::string Database::escape_string(const std::string& str) {
    if (str.empty()) return "";
    std::string escaped;
    escaped.resize(str.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(connection_, &escaped[0], 
                                                  str.c_str(), str.size());
    escaped.resize(len);
    return escaped;
}

int64_t Database::get_current_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

uint64_t Database::get_last_insert_id() {
    return mysql_insert_id(connection_);
}

// ==================== 用户相关实现 ====================

bool Database::create_user(const std::string& username, const std::string& password,
                           const std::string& nickname, uint64_t& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_username = escape_string(username);
    std::string escaped_password = escape_string(password);
    std::string escaped_nickname = escape_string(nickname);
    
    std::ostringstream sql;
    sql << "INSERT INTO users (username, password, nickname, created_at, updated_at) VALUES ('"
        << escaped_username << "', '" << escaped_password << "', '" << escaped_nickname
        << "', " << now << ", " << now << ")";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    user_id = get_last_insert_id();
    return true;
}

bool Database::get_user_by_id(uint64_t user_id, UserInfo& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT user_id, username, nickname, avatar_url, signature, "
        << "online_status, created_at, updated_at FROM users WHERE user_id = " << user_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    user.user_id = std::stoull(row[0]);
    user.username = row[1] ? row[1] : "";
    user.nickname = row[2] ? row[2] : "";
    user.avatar_url = row[3] ? row[3] : "";
    user.signature = row[4] ? row[4] : "";
    user.online_status = static_cast<OnlineStatus>(std::stoi(row[5]));
    user.created_at = std::stoll(row[6]);
    user.updated_at = std::stoll(row[7]);
    
    mysql_free_result(result);
    return true;
}

bool Database::get_user_by_username(const std::string& username, UserInfo& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string escaped_username = escape_string(username);
    std::ostringstream sql;
    sql << "SELECT user_id, username, nickname, avatar_url, signature, "
        << "online_status, created_at, updated_at FROM users WHERE username = '" 
        << escaped_username << "'";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    user.user_id = std::stoull(row[0]);
    user.username = row[1] ? row[1] : "";
    user.nickname = row[2] ? row[2] : "";
    user.avatar_url = row[3] ? row[3] : "";
    user.signature = row[4] ? row[4] : "";
    user.online_status = static_cast<OnlineStatus>(std::stoi(row[5]));
    user.created_at = std::stoll(row[6]);
    user.updated_at = std::stoll(row[7]);
    
    mysql_free_result(result);
    return true;
}

bool Database::verify_user(const std::string& username, const std::string& password, 
                           uint64_t& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string escaped_username = escape_string(username);
    std::string escaped_password = escape_string(password);
    
    std::ostringstream sql;
    sql << "SELECT user_id FROM users WHERE username = '" << escaped_username 
        << "' AND password = '" << escaped_password << "'";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    user_id = std::stoull(row[0]);
    mysql_free_result(result);
    return true;
}

bool Database::update_user(const UserInfo& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_nickname = escape_string(user.nickname);
    std::string escaped_avatar = escape_string(user.avatar_url);
    std::string escaped_signature = escape_string(user.signature);
    
    std::ostringstream sql;
    sql << "UPDATE users SET nickname = '" << escaped_nickname 
        << "', avatar_url = '" << escaped_avatar
        << "', signature = '" << escaped_signature
        << "', updated_at = " << now 
        << " WHERE user_id = " << user.user_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::update_user_password(uint64_t user_id, const std::string& new_password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_password = escape_string(new_password);
    
    std::ostringstream sql;
    sql << "UPDATE users SET password = '" << escaped_password 
        << "', updated_at = " << now 
        << " WHERE user_id = " << user_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::update_user_online_status(uint64_t user_id, OnlineStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "UPDATE users SET online_status = " << static_cast<int>(status)
        << " WHERE user_id = " << user_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

std::vector<UserInfo> Database::search_users(const std::string& keyword, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserInfo> users;
    
    std::string escaped_keyword = escape_string(keyword);
    
    std::ostringstream sql;
    sql << "SELECT user_id, username, nickname, avatar_url, signature, "
        << "online_status, created_at, updated_at FROM users "
        << "WHERE username LIKE '%" << escaped_keyword << "%' "
        << "OR nickname LIKE '%" << escaped_keyword << "%' "
        << "LIMIT " << limit;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return users;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return users;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        UserInfo user;
        user.user_id = std::stoull(row[0]);
        user.username = row[1] ? row[1] : "";
        user.nickname = row[2] ? row[2] : "";
        user.avatar_url = row[3] ? row[3] : "";
        user.signature = row[4] ? row[4] : "";
        user.online_status = static_cast<OnlineStatus>(std::stoi(row[5]));
        user.created_at = std::stoll(row[6]);
        user.updated_at = std::stoll(row[7]);
        users.push_back(user);
    }
    
    mysql_free_result(result);
    return users;
}

// ==================== 好友相关实现 ====================

bool Database::add_friend_request(uint64_t user_id, uint64_t friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    
    std::ostringstream sql;
    sql << "INSERT INTO friends (user_id, friend_id, status, created_at, updated_at) VALUES ("
        << user_id << ", " << friend_id << ", 0, " << now << ", " << now << ")";
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::accept_friend_request(uint64_t user_id, uint64_t friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    
    // 更新请求状态
    std::ostringstream sql1;
    sql1 << "UPDATE friends SET status = 1, updated_at = " << now 
         << " WHERE user_id = " << friend_id << " AND friend_id = " << user_id;
    
    if (mysql_query(connection_, sql1.str().c_str()) != 0) {
        return false;
    }
    
    // 添加反向好友关系
    std::ostringstream sql2;
    sql2 << "INSERT INTO friends (user_id, friend_id, status, created_at, updated_at) VALUES ("
         << user_id << ", " << friend_id << ", 1, " << now << ", " << now << ")";
    
    return mysql_query(connection_, sql2.str().c_str()) == 0;
}

bool Database::reject_friend_request(uint64_t user_id, uint64_t friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    
    std::ostringstream sql;
    sql << "UPDATE friends SET status = 2, updated_at = " << now 
        << " WHERE user_id = " << friend_id << " AND friend_id = " << user_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::remove_friend(uint64_t user_id, uint64_t friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "DELETE FROM friends WHERE (user_id = " << user_id 
        << " AND friend_id = " << friend_id << ") OR (user_id = " << friend_id 
        << " AND friend_id = " << user_id << ")";
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::get_friend_list(uint64_t user_id, 
                               std::vector<std::pair<UserInfo, FriendRelation>>& friends) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT u.user_id, u.username, u.nickname, u.avatar_url, u.signature, "
        << "u.online_status, u.created_at, u.updated_at, "
        << "f.remark, f.status, f.created_at, f.updated_at "
        << "FROM users u INNER JOIN friends f ON u.user_id = f.friend_id "
        << "WHERE f.user_id = " << user_id << " AND f.status = 1";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        UserInfo user;
        user.user_id = std::stoull(row[0]);
        user.username = row[1] ? row[1] : "";
        user.nickname = row[2] ? row[2] : "";
        user.avatar_url = row[3] ? row[3] : "";
        user.signature = row[4] ? row[4] : "";
        user.online_status = static_cast<OnlineStatus>(std::stoi(row[5]));
        user.created_at = std::stoll(row[6]);
        user.updated_at = std::stoll(row[7]);
        
        FriendRelation rel;
        rel.user_id = user_id;
        rel.friend_id = user.user_id;
        rel.remark = row[8] ? row[8] : "";
        rel.status = static_cast<FriendStatus>(std::stoi(row[9]));
        rel.created_at = std::stoll(row[10]);
        rel.updated_at = std::stoll(row[11]);
        
        friends.push_back({user, rel});
    }
    
    mysql_free_result(result);
    return true;
}

bool Database::get_friend_requests(uint64_t user_id,
                                   std::vector<std::pair<UserInfo, FriendRelation>>& requests) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT u.user_id, u.username, u.nickname, u.avatar_url, u.signature, "
        << "u.online_status, u.created_at, u.updated_at, "
        << "f.remark, f.status, f.created_at, f.updated_at "
        << "FROM users u INNER JOIN friends f ON u.user_id = f.user_id "
        << "WHERE f.friend_id = " << user_id << " AND f.status = 0";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        UserInfo user;
        user.user_id = std::stoull(row[0]);
        user.username = row[1] ? row[1] : "";
        user.nickname = row[2] ? row[2] : "";
        user.avatar_url = row[3] ? row[3] : "";
        user.signature = row[4] ? row[4] : "";
        user.online_status = static_cast<OnlineStatus>(std::stoi(row[5]));
        user.created_at = std::stoll(row[6]);
        user.updated_at = std::stoll(row[7]);
        
        FriendRelation rel;
        rel.user_id = user.user_id;
        rel.friend_id = user_id;
        rel.remark = row[8] ? row[8] : "";
        rel.status = static_cast<FriendStatus>(std::stoi(row[9]));
        rel.created_at = std::stoll(row[10]);
        rel.updated_at = std::stoll(row[11]);
        
        requests.push_back({user, rel});
    }
    
    mysql_free_result(result);
    return true;
}

bool Database::is_friend(uint64_t user_id, uint64_t friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM friends WHERE user_id = " << user_id 
        << " AND friend_id = " << friend_id << " AND status = 1";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool is_friend = row && std::stoi(row[0]) > 0;
    mysql_free_result(result);
    
    return is_friend;
}

bool Database::set_friend_remark(uint64_t user_id, uint64_t friend_id, const std::string& remark) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_remark = escape_string(remark);
    
    std::ostringstream sql;
    sql << "UPDATE friends SET remark = '" << escaped_remark 
        << "', updated_at = " << now 
        << " WHERE user_id = " << user_id << " AND friend_id = " << friend_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

// ==================== 私聊消息相关实现 ====================

bool Database::save_private_message(Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_content = escape_string(message.content);
    std::string escaped_media_url = escape_string(message.media_url);
    std::string escaped_extra = escape_string(message.extra);
    
    std::ostringstream sql;
    sql << "INSERT INTO private_messages (sender_id, receiver_id, media_type, content, "
        << "media_url, extra, status, created_at) VALUES ("
        << message.sender_id << ", " << message.receiver_id << ", "
        << static_cast<int>(message.media_type) << ", '" << escaped_content << "', '"
        << escaped_media_url << "', '" << escaped_extra << "', "
        << static_cast<int>(message.status) << ", " << now << ")";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    message.message_id = get_last_insert_id();
    message.created_at = now;
    return true;
}

bool Database::get_private_history(uint64_t user1_id, uint64_t user2_id,
                                   int64_t before_time, int limit, std::vector<Message>& messages) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT message_id, sender_id, receiver_id, media_type, content, media_url, "
        << "extra, status, created_at FROM private_messages "
        << "WHERE ((sender_id = " << user1_id << " AND receiver_id = " << user2_id << ") "
        << "OR (sender_id = " << user2_id << " AND receiver_id = " << user1_id << ")) ";
    
    if (before_time > 0) {
        sql << "AND created_at < " << before_time << " ";
    }
    
    sql << "ORDER BY created_at DESC LIMIT " << limit;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Message msg;
        msg.message_id = std::stoull(row[0]);
        msg.sender_id = std::stoull(row[1]);
        msg.receiver_id = std::stoull(row[2]);
        msg.media_type = static_cast<MediaType>(std::stoi(row[3]));
        msg.content = row[4] ? row[4] : "";
        msg.media_url = row[5] ? row[5] : "";
        msg.extra = row[6] ? row[6] : "";
        msg.status = static_cast<MessageStatus>(std::stoi(row[7]));
        msg.created_at = std::stoll(row[8]);
        messages.push_back(msg);
    }
    
    mysql_free_result(result);
    return true;
}

bool Database::update_message_status(uint64_t message_id, MessageStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "UPDATE private_messages SET status = " << static_cast<int>(status)
        << " WHERE message_id = " << message_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::mark_messages_read(uint64_t user_id, uint64_t peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "UPDATE private_messages SET status = 3 "
        << "WHERE sender_id = " << peer_id << " AND receiver_id = " << user_id 
        << " AND status < 3";
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

int Database::get_unread_count(uint64_t user_id, uint64_t peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM private_messages "
        << "WHERE sender_id = " << peer_id << " AND receiver_id = " << user_id 
        << " AND status < 3";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return 0;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    
    return count;
}

// ==================== 群组相关实现 ====================

bool Database::create_group(const std::string& group_name, uint64_t owner_id,
                            const std::string& description, uint64_t& group_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_name = escape_string(group_name);
    std::string escaped_desc = escape_string(description);
    
    std::ostringstream sql;
    sql << "INSERT INTO `groups` (group_name, owner_id, description, created_at, updated_at) VALUES ('"
        << escaped_name << "', " << owner_id << ", '" << escaped_desc 
        << "', " << now << ", " << now << ")";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    group_id = get_last_insert_id();
    
    // 添加创建者为群成员
    std::ostringstream sql2;
    sql2 << "INSERT INTO group_members (group_id, user_id, is_admin, joined_at) VALUES ("
         << group_id << ", " << owner_id << ", 1, " << now << ")";
    
    return mysql_query(connection_, sql2.str().c_str()) == 0;
}

bool Database::get_group_by_id(uint64_t group_id, GroupInfo& group) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT group_id, group_name, avatar_url, description, owner_id, "
        << "created_at, updated_at FROM `groups` WHERE group_id = " << group_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    group.group_id = std::stoull(row[0]);
    group.group_name = row[1] ? row[1] : "";
    group.avatar_url = row[2] ? row[2] : "";
    group.description = row[3] ? row[3] : "";
    group.owner_id = std::stoull(row[4]);
    group.created_at = std::stoll(row[5]);
    group.updated_at = std::stoll(row[6]);
    
    mysql_free_result(result);
    
    // 获取群成员
    get_group_members(group_id, group.members);
    
    return true;
}

bool Database::update_group(const GroupInfo& group) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_name = escape_string(group.group_name);
    std::string escaped_avatar = escape_string(group.avatar_url);
    std::string escaped_desc = escape_string(group.description);
    
    std::ostringstream sql;
    sql << "UPDATE `groups` SET group_name = '" << escaped_name 
        << "', avatar_url = '" << escaped_avatar
        << "', description = '" << escaped_desc
        << "', updated_at = " << now 
        << " WHERE group_id = " << group.group_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::dismiss_group(uint64_t group_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "DELETE FROM `groups` WHERE group_id = " << group_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::add_group_member(uint64_t group_id, uint64_t user_id, bool is_admin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    
    std::ostringstream sql;
    sql << "INSERT INTO group_members (group_id, user_id, is_admin, joined_at) VALUES ("
        << group_id << ", " << user_id << ", " << (is_admin ? 1 : 0) << ", " << now << ")";
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::remove_group_member(uint64_t group_id, uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "DELETE FROM group_members WHERE group_id = " << group_id 
        << " AND user_id = " << user_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::get_user_groups(uint64_t user_id, std::vector<GroupInfo>& groups) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT g.group_id, g.group_name, g.avatar_url, g.description, g.owner_id, "
        << "g.created_at, g.updated_at FROM `groups` g "
        << "INNER JOIN group_members gm ON g.group_id = gm.group_id "
        << "WHERE gm.user_id = " << user_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        GroupInfo group;
        group.group_id = std::stoull(row[0]);
        group.group_name = row[1] ? row[1] : "";
        group.avatar_url = row[2] ? row[2] : "";
        group.description = row[3] ? row[3] : "";
        group.owner_id = std::stoull(row[4]);
        group.created_at = std::stoll(row[5]);
        group.updated_at = std::stoll(row[6]);
        
        // 获取成员列表
        get_group_members(group.group_id, group.members);
        
        groups.push_back(group);
    }
    
    mysql_free_result(result);
    return true;
}

bool Database::get_group_members(uint64_t group_id, std::vector<uint64_t>& members) {
    std::ostringstream sql;
    sql << "SELECT user_id FROM group_members WHERE group_id = " << group_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        members.push_back(std::stoull(row[0]));
    }
    
    mysql_free_result(result);
    return true;
}

bool Database::is_group_member(uint64_t group_id, uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM group_members WHERE group_id = " << group_id 
        << " AND user_id = " << user_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool is_member = row && std::stoi(row[0]) > 0;
    mysql_free_result(result);
    
    return is_member;
}

bool Database::is_group_admin(uint64_t group_id, uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT is_admin FROM group_members WHERE group_id = " << group_id 
        << " AND user_id = " << user_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool is_admin = row && std::stoi(row[0]) == 1;
    mysql_free_result(result);
    
    return is_admin;
}

bool Database::is_group_owner(uint64_t group_id, uint64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT owner_id FROM `groups` WHERE group_id = " << group_id;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool is_owner = row && std::stoull(row[0]) == user_id;
    mysql_free_result(result);
    
    return is_owner;
}

bool Database::set_group_admin(uint64_t group_id, uint64_t user_id, bool is_admin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "UPDATE group_members SET is_admin = " << (is_admin ? 1 : 0)
        << " WHERE group_id = " << group_id << " AND user_id = " << user_id;
    
    return mysql_query(connection_, sql.str().c_str()) == 0;
}

bool Database::transfer_group_owner(uint64_t group_id, uint64_t old_owner_id, uint64_t new_owner_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 更新群主
    std::ostringstream sql1;
    sql1 << "UPDATE `groups` SET owner_id = " << new_owner_id 
         << " WHERE group_id = " << group_id << " AND owner_id = " << old_owner_id;
    
    if (mysql_query(connection_, sql1.str().c_str()) != 0) {
        return false;
    }
    
    // 取消原群主的管理员身份（如果有）
    std::ostringstream sql2;
    sql2 << "UPDATE group_members SET is_admin = 0 WHERE group_id = " << group_id 
         << " AND user_id = " << old_owner_id;
    mysql_query(connection_, sql2.str().c_str());
    
    // 设置新群主为管理员
    std::ostringstream sql3;
    sql3 << "UPDATE group_members SET is_admin = 1 WHERE group_id = " << group_id 
         << " AND user_id = " << new_owner_id;
    
    return mysql_query(connection_, sql3.str().c_str()) == 0;
}

// ==================== 群聊消息相关实现 ====================

bool Database::save_group_message(Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_content = escape_string(message.content);
    std::string escaped_media_url = escape_string(message.media_url);
    std::string escaped_extra = escape_string(message.extra);
    
    std::ostringstream sql;
    sql << "INSERT INTO group_messages (group_id, sender_id, media_type, content, "
        << "media_url, extra, status, created_at) VALUES ("
        << message.group_id << ", " << message.sender_id << ", "
        << static_cast<int>(message.media_type) << ", '" << escaped_content << "', '"
        << escaped_media_url << "', '" << escaped_extra << "', "
        << static_cast<int>(message.status) << ", " << now << ")";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    message.message_id = get_last_insert_id();
    message.created_at = now;
    return true;
}

bool Database::get_group_history(uint64_t group_id, int64_t before_time,
                                 int limit, std::vector<Message>& messages) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream sql;
    sql << "SELECT message_id, sender_id, group_id, media_type, content, media_url, "
        << "extra, status, created_at FROM group_messages "
        << "WHERE group_id = " << group_id << " ";
    
    if (before_time > 0) {
        sql << "AND created_at < " << before_time << " ";
    }
    
    sql << "ORDER BY created_at DESC LIMIT " << limit;
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return false;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Message msg;
        msg.message_id = std::stoull(row[0]);
        msg.sender_id = std::stoull(row[1]);
        msg.group_id = std::stoull(row[2]);
        msg.media_type = static_cast<MediaType>(std::stoi(row[3]));
        msg.content = row[4] ? row[4] : "";
        msg.media_url = row[5] ? row[5] : "";
        msg.extra = row[6] ? row[6] : "";
        msg.status = static_cast<MessageStatus>(std::stoi(row[7]));
        msg.created_at = std::stoll(row[8]);
        messages.push_back(msg);
    }
    
    mysql_free_result(result);
    return true;
}

// ==================== 媒体文件相关实现 ====================

bool Database::save_media_file(uint64_t user_id, const std::string& file_name,
                               const std::string& file_path, MediaType type,
                               uint64_t& file_id, std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = get_current_timestamp();
    std::string escaped_name = escape_string(file_name);
    std::string escaped_path = escape_string(file_path);
    
    // 获取文件大小
    struct stat st;
    uint64_t file_size = stat(file_path.c_str(), &st) == 0 ? st.st_size : 0;
    
    std::ostringstream sql;
    sql << "INSERT INTO media_files (user_id, file_name, file_path, file_size, "
        << "media_type, created_at) VALUES (" << user_id << ", '" << escaped_name 
        << "', '" << escaped_path << "', " << file_size << ", "
        << static_cast<int>(type) << ", " << now << ")";
    
    if (mysql_query(connection_, sql.str().c_str()) != 0) {
        return false;
    }
    
    file_id = get_last_insert_id();
    
    std::ostringstream url_stream;
    url_stream << "/media/" << file_id << "/" << file_name;
    url = url_stream.str();
    
    return true;
}

bool Database::get_media_file(uint64_t file_id, std::string& file_path, MediaType& type) {

    std::lock_guard<std::mutex> lock(mutex_);

    

    std::ostringstream sql;

    sql << "SELECT file_path, media_type FROM media_files WHERE file_id = " << file_id;

    

    if (mysql_query(connection_, sql.str().c_str()) != 0) {

        return false;

    }

    

    MYSQL_RES* result = mysql_store_result(connection_);

    if (!result) return false;

    

    MYSQL_ROW row = mysql_fetch_row(result);

    if (!row) {

        mysql_free_result(result);

        return false;

    }

    

    file_path = row[0] ? row[0] : "";

    type = static_cast<MediaType>(std::stoi(row[1]));

    

    mysql_free_result(result);

    return true;

}



// ==================== 机器人聊天记录相关实现 ====================



bool Database::save_bot_conversation(uint64_t user_id, const std::string& conversation_id,

                                     const std::string& role, const std::string& content) {

    std::lock_guard<std::mutex> lock(mutex_);

    

    int64_t now = get_current_timestamp();

    std::string escaped_conv_id = escape_string(conversation_id);

    std::string escaped_role = escape_string(role);

    std::string escaped_content = escape_string(content);

    int char_count = static_cast<int>(content.size());

    

    std::ostringstream sql;

    sql << "INSERT INTO bot_conversations (user_id, conversation_id, role, content, "

        << "char_count, created_at) VALUES (" << user_id << ", '" << escaped_conv_id 

        << "', '" << escaped_role << "', '" << escaped_content << "', "

        << char_count << ", " << now << ")";

    

    if (mysql_query(connection_, sql.str().c_str()) != 0) {

        std::cerr << "Failed to save bot conversation: " << mysql_error(connection_) << std::endl;

        return false;

    }

    

    return true;

}



bool Database::get_bot_conversation(uint64_t user_id, const std::string& conversation_id,

                                    std::vector<std::pair<std::string, std::string>>& messages,

                                    int limit) {

    std::lock_guard<std::mutex> lock(mutex_);

    

    std::string escaped_conv_id = escape_string(conversation_id);

    

    std::ostringstream sql;

    sql << "SELECT role, content FROM bot_conversations "

        << "WHERE user_id = " << user_id << " AND conversation_id = '" << escaped_conv_id << "' "

        << "ORDER BY created_at ASC LIMIT " << limit;

    

    if (mysql_query(connection_, sql.str().c_str()) != 0) {

        std::cerr << "Failed to get bot conversation: " << mysql_error(connection_) << std::endl;

        return false;

    }

    

    MYSQL_RES* result = mysql_store_result(connection_);

    if (!result) return false;

    

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result))) {

        std::string role = row[0] ? row[0] : "";

        std::string content = row[1] ? row[1] : "";

        messages.push_back({role, content});

    }

    

    mysql_free_result(result);

    return true;

}



bool Database::clear_bot_conversation(uint64_t user_id, const std::string& conversation_id) {

    std::lock_guard<std::mutex> lock(mutex_);

    

    std::string escaped_conv_id = escape_string(conversation_id);

    

    std::ostringstream sql;

    sql << "DELETE FROM bot_conversations "

        << "WHERE user_id = " << user_id << " AND conversation_id = '" << escaped_conv_id << "'";

    

    if (mysql_query(connection_, sql.str().c_str()) != 0) {

        return false;

    }

    

    return true;

}



int Database::get_bot_conversation_char_count(uint64_t user_id, const std::string& conversation_id) {

    std::lock_guard<std::mutex> lock(mutex_);

    

    std::string escaped_conv_id = escape_string(conversation_id);

    

    std::ostringstream sql;

    sql << "SELECT SUM(char_count) FROM bot_conversations "

        << "WHERE user_id = " << user_id << " AND conversation_id = '" << escaped_conv_id << "'";

    

    if (mysql_query(connection_, sql.str().c_str()) != 0) {

        return 0;

    }

    

    MYSQL_RES* result = mysql_store_result(connection_);

    if (!result) return 0;

    

    MYSQL_ROW row = mysql_fetch_row(result);

    int count = 0;

    if (row && row[0]) {

        count = std::stoi(row[0]);

    }

    

    mysql_free_result(result);

    return count;

}



bool Database::create_new_bot_session(uint64_t user_id, std::string& new_conversation_id) {

    // 生成新的会话ID: user_{user_id}_{timestamp}

    int64_t now = get_current_timestamp();

    std::ostringstream ss;

    ss << "user_" << user_id << "_" << now;

    new_conversation_id = ss.str();

    return true;

}



bool Database::get_user_bot_sessions(uint64_t user_id, std::vector<std::string>& session_ids) {

    std::lock_guard<std::mutex> lock(mutex_);

    

    std::ostringstream sql;

    sql << "SELECT DISTINCT conversation_id FROM bot_conversations "

        << "WHERE user_id = " << user_id << " ORDER BY created_at DESC";

    

    if (mysql_query(connection_, sql.str().c_str()) != 0) {

        return false;

    }

    

    MYSQL_RES* result = mysql_store_result(connection_);

    if (!result) return false;

    

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result))) {

        if (row[0]) {

            session_ids.push_back(row[0]);

        }

    }

    

        mysql_free_result(result);

    

        return true;

    

    }

    

    

    

    // ==================== 端到端加密密钥相关实现 ====================

    

    

    

    bool Database::save_user_public_key(uint64_t user_id, const std::string& public_key) {

    

        std::lock_guard<std::mutex> lock(mutex_);

    

        

    

        int64_t now = get_current_timestamp();

    

        std::string escaped_key = escape_string(public_key);

    

        

    

        // 使用 INSERT ... ON DUPLICATE KEY UPDATE

    

        std::ostringstream sql;

    

        sql << "INSERT INTO user_keys (user_id, public_key, created_at, updated_at) VALUES ("

    

            << user_id << ", '" << escaped_key << "', " << now << ", " << now << ") "

    

            << "ON DUPLICATE KEY UPDATE public_key = '" << escaped_key 

    

            << "', updated_at = " << now << ", key_version = key_version + 1";

    

        

    

        if (mysql_query(connection_, sql.str().c_str()) != 0) {

    

            std::cerr << "Failed to save user public key: " << mysql_error(connection_) << std::endl;

    

            return false;

    

        }

    

        

    

        return true;

    

    }

    

    

    

    bool Database::get_user_public_key(uint64_t user_id, std::string& public_key) {

    

        std::lock_guard<std::mutex> lock(mutex_);

    

        

    

        std::ostringstream sql;

    

        sql << "SELECT public_key FROM user_keys WHERE user_id = " << user_id;

    

        

    

        if (mysql_query(connection_, sql.str().c_str()) != 0) {

    

            return false;

    

        }

    

        

    

        MYSQL_RES* result = mysql_store_result(connection_);

    

        if (!result) return false;

    

        

    

        MYSQL_ROW row = mysql_fetch_row(result);

    

        if (!row || !row[0]) {

    

            mysql_free_result(result);

    

            return false;

    

        }

    

        

    

        public_key = row[0];

    

        mysql_free_result(result);

    

        return true;

    

    }

    

    

    

        bool Database::delete_user_key(uint64_t user_id) {

    

    

    

            std::lock_guard<std::mutex> lock(mutex_);

    

    

    

            

    

    

    

            std::ostringstream sql;

    

    

    

            sql << "DELETE FROM user_keys WHERE user_id = " << user_id;

    

    

    

            

    

    

    

            return mysql_query(connection_, sql.str().c_str()) == 0;

    

    

    

        }

    

    

    

    

    

    

    

        bool Database::get_message_sender(uint64_t message_id, uint64_t& sender_id, bool& is_group, uint64_t& group_id) {

    

    

    

            std::lock_guard<std::mutex> lock(mutex_);

    

    

    

            

    

    

    

            // 先查私聊消息表

    

    

    

            std::ostringstream sql;

    

    

    

            sql << "SELECT sender_id, group_id FROM private_messages WHERE message_id = " << message_id;

    

    

    

            

    

    

    

            if (mysql_query(connection_, sql.str().c_str()) == 0) {

    

    

    

                MYSQL_RES* result = mysql_store_result(connection_);

    

    

    

                if (result) {

    

    

    

                    MYSQL_ROW row = mysql_fetch_row(result);

    

    

    

                    if (row) {

    

    

    

                        sender_id = std::stoull(row[0]);

    

    

    

                        is_group = false;

    

    

    

                        group_id = 0;

    

    

    

                        mysql_free_result(result);

    

    

    

                        return true;

    

    

    

                    }

    

    

    

                    mysql_free_result(result);

    

    

    

                }

    

    

    

            }

    

    

    

            

    

    

    

            // 再查群聊消息表

    

    

    

            sql.str("");

    

    

    

            sql << "SELECT sender_id, group_id FROM group_messages WHERE message_id = " << message_id;

    

    

    

            

    

    

    

            if (mysql_query(connection_, sql.str().c_str()) == 0) {

    

    

    

                MYSQL_RES* result = mysql_store_result(connection_);

    

    

    

                if (result) {

    

    

    

                    MYSQL_ROW row = mysql_fetch_row(result);

    

    

    

                    if (row) {

    

    

    

                        sender_id = std::stoull(row[0]);

    

    

    

                        is_group = true;

    

    

    

                        group_id = std::stoull(row[1]);

    

    

    

                        mysql_free_result(result);

    

    

    

                        return true;

    

    

    

                    }

    

    

    

                    mysql_free_result(result);

    

    

    

                }

    

    

    

            }

    

    

    

            

    

    

    

            return false;

    

    

    

        }

    

    

    

    

    

    

    

        bool Database::recall_private_message(uint64_t message_id, uint64_t user_id) {

    

    

    

            std::lock_guard<std::mutex> lock(mutex_);

    

    

    

            

    

    

    

            // 检查时间限制（2分钟内）

    

    

    

            std::ostringstream sql;

    

    

    

            sql << "SELECT sender_id, created_at FROM private_messages WHERE message_id = " << message_id;

    

    

    

            

    

    

    

            if (mysql_query(connection_, sql.str().c_str()) != 0) {

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            MYSQL_RES* result = mysql_store_result(connection_);

    

    

    

            if (!result) return false;

    

    

    

            

    

    

    

            MYSQL_ROW row = mysql_fetch_row(result);

    

    

    

            if (!row) {

    

    

    

                mysql_free_result(result);

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            uint64_t sender_id = std::stoull(row[0]);

    

    

    

            int64_t created_at = std::stoll(row[1]);

    

    

    

            mysql_free_result(result);

    

    

    

            

    

    

    

            // 验证发送者

    

    

    

            if (sender_id != user_id) {

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            // 检查时间限制（2分钟 = 120秒）

    

    

    

            int64_t now = get_current_timestamp();

    

    

    

            if (now - created_at > 120) {

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            // 标记消息为已撤回（更新内容）

    

    

    

            sql.str("");

    

    

    

            sql << "UPDATE private_messages SET content = '[消息已撤回]', media_type = 0, media_url = '' "

    

    

    

                << "WHERE message_id = " << message_id;

    

    

    

            

    

    

    

            return mysql_query(connection_, sql.str().c_str()) == 0;

    

    

    

        }

    

    

    

    

    

    

    

        bool Database::recall_group_message(uint64_t message_id, uint64_t user_id) {

    

    

    

            std::lock_guard<std::mutex> lock(mutex_);

    

    

    

            

    

    

    

            // 获取消息信息和群组信息

    

    

    

            std::ostringstream sql;

    

    

    

            sql << "SELECT sender_id, group_id, created_at FROM group_messages WHERE message_id = " << message_id;

    

    

    

            

    

    

    

            if (mysql_query(connection_, sql.str().c_str()) != 0) {

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            MYSQL_RES* result = mysql_store_result(connection_);

    

    

    

            if (!result) return false;

    

    

    

            

    

    

    

            MYSQL_ROW row = mysql_fetch_row(result);

    

    

    

            if (!row) {

    

    

    

                mysql_free_result(result);

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            uint64_t sender_id = std::stoull(row[0]);

    

    

    

            uint64_t group_id = std::stoull(row[1]);

    

    

    

            int64_t created_at = std::stoll(row[2]);

    

    

    

            mysql_free_result(result);

    

    

    

            

    

    

    

            // 检查权限：发送者或群管理员可以撤回

    

    

    

            bool is_sender = (sender_id == user_id);

    

    

    

            bool is_admin = is_group_admin(group_id, user_id);

    

    

    

            bool is_owner = is_group_owner(group_id, user_id);

    

    

    

            

    

    

    

            if (!is_sender && !is_admin && !is_owner) {

    

    

    

                return false;

    

    

    

            }

    

    

    

            

    

    

    

            // 检查时间限制（群管理员撤回无时间限制，发送者撤回2分钟内）

    

    

    

            int64_t now = get_current_timestamp();

    

    

    

            if (is_sender && !is_admin && !is_owner) {

    

    

    

                if (now - created_at > 120) {

    

    

    

                    return false;

    

    

    

                }

    

    

    

            }

    

    

    

            

    

    

    

            // 标记消息为已撤回

    

    

    

            sql.str("");

    

    

    

            sql << "UPDATE group_messages SET content = '[消息已撤回]', media_type = 0, media_url = '' "

    

    

    

                << "WHERE message_id = " << message_id;

    

    

    

            

    

    

    

            return mysql_query(connection_, sql.str().c_str()) == 0;

    

    

    

        }

    

    

    

    

    

    

    

    } // namespace chat
