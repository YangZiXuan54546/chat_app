package com.ey.echat

import android.content.Context
import android.content.SharedPreferences
import io.flutter.plugin.common.MethodChannel

/**
 * 后台服务管理类
 * 提供 Flutter 和原生代码之间的接口
 */
class BackgroundServiceManager(private val context: Context) {
    
    companion object {
        const val CHANNEL_NAME = "com.ey.echat/background_service"
        private const val PREFS_NAME = "chat_app_prefs"
        private const val KEY_AUTO_START = "auto_start_service"
        private const val KEY_KEEP_ALIVE = "keep_alive_enabled"
        
        @Volatile
        private var instance: BackgroundServiceManager? = null
        
        fun getInstance(context: Context): BackgroundServiceManager {
            return instance ?: synchronized(this) {
                instance ?: BackgroundServiceManager(context.applicationContext).also {
                    instance = it
                }
            }
        }
    }
    
    private val prefs: SharedPreferences = 
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    
    /**
     * 启动后台服务
     */
    fun startService(): Boolean {
        return try {
            prefs.edit().putBoolean(KEY_AUTO_START, true).apply()
            prefs.edit().putBoolean(KEY_KEEP_ALIVE, true).apply()
            ForegroundService.start(context)
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
    
    /**
     * 停止后台服务
     */
    fun stopService(): Boolean {
        return try {
            prefs.edit().putBoolean(KEY_AUTO_START, false).apply()
            prefs.edit().putBoolean(KEY_KEEP_ALIVE, false).apply()
            ForegroundService.stop(context)
            true
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }
    
    /**
     * 检查服务是否运行
     */
    fun isServiceRunning(): Boolean {
        return ForegroundService.isServiceRunning()
    }
    
    /**
     * 设置自启动
     */
    fun setAutoStart(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_AUTO_START, enabled).apply()
    }
    
    /**
     * 获取自启动设置
     */
    fun isAutoStartEnabled(): Boolean {
        return prefs.getBoolean(KEY_AUTO_START, false)
    }
    
    /**
     * 注册 MethodChannel
     */
    fun registerChannel(flutterEngine: io.flutter.embedding.engine.FlutterEngine) {
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL_NAME)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "startService" -> {
                        val success = startService()
                        result.success(success)
                    }
                    "stopService" -> {
                        val success = stopService()
                        result.success(success)
                    }
                    "isServiceRunning" -> {
                        result.success(isServiceRunning())
                    }
                    "setAutoStart" -> {
                        val enabled = call.argument<Boolean>("enabled") ?: false
                        setAutoStart(enabled)
                        result.success(true)
                    }
                    "isAutoStartEnabled" -> {
                        result.success(isAutoStartEnabled())
                    }
                    else -> {
                        result.notImplemented()
                    }
                }
            }
    }
}
