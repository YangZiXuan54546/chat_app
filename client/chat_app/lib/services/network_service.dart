import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import '../models/protocol.dart';

typedef MessageCallback = void Function(MessageType type, int sequence, Map<String, dynamic> body);
typedef ConnectionCallback = void Function(bool connected);
typedef ErrorCallback = void Function(String error);

class NetworkService {
  static final NetworkService _instance = NetworkService._internal();
  factory NetworkService() => _instance;
  NetworkService._internal();

  Socket? _socket;
  bool _isConnected = false;
  final List<int> _buffer = [];
  
  final List<MessageCallback> _messageCallbacks = [];
  final List<ConnectionCallback> _connectionCallbacks = [];
  final List<ErrorCallback> _errorCallbacks = [];
  
  Timer? _heartbeatTimer;
  Timer? _heartbeatTimeoutTimer;
  Timer? _reconnectTimer;
  
  String _host = 'localhost';
  int _port = 8888;
  bool _autoReconnect = true;
  int _reconnectDelay = 1000; // 初始重连延迟 1 秒
  int _maxReconnectDelay = 30000; // 最大重连延迟 30 秒
  int _heartbeatInterval = 30; // 心跳间隔 30 秒
  int _heartbeatTimeout = 10; // 心跳响应超时 10 秒
  int _missedHeartbeats = 0; // 连续未响应的心跳次数
  int _maxMissedHeartbeats = 3; // 最大允许未响应心跳次数
  
  // 保存登录凭据用于重连后自动登录
  String? _savedUsername;
  String? _savedPassword;
  
  // 重连回调
  Function(String username, String password)? _onReconnectLogin;

  bool get isConnected => _isConnected;
  String? get savedUsername => _savedUsername;
  String? get savedPassword => _savedPassword;

  /// 添加消息回调
  void addMessageCallback(MessageCallback callback) {
    _messageCallbacks.add(callback);
  }

  /// 移除消息回调
  void removeMessageCallback(MessageCallback callback) {
    _messageCallbacks.remove(callback);
  }

  /// 添加连接状态回调
  void addConnectionCallback(ConnectionCallback callback) {
    _connectionCallbacks.add(callback);
  }

  /// 移除连接状态回调
  void removeConnectionCallback(ConnectionCallback callback) {
    _connectionCallbacks.remove(callback);
  }

  /// 添加错误回调
  void addErrorCallback(ErrorCallback callback) {
    _errorCallbacks.add(callback);
  }

  /// 移除错误回调
  void removeErrorCallback(ErrorCallback callback) {
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
  Future<bool> connect(String host, int port) async {
    _host = host;
    _port = port;
    
    try {
      _socket = await Socket.connect(host, port);
      _isConnected = true;
      _missedHeartbeats = 0;
      _reconnectDelay = 1000; // 重置重连延迟
      
      _notifyConnection(true);
      _startListening();
      _startHeartbeat();
      
      debugPrint('Connected to $host:$port');
      return true;
    } catch (e) {
      _isConnected = false;
      _notifyError('Connection failed: $e');
      _scheduleReconnect();
      return false;
    }
  }

  /// 断开连接
  void disconnect() {
    _autoReconnect = false;
    _stopHeartbeat();
    _reconnectTimer?.cancel();
    _socket?.destroy();
    _socket = null;
    _isConnected = false;
    _buffer.clear();
    _notifyConnection(false);
  }

  /// 发送消息
  bool send(MessageType type, Map<String, dynamic> body) {
    if (!_isConnected || _socket == null) {
      _notifyError('Not connected');
      return false;
    }

    try {
      final data = Protocol.createRequest(type, body);
      _socket!.add(data);
      return true;
    } catch (e) {
      _notifyError('Send failed: $e');
      return false;
    }
  }

  /// 发送原始数据
  bool sendRaw(Uint8List data) {
    if (!_isConnected || _socket == null) {
      _notifyError('Not connected');
      return false;
    }

    try {
      _socket!.add(data);
      return true;
    } catch (e) {
      _notifyError('Send failed: $e');
      return false;
    }
  }

  /// 开始监听
  void _startListening() {
    _socket!.listen(
      (data) {
        _buffer.addAll(data);
        _processBuffer();
      },
      onError: (error) {
        debugPrint('Socket error: $error');
        _handleDisconnect();
      },
      onDone: () {
        debugPrint('Socket closed by server');
        _handleDisconnect();
      },
    );
  }

  /// 处理接收缓冲区
  void _processBuffer() {
    while (_buffer.length >= 9) {
      // 解析消息头 (9 字节)
      final headerData = Uint8List.fromList(_buffer.sublist(0, 9));
      final header = Protocol.parseHeader(headerData);
      
      if (header == null) {
        _buffer.removeRange(0, 9);
        continue;
      }

      // 检查是否有完整的消息体
      if (_buffer.length < 9 + header.length) {
        break;
      }

      // 提取消息体
      final bodyData = Uint8List.fromList(
        _buffer.sublist(9, 9 + header.length),
      );
      _buffer.removeRange(0, 9 + header.length);

      // 解析并回调
      final body = Protocol.parseBody(bodyData);
      
      // 处理心跳响应
      if (header.type == MessageType.heartbeatResponse) {
        _handleHeartbeatResponse();
      } else {
        _notifyMessage(header.type, header.sequence, body);
      }
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
    
    debugPrint('Scheduling reconnect in ${_reconnectDelay}ms');
    
    _reconnectTimer = Timer(Duration(milliseconds: _reconnectDelay), () async {
      debugPrint('Attempting to reconnect...');
      final success = await connect(_host, _port);
      
      if (success && _savedUsername != null && _savedPassword != null) {
        // 重连成功后自动登录
        debugPrint('Reconnected, attempting auto-login...');
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
    
    debugPrint('Sending heartbeat, missed: $_missedHeartbeats');
    send(MessageType.heartbeat, {});
    
    // 设置心跳超时检测
    _heartbeatTimeoutTimer?.cancel();
    _heartbeatTimeoutTimer = Timer(Duration(seconds: _heartbeatTimeout), () {
      if (_isConnected) {
        _missedHeartbeats++;
        debugPrint('Heartbeat timeout, missed: $_missedHeartbeats');
        
        if (_missedHeartbeats >= _maxMissedHeartbeats) {
          // 连续多次心跳无响应，认为连接已断开
          debugPrint('Too many missed heartbeats, disconnecting...');
          _handleDisconnect();
        }
      }
    });
  }
  
  /// 处理心跳响应
  void _handleHeartbeatResponse() {
    debugPrint('Heartbeat response received');
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
  void _notifyMessage(MessageType type, int sequence, Map<String, dynamic> body) {
    for (final callback in _messageCallbacks) {
      callback(type, sequence, body);
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