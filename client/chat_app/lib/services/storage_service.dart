import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/models.dart';

class StorageService {
  static final StorageService _instance = StorageService._internal();
  factory StorageService() => _instance;
  StorageService._internal();

  SharedPreferences? _prefs;

  static const String _keyRememberMe = 'remember_me';
  static const String _keySavedUsername = 'saved_username';
  static const String _keySavedPassword = 'saved_password';
  static const String _keyServerHost = 'server_host';
  static const String _keyServerPort = 'server_port';
  static const String _keyCurrentUser = 'current_user';
  static const String _keyToken = 'auth_token';
  static const String _keyUseFCMPush = 'use_fcm_push';
  static const String _keyAutoStartService = 'auto_start_service';

  /// 初始化
  Future<void> init() async {
    _prefs = await SharedPreferences.getInstance();
  }

  /// 保存认证信息
  Future<void> saveCredentials(String username, String password, bool rememberMe) async {
    if (rememberMe) {
      await _prefs?.setBool(_keyRememberMe, true);
      await _prefs?.setString(_keySavedUsername, username);
      await _prefs?.setString(_keySavedPassword, password);
    } else {
      await _prefs?.remove(_keyRememberMe);
      await _prefs?.remove(_keySavedUsername);
      await _prefs?.remove(_keySavedPassword);
    }
  }

  /// 获取保存的用户名
  String? get savedUsername => _prefs?.getString(_keySavedUsername);

  /// 获取保存的密码
  String? get savedPassword => _prefs?.getString(_keySavedPassword);

  /// 是否记住密码
  bool get rememberMe => _prefs?.getBool(_keyRememberMe) ?? false;

  /// 保存服务器配置
  Future<void> saveServerConfig(String host, int port) async {
    await _prefs?.setString(_keyServerHost, host);
    await _prefs?.setInt(_keyServerPort, port);
  }

  /// 获取服务器主机
  String get serverHost => _prefs?.getString(_keyServerHost) ?? '127.0.0.1';

  /// 获取服务器端口
  int get serverPort => _prefs?.getInt(_keyServerPort) ?? 8888;
  
  /// 获取媒体服务器地址 (HTTP端口固定为8889)
  String get mediaServerHost => '$serverHost:8889';
  
  /// 替换URL中的localhost为实际服务器地址
  String fixMediaUrl(String url) {
    if (url.contains('localhost')) {
      return url.replaceFirst(RegExp(r'http://localhost:\d+'), 'http://$mediaServerHost');
    }
    return url;
  }

  /// 保存当前用户
  Future<void> saveCurrentUser(User user) async {
    await _prefs?.setString(_keyCurrentUser, jsonEncode(user.toJson()));
  }

  /// 获取当前用户
  User? get currentUser {
    final json = _prefs?.getString(_keyCurrentUser);
    if (json == null) return null;
    try {
      return User.fromJson(jsonDecode(json) as Map<String, dynamic>);
    } catch (e) {
      return null;
    }
  }

  /// 清除当前用户
  Future<void> clearCurrentUser() async {
    await _prefs?.remove(_keyCurrentUser);
    await _prefs?.remove(_keyToken);
  }

  /// 保存token
  Future<void> saveToken(String token) async {
    await _prefs?.setString(_keyToken, token);
  }

  /// 获取token
  String? get token => _prefs?.getString(_keyToken);

  /// 保存主题模式
  Future<void> saveThemeMode(int mode) async {
    await _prefs?.setInt('theme_mode', mode);
  }

  /// 获取主题模式
  int getThemeMode() {
    return _prefs?.getInt('theme_mode') ?? 0;
  }

  /// 是否使用 FCM 推送 (国外模式)
  bool get useFCMPush => _prefs?.getBool(_keyUseFCMPush) ?? false;
  
  /// 设置是否使用 FCM 推送
  Future<void> setUseFCMPush(bool useFCM) async {
    await _prefs?.setBool(_keyUseFCMPush, useFCM);
  }
  
  /// 是否自启动后台服务 (国内模式)
  bool get autoStartService => _prefs?.getBool(_keyAutoStartService) ?? true;
  
  /// 设置是否自启动后台服务
  Future<void> setAutoStartService(bool autoStart) async {
    await _prefs?.setBool(_keyAutoStartService, autoStart);
  }

  /// 清除所有数据
  Future<void> clearAll() async {
    await _prefs?.clear();
  }
}
