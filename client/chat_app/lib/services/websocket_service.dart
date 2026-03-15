import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:web_socket_channel/status.dart' as status;
import '../models/protocol.dart';

/// WebSocket 消息类型映射
/// 服务器端使用 JSON 文本消息，type 字段标识消息类型
class WsMessageType {
  // 认证相关
  static const String login = 'login';
  static const String loginResponse = 'loginResponse';
  static const String register = 'register';
  static const String registerResponse = 'registerResponse';
  static const String logout = 'logout';
  
  // 用户相关
  static const String userSearch = 'userSearch';
  static const String userSearchResponse = 'userSearchResponse';
  static const String passwordUpdate = 'passwordUpdate';
  static const String passwordUpdateResponse = 'passwordUpdateResponse';
  static const String userInfo = 'userInfo';
  static const String userInfoResponse = 'userInfoResponse';
  static const String userUpdate = 'userUpdate';
  static const String userUpdateResponse = 'userUpdateResponse';
  
  // 好友相关
  static const String friendRequest = 'friendRequest';
  static const String friendAddResponse = 'friendAddResponse';
  static const String friendRequestNotification = 'friendRequestNotification';
  static const String friendAccept = 'friendAccept';
  static const String friendAcceptResponse = 'friendAcceptResponse';
  static const String friendAcceptNotification = 'friendAcceptNotification';
  static const String friendReject = 'friendReject';
  static const String friendRejectResponse = 'friendRejectResponse';
  static const String friendRemove = 'friendRemove';
  static const String friendRemoveResponse = 'friendRemoveResponse';
  static const String friendList = 'friendList';
  static const String friendListResponse = 'friendListResponse';
  static const String friendRequests = 'friendRequests';
  static const String friendRequestsResponse = 'friendRequestsResponse';
  static const String friendRemark = 'friendRemark';
  static const String friendRemarkResponse = 'friendRemarkResponse';
  
  // 私聊消息
  static const String privateMessage = 'privateMessage';
  static const String privateMessageResponse = 'privateMessageResponse';
  static const String privateHistory = 'privateHistory';
  static const String privateHistoryResponse = 'privateHistoryResponse';
  
  // 群组相关
  static const String createGroup = 'createGroup';
  static const String createGroupResponse = 'createGroupResponse';
  static const String inviteGroupMembers = 'inviteGroupMembers';
  static const String inviteGroupMembersResponse = 'inviteGroupMembersResponse';
  static const String groupKick = 'groupKick';
  static const String groupKickResponse = 'groupKickResponse';
  static const String groupQuit = 'groupQuit';
  static const String groupQuitResponse = 'groupQuitResponse';
  static const String groupDismiss = 'groupDismiss';
  static const String groupDismissResponse = 'groupDismissResponse';
  static const String groupSetAdmin = 'groupSetAdmin';
  static const String groupSetAdminResponse = 'groupSetAdminResponse';
  static const String groupTransferOwner = 'groupTransferOwner';
  static const String groupTransferOwnerResponse = 'groupTransferOwnerResponse';
  static const String groupList = 'groupList';
  static const String groupListResponse = 'groupListResponse';
  static const String groupMembers = 'groupMembers';
  static const String groupMembersResponse = 'groupMembersResponse';
  static const String groupJoin = 'groupJoin';
  static const String groupJoinResponse = 'groupJoinResponse';
  
  // 群聊消息
  static const String groupMessage = 'groupMessage';
  static const String groupMessageResponse = 'groupMessageResponse';
  static const String groupHistory = 'groupHistory';
  static const String groupHistoryResponse = 'groupHistoryResponse';
  
  // 多媒体
  static const String mediaUpload = 'mediaUpload';
  static const String mediaUploadResponse = 'mediaUploadResponse';
  
  // 心跳
  static const String heartbeat = 'heartbeat';
  static const String heartbeatResponse = 'heartbeatResponse';
  
  // FCM
  static const String fcmTokenRegister = 'fcmTokenRegister';
  static const String fcmTokenRegisterResponse = 'fcmTokenRegisterResponse';
  
  // 收藏
  static const String favoriteAdd = 'favoriteAdd';
  static const String favoriteAddResponse = 'favoriteAddResponse';
  static const String favoriteRemove = 'favoriteRemove';
  static const String favoriteRemoveResponse = 'favoriteRemoveResponse';
  static const String favoriteList = 'favoriteList';
  static const String favoriteListResponse = 'favoriteListResponse';
  
  // 端到端加密
  static const String keyUpload = 'keyUpload';
  static const String keyUploadResponse = 'keyUploadResponse';
  static const String keyRequest = 'keyRequest';
  static const String keyResponse = 'keyResponse';
  static const String encryptedMessage = 'encryptedMessage';
  static const String encryptedMessageResponse = 'encryptedMessageResponse';
  
  // 消息撤回
  static const String messageRecall = 'messageRecall';
  static const String messageRecallResponse = 'messageRecallResponse';
  
