import 'dart:convert';
import 'package:hive_flutter/hive_flutter.dart';
import '../models/models.dart';

/// 本地消息数据库服务
/// 使用 Hive 存储聊天消息，支持离线查看历史记录
class MessageDatabase {
  static final MessageDatabase _instance = MessageDatabase._internal();
  factory MessageDatabase() => _instance;
  MessageDatabase._internal();

  static const String _messagesBoxName = 'chat_messages';
  static const String _conversationsBoxName = 'conversations';
  static const String _lastSyncBoxName = 'last_sync';
  
  Box<String>? _messagesBox;
  Box<String>? _conversationsBox;
  Box<int>? _lastSyncBox;

  /// 初始化数据库
  Future<void> init() async {
    await Hive.initFlutter();
    _messagesBox = await Hive.openBox<String>(_messagesBoxName);
    _conversationsBox = await Hive.openBox<String>(_conversationsBoxName);
    _lastSyncBox = await Hive.openBox<int>(_lastSyncBoxName);
  }

  /// 生成消息存储的 key
  /// 对于私聊：使用 "p_{peerId}" 格式
  /// 对于群聊：使用 "g_{groupId}" 格式
  String _getMessageKey(int peerId, {bool isGroup = false}) {
    return isGroup ? 'g_$peerId' : 'p_$peerId';
  }

  /// 保存消息到本地
  Future<void> saveMessage(Message message) async {
    final key = _getMessageKey(
      message.groupId > 0 ? message.groupId : message.receiverId,
      isGroup: message.groupId > 0,
    );
    
    // 获取现有消息列表
    final messages = await getMessages(
      message.groupId > 0 ? message.groupId : message.receiverId,
      isGroup: message.groupId > 0,
    );
    
    // 检查消息是否已存在
    if (!messages.any((m) => m.messageId == message.messageId)) {
      messages.add(message);
      // 按时间排序
      messages.sort((a, b) => a.createdAt.compareTo(b.createdAt));
      
      // 保存到 Hive
      final jsonList = messages.map((m) => jsonEncode(m.toJson())).toList();
      await _messagesBox?.put(key, jsonEncode(jsonList));
    }
  }

  /// 批量保存消息
  Future<void> saveMessages(int peerId, List<Message> messages, {bool isGroup = false}) async {
    final key = _getMessageKey(peerId, isGroup: isGroup);
    
    // 获取现有消息
    final existingMessages = await getMessages(peerId, isGroup: isGroup);
    
    // 合并消息（去重）
    final messageIdSet = existingMessages.map((m) => m.messageId).toSet();
    for (final message in messages) {
      if (!messageIdSet.contains(message.messageId)) {
        existingMessages.add(message);
        messageIdSet.add(message.messageId);
      }
    }
    
    // 按时间排序
    existingMessages.sort((a, b) => a.createdAt.compareTo(b.createdAt));
    
    // 保存到 Hive
    final jsonList = existingMessages.map((m) => jsonEncode(m.toJson())).toList();
    await _messagesBox?.put(key, jsonEncode(jsonList));
  }

  /// 获取本地消息
  Future<List<Message>> getMessages(int peerId, {bool isGroup = false}) async {
    final key = _getMessageKey(peerId, isGroup: isGroup);
    final jsonString = _messagesBox?.get(key);
    
    if (jsonString == null) {
      return [];
    }
    
    try {
      final jsonList = jsonDecode(jsonString) as List<dynamic>;
      return jsonList
          .map((json) => Message.fromJson(jsonDecode(json as String) as Map<String, dynamic>))
          .toList();
    } catch (e) {
      // 如果解析失败，返回空列表
      return [];
    }
  }

  /// 获取最近的消息（分页）
  Future<List<Message>> getRecentMessages(int peerId, {bool isGroup = false, int limit = 50, int beforeTime = 0}) async {
    final messages = await getMessages(peerId, isGroup: isGroup);
    
    if (beforeTime > 0) {
      return messages.where((m) => m.createdAt < beforeTime).take(limit).toList();
    }
    
    return messages.reversed.take(limit).toList().reversed.toList();
  }

  /// 清除某个对话的消息
  Future<void> clearMessages(int peerId, {bool isGroup = false}) async {
    final key = _getMessageKey(peerId, isGroup: isGroup);
    await _messagesBox?.delete(key);
  }

  /// 清除所有消息
  Future<void> clearAllMessages() async {
    await _messagesBox?.clear();
    await _conversationsBox?.clear();
    await _lastSyncBox?.clear();
  }

  /// 保存会话信息
  Future<void> saveConversation(Conversation conversation) async {
    final key = conversation.isGroup 
        ? 'g_${conversation.groupId}' 
        : 'p_${conversation.peerId}';
    
    await _conversationsBox?.put(key, jsonEncode(conversation.toJson()));
  }

  /// 获取所有会话
  Future<List<Conversation>> getConversations() async {
    final conversations = <Conversation>[];
    
    for (final key in _conversationsBox?.keys ?? []) {
      final jsonString = _conversationsBox?.get(key);
      if (jsonString != null) {
        try {
          final json = jsonDecode(jsonString) as Map<String, dynamic>;
          conversations.add(Conversation.fromJson(json));
        } catch (e) {
          // 忽略解析错误
        }
      }
    }
    
    // 按最后消息时间排序
    conversations.sort((a, b) {
      final aTime = a.lastMessage?.createdAt ?? 0;
      final bTime = b.lastMessage?.createdAt ?? 0;
      return bTime.compareTo(aTime);
    });
    
    return conversations;
  }

  /// 删除会话
  Future<void> deleteConversation(int peerId, {bool isGroup = false}) async {
    final key = isGroup ? 'g_$peerId' : 'p_$peerId';
    await _conversationsBox?.delete(key);
  }

  /// 更新最后同步时间
  Future<void> updateLastSyncTime(int peerId, {bool isGroup = false}) async {
    final key = _getMessageKey(peerId, isGroup: isGroup);
    await _lastSyncBox?.put(key, DateTime.now().millisecondsSinceEpoch);
  }

  /// 获取最后同步时间
  int? getLastSyncTime(int peerId, {bool isGroup = false}) {
    final key = _getMessageKey(peerId, isGroup: isGroup);
    return _lastSyncBox?.get(key);
  }

  /// 搜索消息
  Future<List<Message>> searchMessages(String keyword) async {
    final results = <Message>[];
    
    for (final key in _messagesBox?.keys ?? []) {
      final jsonString = _messagesBox?.get(key);
      if (jsonString != null) {
        try {
          final jsonList = jsonDecode(jsonString) as List<dynamic>;
          for (final json in jsonList) {
            final message = Message.fromJson(jsonDecode(json as String) as Map<String, dynamic>);
            if (message.content.toLowerCase().contains(keyword.toLowerCase())) {
              results.add(message);
            }
          }
        } catch (e) {
          // 忽略解析错误
        }
      }
    }
    
    // 按时间排序
    results.sort((a, b) => b.createdAt.compareTo(a.createdAt));
    return results;
  }
}
