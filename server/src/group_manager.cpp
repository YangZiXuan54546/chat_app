#include "group_manager.hpp"
#include "server.hpp"

namespace chat {

GroupManager::GroupManager(std::shared_ptr<Database> database)
    : database_(database) {
}

bool GroupManager::create_group(uint64_t owner_id, const std::string& group_name,
                                const std::string& description, uint64_t& group_id,
                                std::string& error) {
    if (group_name.empty()) {
        error = "Group name is required";
        return false;
    }
    
    if (!database_->create_group(group_name, owner_id, description, group_id)) {
        error = "Failed to create group";
        return false;
    }
    
    return true;
}

bool GroupManager::get_group_info(uint64_t group_id, GroupInfo& group) {
    return database_->get_group_by_id(group_id, group);
}

bool GroupManager::update_group_info(const GroupInfo& group) {
    return database_->update_group(group);
}

bool GroupManager::dismiss_group(uint64_t owner_id, uint64_t group_id, std::string& error) {
    if (!is_owner(group_id, owner_id)) {
        error = "Only group owner can dismiss the group";
        return false;
    }
    
    if (!database_->dismiss_group(group_id)) {
        error = "Failed to dismiss group";
        return false;
    }
    
    return true;
}

bool GroupManager::join_group(uint64_t group_id, uint64_t user_id, std::string& error) {
    // 检查群组是否存在
    GroupInfo group;
    if (!database_->get_group_by_id(group_id, group)) {
        error = "Group not found";
        return false;
    }
    
    // 检查是否已是成员
    if (database_->is_group_member(group_id, user_id)) {
        error = "Already a member";
        return false;
    }
    
    if (!database_->add_group_member(group_id, user_id)) {
        error = "Failed to join group";
        return false;
    }
    
    return true;
}

bool GroupManager::leave_group(uint64_t group_id, uint64_t user_id, std::string& error) {
    // 群主不能退群
    if (is_owner(group_id, user_id)) {
        error = "Group owner cannot leave, please dismiss the group instead";
        return false;
    }
    
    if (!database_->remove_group_member(group_id, user_id)) {
        error = "Failed to leave group";
        return false;
    }
    
    return true;
}

bool GroupManager::remove_member(uint64_t operator_id, uint64_t group_id,
                                 uint64_t user_id, std::string& error) {
    // 检查操作者权限
    if (!is_owner(group_id, operator_id) && !is_admin(group_id, operator_id)) {
        error = "No permission to remove member";
        return false;
    }
    
    // 不能踢群主
    if (is_owner(group_id, user_id)) {
        error = "Cannot remove group owner";
        return false;
    }
    
    // 管理员不能踢管理员，只有群主可以
    if (is_admin(group_id, user_id) && !is_owner(group_id, operator_id)) {
        error = "No permission to remove admin";
        return false;
    }
    
    if (!database_->remove_group_member(group_id, user_id)) {
        error = "Failed to remove member";
        return false;
    }
    
    return true;
}

std::vector<GroupInfo> GroupManager::get_user_groups(uint64_t user_id) {
    std::vector<GroupInfo> groups;
    database_->get_user_groups(user_id, groups);
    return groups;
}

std::vector<uint64_t> GroupManager::get_group_members(uint64_t group_id) {
    std::vector<uint64_t> members;
    database_->get_group_members(group_id, members);
    return members;
}

bool GroupManager::is_member(uint64_t group_id, uint64_t user_id) {
    return database_->is_group_member(group_id, user_id);
}

bool GroupManager::is_admin(uint64_t group_id, uint64_t user_id) {
    return database_->is_group_admin(group_id, user_id);
}

bool GroupManager::is_owner(uint64_t group_id, uint64_t user_id) {
    return database_->is_group_owner(group_id, user_id);
}

bool GroupManager::set_admin(uint64_t operator_id, uint64_t group_id,
                             uint64_t user_id, bool is_admin, std::string& error) {
    // 只有群主可以设置管理员
    if (!is_owner(group_id, operator_id)) {
        error = "Only group owner can set admin";
        return false;
    }
    
    // 检查目标用户是否是群成员
    if (!is_member(group_id, user_id)) {
        error = "User is not a group member";
        return false;
    }
    
    if (!database_->add_group_member(group_id, user_id, is_admin)) {
        error = "Failed to set admin";
        return false;
    }
    
    return true;
}

} // namespace chat
