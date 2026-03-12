import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import '../models/models.dart';
import '../models/protocol.dart';
import 'network_service.dart';
import 'message_database.dart';
import 'notification_service.dart';
import 'e2ee_service.dart';

class ChatService extends ChangeNotifier {
  final NetworkService _network = NetworkService();
  final MessageDatabase _messageDb = MessageDatabase();
  final NotificationService _notificationService = NotificationService();
  final E2EEService _e2ee = E2EEService();
  
  User? _currentUser;
  bool _isConnected = false;
  bool _isAuthenticated = false;
  String _mediaServerHost = "10.0.2.2:8889"; // Android模拟器访问宿主机
  
  // 替换URL中的localhost为实际的媒体服务器地址
  String _fixMediaUrl(String url) {
    if (url.contains("localhost")) {
      return url.replaceFirst(RegExp(r"http://localhost:\d+"), "http://$_mediaServerHost");
    }
    return url;
  }

  void setMediaServerHost(String host) {
    _mediaServerHost = host;
  }

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
    // 初始化通知服务（非阻塞）
    _notificationService.init().catchError((e) {
      debugPrint('NotificationService init error: $e');
    });
    
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
    
    _reconnectLoginSuccess = _loginSuccess;
    if (_loginSuccess) {
      debugPrint('Reconnect login successful');
    } else {
      debugPrint('Reconnect login failed: $_loginError');
    }
  }

  // 用户数据
  final Map<int, User> _users = {};
  final Map<int, Group> _groups = {};
  final Map<int, List<Message>> _messages = {};
  final List<Conversation> _conversations = [];
  final Map<int, int> _unreadCounts = {};
  List<User> _searchResults = [];

  User? get currentUser => _currentUser;
  bool get isConnected => _isConnected;
  bool get isAuthenticated => _isAuthenticated;
  int get currentUserId => _currentUser?.userId ?? 0;
  List<User> get searchResults => _searchResults;
  List<Conversation> get conversations => _conversations;

  // 登录状态
  bool _loginSuccess = false;
  String? _loginError;
  bool get loginSuccess => _loginSuccess;
  String? get loginError => _loginError;

  // 注册状态
  bool _registerSuccess = false;
  String? _registerError;
  int? _registeredUserId;
  bool get registerSuccess => _registerSuccess;
  String? get registerError => _registerError;
  int? get registeredUserId => _registeredUserId;

  // 上传状态
  bool _mediaUploading = false;
  double _uploadProgress = 0.0;
  String? _uploadedMediaUrl;
  int? _uploadedFileId;
  String? _uploadError;
  bool get mediaUploading => _mediaUploading;
  double get uploadProgress => _uploadProgress;
  String? get uploadedMediaUrl => _uploadedMediaUrl;
  int? get uploadedFileId => _uploadedFileId;
  String? get uploadError => _uploadError;

  // 用户相关
  User? getUser(int userId) => _users[userId];
  Group? getGroup(int groupId) => _groups[groupId];

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
    
    // 检查网络连接
    if (!_isConnected) {
      _registerError = '未连接到服务器';
      notifyListeners();
      return false;
    }
    
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
    
    _registerError = '注册超时';
    return false;
  }

  /// 登录
  Future<bool> login(String username, String password) async {
    // 重置状态
    _loginSuccess = false;
    _loginError = null;
    
    // 检查网络连接
    if (!_isConnected) {
      _loginError = '未连接到服务器';
      notifyListeners();
      return false;
    }
    
    // 保存凭据用于重连
    _network.saveCredentials(username, password);
    
    _network.send(MessageType.login, {
      'username': username,
      'password': password,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_loginSuccess || _loginError != null) {
        return _loginSuccess;
      }
    }
    
    _loginError = '登录超时';
    return false;
  }

  /// 登出
  void logout() {
    _network.send(MessageType.logout, {});
    _network.clearCredentials();
    _isAuthenticated = false;
    _currentUser = null;
    _messages.clear();
    _conversations.clear();
    _users.clear();
    _groups.clear();
    notifyListeners();
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

  // ==================== 消息处理 ====================

  void _handleMessage(MessageType type, int sequence, Map<String, dynamic> body) {
    switch (type) {
      case MessageType.registerResponse:
        _handleRegisterResponse(body);
        break;
      case MessageType.loginResponse:
        _handleLoginResponse(body);
        break;
      case MessageType.logoutResponse:
        _isAuthenticated = false;
        notifyListeners();
        break;
      case MessageType.userInfoResponse:
        _handleUserInfoResponse(body);
        break;
      case MessageType.userSearchResponse:
        _handleUserSearchResponse(body);
        break;
      case MessageType.friendAddResponse:
      case MessageType.friendAdd:
        _handleFriendAddResponse(body);
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
      case MessageType.friendListResponse:
        _handleFriendListResponse(body);
        break;
      case MessageType.friendRequestsResponse:
        _handleFriendRequestsResponse(body);
        break;
      case MessageType.friendRemarkResponse:
        _handleFriendRemarkResponse(body);
        break;
      case MessageType.privateMessage:
        _handleIncomingPrivateMessage(body);
        break;
      case MessageType.privateMessageResponse:
        _handlePrivateMessageResponse(body);
        break;
      case MessageType.privateHistoryResponse:
        _handlePrivateHistoryResponse(body);
        break;
      case MessageType.groupCreateResponse:
        _handleGroupCreateResponse(body);
        break;
      case MessageType.groupListResponse:
        _handleGroupListResponse(body);
        break;
      case MessageType.groupMembersResponse:
        _handleGroupMembersResponse(body);
        break;
      case MessageType.groupMessage:
        _handleIncomingGroupMessage(body);
        break;
      case MessageType.groupMessageResponse:
        _handleGroupMessageResponse(body);
        break;
      case MessageType.groupHistoryResponse:
        _handleGroupHistoryResponse(body);
        break;
      case MessageType.mediaUploadResponse:
        _handleMediaUploadResponse(body);
        break;
      case MessageType.keyUploadResponse:
        _handleKeyUploadResponse(body);
        break;
      case MessageType.keyResponse:
        _handleKeyResponse(body);
        break;
      case MessageType.encryptedMessage:
      case MessageType.encryptedMessageResponse:
        _handleEncryptedMessage(body);
        break;
      case MessageType.messageRecall:
        _handleMessageRecall(body);
        break;
      case MessageType.messageRecallResponse:
        _handleMessageRecallResponse(body);
        break;
      default:
        break;
    }
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
        
        // 保存用户信息
        _users[_currentUser!.userId] = _currentUser!;
        
        if (_isReconnecting) {
          _reconnectLoginSuccess = true;
          debugPrint('Reconnect login successful');
        }
        
        // 初始化端到端加密
        _initE2EE();
        
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

  /// 发送群聊消息
  void sendGroupMessage(int groupId, String content, {int mediaType = 0, String mediaUrl = '', List<int>? mentionedUsers}) {
    final body = <String, dynamic>{
      'group_id': groupId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
    };
    if (mentionedUsers != null && mentionedUsers.isNotEmpty) {
      body['mentioned_users'] = mentionedUsers;
    }
    _network.send(MessageType.groupMessage, body);
  }

  /// 处理接收到的私聊消息
  void _handleIncomingPrivateMessage(Map<String, dynamic> body) {
    final message = Message.fromJson(body);
    final peerId = message.senderId;
    
    // 缓存发送者信息
    if (body['sender'] != null) {
      final sender = User.fromJson(body['sender'] as Map<String, dynamic>);
      _users[sender.userId] = sender;
    }
    
    // 添加到消息列表
    final key = peerId;
    if (!_messages.containsKey(key)) {
      _messages[key] = [];
    }
    _messages[key]!.add(message);
    
    // 保存到本地数据库
    _messageDb.saveMessage(message);
    
    // 更新会话列表
    _updateConversation(message);
    
    // 发送通知（如果不在当前聊天界面）
    if (message.senderId != currentUserId) {
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
        final peerId = data['peer_id'] as int? ?? 0;
        
        final messages = messagesJson
            .map((json) => Message.fromJson(json as Map<String, dynamic>))
            .toList();
        
        final key = peerId;
        if (!_messages.containsKey(key)) {
          _messages[key] = [];
        }
        
        // 合并消息（去重）
        final existingIds = _messages[key]!.map((m) => m.messageId).toSet();
        for (final message in messages.reversed) {
          if (!existingIds.contains(message.messageId)) {
            _messages[key]!.insert(0, message);
            existingIds.add(message.messageId);
          }
        }
        
        // 保存到本地数据库
        _messageDb.saveMessages(peerId, messages, isGroup: false);
        
        notifyListeners();
      }
    }
  }

  /// 处理群聊消息响应
  void _handleGroupMessageResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final message = Message.fromJson(data);
        final key = -message.groupId; // 使用负数作为群聊的key
        
        if (!_messages.containsKey(key)) {
          _messages[key] = [];
        }
        _messages[key]!.add(message);
        
        // 保存到本地数据库
        _messageDb.saveMessage(message);
        
        _updateConversation(message);
        notifyListeners();
      }
    }
  }

  /// 处理接收到的群聊消息
  void _handleIncomingGroupMessage(Map<String, dynamic> body) {
    final message = Message.fromJson(body);
    final groupId = message.groupId;
    
    // 缓存发送者信息
    if (body['sender'] != null) {
      final sender = User.fromJson(body['sender'] as Map<String, dynamic>);
      _users[sender.userId] = sender;
    }
    
    // 添加到消息列表
    final key = -groupId;
    if (!_messages.containsKey(key)) {
      _messages[key] = [];
    }
    _messages[key]!.add(message);
    
    // 保存到本地数据库
    _messageDb.saveMessage(message);
    
    // 更新会话列表
    _updateConversation(message);
    
    // 发送通知
    if (message.senderId != currentUserId) {
      if (!_isInChatScreen || _currentChatPeerId != groupId) {
        String senderName = '用户';
        if (_users.containsKey(message.senderId)) {
          senderName = _users[message.senderId]!.nickname.isNotEmpty 
              ? _users[message.senderId]!.nickname 
              : _users[message.senderId]!.username;
        }
        
        String groupName = '群组';
        if (_groups.containsKey(groupId)) {
          groupName = _groups[groupId]!.groupName;
        }
        
        String notificationBody;
        if (message.isImage) {
          notificationBody = '$senderName: [图片]';
        } else if (message.isFile) {
          notificationBody = '$senderName: [文件] ${message.content}';
        } else {
          notificationBody = '$senderName: ${message.content}';
        }
        
        _notificationService.showMessageNotification(
          id: groupId + 10000,
          title: groupName,
          body: notificationBody,
          payload: 'group_$groupId',
        );
      }
    }
    
    notifyListeners();
  }

  /// 处理群聊历史响应
  void _handleGroupHistoryResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final messagesJson = data['messages'] as List<dynamic>? ?? [];
        final groupId = data['group_id'] as int? ?? 0;
        
        final messages = messagesJson
            .map((json) => Message.fromJson(json as Map<String, dynamic>))
            .toList();
        
        final key = -groupId;
        if (!_messages.containsKey(key)) {
          _messages[key] = [];
        }
        
        // 合并消息（去重）
        final existingIds = _messages[key]!.map((m) => m.messageId).toSet();
        for (final message in messages.reversed) {
          if (!existingIds.contains(message.messageId)) {
            _messages[key]!.insert(0, message);
            existingIds.add(message.messageId);
          }
        }
        
        // 保存到本地数据库
        _messageDb.saveMessages(groupId, messages, isGroup: true);
        
        notifyListeners();
      }
    }
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

  // ==================== 好友相关 ====================

  /// 添加好友
  void addFriend(int friendId) {
    _network.send(MessageType.friendAdd, {
      'friend_id': friendId,
    });
  }

  /// 接受好友请求
  void acceptFriend(int friendId) {
    _network.send(MessageType.friendAccept, {
      'friend_id': friendId,
    });
  }

  /// 拒绝好友请求
  void rejectFriend(int friendId) {
    _network.send(MessageType.friendReject, {
      'friend_id': friendId,
    });
  }

  /// 删除好友
  void removeFriend(int friendId) {
    _network.send(MessageType.friendRemove, {
      'friend_id': friendId,
    });
  }

  /// 设置好友备注
  void setFriendRemark(int friendId, String remark) {
    _network.send(MessageType.friendRemark, {
      'friend_id': friendId,
      'remark': remark,
    });
  }

  void _handleFriendAddResponse(Map<String, dynamic> body) {
    // 处理好友添加响应
    notifyListeners();
  }

  void _handleFriendAcceptResponse(Map<String, dynamic> body) {
    // 处理好友接受响应
    notifyListeners();
  }

  void _handleFriendRejectResponse(Map<String, dynamic> body) {
    notifyListeners();
  }

  void _handleFriendRemoveResponse(Map<String, dynamic> body) {
    notifyListeners();
  }

  void _handleFriendListResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final friendsJson = data['friends'] as List<dynamic>? ?? [];
        for (final json in friendsJson) {
          final friendData = json as Map<String, dynamic>;
          if (friendData['user'] != null) {
            final user = User.fromJson(friendData['user'] as Map<String, dynamic>);
            _users[user.userId] = user;
          }
        }
        notifyListeners();
      }
    }
  }

  void _handleFriendRequestsResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final requestsJson = data['requests'] as List<dynamic>? ?? [];
        for (final json in requestsJson) {
          final requestData = json as Map<String, dynamic>;
          if (requestData['user'] != null) {
            final user = User.fromJson(requestData['user'] as Map<String, dynamic>);
            _users[user.userId] = user;
          }
        }
        notifyListeners();
      }
    }
  }

  void _handleFriendRemarkResponse(Map<String, dynamic> body) {
    notifyListeners();
  }

  // ==================== 群组相关 ====================

  /// 创建群组
  void createGroup(String groupName, List<int> memberIds) {
    _network.send(MessageType.groupCreate, {
      'group_name': groupName,
      'member_ids': memberIds,
    });
  }

  void _handleGroupCreateResponse(Map<String, dynamic> body) {
    notifyListeners();
  }

  void _handleGroupListResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final groupsJson = data['groups'] as List<dynamic>? ?? [];
        for (final json in groupsJson) {
          final group = Group.fromJson(json as Map<String, dynamic>);
          _groups[group.groupId] = group;
        }
        notifyListeners();
      }
    }
  }

  void _handleGroupMembersResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final membersJson = data['members'] as List<dynamic>? ?? [];
        for (final json in membersJson) {
          final user = User.fromJson(json as Map<String, dynamic>);
          _users[user.userId] = user;
        }
        notifyListeners();
      }
    }
  }

  /// 获取群组成员
  Future<List<User>> fetchGroupMembers(int groupId) async {
    _network.send(MessageType.groupMembers, {
      'group_id': groupId,
    });
    // 等待响应
    await Future.delayed(const Duration(milliseconds: 500));
    // 返回缓存的用户
    // 这里需要更复杂的逻辑，暂时简化
    return _users.values.toList();
  }

  /// 处理用户信息响应
  void _handleUserInfoResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final user = User.fromJson(data['user'] as Map<String, dynamic>? ?? data);
        _users[user.userId] = user;
        notifyListeners();
      }
    }
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

  // ==================== 媒体上传 ====================

  /// 上传媒体文件
  /// 返回上传成功后的文件 URL
  Future<String?> uploadMedia(File file, {int mediaType = 1}) async {
    // 检查网络连接
    if (!_isConnected) {
      _uploadError = '未连接到服务器';
      notifyListeners();
      return null;
    }
    
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
      
      debugPrint('Uploading media: $fileName, size: ${bytes.length} bytes');
      
      _uploadProgress = 0.5;
      notifyListeners();
      
      // 发送上传请求
      final sent = _network.send(MessageType.mediaUpload, {
        'file_name': fileName,
        'file_data': base64Data,
        'media_type': mediaType,
      });
      
      if (!sent) {
        _mediaUploading = false;
        _uploadError = '发送上传请求失败';
        notifyListeners();
        return null;
      }
      
      debugPrint('Upload request sent, waiting for response...');
      
      // 等待响应（最多30秒，因为文件可能较大）
      for (int i = 0; i < 300; i++) {
        await Future.delayed(const Duration(milliseconds: 100));
        if (_uploadedMediaUrl != null || _uploadError != null) {
          _mediaUploading = false;
          _uploadProgress = 1.0;
          notifyListeners();
          debugPrint('Upload complete: $_uploadedMediaUrl, error: $_uploadError');
          return _uploadedMediaUrl;
        }
      }
      
      _mediaUploading = false;
      _uploadError = '上传超时';
      notifyListeners();
      debugPrint('Upload timeout');
      return null;
    } catch (e) {
      _mediaUploading = false;
      _uploadError = '上传失败: $e';
      notifyListeners();
      debugPrint('Upload error: $e');
      return null;
    }
  }
  
  /// 处理媒体上传响应
  void _handleMediaUploadResponse(Map<String, dynamic> body) {
    debugPrint('Received media upload response: $body');
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        _uploadedFileId = data['file_id'] as int?;
        // 替换localhost为实际的媒体服务器地址
        final rawUrl = data['url'] as String?;
        _uploadedMediaUrl = rawUrl != null ? _fixMediaUrl(rawUrl) : null;
        _uploadError = null;
        debugPrint('Media uploaded successfully: $_uploadedMediaUrl');
      }
    } else {
      _uploadedMediaUrl = null;
      _uploadedFileId = null;
      _uploadError = body['message'] as String? ?? 'Upload failed';
      debugPrint('Media upload failed: $_uploadError');
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

  // ==================== 端到端加密 ====================

  /// 初始化端到端加密
  Future<void> _initE2EE() async {
    try {
      await _e2ee.init();
      
      if (!_e2ee.isInitialized) {
        await _e2ee.generateKeyPair();
        debugPrint('Generated new E2EE key pair');
      }
      
      // 上传公钥到服务器
      final publicKey = _e2ee.getPublicKeyPem();
      if (publicKey != null) {
        _network.send(MessageType.keyUpload, {
          'public_key': publicKey,
        });
        debugPrint('Uploading public key to server');
      }
      
      _e2eeEnabled = true;
    } catch (e) {
      debugPrint('Failed to initialize E2EE: $e');
      _e2eeEnabled = false;
    }
  }
  
  void _handleKeyUploadResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      debugPrint('Public key uploaded successfully');
    } else {
      debugPrint('Failed to upload public key: ${body['message']}');
    }
  }
  
  Future<String?> _getRecipientPublicKey(int userId) async {
    // 先检查缓存
    final cached = _e2ee.getCachedPublicKey(userId);
    if (cached != null) {
      return cached;
    }
    
    // 请求公钥
    _network.send(MessageType.keyRequest, {
      'user_id': userId,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      final key = _e2ee.getCachedPublicKey(userId);
      if (key != null) {
        return key;
      }
    }
    
    return null;
  }
  
  void _handleKeyResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final userId = data['user_id'] as int?;
        final publicKey = data['public_key'] as String?;
        if (userId != null && publicKey != null) {
          _e2ee.cachePublicKey(userId, publicKey);
        }
      }
    }
  }
  
  Future<bool> sendEncryptedPrivateMessage(int receiverId, String content) async {
    if (!_e2eeEnabled) {
      debugPrint('E2EE not enabled, sending plain message');
      sendPrivateMessage(receiverId, content);
      return true;
    }
    
    // 获取接收者公钥
    String? recipientPublicKey = _e2ee.getCachedPublicKey(receiverId);
    if (recipientPublicKey == null) {
      recipientPublicKey = await _getRecipientPublicKey(receiverId);
    }
    
    if (recipientPublicKey == null) {
      debugPrint('Failed to get recipient public key');
      sendPrivateMessage(receiverId, content);
      return true;
    }
    
    try {
      // 加密消息
      final encrypted = await _e2ee.encryptMessage(content, recipientPublicKey);
      
      _network.send(MessageType.encryptedMessage, {
        'receiver_id': receiverId,
        'encrypted_key': encrypted['encrypted_key'],
        'iv': encrypted['iv'],
        'encrypted_content': encrypted['encrypted_content'],
      });
      
      return true;
    } catch (e) {
      debugPrint('Failed to send encrypted message: $e');
      sendPrivateMessage(receiverId, content);
      return true;
    }
  }
  
  void _handleEncryptedMessage(Map<String, dynamic> body) {
    final message = Message.fromJson(body);
    
    // 尝试解密
    try {
      final extra = message.extra;
      final isEncrypted = extra['encrypted'] as bool? ?? false;
      
      if (!isEncrypted) {
        // 普通消息
        _handleIncomingPrivateMessage(body);
        return;
      }
      
      final encryptedKey = extra['encrypted_key'] as String? ?? '';
      final iv = extra['iv'] as String? ?? '';
      final encryptedContent = extra['encrypted_content'] as String? ?? '';
      
      if (encryptedKey.isEmpty || iv.isEmpty || encryptedContent.isEmpty) {
        _handleIncomingPrivateMessage(body);
        return;
      }
      
      // 解密消息
      final decryptedContent = await _e2ee.decryptMessage(
        encryptedKey: encryptedKey,
        iv: iv,
        encryptedContent: encryptedContent,
      );
      
      // 更新消息内容
      final decryptedMessage = Message(
        messageId: message.messageId,
        senderId: message.senderId,
        receiverId: message.receiverId,
        groupId: message.groupId,
        mediaType: message.mediaType,
        content: decryptedContent,
        mediaUrl: message.mediaUrl,
        extra: message.extra,
        status: message.status,
        createdAt: message.createdAt,
      );
      
      // 处理解密后的消息
      final bodyWithDecrypted = Map<String, dynamic>.from(body);
      bodyWithDecrypted['content'] = decryptedContent;
      _handleIncomingPrivateMessage(bodyWithDecrypted);
    } catch (e) {
      debugPrint('Failed to decrypt message: $e');
      _handleIncomingPrivateMessage(body);
    }
  }
  
  bool _e2eeEnabled = false;
  
  /// 端到端加密是否启用
  bool get e2eeEnabled => _e2eeEnabled;

  // ==================== 消息撤回 ====================

  /// 撤回消息
  Future<bool> recallMessage(int messageId, {bool isGroup = false, int? groupId}) async {
    if (!_isAuthenticated || _currentUser == null) {
      return false;
    }
    
    _recallSuccess = false;
    _recallError = null;
    
    _network.send(MessageType.messageRecall, {
      'message_id': messageId,
      'is_group': isGroup,
      'group_id': groupId ?? 0,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_recallSuccess || _recallError != null) {
        return _recallSuccess;
      }
    }
    
    _recallError = '撤回超时';
    return false;
  }
  
  bool _recallSuccess = false;
  String? _recallError;
  
  bool get recallSuccess => _recallSuccess;
  String? get recallError => _recallError;

  /// 处理撤回通知
  void _handleMessageRecall(Map<String, dynamic> body) {
    final messageId = body['message_id'] as int?;
    if (messageId == null) return;
    
    // 更新本地消息
    _updateRecalledMessage(messageId, false);
  }
  
  /// 处理撤回响应
  void _handleMessageRecallResponse(Map<String, dynamic> body) {
    final data = body['data'] as Map<String, dynamic>?;
    if (data == null) return;
    
    final success = data['success'] as bool? ?? false;
    if (success) {
      _recallSuccess = true;
      final messageId = data['message_id'] as int?;
      if (messageId != null) {
        // 更新本地消息
        _updateRecalledMessage(messageId, false);
      }
    } else {
      _recallError = data['error'] as String? ?? 'Failed to recall message';
    }
    notifyListeners();
  }
  
  /// 更新被撤回的消息
  void _updateRecalledMessage(int messageId, bool isGroup) {
    debugPrint('Updating recalled message: $messageId');
    
    // 遍历所有会话找到该消息
    for (var entry in _messages.entries) {
      final messages = entry.value;
      for (var i = 0; i < messages.length; i++) {
        if (messages[i].messageId == messageId) {
          debugPrint('Found message to recall at index $i');
          
          // 创建新的已撤回消息
          final recalledMessage = Message(
            messageId: messages[i].messageId,
            senderId: messages[i].senderId,
            receiverId: messages[i].receiverId,
            groupId: messages[i].groupId,
            mediaType: 0, // 文本类型
            content: '[消息已撤回]',
            mediaUrl: '',
            extra: messages[i].extra,
            status: messages[i].status,
            createdAt: messages[i].createdAt,
          );
          
          messages[i] = recalledMessage;
          
          // 更新本地数据库
          _messageDb.saveMessage(recalledMessage);
          
          notifyListeners();
          return;
        }
      }
    }
    
    debugPrint('Message not found for recall: $messageId');
  }
}