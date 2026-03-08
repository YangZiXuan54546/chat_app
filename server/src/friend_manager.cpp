#include "friend_manager.hpp"
#include "server.hpp"

namespace chat {

FriendManager::FriendManager(std::shared_ptr<Database> database)
    : database_(database) {
}

bool FriendManager::add_friend_request(uint64_t user_id, uint64_t friend_id, std::string& error) {
    if (user_id == friend_id) {
        error = "Cannot add yourself as friend";
        return false;
    }
    
    // 检查目标用户是否存在
    UserInfo target_user;
    if (!database_->get_user_by_id(friend_id, target_user)) {
        error = "User not found";
        return false;
    }
    
    // 检查是否已经是好友
    if (database_->is_friend(user_id, friend_id)) {
        error = "Already friends";
        return false;
    }
    
    // 检查是否已经发送过请求
    auto requests = get_friend_requests(friend_id);
    for (const auto& [user, relation] : requests) {
        if (user.user_id == user_id) {
            error = "Friend request already sent";
            return false;
        }
    }
    
    if (!database_->add_friend_request(user_id, friend_id)) {
        error = "Failed to send friend request";
        return false;
    }
    
    return true;
}

bool FriendManager::accept_friend_request(uint64_t user_id, uint64_t friend_id, std::string& error) {
    // 检查是否有这个好友请求
    auto requests = get_friend_requests(user_id);
    bool found = false;
    for (const auto& [user, relation] : requests) {
        if (user.user_id == friend_id) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        error = "No such friend request";
        return false;
    }
    
    if (!database_->accept_friend_request(user_id, friend_id)) {
        error = "Failed to accept friend request";
        return false;
    }
    
    return true;
}

bool FriendManager::reject_friend_request(uint64_t user_id, uint64_t friend_id, std::string& error) {
    if (!database_->reject_friend_request(user_id, friend_id)) {
        error = "Failed to reject friend request";
        return false;
    }
    
    return true;
}

bool FriendManager::remove_friend(uint64_t user_id, uint64_t friend_id, std::string& error) {
    if (!database_->is_friend(user_id, friend_id)) {
        error = "Not friends";
        return false;
    }
    
    if (!database_->remove_friend(user_id, friend_id)) {
        error = "Failed to remove friend";
        return false;
    }
    
    return true;
}

std::vector<std::pair<UserInfo, FriendRelation>> FriendManager::get_friend_list(uint64_t user_id) {
    std::vector<std::pair<UserInfo, FriendRelation>> friends;
    database_->get_friend_list(user_id, friends);
    return friends;
}

std::vector<std::pair<UserInfo, FriendRelation>> FriendManager::get_friend_requests(uint64_t user_id) {
    std::vector<std::pair<UserInfo, FriendRelation>> requests;
    database_->get_friend_requests(user_id, requests);
    return requests;
}

bool FriendManager::is_friend(uint64_t user_id, uint64_t friend_id) {
    return database_->is_friend(user_id, friend_id);
}

bool FriendManager::set_friend_remark(uint64_t user_id, uint64_t friend_id,
                                      const std::string& remark, std::string& error) {
    if (!database_->is_friend(user_id, friend_id)) {
        error = "Not friends";
        return false;
    }
    
    if (!database_->set_friend_remark(user_id, friend_id, remark)) {
        error = "Failed to set remark";
        return false;
    }
    
    return true;
}

} // namespace chat
