import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:jpush_flutter/jpush_flutter.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'chat_service.dart';

/// 极光推送服务
/// 支持国内各厂商推送通道 (小米/华为/OPPO/vivo等)
class JPushService {
  static final JPushService _instance = JPushService._internal();
  factory JPushService() => _instance;
  JPushService._internal();

  static const String _keyRegistrationId = 'jpush_registration_id';
  
  final JPush _jPush = JPush();
  String? _registrationId;
  bool _isInitialized = false;
  
  // 回调
  Function(Map<String, dynamic> message)? onNotificationReceived;
  Function(Map<String, dynamic> message)? onNotificationOpened;
  
  String? get registrationId => _registrationId;
  bool get isInitialized => _isInitialized;
  
  /// 初始化极光推送
  Future<bool> init() async {
    if (_isInitialized) return true;
    
    try {
      // 设置调试模式
      _jPush.setDebugMode(enable: kDebugMode);
      
      // 初始化 JPush
      _jPush.addEventHandler(
        // 接收通知回调
        onReceiveNotification: (Map<String, dynamic> message) async {
          debugPrint('JPush onReceiveNotification: $message');
          onNotificationReceived?.call(message);
        },
        // 点击通知回调
        onOpenNotification: (Map<String, dynamic> message) async {
          debugPrint('JPush onOpenNotification: $message');
          onNotificationOpened?.call(message);
          _handleNotificationOpen(message);
        },
        // 接收自定义消息回调
        onReceiveMessage: (Map<String, dynamic> message) async {
          debugPrint('JPush onReceiveMessage: $message');
        },
        // 通知授权状态变化
        onNotifyMessageUnShow: (Map<String, dynamic> message) async {
          debugPrint('JPush onNotifyMessageUnShow: $message');
        },
        // 通知设置状态
        onNotifyMessageOpened: (Map<String, dynamic> message) async {
          debugPrint('JPush onNotifyMessageOpened: $message');
        },
        // 连接状态
        onConnected: (Map<String, dynamic> message) async {
          debugPrint('JPush onConnected: $message');
        },
      );
      
      // 获取 Registration ID
      _registrationId = await _jPush.getRegistrationID();
      debugPrint('JPush Registration ID: $_registrationId');
      
      if (_registrationId != null && _registrationId!.isNotEmpty) {
        // 保存到本地
        final prefs = await SharedPreferences.getInstance();
        await prefs.setString(_keyRegistrationId, _registrationId!);
        
        // 上传到服务器
        await _uploadRegistrationId(_registrationId!);
      }
      
      _isInitialized = true;
      debugPrint('JPush 初始化成功');
      return true;
    } catch (e) {
      debugPrint('JPush 初始化失败: $e');
      return false;
    }
  }
  
  /// 上传 Registration ID 到服务器
  Future<void> _uploadRegistrationId(String registrationId) async {
    try {
      final chatService = ChatService();
      if (chatService.isConnected) {
        chatService.registerJPushToken(registrationId);
        debugPrint('JPush Registration ID 已上传到服务器');
      }
    } catch (e) {
      debugPrint('上传 JPush Registration ID 失败: $e');
    }
  }
  
  /// 处理通知点击
  void _handleNotificationOpen(Map<String, dynamic> message) {
    try {
      // 解析消息数据
      final extras = message['extras'] as Map<String, dynamic>?;
      if (extras == null) return;
      
      final type = extras['type'] as String?;
      final senderId = extras['sender_id'];
      final groupId = extras['group_id'];
      
      // TODO: 根据类型跳转到对应页面
      debugPrint('通知点击 - type: $type, senderId: $senderId, groupId: $groupId');
    } catch (e) {
      debugPrint('处理通知点击失败: $e');
    }
  }
  
  /// 设置别名 (用户ID)
  Future<bool> setAlias(String alias) async {
    try {
      await _jPush.setAlias(alias);
      debugPrint('JPush 设置别名成功: $alias');
      return true;
    } catch (e) {
      debugPrint('JPush 设置别名失败: $e');
      return false;
    }
  }
  
  /// 删除别名
  Future<bool> deleteAlias() async {
    try {
      await _jPush.deleteAlias();
      debugPrint('JPush 删除别名成功');
      return true;
    } catch (e) {
      debugPrint('JPush 删除别名失败: $e');
      return false;
    }
  }
  
  /// 设置标签
  Future<bool> setTags(List<String> tags) async {
    try {
      await _jPush.setTags(tags);
      debugPrint('JPush 设置标签成功: $tags');
      return true;
    } catch (e) {
      debugPrint('JPush 设置标签失败: $e');
      return false;
    }
  }
  
  /// 清除所有通知
  Future<void> clearAllNotifications() async {
    try {
      await _jPush.clearAllNotifications();
      debugPrint('JPush 清除所有通知');
    } catch (e) {
      debugPrint('JPush 清除通知失败: $e');
    }
  }
  
  /// 申请通知权限 (Android 13+)
  Future<void> requestPermission() async {
    if (Platform.isAndroid) {
      try {
        await _jPush.applyPushAuthority(
          NotificationSettingsIOS(
            sound: true,
            alert: true,
            badge: true,
          ),
        );
        debugPrint('JPush 申请通知权限');
      } catch (e) {
        debugPrint('JPush 申请权限失败: $e');
      }
    }
  }
  
  /// 停止推送服务
  Future<void> stop() async {
    try {
      // JPush 不支持停止，只能删除别名
      await deleteAlias();
      _isInitialized = false;
      debugPrint('JPush 服务已停止');
    } catch (e) {
      debugPrint('JPush 停止失败: $e');
    }
  }
}
