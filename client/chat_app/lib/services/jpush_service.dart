import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:jpush_flutter/jpush_flutter.dart';
import 'package:shared_preferences/shared_preferences.dart';

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
  Function(String registrationId)? onRegistrationIdReceived;
  
  String? get registrationId => _registrationId;
  bool get isInitialized => _isInitialized;
  
  /// 初始化极光推送
  Future<bool> init() async {
    if (_isInitialized) return true;
    
    try {
      debugPrint('JPush 开始初始化...');
      
      // 设置事件处理器
      _jPush.addEventHandler(
        onReceiveNotification: (Map<String, dynamic> message) async {
          debugPrint('JPush 收到通知: $message');
          onNotificationReceived?.call(message);
        },
        onOpenNotification: (Map<String, dynamic> message) async {
          debugPrint('JPush 打开通知: $message');
          onNotificationOpened?.call(message);
        },
        onReceiveMessage: (Map<String, dynamic> message) async {
          debugPrint('JPush 收到消息: $message');
        },
        onConnected: (Map<String, dynamic> message) async {
          debugPrint('JPush 已连接: $message');
        },
      );
      
      // 初始化 JPush (使用配置的 AppKey)
      // 注意：AppKey 已在 AndroidManifest.xml 中配置
      _jPush.setup(
        appKey: '16d9f5ae7a467d54f3d9f775',
        channel: 'developer-default',
        production: false,
        debug: kDebugMode,
      );
      
      _isInitialized = true;
      debugPrint('JPush 初始化完成 (异步)');
      
      // 异步获取 Registration ID（不阻塞主流程）
      _getRegistrationIdAsync();
      
      return true;
    } catch (e) {
      debugPrint('JPush 初始化失败: $e');
      return false;
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
      debugPrint('JPush 通知权限由系统管理');
    }
  }
  
  /// 停止推送服务
  Future<void> stop() async {
    try {
      await deleteAlias();
      _isInitialized = false;
      debugPrint('JPush 服务已停止');
    } catch (e) {
      debugPrint('JPush 停止失败: $e');
    }
  }
  
  /// 异步获取 Registration ID（非阻塞）
  void _getRegistrationIdAsync() {
    Future(() async {
      // 最多尝试 10 次，每次间隔 500ms
      for (int i = 0; i < 10; i++) {
        try {
          final regId = await _jPush.getRegistrationID();
          if (regId != null && regId.isNotEmpty) {
            _registrationId = regId;
            debugPrint('JPush Registration ID: $_registrationId');
            
            // 保存到本地
            final prefs = await SharedPreferences.getInstance();
            await prefs.setString(_keyRegistrationId, _registrationId!);
            
            // 通知外部
            onRegistrationIdReceived?.call(_registrationId!);
            return;
          }
        } catch (e) {
          debugPrint('获取 JPush Registration ID 失败 (尝试 ${i + 1}): $e');
        }
        
        await Future.delayed(const Duration(milliseconds: 500));
      }
      
      debugPrint('未能获取 JPush Registration ID');
    });
  }
}
