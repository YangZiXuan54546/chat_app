import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import '../models/models.dart';
import '../models/protocol.dart';
import 'websocket_service.dart';
import 'message_database.dart';
import 'notification_service.dart';
import 'e2ee_service.dart';
import 'fcm_service.dart';
import 'background_service.dart';
import 'storage_service.dart';
import 'jpush_service.dart';

class ChatService extends ChangeNotifier {
  final WebSocketService _network = WebSocketService();
  final MessageDatabase _messageDb = MessageDatabase();
  final NotificationService _notificationService = NotificationService();
  final E2EEService _e2ee = E2EEService();
  final JPushService _jPush = JPushService();
  
  User? _currentUser;
  bool _isConnected = false;
  bool _isAuthenticated = false;
  
  // 端到端加密状态
  bool _e2eeEnabled = false;
  
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
  final Map<int, List<int>> _groupMembers = {}; // 群成员ID列表
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

  // 媒体服务器地址（用于替换服务器返回的localhost URL）
  String _mediaServerHost = "10.0.2.2:8889"; // Android模拟器访问宿主机
  
  void setMediaServerHost(String host) {
    _mediaServerHost = host;
  }
  
  // 替换URL中的localhost为实际的媒体服务器地址
  String _fixMediaUrl(String url) {
    if (url.contains("localhost")) {
      return url.replaceFirst(RegExp(r"http://localhost:\d+"), "http://$_mediaServerHost");
    }
    return url;
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

    _network.addMessageCallback(_handleWsMessage);
  }
  
  /// 处理 WebSocket 消息
  void _handleWsMessage(String type, Map<String, dynamic> body) {
    debugPrint('[ChatService] Received message type: $type');
    
    switch (type) {
      case WsMessageType.loginResponse:
        _handleLoginResponse(body);
        break;
      case WsMessageType.registerResponse:
        _handleRegisterResponse(body);
        break;
      case WsMessageType.privateMessage:
        _handlePrivateMessage(body);
        break;
      case WsMessageType.privateMessageResponse:
        _handlePrivateMessageResponse(body);
        break;
      case WsMessageType.privateHistoryResponse:
        _handlePrivateHistoryResponse(body);
        break;
      case WsMessageType.groupMessage:
        _handleGroupMessage(body);
        break;
      case WsMessageType.friendListResponse:
        _handleFriendListResponse(body);
        break;
      case WsMessageType.friendRequestsResponse:
        _handleFriendRequestsResponse(body);
        break;
      case WsMessageType.groupListResponse:
        _handleGroupListResponse(body);
        break;
      case WsMessageType.userSearchResponse:
        _handleUserSearchResponse(body);
        break;
      case WsMessageType.friendAddResponse:
        _handleFriendAddResponse(body);
        break;
      case WsMessageType.friendRequestNotification:
        _handleFriendRequestNotification(body);
        break;
      case WsMessageType.friendAcceptNotification:
        _handleFriendAcceptNotification(body);
        break;
      case WsMessageType.friendAcceptResponse:
        _handleFriendAcceptResponse(body);
        break;
      case WsMessageType.friendRejectResponse:
        _handleFriendRejectResponse(body);
        break;
      case WsMessageType.friendRemoveResponse:
        _handleFriendRemoveResponse(body);
        break;
      case WsMessageType.friendRemarkResponse:
        _handleFriendRemarkResponse(body);
        break;
      case WsMessageType.createGroupResponse:
        _handleGroupCreateResponse(body);
        break;
      case WsMessageType.inviteGroupMembersResponse:
        _handleGroupAddMemberResponse(body);
        break;
      case WsMessageType.groupSetAdminResponse:
        _handleGroupSetAdminResponse(body);
        break;
      case WsMessageType.groupTransferOwnerResponse:
        _handleGroupTransferOwnerResponse(body);
        break;
      case WsMessageType.groupKickResponse:
        _handleGroupRemoveMemberResponse(body);
        break;
      case WsMessageType.groupQuitResponse:
        _handleGroupLeaveResponse(body);
        break;
      case WsMessageType.groupDismissResponse:
        _handleGroupDismissResponse(body);
        break;
      case WsMessageType.groupMembersResponse:
        _handleGroupMembersResponse(body);
        break;
      case WsMessageType.mediaUploadResponse:
        _handleMediaUploadResponse(body);
        break;
      case WsMessageType.keyUploadResponse:
        _handleKeyUploadResponse(body);
        break;
      case WsMessageType.keyResponse:
        _handleKeyResponse(body);
        break;
      case WsMessageType.encryptedMessage:
      case WsMessageType.encryptedMessageResponse:
        _handleEncryptedMessage(body);
        break;
      case WsMessageType.messageRecall:
        _handleMessageRecall(body);
        break;
      case WsMessageType.messageRecallResponse:
        _handleMessageRecallResponse(body);
        break;
      case WsMessageType.passwordUpdateResponse:
        _handlePasswordUpdateResponse(body);
        break;
      case WsMessageType.favoriteAddResponse:
        _handleFavoriteAddResponse(body);
        break;
      case WsMessageType.favoriteRemoveResponse:
        _handleFavoriteRemoveResponse(body);
        break;
      case WsMessageType.favoriteListResponse:
        _handleFavoriteListResponse(body);
        break;
      case WsMessageType.error:
        debugPrint('[ChatService] Error: ${body['message']}');
        break;
      case WsMessageType.kicked:
        debugPrint('[ChatService] Kicked: ${body['reason']}');
        // 被踢下线
        _isAuthenticated = false;
        _currentUser = null;
        notifyListeners();
        break;
      default:
        debugPrint('[ChatService] Unknown message type: $type');
        break;
    }
  }
  
