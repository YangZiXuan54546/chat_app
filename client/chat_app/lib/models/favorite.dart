/// 收藏消息模型
class Favorite {
  final int messageId;
  final String messageType;
  final int senderId;
  final String content;
  final int mediaType;
  final String mediaUrl;
  final int createdAt;
  final String senderUsername;
  final String senderNickname;

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
    );
  }

  bool get isImage => mediaType == 1;
  bool get isFile => mediaType == 4;
  String get displayName => senderNickname.isNotEmpty ? senderNickname : senderUsername;
}