  // 错误
  static const String error = 'error';
  static const String kicked = 'kicked';
}

typedef WsMessageCallback = void Function(String type, Map<String, dynamic> body);
typedef WsConnectionCallback = void Function(bool connected);
typedef WsErrorCallback = void Function(String error);

/// WebSocket 服务
/// 使用 WebSocket 连接服务器，发送/接收 JSON 文本消息
class WebSocketService {
  static final WebSocketService _instance = WebSocketService._internal();
  factory WebSocketService() => _instance;
  WebSocketService._internal();

  WebSocketChannel? _channel;
  bool _isConnected = false;
  bool _isConnecting = false;
  
  final List<WsMessageCallback> _messageCallbacks = [];
  final List<WsConnectionCallback> _connectionCallbacks = [];
  final List<WsErrorCallback> _errorCallbacks = [];
  
  StreamSubscription? _subscription;
  
  Timer? _heartbeatTimer;
  Timer? _heartbeatTimeoutTimer;
  Timer? _reconnectTimer;
  
  String _host = '127.0.0.1';
  int _port = 8888;
  String _wsPath = '/ws';
  bool _autoReconnect = true;
  int _reconnectDelay = 1000; // 初始重连延迟 1 秒
  int _maxReconnectDelay = 30000; // 最大重连延迟 30 秒
  int _heartbeatInterval = 30; // 心跳间隔 30 秒
  int _heartbeatTimeout = 10; // 心跳响应超时 10 秒
  int _missedHeartbeats = 0; // 连续未响应的心跳次数
  int _maxMissedHeartbeats = 3; // 最大允许未响应心跳次数
  int _connectionTimeout = 5; // 连接超时 5 秒
  
  // 保存登录凭据用于重连后自动登录
  String? _savedUsername;
  String? _savedPassword;
  
  // 重连回调
  Function(String username, String password)? _onReconnectLogin;

  bool get isConnected => _isConnected;
  String? get savedUsername => _savedUsername;
  String? get savedPassword => _savedPassword;

  /// 添加消息回调
  void addMessageCallback(WsMessageCallback callback) {
    _messageCallbacks.add(callback);
  }

  /// 移除消息回调
  void removeMessageCallback(WsMessageCallback callback) {
    _messageCallbacks.remove(callback);
  }

  /// 添加连接状态回调
  void addConnectionCallback(WsConnectionCallback callback) {
    _connectionCallbacks.add(callback);
  }

  /// 移除连接状态回调
  void removeConnectionCallback(WsConnectionCallback callback) {
    _connectionCallbacks.remove(callback);
  }

  /// 添加错误回调
  void addErrorCallback(WsErrorCallback callback) {
    _errorCallbacks.add(callback);
  }

  /// 移除错误回调
  void removeErrorCallback(WsErrorCallback callback) {
    _errorCallbacks.remove(callback);
  }
  
  /// 设置重连登录回调
  void setReconnectLoginCallback(Function(String username, String password) callback) {
    _onReconnectLogin = callback;
  }
  
  /// 保存登录凭据
  void saveCredentials(String username, String password) {
    _savedUsername = username;
    _savedPassword = password;
  }
  
  /// 清除登录凭据
  void clearCredentials() {
    _savedUsername = null;
    _savedPassword = null;
  }

  /// 连接服务器
  Future<bool> connect(String host, int port, {String path = '/ws'}) async {
    if (_isConnecting) return false;
    
    _host = host;
    _port = port;
    _wsPath = path;
    
    _isConnecting = true;
    
    try {
      final uri = Uri.parse('ws://$host:$port$path');
      
      debugPrint('[WS] Connecting to $uri');
      
      // 创建 WebSocket 连接
      _channel = WebSocketChannel.connect(
        uri,
      );
      
      // 等待连接就绪
      await _channel!.ready.timeout(
        Duration(seconds: _connectionTimeout),
        onTimeout: () {
          throw TimeoutException('Connection timeout');
        },
      );
      
      _isConnected = true;
      _isConnecting = false;
      _missedHeartbeats = 0;
      _reconnectDelay = 1000; // 重置重连延迟
      
      _notifyConnection(true);
      _startListening();
      _startHeartbeat();
      
      debugPrint('[WS] Connected to $host:$port$path');
      return true;
    } catch (e) {
      _isConnected = false;
      _isConnecting = false;
      debugPrint('[WS] Connection failed: $e');
      _notifyError('Connection failed: $e');
      if (_autoReconnect) {
        _scheduleReconnect();
      }
      return false;
    }
  }

  /// 断开连接
  void disconnect() {
    _autoReconnect = false;
    _stopHeartbeat();
    _reconnectTimer?.cancel();
    _subscription?.cancel();
    _subscription = null;
    _channel?.sink.close(status.goingAway);
    _channel = null;
    _isConnected = false;
    _isConnecting = false;
    _notifyConnection(false);
  }

