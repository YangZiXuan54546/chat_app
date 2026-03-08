import 'package:flutter/material.dart';
import '../services/chat_service.dart';
import '../services/storage_service.dart';

class AppProvider extends ChangeNotifier {
  bool _isInitialized = false;
  bool _isLoading = false;
  String? _error;
  
  bool get isInitialized => _isInitialized;
  bool get isLoading => _isLoading;
  String? get error => _error;

  /// 初始化应用
  Future<void> init() async {
    if (_isInitialized) return;
    
    _isLoading = true;
    notifyListeners();
    
    try {
      final storage = StorageService();
      final chatService = ChatService();
      
      // 尝试自动连接
      final host = storage.serverHost;
      final port = storage.serverPort;
      
      await chatService.connect(host, port);
      
      // 检查是否有保存的用户
      final savedUser = storage.currentUser;
      if (savedUser != null && chatService.isConnected) {
        // 可以尝试自动登录
      }
      
      _isInitialized = true;
      _error = null;
    } catch (e) {
      _error = e.toString();
    }
    
    _isLoading = false;
    notifyListeners();
  }

  /// 连接服务器
  Future<bool> connect(String host, int port) async {
    _isLoading = true;
    _error = null;
    notifyListeners();
    
    try {
      final success = await ChatService().connect(host, port);
      if (success) {
        await StorageService().saveServerConfig(host, port);
      }
      _isLoading = false;
      notifyListeners();
      return success;
    } catch (e) {
      _error = e.toString();
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  /// 清除错误
  void clearError() {
    _error = null;
    notifyListeners();
  }
}
