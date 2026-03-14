/// 收藏消息模型
class Favorite {
  final int messageId;
  final String messageType; // 'private' 或 'group'
  final int senderId;
  final String content;
  final int mediaType;
  final String mediaUrl;
  final int createdAt;
  final String senderUsername;
  final String senderNickname;
  final String senderAvatar;

  Favorite({
    required this.messageId,
    required this.messageType,
    required this.senderId,
    required this.content,
    required this.mediaType,
    required this.mediaUrl,
    required this.createdAt,
    required this.senderUsername,
    required this.senderNickname,
    required this.senderAvatar,
  });

  factory Favorite.fromJson(Map<String, dynamic> json) {
    return Favorite(
      messageId: json['message_id'] as int? ?? 0,
      messageType: json['message_type'] as String? ?? 'private',
      senderId: json['sender_id'] as int? ?? 0,
      content: json['content'] as String? ?? '',
      mediaType: json['media_type'] as int? ?? 0,
      mediaUrl: json['media_url'] as String? ?? '',
      createdAt: json['created_at'] as int? ?? 0,
      senderUsername: json['sender_username'] as String? ?? '',
      senderNickname: json['sender_nickname'] as String? ?? '',
      senderAvatar: json['sender_avatar'] as String? ?? '',
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'message_id': messageId,
      'message_type': messageType,
      'sender_id': senderId,
      'content': content,
      'media_type': mediaType,
      'media_url': mediaUrl,
      'created_at': createdAt,
      'sender_username': senderUsername,
      'sender_nickname': senderNickname,
      'sender_avatar': senderAvatar,
    };
  }

  /// 是否是图片消息
  bool get isImage => mediaType == 1;

  /// 是否是文件消息
  bool get isFile => mediaType == 4;

  /// 获取显示名称
  String get displayName => senderNickname.isNotEmpty ? senderNickname : senderUsername;

  /// 格式化时间
  String get formattedTime {
    final dt = DateTime.fromMillisecondsSinceEpoch(createdAt * 1000);
    final now = DateTime.now();
    final diff = now.difference(dt);

    if (diff.inDays == 0) {
      return '${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
    } else if (diff.inDays == 1) {
      return '昨天 ${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
    } else if (diff.inDays < 7) {
      const weekdays = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
      return '${weekdays[dt.weekday - 1]} ${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
    } else {
      return '${dt.month}/${dt.day} ${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
    }
  }
}