  /// 发送消息 (JSON 格式)
  bool send(String type, Map<String, dynamic> body) {
    if (!_isConnected || _channel == null) {
      _notifyError('Not connected');
      return false;
    }

    try {
      final message = {
        'type': type,
        ...body,
      };
      final jsonStr = jsonEncode(message);
      _channel!.sink.add(jsonStr);
      debugPrint('[WS] Sent: $jsonStr');
      return true;
    } catch (e) {
      _notifyError('Send failed: $e');
      return false;
    }
  }

  /// 发送原始 JSON 消息
  bool sendJson(Map<String, dynamic> message) {
    if (!_isConnected || _channel == null) {
      _notifyError('Not connected');
      return false;
    }

    try {
      final jsonStr = jsonEncode(message);
      _channel!.sink.add(jsonStr);
      return true;
    } catch (e) {
      _notifyError('Send failed: $e');
      return false;
    }
  }

  /// 开始监听
  void _startListening() {
    _subscription = _channel!.stream.listen(
      (data) {
        _handleData(data);
      },
      onError: (error) {
        debugPrint('[WS] Socket error: $error');
        _handleDisconnect();
      },
      onDone: () {
        debugPrint('[WS] Socket closed by server');
        _handleDisconnect();
      },
    );
  }

  /// 处理接收数据
  void _handleData(dynamic data) {
    try {
      final jsonStr = data as String;
      debugPrint('[WS] Received: $jsonStr');
      
      final body = jsonDecode(jsonStr) as Map<String, dynamic>;
      final type = body['type'] as String? ?? WsMessageType.error;
      
      // 处理心跳响应
      if (type == WsMessageType.heartbeatResponse) {
        _handleHeartbeatResponse();
        return;
      }
      
      _notifyMessage(type, body);
    } catch (e) {
      debugPrint('[WS] Parse error: $e');
      _notifyError('Parse error: $e');
    }
  }

  /// 处理断开连接
  void _handleDisconnect() {
    _isConnected = false;
    _stopHeartbeat();
    _notifyConnection(false);
    
    if (_autoReconnect) {
      _scheduleReconnect();
    }
  }

  /// 安排重连（指数退避）
  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    
    debugPrint('[WS] Scheduling reconnect in ${_reconnectDelay}ms');
    
    _reconnectTimer = Timer(Duration(milliseconds: _reconnectDelay), () async {
      debugPrint('[WS] Attempting to reconnect...');
      final success = await connect(_host, _port, path: _wsPath);
      
      if (success && _savedUsername != null && _savedPassword != null) {
        // 重连成功后自动登录
        debugPrint('[WS] Reconnected, attempting auto-login...');
        if (_onReconnectLogin != null) {
          _onReconnectLogin!(_savedUsername!, _savedPassword!);
        }
      }
      
      if (!success) {
        // 指数退避增加重连延迟
        _reconnectDelay = (_reconnectDelay * 2).clamp(1000, _maxReconnectDelay);
      }
    });
  }

  /// 开始心跳
  void _startHeartbeat() {
    _stopHeartbeat();
    _missedHeartbeats = 0;
    
    _heartbeatTimer = Timer.periodic(Duration(seconds: _heartbeatInterval), (_) {
      if (_isConnected) {
        _sendHeartbeat();
      }
    });
  }
  
  /// 发送心跳
  void _sendHeartbeat() {
    if (!_isConnected) return;
    
    debugPrint('[WS] Sending heartbeat, missed: $_missedHeartbeats');
    send(WsMessageType.heartbeat, {});
    
    // 设置心跳超时检测
    _heartbeatTimeoutTimer?.cancel();
    _heartbeatTimeoutTimer = Timer(Duration(seconds: _heartbeatTimeout), () {
      if (_isConnected) {
        _missedHeartbeats++;
        debugPrint('[WS] Heartbeat timeout, missed: $_missedHeartbeats');
        
        if (_missedHeartbeats >= _maxMissedHeartbeats) {
          // 连续多次心跳无响应，认为连接已断开
          debugPrint('[WS] Too many missed heartbeats, disconnecting...');
          _channel?.sink.close(status.goingAway);
          _handleDisconnect();
        }
      }
    });
  }
  
  /// 处理心跳响应
  void _handleHeartbeatResponse() {
    debugPrint('[WS] Heartbeat response received');
    _missedHeartbeats = 0;
    _heartbeatTimeoutTimer?.cancel();
  }

  /// 停止心跳
  void _stopHeartbeat() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
    _heartbeatTimeoutTimer?.cancel();
    _heartbeatTimeoutTimer = null;
  }

  /// 通知消息
  void _notifyMessage(String type, Map<String, dynamic> body) {
    for (final callback in _messageCallbacks) {
      callback(type, body);
    }
  }

  /// 通知连接状态
  void _notifyConnection(bool connected) {
    for (final callback in _connectionCallbacks) {
      callback(connected);
    }
  }

  /// 通知错误
  void _notifyError(String error) {
    for (final callback in _errorCallbacks) {
      callback(error);
    }
  }
}
