import 'package:flutter/foundation.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'notification_service.dart';

/// 后台消息处理器 (必须是顶级函数)
@pragma('vm:entry-point')
Future<void> _firebaseMessagingBackgroundHandler(RemoteMessage message) async {
  try {
    // 初始化 Firebase
    await Firebase.initializeApp();
    debugPrint('后台消息: ${message.notification?.title} - ${message.notification?.body}');
  } catch (e) {
    debugPrint('后台消息处理错误: $e');
  }
}

class FcmService {
  static final FcmService _instance = FcmService._internal();
  factory FcmService() => _instance;
  FcmService._internal();

  FirebaseMessaging? _messaging;
  final NotificationService _notificationService = NotificationService();
  
  String? _fcmToken;
  bool _initialized = false;
  bool _initFailed = false;
  
  // Token 更新回调
  Function(String token)? onTokenRefresh;

  String? get fcmToken => _fcmToken;
  bool get isInitialized => _initialized;
  bool get isAvailable => _initialized && !_initFailed;

  /// 初始化 FCM 服务
  Future<void> init() async {
    if (_initialized || _initFailed) return;

    try {
      // 初始化 Firebase
      await Firebase.initializeApp();
      _messaging = FirebaseMessaging.instance;
      debugPrint('Firebase 初始化成功');

      // 设置后台消息处理器
      FirebaseMessaging.onBackgroundMessage(_firebaseMessagingBackgroundHandler);

      // 请求权限 (iOS)
      final settings = await _messaging!.requestPermission(
        alert: true,
        badge: true,
        sound: true,
        provisional: false,
      );
      debugPrint('FCM 权限状态: ${settings.authorizationStatus}');

      // 获取 FCM Token
      _fcmToken = await _messaging!.getToken();
      debugPrint('FCM Token: $_fcmToken');

      // 监听 Token 刷新
      _messaging!.onTokenRefresh.listen((newToken) {
        _fcmToken = newToken;
        debugPrint('FCM Token 刷新: $newToken');
        onTokenRefresh?.call(newToken);
      });

      // 前台消息处理
      FirebaseMessaging.onMessage.listen((RemoteMessage message) {
        debugPrint('前台消息: ${message.notification?.title}');
        _handleForegroundMessage(message);
      });

      // 点击通知打开 App
      FirebaseMessaging.onMessageOpenedApp.listen((RemoteMessage message) {
        debugPrint('点击通知打开: ${message.data}');
        _handleMessageOpenedApp(message);
      });

      // 检查 App 是否从通知打开
      final initialMessage = await _messaging!.getInitialMessage();
      if (initialMessage != null) {
        debugPrint('从通知启动: ${initialMessage.data}');
        _handleMessageOpenedApp(initialMessage);
      }

      _initialized = true;
      debugPrint('FCM 服务初始化完成');
    } catch (e) {
      _initFailed = true;
      debugPrint('FCM 初始化失败，推送通知将不可用: $e');
      // 不抛出异常，允许应用继续运行
    }
  }

  /// 处理前台消息
  void _handleForegroundMessage(RemoteMessage message) {
    final notification = message.notification;
    final data = message.data;
    
    if (notification != null) {
      // 显示本地通知
      _notificationService.showMessageNotification(
        id: notification.hashCode,
        title: notification.title ?? '新消息',
        body: notification.body ?? '',
        payload: data['payload'],
      );
    }
  }

  /// 处理点击通知打开 App
  void _handleMessageOpenedApp(RemoteMessage message) {
    final data = message.data;
    debugPrint('处理通知点击: $data');
    // 可以在这里导航到特定聊天页面
  }

  /// 获取当前 Token
  Future<String?> getToken() async {
    if (!_initialized) {
      await init();
    }
    return _fcmToken;
  }

  /// 删除 Token (用于登出)
  Future<void> deleteToken() async {
    if (_messaging != null) {
      await _messaging!.deleteToken();
      _fcmToken = null;
      debugPrint('FCM Token 已删除');
    }
  }

  /// 订阅主题
  Future<void> subscribeToTopic(String topic) async {
    if (_messaging != null) {
      await _messaging!.subscribeToTopic(topic);
      debugPrint('订阅主题: $topic');
    }
  }

  /// 取消订阅主题
  Future<void> unsubscribeFromTopic(String topic) async {
    if (_messaging != null) {
      await _messaging!.unsubscribeFromTopic(topic);
      debugPrint('取消订阅主题: $topic');
    }
  }
}
