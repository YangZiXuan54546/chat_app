#ifndef GROUP_MANAGER_HPP
#define GROUP_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include "protocol.hpp"
#include "database.hpp"

namespace chat {

class Session;
class Server;

class GroupManager {
public:
    GroupManager(std::shared_ptr<Database> database);
    
    // 创建群组
    bool create_group(uint64_t owner_id, const std::string& group_name,
                      const std::string& description, uint64_t& group_id,
                      std::string& error);
    
    // 获取群组信息
    bool get_group_info(uint64_t group_id, GroupInfo& group);
    
    // 更新群组信息
    bool update_group_info(const GroupInfo& group);
    
    // 解散群组
    bool dismiss_group(uint64_t owner_id, uint64_t group_id, std::string& error);
    
    // 加入群组
    bool join_group(uint64_t group_id, uint64_t user_id, std::string& error);
    
    // 离开群组
    bool leave_group(uint64_t group_id, uint64_t user_id, std::string& error);
    
    // 踢出群成员
    bool remove_member(uint64_t operator_id, uint64_t group_id, 
                       uint64_t user_id, std::string& error);
    
    // 获取用户所在的群组列表
    std::vector<GroupInfo> get_user_groups(uint64_t user_id);
    
    // 获取群成员列表
    std::vector<uint64_t> get_group_members(uint64_t group_id);
    
    // 检查是否是群成员
    bool is_member(uint64_t group_id, uint64_t user_id);
    
    // 检查是否是群管理员
    bool is_admin(uint64_t group_id, uint64_t user_id);
    
    // 检查是否是群主
    bool is_owner(uint64_t group_id, uint64_t user_id);
    
    // 设置管理员
    bool set_admin(uint64_t operator_id, uint64_t group_id, 
                   uint64_t user_id, bool is_admin, std::string& error);
    
    void set_server(std::shared_ptr<Server> server) { server_ = server; }
    
private:
    std::shared_ptr<Database> database_;
    std::shared_ptr<Server> server_;
};

} // namespace chat

#endif // GROUP_MANAGER_HPP
