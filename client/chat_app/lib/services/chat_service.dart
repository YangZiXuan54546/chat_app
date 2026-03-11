import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import '../models/models.dart';
import '../models/protocol.dart';
import 'network_service.dart';
import 'message_database.dart';
import 'notification_service.dart';

class ChatService extends ChangeNotifier {
  final NetworkService _network = NetworkService();
  final MessageDatabase _messageDb = MessageDatabase();
  final NotificationService _notificationService = NotificationService();
  
  User? _currentUser;
  bool _isConnected = false;
  bool _isAuthenticated = false;
  
  // 注册状态
  bool _registerSuccess = false;
  String? _registerError;
  int? _registeredUserId;
  
  // 登录状态
  bool _loginSuccess = false;
  String? _loginError;
  
  final Map<int, User> _users = {};
  final Map<int, Group> _groups = {};
  final Map<int, List<Message>> _messages = {}; // key: peerId or -groupId
  final List<Friend> _friends = [];
  final List<Friend> _friendRequests = [];
  final List<Conversation> _conversations = [];
  List<User> _searchResults = [];
  
  // 好友请求相关状态
  bool _friendAddSuccess = false;
  String? _friendAddError;
  
  // 群组创建相关状态
  bool _groupCreateSuccess = false;
  String? _groupCreateError;
  int? _createdGroupId;
  
  // 媒体上传相关状态
  bool _mediaUploading = false;
  double _uploadProgress = 0.0;
  String? _uploadedMediaUrl;
  int? _uploadedFileId;
  String? _uploadError;

  User? get currentUser => _currentUser;
  bool get isConnected => _isConnected;
  bool get isAuthenticated => _isAuthenticated;
  bool get registerSuccess => _registerSuccess;
  String? get registerError => _registerError;
  int? get registeredUserId => _registeredUserId;
  bool get loginSuccess => _loginSuccess;
  String? get loginError => _loginError;
  List<Friend> get friends => _friends;
  List<Friend> get friendRequests => _friendRequests;
  List<Conversation> get conversations => _conversations;
  List<Group> get groups => _groups.values.toList();
  List<User> get searchResults => _searchResults;
  bool get friendAddSuccess => _friendAddSuccess;
  String? get friendAddError => _friendAddError;
  bool get groupCreateSuccess => _groupCreateSuccess;
  String? get groupCreateError => _groupCreateError;
  int? get createdGroupId => _createdGroupId;
  
  // 媒体上传相关 getter
  bool get mediaUploading => _mediaUploading;
  double get uploadProgress => _uploadProgress;
  String? get uploadedMediaUrl => _uploadedMediaUrl;
  int? get uploadedFileId => _uploadedFileId;
  String? get uploadError => _uploadError;
  
  int get currentUserId => _currentUser?.userId ?? 0;

  // 重连状态
  bool _isReconnecting = false;
  bool _reconnectLoginSuccess = false;
  
  bool get isReconnecting => _isReconnecting;
  bool get reconnectLoginSuccess => _reconnectLoginSuccess;
  
  // 当前聊天界面状态（用于避免在自己聊天的页面显示通知）
  int _currentChatPeerId = 0;
  bool _isInChatScreen = false;
  
  int get currentChatPeerId => _currentChatPeerId;
  bool get isInChatScreen => _isInChatScreen;
  
  /// 设置当前聊天界面状态
  void setCurrentChatScreen(int peerId, bool inScreen) {
    _currentChatPeerId = peerId;
    _isInChatScreen = inScreen;
  }

  ChatService() {
    _init();
  }

  void _init() {
    // 初始化通知服务
    _notificationService.init();
    
    _network.addConnectionCallback((connected) {
      final wasConnected = _isConnected;
      _isConnected = connected;
      
      if (!connected && wasConnected && _isAuthenticated) {
        // 连接断开且之前已登录，开始重连
        _isReconnecting = true;
        _reconnectLoginSuccess = false;
        debugPrint('Connection lost, will attempt to reconnect...');
      } else if (connected && _isReconnecting) {
        // 重连成功，等待自动登录
        debugPrint('Reconnected, waiting for auto-login...');
      }
      
      notifyListeners();
    });
    
    // 设置重连登录回调
    _network.setReconnectLoginCallback((username, password) async {
      await _handleReconnectLogin(username, password);
    });

    _network.addMessageCallback(_handleMessage);
  }
  