  /// 处理重连后的自动登录
  Future<void> _handleReconnectLogin(String username, String password) async {
    debugPrint('Auto-login after reconnect: $username');
    
    _reconnectLoginSuccess = false;
    _loginSuccess = false;
    _loginError = null;
    
    _network.send(WsMessageType.login, {
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

  /// 连接服务器
  Future<bool> connect(String host, int port) async {
    return await _network.connect(host, port, path: '/ws');
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
    
    _network.send(WsMessageType.register, {
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
    
    _network.send(WsMessageType.login, {
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
    _network.send(WsMessageType.logout, {});
    _network.clearCredentials();
    _isAuthenticated = false;
    _isReconnecting = false;
    _reconnectLoginSuccess = false;
    _currentUser = null;
    
    // 删除 JPush 别名
    _jPush.deleteAlias();
    
    notifyListeners();
  }

  /// 处理登录响应 (WebSocket JSON 格式)
  void _handleLoginResponse(Map<String, dynamic> body) {
    final success = body['success'] as bool? ?? false;
    if (success) {
      final userInfo = body['user_info'] as Map<String, dynamic>?;
      if (userInfo != null) {
        _currentUser = User.fromJson(userInfo);
      } else {
        // 从顶层字段构建用户信息
        _currentUser = User(
          userId: body['user_id'] as int? ?? 0,
          username: body['username'] as String? ?? '',
          nickname: body['nickname'] as String? ?? '',
          avatarUrl: body['avatar_url'] as String? ?? '',
          status: 'ONLINE',
        );
      }
      _isAuthenticated = true;
      _loginSuccess = true;
      _loginError = null;
      
      // 如果是重连登录成功
      if (_isReconnecting) {
        _reconnectLoginSuccess = true;
        _isReconnecting = false;
        debugPrint('Reconnect login successful');
      }
      
      // 初始化端到端加密
      _initE2EE();
      
      // 注册 FCM Token
      _registerFcmToken();
      
      // 加载好友列表
      _network.send(WsMessageType.friendList, {});
      // 加载好友请求列表
      _network.send(WsMessageType.friendRequests, {});
      // 加载群组列表
      _network.send(WsMessageType.groupList, {});
    } else {
      _loginSuccess = false;
      _loginError = body['error'] as String? ?? 'Login failed';
      
      // 重连登录失败
      if (_isReconnecting) {
        _reconnectLoginSuccess = false;
        debugPrint('Reconnect login failed: $_loginError');
      }
    }
    notifyListeners();
  }

  /// 处理注册响应 (WebSocket JSON 格式)
  void _handleRegisterResponse(Map<String, dynamic> body) {
    final success = body['success'] as bool? ?? false;
    if (success) {
      _registerSuccess = true;
      _registerError = null;
      _registeredUserId = body['user_id'] as int?;
    } else {
      _registerSuccess = false;
      _registerError = body['error'] as String? ?? 'Registration failed';
    }
    notifyListeners();
  }

  /// 发送私聊消息
  void sendPrivateMessage(int receiverId, String content, {int mediaType = 0, String mediaUrl = ''}) {
    _network.send(WsMessageType.privateMessage, {
      'receiver_id': receiverId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
    });
  }

  /// 处理私聊消息
  void _handlePrivateMessage(Map<String, dynamic> body) {
    debugPrint('========== _handlePrivateMessage received ==========');
    debugPrint('Message body: $body');
    
    final message = Message.fromJson(body);
    debugPrint('Message from: ${message.senderId}, to: ${message.receiverId}, currentUserId: $currentUserId');
    
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
      debugPrint('Checking notification: _isInChatScreen=$_isInChatScreen, _currentChatPeerId=$_currentChatPeerId, peerId=$peerId');
      
      if (!_isInChatScreen || _currentChatPeerId != peerId) {
        // 获取发送者名称
        String senderName = '用户';
        if (_users.containsKey(peerId)) {
          senderName = _users[peerId]!.nickname.isNotEmpty 
              ? _users[peerId]!.nickname 
              : _users[peerId]!.username;
        }
        
        debugPrint('Showing notification: senderName=$senderName, content=${message.content}');
        
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

  /// 处理发送私聊消息的响应 (WebSocket JSON 格式)
  void _handlePrivateMessageResponse(Map<String, dynamic> body) {
    // WebSocket 格式：直接包含消息字段
    final messageId = body['message_id'] as int?;
    if (messageId != null && messageId > 0) {
      // 消息发送成功，构建消息对象
      final message = Message(
        messageId: messageId,
        senderId: body['sender_id'] as int? ?? currentUserId,
        receiverId: body['receiver_id'] as int? ?? 0,
        groupId: 0,
        mediaType: body['message_type'] as int? ?? 0,
        content: body['content'] as String? ?? '',
        mediaUrl: body['media_url'] as String? ?? '',
        extra: '',
        status: 1,
        createdAt: body['created_at'] as int? ?? 0,
      );
      
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
  void sendGroupMessage(int groupId, String content, {int mediaType = 0, String mediaUrl = '', List<int>? mentionedUsers}) {
    final body = {
      'group_id': groupId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
    };
    
    // 添加 @成员 信息
    if (mentionedUsers != null && mentionedUsers.isNotEmpty) {
      body['mentioned_users'] = mentionedUsers;
    }
    
    _network.send(WsMessageType.groupMessage, body);
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
      _network.send(WsMessageType.groupHistory, {
        'group_id': peerId,
        'before_time': beforeTime,
        'limit': 50,
      });
    } else {
      _network.send(WsMessageType.privateHistory, {
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
    
    _network.send(WsMessageType.friendAdd, {
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
    _network.send(WsMessageType.friendAccept, {
      'friend_id': friendId,
    });
    // 立即从本地列表移除该请求（乐观更新）
    _friendRequests.removeWhere((f) => f.user.userId == friendId);
    notifyListeners();
  }

  /// 拒绝好友请求
  void rejectFriend(int friendId) {
    _network.send(WsMessageType.friendReject, {
      'friend_id': friendId,
    });
    // 立即从本地列表移除该请求（乐观更新）
    _friendRequests.removeWhere((f) => f.user.userId == friendId);
    notifyListeners();
  }

  /// 删除好友
  void removeFriend(int friendId) {
    _network.send(WsMessageType.friendRemove, {
      'friend_id': friendId,
    });
    _loadFriendList();
  }

  /// 加载好友列表
  void _loadFriendList() {
    _network.send(WsMessageType.friendList, {});
  }

  /// 加载好友请求
  void _loadFriendRequests() {
    _network.send(WsMessageType.friendRequests, {});
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
    
    _network.send(WsMessageType.groupCreate, {
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
      _network.send(WsMessageType.groupAddMember, {
        'group_id': groupId,
        'user_id': memberId,
      });
    }
    // 刷新群组列表
    _network.send(WsMessageType.groupList, {});
  }
  
  /// 设置/取消管理员
  void setGroupAdmin(int groupId, int userId, bool isAdmin) {
    _network.send(WsMessageType.groupSetAdmin, {
      'group_id': groupId,
      'user_id': userId,
      'is_admin': isAdmin,
    });
  }
  
  /// 转让群主
  void transferGroupOwner(int groupId, int newOwnerId) {
    _network.send(WsMessageType.groupTransferOwner, {
      'group_id': groupId,
      'new_owner_id': newOwnerId,
    });
  }
  
  /// 踢出群成员
  void removeGroupMember(int groupId, int userId) {
    _network.send(WsMessageType.groupRemoveMember, {
      'group_id': groupId,
      'user_id': userId,
    });
  }
  
  /// 退出群组
  void leaveGroup(int groupId) {
    _network.send(WsMessageType.groupLeave, {
      'group_id': groupId,
    });
  }
  
  /// 解散群组
  void dismissGroup(int groupId) {
    _network.send(WsMessageType.groupDismiss, {
      'group_id': groupId,
    });
  }
  
  /// 获取群成员列表
  void getGroupMembers(int groupId) {
    _network.send(WsMessageType.groupMembers, {
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
      _network.send(WsMessageType.groupList, {});
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
      _network.send(WsMessageType.groupList, {});
    }
    notifyListeners();
  }

  /// 加入群组
  void joinGroup(int groupId) {
    _network.send(WsMessageType.groupJoin, {
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
    
    Conversation? existingConv;
    int unreadCount = 0;
    bool isPinned = false;
    bool isMuted = false;
    
    if (index >= 0) {
      // 更新现有会话，保留原有属性
      existingConv = _conversations[index];
      unreadCount = existingConv.unreadCount;
      isPinned = existingConv.isPinned;
      isMuted = existingConv.isMuted;
      _conversations.removeAt(index);
    }
    
    // 如果是收到的消息，增加未读数（不在当前聊天界面时）
    if (message.senderId != currentUserId) {
      final currentChatPeerId = _currentChatPeerId;
      final isCurrentChat = groupId > 0 
          ? currentChatPeerId == groupId 
          : currentChatPeerId == peerId;
      
      if (!isCurrentChat) {
        unreadCount++;
      }
    }
    
    // 创建新会话
    final newConv = Conversation(
      peerId: peerId,
      groupId: groupId,
      peer: _users[peerId],
      group: groupId > 0 ? _groups[groupId] : null,
      lastMessage: message,
      unreadCount: unreadCount,
      isPinned: isPinned,
      isMuted: isMuted,
    );
    
    // 插入会话列表
    _conversations.insert(0, newConv);
    
    // 重新排序（置顶在前，按时间排序）
    _sortConversations();
    
    // 通知 UI 更新
    notifyListeners();
  }
  
  /// 排序会话列表（置顶在前，其他按时间排序）
  void _sortConversations() {
    _conversations.sort((a, b) {
      // 置顶的在前
      if (a.isPinned && !b.isPinned) return -1;
      if (!a.isPinned && b.isPinned) return 1;
      
      // 按最后消息时间排序
      final aTime = a.lastMessage?.createdAt ?? 0;
      final bTime = b.lastMessage?.createdAt ?? 0;
      return bTime.compareTo(aTime);
    });
  }
  
  /// 切换会话置顶状态
  void toggleConversationPin(int peerId, {bool isGroup = false}) {
    final index = _conversations.indexWhere((c) => 
      isGroup ? c.groupId == peerId : c.peerId == peerId
    );
    
    if (index >= 0) {
      final conv = _conversations[index];
      _conversations[index] = Conversation(
        peerId: conv.peerId,
        groupId: conv.groupId,
        peer: conv.peer,
        group: conv.group,
        lastMessage: conv.lastMessage,
        unreadCount: conv.unreadCount,
        isPinned: !conv.isPinned,
        isMuted: conv.isMuted,
      );
      _sortConversations();
      notifyListeners();
    }
  }
  
  /// 切换会话免打扰状态
  void toggleConversationMute(int peerId, {bool isGroup = false}) {
    final index = _conversations.indexWhere((c) => 
      isGroup ? c.groupId == peerId : c.peerId == peerId
    );
    
    if (index >= 0) {
      final conv = _conversations[index];
      _conversations[index] = Conversation(
        peerId: conv.peerId,
        groupId: conv.groupId,
        peer: conv.peer,
        group: conv.group,
        lastMessage: conv.lastMessage,
        unreadCount: conv.unreadCount,
        isPinned: conv.isPinned,
        isMuted: !conv.isMuted,
      );
      notifyListeners();
    }
  }
  
  /// 标记会话为已读
  void markConversationRead(int peerId, {bool isGroup = false}) {
    final index = _conversations.indexWhere((c) => 
      isGroup ? c.groupId == peerId : c.peerId == peerId
    );
    
    if (index >= 0) {
      final conv = _conversations[index];
      _conversations[index] = Conversation(
        peerId: conv.peerId,
        groupId: conv.groupId,
        peer: conv.peer,
        group: conv.group,
        lastMessage: conv.lastMessage,
        unreadCount: 0,
        isPinned: conv.isPinned,
        isMuted: conv.isMuted,
      );
      notifyListeners();
    }
  }
  
  /// 删除会话
  void deleteConversation(int peerId, {bool isGroup = false}) {
    _conversations.removeWhere((c) => 
      isGroup ? c.groupId == peerId : c.peerId == peerId
    );
    notifyListeners();
  }

  /// 搜索用户
  void searchUsers(String keyword) {
    _network.send(WsMessageType.userSearch, {
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
      // 刷新好友列表
      _loadFriendList();
    }
    notifyListeners();
  }

  /// 处理拒绝好友请求响应
  void _handleFriendRejectResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    // 已经在 rejectFriend 中乐观移除了
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
    _network.send(WsMessageType.friendRemark, {
      'friend_id': friendId,
      'remark': remark,
    });
  }

  /// 加载好友请求列表
  void getUserInfo(int userId) {
    _network.send(WsMessageType.userInfo, {
      'user_id': userId,
    });
  }

  /// 更新用户信息
  void updateUserInfo({String? nickname, String? signature, String? avatarUrl}) {
    final data = <String, dynamic>{};
    if (nickname != null) data['nickname'] = nickname;
    if (signature != null) data['signature'] = signature;
    if (avatarUrl != null) data['avatar_url'] = avatarUrl;
    
    _network.send(WsMessageType.userUpdate, data);
  }
  
  // 修改密码相关状态
  bool _passwordUpdateSuccess = false;
  String? _passwordUpdateError;
  
  bool get passwordUpdateSuccess => _passwordUpdateSuccess;
  String? get passwordUpdateError => _passwordUpdateError;
  
  /// 修改密码
  Future<bool> updatePassword(String oldPassword, String newPassword) async {
    // 重置状态
    _passwordUpdateSuccess = false;
    _passwordUpdateError = null;
    
    // 验证新密码长度
    if (newPassword.length < 6) {
      _passwordUpdateError = '新密码至少需要6个字符';
      notifyListeners();
      return false;
    }
    
    _network.send(WsMessageType.passwordUpdate, {
      'old_password': oldPassword,
      'new_password': newPassword,
    });
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_passwordUpdateSuccess || _passwordUpdateError != null) {
        return _passwordUpdateSuccess;
      }
    }
    
    _passwordUpdateError = '请求超时';
    return false;
  }
  
  /// 处理修改密码响应
  void _handlePasswordUpdateResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      _passwordUpdateSuccess = true;
      _passwordUpdateError = null;
    } else {
      _passwordUpdateSuccess = false;
      _passwordUpdateError = body['message'] as String? ?? '修改密码失败';
    }
    notifyListeners();
  }
  
  /// 处理设置管理员响应
  void _handleGroupSetAdminResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(WsMessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理转让群主响应
  void _handleGroupTransferOwnerResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(WsMessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理踢出成员响应
  void _handleGroupRemoveMemberResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(WsMessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理退出群组响应
  void _handleGroupLeaveResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(WsMessageType.groupList, {});
    }
    notifyListeners();
  }
  
  /// 处理解散群组响应
  void _handleGroupDismissResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      // 刷新群组列表
      _network.send(WsMessageType.groupList, {});
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
        final groupId = data['group_id'] as int? ?? 0;
        
        // 存储群成员 ID 列表
        final memberIds = <int>[];
        
        // 存储群成员信息到本地
        for (final item in membersJson) {
          final memberData = item as Map<String, dynamic>;
          final user = User.fromJson(memberData);
          _users[user.userId] = user;
          memberIds.add(user.userId);
        }
        
        _groupMembers[groupId] = memberIds;
      }
    }
    notifyListeners();
  }
  
  /// 获取群成员列表
  List<User> getGroupMembersList(int groupId) {
    final memberIds = _groupMembers[groupId] ?? [];
    return memberIds.map((id) => _users[id]).whereType<User>().toList();
  }
  
  /// 请求群成员列表并等待响应
  Future<List<User>> fetchGroupMembers(int groupId) async {
    // 先检查是否已有缓存
    if (_groupMembers.containsKey(groupId)) {
      return getGroupMembersList(groupId);
    }
    
    // 发送请求
    getGroupMembers(groupId);
    
    // 等待响应（最多5秒）
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_groupMembers.containsKey(groupId)) {
        return getGroupMembersList(groupId);
      }
    }
    
    return [];
  }
  
  // ==================== 媒体上传相关 ====================
  
  /// 上传媒体文件
  /// 使用 HTTP POST 上传到 HTTP Gateway (端口 8889)
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
      final fileName = file.path.split('/').last;
      
      debugPrint('Uploading media via HTTP: $fileName, size: ${bytes.length} bytes');
      
      _uploadProgress = 0.3;
      notifyListeners();
      
      // 使用 HTTP POST 上传到 HTTP Gateway
      // 媒体服务器地址使用 _mediaServerHost (默认 10.0.2.2:8889)
      final uri = Uri.parse('http://$_mediaServerHost/api/upload');
      
      // 创建 multipart 请求
      final request = http.MultipartRequest('POST', uri);
      
      // 添加授权头
      if (_currentUser != null) {
        request.headers['Authorization'] = 'Bearer ${_currentUser!.userId}:session_token';
      }
      
      // 添加文件
      request.files.add(
        http.MultipartFile.fromBytes(
          'file',
          bytes,
          filename: fileName,
        ),
      );
      
      // 添加媒体类型字段
      request.fields['media_type'] = mediaType.toString();
      
      _uploadProgress = 0.5;
      notifyListeners();
      
      debugPrint('Sending HTTP upload request to: $uri');
      
      // 发送请求
      final streamedResponse = await request.send().timeout(const Duration(seconds: 60));
      final response = await http.Response.fromStream(streamedResponse);
      
      _uploadProgress = 0.8;
      notifyListeners();
      
      debugPrint('HTTP response status: ${response.statusCode}');
      debugPrint('HTTP response body: ${response.body}');
      
      if (response.statusCode == 200) {
        final json = jsonDecode(response.body);
        final code = json['code'] ?? -1;
        
        if (code == 0) {
          final data = json['data'] as Map<String, dynamic>?;
          if (data != null) {
            _uploadedFileId = data['file_id'] as int?;
            final rawUrl = data['url'] as String?;
            // 构建完整的 URL
            if (rawUrl != null) {
              _uploadedMediaUrl = 'http://$_mediaServerHost$rawUrl';
            }
            _uploadError = null;
            debugPrint('Media uploaded successfully: $_uploadedMediaUrl');
          }
        } else {
          _uploadedMediaUrl = null;
          _uploadedFileId = null;
          _uploadError = json['message'] as String? ?? 'Upload failed';
          debugPrint('Media upload failed: $_uploadError');
        }
      } else {
        _uploadError = 'HTTP error: ${response.statusCode}';
        debugPrint('Media upload HTTP error: ${response.statusCode}');
      }
      
      _mediaUploading = false;
      _uploadProgress = 1.0;
      notifyListeners();
      
      return _uploadedMediaUrl;
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
  
  // ==================== 端到端加密相关 ====================
  
  /// 初始化端到端加密
  Future<void> _initE2EE() async {
    try {
      await _e2ee.init();
      
      // 如果没有密钥对，生成新的
      if (!_e2ee.isInitialized) {
        await _e2ee.generateKeyPair();
        debugPrint('Generated new E2EE key pair');
      }
      
      // 上传公钥到服务器
      final publicKey = _e2ee.getPublicKeyPem();
      if (publicKey != null) {
        _network.send(WsMessageType.keyUpload, {
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
  
  /// 获取用户的公钥
  Future<String?> getUserPublicKey(int userId) async {
    // 先检查缓存
    final cached = _e2ee.getCachedPublicKey(userId);
    if (cached != null) {
      return cached;
    }
    
    // 从服务器获取
    // 发送请求
    _network.send(WsMessageType.keyRequest, {
      'user_id': userId,
    });
    
    // 等待响应（通过监听器实现）
    // 简化处理：返回 null，让调用者等待
    return null;
  }
  
  /// 发送加密私聊消息
  Future<bool> sendEncryptedPrivateMessage(int receiverId, String content) async {
    if (!_e2eeEnabled) {
      debugPrint('E2EE not enabled, sending plain message');
      sendPrivateMessage(receiverId, content);
      return true;
    }
    
    try {
      // 获取接收者的公钥
      String? recipientPublicKey = _e2ee.getCachedPublicKey(receiverId);
      
      if (recipientPublicKey == null) {
        // 请求公钥
        _network.send(WsMessageType.keyRequest, {
          'user_id': receiverId,
        });
        // 简化：先发送普通消息
        sendPrivateMessage(receiverId, content);
        return true;
      }
      
      // 加密消息
      final encrypted = await _e2ee.encryptMessage(content, recipientPublicKey);
      
      // 发送加密消息
      _network.send(WsMessageType.encryptedMessage, {
        'receiver_id': receiverId,
        'encrypted_key': encrypted['encrypted_key'],
        'iv': encrypted['iv'],
        'encrypted_content': encrypted['encrypted_content'],
        'media_type': 0,
        'media_url': '',
      });
      
      return true;
    } catch (e) {
      debugPrint('Failed to send encrypted message: $e');
      // 回退到普通消息
      sendPrivateMessage(receiverId, content);
      return false;
    }
  }
  
  /// 解密消息
  Future<String?> decryptMessage(Message message) async {
    if (message.extra.isEmpty) {
      return message.content;
    }
    
    try {
      final extra = jsonDecode(message.extra) as Map<String, dynamic>;
      final isEncrypted = extra['encrypted'] as bool? ?? false;
      
      if (!isEncrypted) {
        return message.content;
      }
      
      final encryptedKey = extra['encrypted_key'] as String? ?? '';
      final iv = extra['iv'] as String? ?? '';
      final encryptedContent = extra['encrypted_content'] as String? ?? '';
      
      if (encryptedKey.isEmpty || iv.isEmpty || encryptedContent.isEmpty) {
        return message.content;
      }
      
      return await _e2ee.decryptMessage(
        encryptedKey: encryptedKey,
        iv: iv,
        encryptedContent: encryptedContent,
      );
    } catch (e) {
      debugPrint('Failed to decrypt message: $e');
      return message.content;
    }
  }
  
  /// 处理公钥上传响应
  void _handleKeyUploadResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      debugPrint('Public key uploaded successfully');
    } else {
      debugPrint('Failed to upload public key: ${body['message']}');
    }
  }
  
  /// 处理公钥请求响应
  void _handleKeyResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final data = body['data'] as Map<String, dynamic>?;
      if (data != null) {
        final userId = data['user_id'] as int;
        final publicKey = data['public_key'] as String;
        _e2ee.cachePublicKey(userId, publicKey);
        debugPrint('Cached public key for user $userId');
      }
    }
  }
  
  /// 处理加密消息
  void _handleEncryptedMessage(Map<String, dynamic> body) {
    final message = Message.fromJson(body);
    final key = message.senderId;
    
    if (!_messages.containsKey(key)) {
      _messages[key] = [];
    }
    _messages[key]!.add(message);
    
    // 保存到本地数据库
    _messageDb.saveMessage(message);
    
    _updateConversation(message);
    notifyListeners();
  }
  
  /// 端到端加密是否启用
  bool get e2eeEnabled => _e2eeEnabled;
  
  /// 撤回消息
  Future<bool> recallMessage(int messageId, {bool isGroup = false, int? groupId}) async {
    if (!_isAuthenticated || _currentUser == null) {
      return false;
    }
    
    _recallSuccess = false;
    _recallError = null;
    
    _network.send(WsMessageType.messageRecall, {
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
    
    _recallError = 'Recall timeout';
    return false;
  }
  
  bool _recallSuccess = false;
  String? _recallError;
  
  /// 获取撤回错误
  String? get recallError => _recallError;
  
  /// 处理消息撤回通知
  void _handleMessageRecall(Map<String, dynamic> body) {
    final messageId = body['message_id'] as int?;
    final isGroup = body['is_group'] as bool? ?? false;
    
    if (messageId == null) return;
    
    // 更新本地消息
    _updateRecalledMessage(messageId, isGroup);
  }
  
  /// 处理撤回响应
  void _handleMessageRecallResponse(Map<String, dynamic> body) {
    debugPrint('Received recall response: $body');
    
    // 检查响应格式
    final code = body['code'] as int?;
    if (code != null && code != 0) {
      _recallError = body['message'] as String? ?? 'Failed to recall message';
      notifyListeners();
      return;
    }
    
    final data = body['data'] as Map<String, dynamic>?;
    if (data == null) {
      debugPrint('Recall response missing data field');
      _recallError = 'Invalid response format';
      notifyListeners();
      return;
    }
    
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
    debugPrint('Updating recalled message: messageId=$messageId, isGroup=$isGroup');
    
    // 遍历所有会话找到该消息
    bool found = false;
    for (var entry in _messages.entries) {
      final messages = entry.value;
      for (var i = 0; i < messages.length; i++) {
        if (messages[i].messageId == messageId) {
          found = true;
          debugPrint('Found message at key=${entry.key}, index=$i');
          
          // 创建新的已撤回消息
          final recalledMessage = Message(
            messageId: messages[i].messageId,
            senderId: messages[i].senderId,
            receiverId: messages[i].receiverId,
            groupId: messages[i].groupId,
            mediaType: 0, // 文本类型
            content: '[消息已撤回]',
            mediaUrl: '',
            extra: '',
            status: messages[i].status,
            createdAt: messages[i].createdAt,
          );
          
          messages[i] = recalledMessage;
          
          debugPrint('Message updated in memory: ${recalledMessage.content}');
          
          // 更新本地数据库
          _messageDb.saveMessage(recalledMessage).then((_) {
            debugPrint('Message saved to local database');
          });
          
          notifyListeners();
          return;
        }
      }
    }
    
    if (!found) {
      debugPrint('Message not found in memory: messageId=$messageId');
      debugPrint('Available conversation keys: ${_messages.keys.toList()}');
    }
  }
  
  // ==================== 推送通知相关 ====================
  
  /// 注册推送通知 Token 到服务器
  /// FCM 模式: 注册 FCM Token (国外)
  /// JPush 模式: 注册 JPush Registration ID (国内)
  Future<void> _registerFcmToken() async {
    try {
      final storage = StorageService();
      
      if (storage.useFCMPush) {
        // FCM 模式 (国外)
        final fcmService = FcmService();
        await fcmService.init();
        
        final token = await fcmService.getToken();
        if (token != null && token.isNotEmpty) {
          debugPrint('注册 FCM Token: $token');
          _network.send(WsMessageType.fcmTokenRegister, {
            'fcm_token': token,
          });
        }
      } else {
        // JPush 模式 (国内) - 优先使用极光推送
        await _initJPush();
        
        // 同时启动本地后台服务作为备选
        final bgService = BackgroundService();
        if (!bgService.isRunning) {
          await bgService.startService();
        }
      }
    } catch (e) {
      debugPrint('注册推送通知失败: $e');
    }
  }
  
  /// 初始化极光推送
  Future<void> _initJPush() async {
    try {
      // JPush 已在 main.dart 中初始化，这里只需要设置回调和注册 Token
      _jPush.onNotificationReceived = (message) {
        debugPrint('收到 JPush 通知: $message');
      };
      
      _jPush.onNotificationOpened = (message) {
        debugPrint('点击 JPush 通知: $message');
      };
      
      // 获取 Registration ID 并注册
      final registrationId = _jPush.registrationId;
      if (registrationId != null && registrationId.isNotEmpty) {
        debugPrint('JPush Registration ID: $registrationId');
        registerJPushToken(registrationId);
        if (_currentUser != null) {
          _jPush.setAlias('user_${_currentUser!.userId}');
        }
      } else {
        // 如果还没有获取到，设置回调等待
        _jPush.onRegistrationIdReceived = (regId) {
          debugPrint('收到 JPush Registration ID: $regId');
          registerJPushToken(regId);
          if (_currentUser != null) {
            _jPush.setAlias('user_${_currentUser!.userId}');
          }
        };
      }
      
      debugPrint('JPush 配置完成');
    } catch (e) {
      debugPrint('配置 JPush 失败: $e');
    }
  }
  
  /// 注册 JPush Token 到服务器
  void registerJPushToken(String registrationId) {
    debugPrint('注册 JPush Registration ID: $registrationId');
    _network.send(WsMessageType.fcmTokenRegister, {
      'fcm_token': registrationId,
      'token_type': 'jpush',
    });
  }
  
  /// 获取图片消息（用于相册功能）
  Future<List<Message>> getImageMessages({int limit = 100, int beforeTime = 0}) async {
    return await _messageDb.getImageMessages(limit: limit, beforeTime: beforeTime);
  }
  
  /// 获取文件消息（用于文件管理功能）
  Future<List<Message>> getFileMessages({int limit = 100, int beforeTime = 0}) async {
    return await _messageDb.getFileMessages(limit: limit, beforeTime: beforeTime);
  }
  
  // 收藏相关状态
  final List<Favorite> _favorites = [];
  bool _favoriteAddSuccess = false;
  String? _favoriteError;
  
  List<Favorite> get favorites => _favorites;
  bool get favoriteAddSuccess => _favoriteAddSuccess;
  String? get favoriteError => _favoriteError;
  
  /// 添加收藏
  Future<bool> addFavorite({
    required int messageId,
    required String messageType,
    required int senderId,
    required String content,
    required int mediaType,
    required String mediaUrl,
  }) async {
    _favoriteAddSuccess = false;
    _favoriteError = null;
    
    _network.send(WsMessageType.favoriteAdd, {
      'message_id': messageId,
      'message_type': messageType,
      'sender_id': senderId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
    });
    
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_favoriteAddSuccess || _favoriteError != null) {
        return _favoriteAddSuccess;
      }
    }
    
    _favoriteError = '请求超时';
    return false;
  }
  
  /// 移除收藏
  Future<bool> removeFavorite(int messageId) async {
    _favoriteError = null;
    
    _network.send(WsMessageType.favoriteRemove, {
      'message_id': messageId,
    });
    
    for (int i = 0; i < 50; i++) {
      await Future.delayed(const Duration(milliseconds: 100));
      if (_favoriteError != null || !_favorites.any((f) => f.messageId == messageId)) {
        return _favoriteError == null;
      }
    }
    
    return false;
  }
  
  /// 获取收藏列表
  void loadFavorites() {
    _network.send(WsMessageType.favoriteList, {});
  }
  
  void _handleFavoriteAddResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      _favoriteAddSuccess = true;
      loadFavorites();
    } else {
      _favoriteAddSuccess = false;
      _favoriteError = body['message'] as String? ?? '收藏失败';
    }
    notifyListeners();
  }
  
  void _handleFavoriteRemoveResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final messageId = body['message_id'] as int?;
      if (messageId != null) {
        _favorites.removeWhere((f) => f.messageId == messageId);
      }
    } else {
      _favoriteError = body['message'] as String? ?? '取消收藏失败';
    }
    notifyListeners();
  }
  
  void _handleFavoriteListResponse(Map<String, dynamic> body) {
    final code = body['code'] ?? -1;
    if (code == 0) {
      final list = body['favorites'] as List<dynamic>? ?? [];
      _favorites.clear();
      for (final item in list) {
        _favorites.add(Favorite.fromJson(item as Map<String, dynamic>));
      }
    }
    notifyListeners();
  }
}
