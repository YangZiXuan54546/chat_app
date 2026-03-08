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
  Timer? _reconnectTimer;
  
  String _host = 'localhost';
  int _port = 8888;
  bool _autoReconnect = true;
  int _reconnectDelay = 3000;

  bool get isConnected => _isConnected;

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

  /// 连接服务器
  Future<bool> connect(String host, int port) async {
    _host = host;
    _port = port;
    
    try {
      _socket = await Socket.connect(host, port);
      _isConnected = true;
      
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
    while (_buffer.length >= 12) {
      // 解析消息头
      final headerData = Uint8List.fromList(_buffer.sublist(0, 12));
      final header = Protocol.parseHeader(headerData);
      
      if (header == null) {
        _buffer.removeRange(0, 12);
        continue;
      }

      // 检查是否有完整的消息体
      if (_buffer.length < 12 + header.length) {
        break;
      }

      // 提取消息体
      final bodyData = Uint8List.fromList(
        _buffer.sublist(12, 12 + header.length),
      );
      _buffer.removeRange(0, 12 + header.length);

      // 解析并回调
      final body = Protocol.parseBody(bodyData);
      _notifyMessage(header.type, header.sequence, body);
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

  /// 安排重连
  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(milliseconds: _reconnectDelay), () {
      connect(_host, _port);
    });
  }

  /// 开始心跳
  void _startHeartbeat() {
    _stopHeartbeat();
    _heartbeatTimer = Timer.periodic(const Duration(seconds: 30), (_) {
      send(MessageType.heartbeat, {});
    });
  }

  /// 停止心跳
  void _stopHeartbeat() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
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