  /// 处理重连后的自动登录
  Future<void> _handleReconnectLogin(String username, String password) async {
    debugPrint('Auto-login after reconnect: $username');
    
    _reconnectLoginSuccess = false;
    _loginSuccess = false;
    _loginError = null;
    
    _network.send(MessageType.login, {
      'username': username,
      'password': password,
    });
    
    // 等待登录响应
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_loginSuccess || _loginError != null) {
        break;
      }
    }
    
    if (_loginSuccess) {
      debugPrint('Auto-login after reconnect successful');
      _reconnectLoginSuccess = true;
      _isReconnecting = false;
    } else {
      debugPrint('Auto-login after reconnect failed: $_loginError');
      _reconnectLoginSuccess = false;
    }
    
    notifyListeners();
  }

  /// 处理收到的消息
  void _handleMessage(MessageType type, int sequence, Map<String, dynamic> body) {
    switch (type) {
      case MessageType.loginResponse:
        _handleLoginResponse(body);
        break;
      case MessageType.registerResponse:
        _handleRegisterResponse(body);
        break;
      case MessageType.privateMessage:
        _handlePrivateMessage(body);
        break;
      case MessageType.privateMessageResponse:
        _handlePrivateMessageResponse(body);
        break;
      case MessageType.privateHistoryResponse:
        _handlePrivateHistoryResponse(body);
        break;
      case MessageType.groupMessage:
        _handleGroupMessage(body);
        break;
      case MessageType.friendListResponse:
        _handleFriendListResponse(body);
        break;
      case MessageType.friendRequestsResponse:
        _handleFriendRequestsResponse(body);
        break;
      case MessageType.groupListResponse:
        _handleGroupListResponse(body);
        break;
      case MessageType.userSearchResponse:
        _handleUserSearchResponse(body);
        break;
      case MessageType.friendAddResponse:
        _handleFriendAddResponse(body);
        break;
      case MessageType.friendAdd:
        _handleFriendRequestNotification(body);
        break;
      case MessageType.friendAccept:
        _handleFriendAcceptNotification(body);
        break;
      case MessageType.friendAcceptResponse:
        _handleFriendAcceptResponse(body);
        break;
      case MessageType.friendRejectResponse:
        _handleFriendRejectResponse(body);
        break;
      case MessageType.friendRemoveResponse:
        _handleFriendRemoveResponse(body);
        break;
      case MessageType.friendRemarkResponse:
        _handleFriendRemarkResponse(body);
        break;
      case MessageType.groupCreateResponse:
        _handleGroupCreateResponse(body);
        break;
      case MessageType.groupAddMemberResponse:
        _handleGroupAddMemberResponse(body);
        break;
      case MessageType.groupSetAdminResponse:
        _handleGroupSetAdminResponse(body);
        break;
      case MessageType.groupTransferOwnerResponse:
        _handleGroupTransferOwnerResponse(body);
        break;
      case MessageType.groupRemoveMemberResponse:
        _handleGroupRemoveMemberResponse(body);
        break;
      case MessageType.groupLeaveResponse:
        _handleGroupLeaveResponse(body);
        break;
      case MessageType.groupDismissResponse:
        _handleGroupDismissResponse(body);
        break;
      case MessageType.groupMembersResponse:
        _handleGroupMembersResponse(body);
        break;
      case MessageType.mediaUploadResponse:
        _handleMediaUploadResponse(body);
        break;
      default:
        break;
    }
  }

  /// 连接服务器
  Future<bool> connect(String host, int port) async {
    return await _network.connect(host, port);
  }

  /// 断开连接
  void disconnect() {
    _network.disconnect();
    _isAuthenticated = false;
    _currentUser = null;
    notifyListeners();
  }

  /// 注册
  Future<bool> register(String username, String password, String nickname) async {
    // 重置状态
    _registerSuccess = false;
    _registerError = null;
    _registeredUserId = null;
    
    _network.send(MessageType.register, {
      'username': username,
      'password': password,
      'nickname': nickname,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_registerSuccess || _registerError != null) {
        return _registerSuccess;
      }
    }
    
    _registerError = 'Registration timeout';
    return false;
  }

  /// 登录
  Future<bool> login(String username, String password) async {
    // 重置状态
    _loginSuccess = false;
    _loginError = null;
    
    _network.send(MessageType.login, {
      'username': username,
      'password': password,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_loginSuccess || _loginError != null) {
        // 登录成功后保存凭据用于重连
        if (_loginSuccess) {
          _network.saveCredentials(username, password);
        }
        return _loginSuccess;
      }
    }
    
    _loginError = 'Login timeout';
    return false;
  }

  /// 登出
  void logout() {
    _network.send(MessageType.logout, {});
    _network.clearCredentials();
    _isAuthenticated = false;
    _isReconnecting = false;
    _reconnectLoginSuccess = false;
    _currentUser = null;
    notifyListeners();
  }

  /// 处理登录响应
  void _handleLoginResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        _currentUser = User.fromJson(data['user_info'] as Map<String, dynamic>);
        _isAuthenticated = true;
        _loginSuccess = true;
        _loginError = null;
        
        // 如果是重连登录成功
        if (_isReconnecting) {
          _reconnectLoginSuccess = true;
          _isReconnecting = false;
          debugPrint('Reconnect login successful');
        }
        
        // 加载好友列表
        _network.send(MessageType.friendList, {});
        // 加载好友请求列表
        _network.send(MessageType.friendRequests, {});
        // 加载群组列表
        _network.send(MessageType.groupList, {});
      }
    } else {
      _loginSuccess = false;
      _loginError = body['message'] as String? ?? 'Login failed';
      
      // 重连登录失败
      if (_isReconnecting) {
        _reconnectLoginSuccess = false;
        debugPrint('Reconnect login failed: $_loginError');
      }
    }
    notifyListeners();
  }

  /// 处理注册响应
  void _handleRegisterResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      _registerSuccess = true;
      _registerError = null;
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        _registeredUserId = data['user_id'] as int?;
      }
    } else {
      _registerSuccess = false;
      _registerError = body['message'] as String? ?? 'Registration failed';
    }
    notifyListeners();
  }

  /// 发送私聊消息
  void sendPrivateMessage(int receiverId, String content, {int mediaType = 0, String mediaUrl = ''}) {
    _network.send(MessageType.privateMessage, {
      'receiver_id': receiverId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
    });
  }

  /// 处理私聊消息
  void _handlePrivateMessage(Map<String, dynamic> body) {
    final message = Message.fromJson(body);
    final key = message.groupId > 0 ? -message.groupId : 
                 (message.senderId == currentUserId ? message.receiverId : message.senderId);
    
    if (!_messages.containsKey(key)) {
      _messages[key] = [];
    }
    _messages[key]!.add(message);
    
    // 保存到本地数据库
    _messageDb.saveMessage(message);
    
    _updateConversation(message);
    
    // 发送通知（如果不在当前聊天界面且不是自己发的消息）
    if (message.senderId != currentUserId) {
      final peerId = message.senderId;
      if (!_isInChatScreen || _currentChatPeerId != peerId) {
        // 获取发送者名称
        String senderName = '用户';
        if (_users.containsKey(peerId)) {
          senderName = _users[peerId]!.nickname.isNotEmpty 
              ? _users[peerId]!.nickname 
              : _users[peerId]!.username;
        }
        
        // 根据消息类型显示不同的通知内容
        String notificationBody;
        if (message.isImage) {
          notificationBody = '[图片]';
        } else if (message.isFile) {
          notificationBody = '[文件] ${message.content}';
        } else {
          notificationBody = message.content;
        }
        
        _notificationService.showMessageNotification(
          id: peerId,
          title: senderName,
          body: notificationBody,
          payload: 'chat_$peerId',
        );
      }
    }
    
    notifyListeners();
  }

  /// 处理发送私聊消息的响应
  void _handlePrivateMessageResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 消息发送成功，更新本地消息
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final message = Message.fromJson(data);
        final key = message.receiverId;
        
        if (!_messages.containsKey(key)) {
          _messages[key] = [];
        }
        // 查找并更新临时消息（如果有的话）
        final existingIndex = _messages[key]!.indexWhere(
          (m) => m.senderId == message.senderId && 
                 m.receiverId == message.receiverId && 
                 m.messageId == 0
        );
        if (existingIndex >= 0) {
          _messages[key]![existingIndex] = message;
        } else {
          _messages[key]!.add(message);
        }
        
        // 保存到本地数据库
        _messageDb.saveMessage(message);
        
        _updateConversation(message);
        notifyListeners();
      }
    }
  }

  /// 处理私聊历史消息响应
  void _handlePrivateHistoryResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final messagesJson = data['messages'] as List<dynamic>? ?? [];
        final serverMessages = <Message>[];
        int peerId = 0;
        
        for (final item in messagesJson) {
          final message = Message.fromJson(item as Map<String, dynamic>);
          serverMessages.add(message);
          peerId = message.senderId == currentUserId ? message.receiverId : message.senderId;
          
          final key = message.senderId == currentUserId ? message.receiverId : message.senderId;
          
          if (!_messages.containsKey(key)) {
            _messages[key] = [];
          }
          // 避免重复添加
          if (!_messages[key]!.any((m) => m.messageId == message.messageId)) {
            _messages[key]!.insert(0, message);
          }
        }
        
        // 批量保存到本地数据库
        if (peerId > 0 && serverMessages.isNotEmpty) {
          _messageDb.saveMessages(peerId, serverMessages, isGroup: false);
        }
        
        notifyListeners();
      }
    }
  }

  /// 发送群聊消息
  void sendGroupMessage(int groupId, String content, {int mediaType = 0, String mediaUrl = ''}) {
    _network.send(MessageType.groupMessage, {
      'group_id': groupId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
    });
  }

  /// 处理群聊消息
  void _handleGroupMessage(Map<String, dynamic> body) {
    final message = Message.fromJson(body);
    final key = -message.groupId;
    
    if (!_messages.containsKey(key)) {
      _messages[key] = [];
    }
    _messages[key]!.add(message);
    
    // 保存到本地数据库
    _messageDb.saveMessage(message);
    
    _updateConversation(message);
    
    // 发送通知（如果不在当前群聊界面且不是自己发的消息）
    if (message.senderId != currentUserId) {
      final groupId = message.groupId;
      if (!_isInChatScreen || _currentChatPeerId != groupId) {
        // 获取群组名称
        String groupName = _groups.containsKey(groupId) 
            ? _groups[groupId]!.groupName 
            : '群聊';
        
        // 获取发送者名称
        String senderName = '用户';
        if (_users.containsKey(message.senderId)) {
          senderName = _users[message.senderId]!.nickname.isNotEmpty 
              ? _users[message.senderId]!.nickname 
              : _users[message.senderId]!.username;
        }
        
        // 根据消息类型显示不同的通知内容
        String notificationBody;
        if (message.isImage) {
          notificationBody = '$senderName: [图片]';
        } else if (message.isFile) {
          notificationBody = '$senderName: [文件] ${message.content}';
        } else {
          notificationBody = '$senderName: ${message.content}';
        }
        
        _notificationService.showMessageNotification(
          id: groupId + 10000, // 群组ID加偏移避免与私聊冲突
          title: groupName,
          body: notificationBody,
          payload: 'group_$groupId',
        );
      }
    }
    
    notifyListeners();
  }

  /// 获取消息列表
  List<Message> getMessages(int peerId, {bool isGroup = false}) {
    final key = isGroup ? -peerId : peerId;
    return _messages[key] ?? [];
  }
  
  /// 从本地数据库加载消息
  Future<void> loadLocalMessages(int peerId, {bool isGroup = false}) async {
    final messages = await _messageDb.getMessages(peerId, isGroup: isGroup);
    final key = isGroup ? -peerId : peerId;
    
    if (messages.isNotEmpty) {
      _messages[key] = messages;
      notifyListeners();
    }
  }

  /// 加载历史消息
  void loadHistory(int peerId, {bool isGroup = false, int beforeTime = 0}) {
    if (isGroup) {
      _network.send(MessageType.groupHistory, {
        'group_id': peerId,
        'before_time': beforeTime,
        'limit': 50,
      });
    } else {
      _network.send(MessageType.privateHistory, {
        'peer_id': peerId,
        'before_time': beforeTime,
        'limit': 50,
      });
    }
  }

  /// 添加好友
  Future<bool> addFriend(int friendId) async {
    // 重置状态
    _friendAddSuccess = false;
    _friendAddError = null;
    
    _network.send(MessageType.friendAdd, {
      'friend_id': friendId,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_friendAddSuccess || _friendAddError != null) {
        return _friendAddSuccess;
      }
    }
    
    _friendAddError = 'Request timeout';
    return false;
  }

  /// 接受好友请求
  void acceptFriend(int friendId) {
    _network.send(MessageType.friendAccept, {
      'friend_id': friendId,
    });
    _loadFriendRequests();
  }

  /// 拒绝好友请求
  void rejectFriend(int friendId) {
    _network.send(MessageType.friendReject, {
      'friend_id': friendId,
    });
    _loadFriendRequests();
  }

  /// 删除好友
  void removeFriend(int friendId) {
    _network.send(MessageType.friendRemove, {
      'friend_id': friendId,
    });
    _loadFriendList();
  }

  /// 加载好友列表
  void _loadFriendList() {
    _network.send(MessageType.friendList, {});
  }

  /// 加载好友请求
  void _loadFriendRequests() {
    _network.send(MessageType.friendRequests, {});
  }

  /// 处理好友列表响应
  void _handleFriendListResponse(Map<String, dynamic> body) {
    final data = body['data'] as Map<String, dynamic>?;
    if (data == null) return;
    
    final friendsJson = data['friends'] as List<dynamic>? ?? [];
    _friends.clear();
    
    for (final item in friendsJson) {
      final friendData = item as Map<String, dynamic>;
      final friend = Friend.fromJson(friendData);
      _friends.add(friend);
      _users[friend.user.userId] = friend.user;
    }
    
    notifyListeners();
  }

  /// 处理好友请求响应
  void _handleFriendRequestsResponse(Map<String, dynamic> body) {
    final data = body['data'] as Map<String, dynamic>?;
    if (data == null) return;
    
    final requestsJson = data['requests'] as List<dynamic>? ?? [];
    _friendRequests.clear();
    
    for (final item in requestsJson) {
      final friendData = item as Map<String, dynamic>;
      final friend = Friend.fromJson(friendData);
      _friendRequests.add(friend);
    }
    
    notifyListeners();
  }

  /// 创建群组
  Future<bool> createGroup(String groupName, String description) async {
    // 重置状态
    _groupCreateSuccess = false;
    _groupCreateError = null;
    _createdGroupId = null;
    
    _network.send(MessageType.groupCreate, {
      'group_name': groupName,
      'description': description,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_groupCreateSuccess || _groupCreateError != null) {
        return _groupCreateSuccess;
      }
    }
    
    _groupCreateError = 'Create group timeout';
    return false;
  }
  
  /// 邀请成员加入群组
  void inviteGroupMembers(int groupId, List<int> memberIds) {
    for (final memberId in memberIds) {
      _network.send(MessageType.groupAddMember, {
        'group_id': groupId,
        'user_id': memberId,
      });
    }
    // 刷新群组列表
    _network.send(MessageType.groupList, {});
  }
  
  /// 设置/取消管理员
  void setGroupAdmin(int groupId, int userId, bool isAdmin) {
    _network.send(MessageType.groupSetAdmin, {
      'group_id': groupId,
      'user_id': userId,
      'is_admin': isAdmin,
    });
  }
  
  /// 转让群主
  void transferGroupOwner(int groupId, int newOwnerId) {
    _network.send(MessageType.groupTransferOwner, {
      'group_id': groupId,
      'new_owner_id': newOwnerId,
    });
  }
  
  /// 踢出群成员
  void removeGroupMember(int groupId, int userId) {
    _network.send(MessageType.groupRemoveMember, {
      'group_id': groupId,
      'user_id': userId,
    });
  }
  
  /// 退出群组
  void leaveGroup(int groupId) {
    _network.send(MessageType.groupLeave, {
      'group_id': groupId,
    });
  }
  
  /// 解散群组
  void dismissGroup(int groupId) {
    _network.send(MessageType.groupDismiss, {
      'group_id': groupId,
    });
  }
  
  /// 获取群成员列表
  void getGroupMembers(int groupId) {
    _network.send(MessageType.groupMembers, {
      'group_id': groupId,
    });
  }
  
  /// 处理群组创建响应
  void _handleGroupCreateResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      _groupCreateSuccess = true;
      _groupCreateError = null;
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        _createdGroupId = data['group_id'] as int?;
      }
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    } else {
      _groupCreateSuccess = false;
      _groupCreateError = body['message'] as String? ?? 'Failed to create group';
    }
    notifyListeners();
  }
  
  /// 处理添加群成员响应
  void _handleGroupAddMemberResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    }
    notifyListeners();
  }

  /// 加入群组
  void joinGroup(int groupId) {
    _network.send(MessageType.groupJoin, {
      'group_id': groupId,
    });
  }

  /// 处理群组列表响应
  void _handleGroupListResponse(Map<String, dynamic> body) {
    final data = body['data'] as Map<String, dynamic>?;
    if (data == null) return;
    
    final groupsJson = data['groups'] as List<dynamic>? ?? [];
    
    for (final item in groupsJson) {
      final group = Group.fromJson(item as Map<String, dynamic>);
      _groups[group.groupId] = group;
    }
    
    notifyListeners();
  }

  /// 更新会话列表
  void _updateConversation(Message message) {
    // 找到或创建会话
    final peerId = message.groupId > 0 ? message.senderId : 
                   (message.senderId == currentUserId ? message.receiverId : message.senderId);
    final groupId = message.groupId;
    
    int index = _conversations.indexWhere((c) => 
      c.groupId > 0 ? c.groupId == groupId : c.peerId == peerId
    );
    
    if (index >= 0) {
      // 更新现有会话
      _conversations.removeAt(index);
    }
    
    // 创建新会话并插入到开头
    _conversations.insert(0, Conversation(
      peerId: peerId,
      groupId: groupId,
      peer: _users[peerId],
      group: groupId > 0 ? _groups[groupId] : null,
      lastMessage: message,
      unreadCount: message.senderId != currentUserId ? 1 : 0,
    ));
  }

  /// 搜索用户
  void searchUsers(String keyword) {
    _network.send(MessageType.userSearch, {
      'keyword': keyword,
      'limit': 20,
    });
  }

  /// 处理用户搜索响应
  void _handleUserSearchResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final usersJson = data['users'] as List<dynamic>? ?? [];
        _searchResults = usersJson
            .map((json) => User.fromJson(json as Map<String, dynamic>))
            .toList();
        notifyListeners();
      }
    }
  }

  /// 处理添加好友响应
  void _handleFriendAddResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      _friendAddSuccess = true;
      _friendAddError = null;
    } else {
      _friendAddSuccess = false;
      _friendAddError = body['message'] as String? ?? 'Failed to add friend';
    }
    notifyListeners();
  }

  /// 处理好友请求通知（被他人添加为好友）
  void _handleFriendRequestNotification(Map<String, dynamic> body) {
    // 刷新好友请求列表
    _loadFriendRequests();
    
    // 发送好友请求通知
    final fromUserId = body['from_user_id'] as int?;
    final fromUsername = body['from_username'] as String? ?? '';
    final fromNickname = body['from_nickname'] as String?;
    
    if (fromUserId != null) {
      _notificationService.showFriendRequestNotification(
        id: fromUserId + 20000, // 加偏移避免与其他通知冲突
        username: fromUsername,
        nickname: fromNickname,
      );
    }
    
    notifyListeners();
  }

  /// 处理好友请求被接受的通知（对方接受了你的好友请求）
  void _handleFriendAcceptNotification(Map<String, dynamic> body) {
    // 刷新好友列表和好友请求列表
    _loadFriendList();
    _loadFriendRequests();
    debugPrint('Friend request accepted: $body');
    notifyListeners();
  }

  /// 处理接受好友请求响应
  void _handleFriendAcceptResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新好友列表和好友请求列表
      _loadFriendList();
      _loadFriendRequests();
    }
    notifyListeners();
  }

  /// 处理拒绝好友请求响应
  void _handleFriendRejectResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新好友请求列表
      _loadFriendRequests();
    }
    notifyListeners();
  }

  /// 处理删除好友响应
  void _handleFriendRemoveResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新好友列表
      _loadFriendList();
    }
    notifyListeners();
  }

  /// 处理设置好友备注响应
  void _handleFriendRemarkResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新好友列表
      _loadFriendList();
    }
    notifyListeners();
  }

  /// 设置好友备注
  void setFriendRemark(int friendId, String remark) {
    _network.send(MessageType.friendRemark, {
      'friend_id': friendId,
      'remark': remark,
    });
  }

  /// 加载好友请求列表
  void getUserInfo(int userId) {
    _network.send(MessageType.userInfo, {
      'user_id': userId,
    });
  }

  /// 更新用户信息
  void updateUserInfo({String? nickname, String? signature, String? avatarUrl}) {
    final data = <String, dynamic>{};
    if (nickname != null) data['nickname'] = nickname;
    if (signature != null) data['signature'] = signature;
    if (avatarUrl != null) data['avatar_url'] = avatarUrl;
    
    _network.send(MessageType.userUpdate, data);
  }
  
  /// 处理设置管理员响应
  void _handleGroupSetAdminResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理转让群主响应
  void _handleGroupTransferOwnerResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理踢出成员响应
  void _handleGroupRemoveMemberResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理退出群组响应
  void _handleGroupLeaveResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理解散群组响应
  void _handleGroupDismissResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(MessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理群成员列表响应
  void _handleGroupMembersResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final membersJson = data['members'] as List<dynamic>? ?? [];
        // 存储群成员信息到本地
        for (final item in membersJson) {
          final memberData = item as Map<String, dynamic>;
          final user = User.fromJson(memberData);
          _users[user.userId] = user;
        }
      }
    }
    notifyListeners();
  }
  
  // ==================== 媒体上传相关 ====================
  
  /// 上传媒体文件
  /// 返回上传成功后的文件 URL
  Future<String?> uploadMedia(File file, {int mediaType = 1}) async {
    // 重置状态
    _mediaUploading = true;
    _uploadProgress = 0.0;
    _uploadedMediaUrl = null;
    _uploadedFileId = null;
    _uploadError = null;
    notifyListeners();
    
    try {
      // 读取文件
      final bytes = await file.readAsBytes();
      final base64Data = base64Encode(bytes);
      final fileName = file.path.split('/').last;
      
      _uploadProgress = 0.5;
      notifyListeners();
      
      // 发送上传请求
      _network.send(MessageType.mediaUpload, {
        'file_name': fileName,
        'file_data': base64Data,
        'media_type': mediaType,
      });
      
      // 等待响应（最多30秒，因为文件可能较大）
      for (int i = 0; i < 300; i++) {
        await Future.delayed(const Duration(milliseconds: 100));
        if (_uploadedMediaUrl != null || _uploadError != null) {
          _mediaUploading = false;
          _uploadProgress = 1.0;
          notifyListeners();
          return _uploadedMediaUrl;
        }
      }
      
      _mediaUploading = false;
      _uploadError = 'Upload timeout';
      notifyListeners();
      return null;
    } catch (e) {
      _mediaUploading = false;
      _uploadError = 'Upload failed: $e';
      notifyListeners();
      return null;
    }
  }
  
  /// 处理媒体上传响应
  void _handleMediaUploadResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        _uploadedFileId = data['file_id'] as int?;
        _uploadedMediaUrl = data['url'] as String?;
        _uploadError = null;
      }
    } else {
      _uploadedMediaUrl = null;
      _uploadedFileId = null;
      _uploadError = body['message'] as String? ?? 'Upload failed';
    }
    notifyListeners();
  }
  
  /// 发送图片消息
  Future<bool> sendImageMessage(int peerId, File imageFile, {bool isGroup = false}) async {
    // 上传图片
    final mediaUrl = await uploadMedia(imageFile, mediaType: MediaType.image.value);
    
    if (mediaUrl == null) {
      return false;
    }
    
    // 发送消息
    if (isGroup) {
      sendGroupMessage(peerId, '', mediaType: MediaType.image.value, mediaUrl: mediaUrl);
    } else {
      sendPrivateMessage(peerId, '', mediaType: MediaType.image.value, mediaUrl: mediaUrl);
    }
    
    return true;
  }
  
  /// 发送文件消息
  /// [peerId] 接收者ID或群组ID
  /// [file] 文件对象
  /// [fileName] 文件名
  /// [isGroup] 是否群聊
  Future<bool> sendFileMessage(int peerId, File file, String fileName, {bool isGroup = false}) async {
    // 上传文件
    final mediaUrl = await uploadMedia(file, mediaType: MediaType.file.value);
    
    if (mediaUrl == null) {
      return false;
    }
    
    // 发送消息，content 存储文件名
    if (isGroup) {
      sendGroupMessage(peerId, fileName, mediaType: MediaType.file.value, mediaUrl: mediaUrl);
    } else {
      sendPrivateMessage(peerId, fileName, mediaType: MediaType.file.value, mediaUrl: mediaUrl);
    }
    
    return true;
  }
  
  /// 格式化文件大小
  static String formatFileSize(int bytes) {
    if (bytes < 1024) {
      return '$bytes B';
    } else if (bytes < 1024 * 1024) {
      return '${(bytes / 1024).toStringAsFixed(1)} KB';
    } else if (bytes < 1024 * 1024 * 1024) {
      return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
    } else {
      return '${(bytes / (1024 * 1024 * 1024)).toStringAsFixed(1)} GB';
    }
  }
}
