import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// 后台服务类型
enum BackgroundServiceType {
  /// FCM 推送 (国外)
  fcm,
  /// 本地后台服务 (国内)
  local,
}

/// 后台服务管理器
/// 支持两种模式：
/// 1. FCM 推送 (国外) - 需要 Google Play Services
/// 2. 本地后台服务 (国内) - 自启动保活
class BackgroundService {
  static final BackgroundService _instance = BackgroundService._internal();
  factory BackgroundService() => _instance;
  BackgroundService._internal();

  static const MethodChannel _channel = 
      MethodChannel('com.ey.echat/background_service');
  
  BackgroundServiceType _serviceType = BackgroundServiceType.local;
  bool _isRunning = false;
  bool _autoStart = false;
  
  // 回调
  Function(bool isRunning)? onServiceStateChanged;
  
  bool get isRunning => _isRunning;
  bool get autoStartEnabled => _autoStart;
  BackgroundServiceType get serviceType => _serviceType;
  
  /// 设置服务类型
  void setServiceType(BackgroundServiceType type) {
    _serviceType = type;
    debugPrint('后台服务类型设置为: $type');
  }
  
  /// 初始化后台服务
  Future<bool> init({BackgroundServiceType? type}) async {
    if (type != null) {
      _serviceType = type;
    }
    
    if (_serviceType == BackgroundServiceType.local) {
      return await _initLocalService();
    } else {
      // FCM 模式由 FcmService 管理
      debugPrint('使用 FCM 推送模式');
      return true;
    }
  }
  
  /// 初始化本地后台服务
  Future<bool> _initLocalService() async {
    try {
      // 检查服务状态
      _isRunning = await _channel.invokeMethod('isServiceRunning') ?? false;
      _autoStart = await _channel.invokeMethod('isAutoStartEnabled') ?? false;
      
      debugPrint('本地服务状态: running=$_isRunning, autoStart=$_autoStart');
      
      // 监听服务状态变化
      _channel.setMethodCallHandler(_handleMethodCall);
      
      return true;
    } catch (e) {
      debugPrint('初始化本地后台服务失败: $e');
      return false;
    }
  }
  
  /// 处理原生代码调用
  Future<dynamic> _handleMethodCall(MethodCall call) async {
    switch (call.method) {
      case 'onServiceStateChanged':
        _isRunning = call.arguments as bool? ?? false;
        onServiceStateChanged?.call(_isRunning);
        break;
    }
  }
  
  /// 启动后台服务
  Future<bool> startService() async {
    if (_serviceType == BackgroundServiceType.fcm) {
      debugPrint('FCM 模式不需要手动启动服务');
      return true;
    }
    
    try {
      final result = await _channel.invokeMethod('startService') ?? false;
      if (result) {
        _isRunning = true;
        _autoStart = true;
        onServiceStateChanged?.call(true);
      }
      debugPrint('启动后台服务: $result');
      return result;
    } catch (e) {
      debugPrint('启动后台服务失败: $e');
      return false;
    }
  }
  
  /// 停止后台服务
  Future<bool> stopService() async {
    if (_serviceType == BackgroundServiceType.fcm) {
      debugPrint('FCM 模式不需要手动停止服务');
      return true;
    }
    
    try {
      final result = await _channel.invokeMethod('stopService') ?? false;
      if (result) {
        _isRunning = false;
        _autoStart = false;
        onServiceStateChanged?.call(false);
      }
      debugPrint('停止后台服务: $result');
      return result;
    } catch (e) {
      debugPrint('停止后台服务失败: $e');
      return false;
    }
  }
  
  /// 设置开机自启动
  Future<bool> setAutoStart(bool enabled) async {
    if (_serviceType == BackgroundServiceType.fcm) {
      debugPrint('FCM 模式不支持自启动设置');
      return false;
    }
    
    try {
      await _channel.invokeMethod('setAutoStart', {'enabled': enabled});
      _autoStart = enabled;
      debugPrint('设置自启动: $enabled');
      return true;
    } catch (e) {
      debugPrint('设置自启动失败: $e');
      return false;
    }
  }
  
  /// 检查服务是否运行
  Future<bool> checkServiceRunning() async {
    if (_serviceType == BackgroundServiceType.fcm) {
      return true; // FCM 由系统管理
    }
    
    try {
      _isRunning = await _channel.invokeMethod('isServiceRunning') ?? false;
      return _isRunning;
    } catch (e) {
      debugPrint('检查服务状态失败: $e');
      return false;
    }
  }
  
  /// 获取服务状态描述
  String getStatusDescription() {
    if (_serviceType == BackgroundServiceType.fcm) {
      return '使用 FCM 推送通知';
    } else {
      if (_isRunning) {
        return '后台服务运行中${_autoStart ? "，已启用自启动" : ""}';
      } else {
        return _autoStart ? '已启用自启动' : '后台服务未运行';
      }
    }
  }
}
