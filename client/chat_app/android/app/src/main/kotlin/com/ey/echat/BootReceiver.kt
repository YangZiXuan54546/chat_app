package com.ey.echat

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build

/**
 * 开机自启动广播接收器
 */
class BootReceiver : BroadcastReceiver() {
    
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED) {
            // 检查是否需要自启动
            val prefs = context.getSharedPreferences("chat_app_prefs", Context.MODE_PRIVATE)
            val autoStart = prefs.getBoolean("auto_start_service", false)
            
            if (autoStart) {
                startForegroundService(context)
            }
        }
    }
    
    private fun startForegroundService(context: Context) {
        try {
            val serviceIntent = Intent(context, ForegroundService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(serviceIntent)
            } else {
                context.startService(serviceIntent)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}

/**
 * 网络状态变化广播接收器
 * 网络恢复时自动重连服务
 */
class NetworkReceiver : BroadcastReceiver() {
    
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == "android.net.conn.CONNECTIVITY_CHANGE") {
            // 网络状态变化时尝试启动服务
            val prefs = context.getSharedPreferences("chat_app_prefs", Context.MODE_PRIVATE)
            val autoStart = prefs.getBoolean("auto_start_service", false)
            
            if (autoStart) {
                ForegroundService.start(context)
            }
        }
    }
}
