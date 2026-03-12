import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/chat_service.dart';
import 'services/storage_service.dart';
import 'services/message_database.dart';
import 'services/notification_service.dart';
import 'services/fcm_service.dart';
import 'services/background_service.dart';
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
  
  // 根据设置选择推送模式
  final storage = StorageService();
  final useFCM = storage.useFCMPush;
  
  if (useFCM) {
    // FCM 模式 (国外)
    try {
      await FcmService().init();
      BackgroundService().setServiceType(BackgroundServiceType.fcm);
    } catch (e) {
      debugPrint('FCM 初始化错误: $e');
    }
  } else {
    // 本地后台服务模式 (国内)
    try {
      await BackgroundService().init(type: BackgroundServiceType.local);
      // 自动启动后台服务
      await BackgroundService().startService();
    } catch (e) {
      debugPrint('后台服务初始化错误: $e');
    }
  }
  
  runApp(const ChatApp());
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
