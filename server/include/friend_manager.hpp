#ifndef FRIEND_MANAGER_HPP
#define FRIEND_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include "protocol.hpp"
#include "database.hpp"

namespace chat {

class Session;
class Server;

class FriendManager {
public:
    FriendManager(std::shared_ptr<Database> database);
    
    // 发送好友请求
    bool add_friend_request(uint64_t user_id, uint64_t friend_id, std::string& error);
    
    // 接受好友请求
    bool accept_friend_request(uint64_t user_id, uint64_t friend_id, std::string& error);
    
    // 拒绝好友请求
    bool reject_friend_request(uint64_t user_id, uint64_t friend_id, std::string& error);
    
    // 删除好友
    bool remove_friend(uint64_t user_id, uint64_t friend_id, std::string& error);
    
    // 获取好友列表
    std::vector<std::pair<UserInfo, FriendRelation>> get_friend_list(uint64_t user_id);
    
    // 获取好友请求列表
    std::vector<std::pair<UserInfo, FriendRelation>> get_friend_requests(uint64_t user_id);
    
    // 检查是否是好友
    bool is_friend(uint64_t user_id, uint64_t friend_id);
    
    // 设置好友备注
    bool set_friend_remark(uint64_t user_id, uint64_t friend_id,
                           const std::string& remark, std::string& error);
    
    void set_server(std::shared_ptr<Server> server) { server_ = server; }
    
private:
    std::shared_ptr<Database> database_;
    std::shared_ptr<Server> server_;
};

} // namespace chat

#endif // FRIEND_MANAGER_HPP
