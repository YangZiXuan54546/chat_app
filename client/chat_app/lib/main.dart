import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/chat_service.dart';
import 'services/storage_service.dart';
import 'services/message_database.dart';
import 'services/notification_service.dart';
import 'services/fcm_service.dart';
import 'services/background_service.dart';
import 'services/jpush_service.dart';
import 'screens/splash_screen.dart';
import 'providers/app_provider.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  try {
    // 初始化存储服务
    await StorageService().init();
    
    // 初始化本地消息数据库
    await MessageDatabase().init();
    
    // 初始化本地通知服务
    await NotificationService().init();
  } catch (e) {
    debugPrint('初始化基础服务错误: $e');
  }
  
  // JPush 和后台服务初始化改为异步，不阻塞主流程
  _initPushAndBackgroundService();
  
  runApp(const ChatApp());
}

/// 异步初始化推送和后台服务
void _initPushAndBackgroundService() async {
  try {
    final storage = StorageService();
    
    if (!storage.useFCMPush) {
      // JPush 模式 (国内推送)
      try {
        final success = await JPushService().init().timeout(
          const Duration(seconds: 5),
          onTimeout: () {
            debugPrint('JPush 初始化超时，继续启动');
            return false;
          },
        );
        debugPrint('JPush 初始化完成: $success');
      } catch (e) {
        debugPrint('JPush 初始化错误: $e');
      }
      
      // 本地后台服务模式 (国内)
      await BackgroundService().init(type: BackgroundServiceType.local);
      final started = await BackgroundService().startService();
      debugPrint('本地后台服务启动: $started');
    } else {
      // FCM 模式 (国外)
      await FcmService().init();
      BackgroundService().setServiceType(BackgroundServiceType.fcm);
    }
    debugPrint('推送和后台服务初始化完成');
  } catch (e) {
    debugPrint('推送和后台服务初始化错误: $e');
  }
}

class ChatApp extends StatelessWidget {
  const ChatApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => ChatService()),
        ChangeNotifierProvider(create: (_) => AppProvider()),
      ],
      child: Consumer<AppProvider>(
        builder: (context, appProvider, child) {
          return MaterialApp(
            title: 'Chat App',
            debugShowCheckedModeBanner: false,
            theme: ThemeData(
              useMaterial3: true,
              colorScheme: ColorScheme.fromSeed(
                seedColor: const Color(0xFF6200EE),
                brightness: Brightness.light,
              ),
              appBarTheme: const AppBarTheme(
                centerTitle: true,
                elevation: 0,
              ),
              inputDecorationTheme: InputDecorationTheme(
                filled: true,
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
              ),
              elevatedButtonTheme: ElevatedButtonThemeData(
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(12),
                  ),
                ),
              ),
            ),
            darkTheme: ThemeData(
              useMaterial3: true,
              colorScheme: ColorScheme.fromSeed(
                seedColor: const Color(0xFF6200EE),
                brightness: Brightness.dark,
              ),
              appBarTheme: const AppBarTheme(
                centerTitle: true,
                elevation: 0,
              ),
              inputDecorationTheme: InputDecorationTheme(
                filled: true,
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
              ),
            ),
            themeMode: appProvider.themeMode,
            home: const SplashScreen(),
          );
        },
      ),
    );
  }
}
