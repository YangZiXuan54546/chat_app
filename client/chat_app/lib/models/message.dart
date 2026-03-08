import 'package:json_annotation/json_annotation.dart';
import 'user.dart';
import 'group.dart';

part 'message.g.dart';

enum MediaType {
  text(0),
  image(1),
  audio(2),
  video(3),
  file(4),
  location(5);

  const MediaType(this.value);
  final int value;

  static MediaType fromValue(int value) {
    return MediaType.values.firstWhere(
      (e) => e.value == value,
      orElse: () => MediaType.text,
    );
  }
}

enum MessageStatus {
  sending(0),
  sent(1),
  delivered(2),
  read(3),
  failed(4);

  const MessageStatus(this.value);
  final int value;

  static MessageStatus fromValue(int value) {
    return MessageStatus.values.firstWhere(
      (e) => e.value == value,
      orElse: () => MessageStatus.sending,
    );
  }
}

@JsonSerializable()
class Message {
  final int messageId;
  final int senderId;
  final int receiverId;
  final int groupId;
  final int mediaType;
  final String content;
  final String mediaUrl;
  final String extra;
  final int status;
  final int createdAt;

  Message({
    this.messageId = 0,
    required this.senderId,
    this.receiverId = 0,
    this.groupId = 0,
    this.mediaType = 0,
    this.content = '',
    this.mediaUrl = '',
    this.extra = '',
    this.status = 0,
    this.createdAt = 0,
  });

  factory Message.fromJson(Map<String, dynamic> json) => _$MessageFromJson(json);
  Map<String, dynamic> toJson() => _$MessageToJson(this);

  bool get isText => mediaType == MediaType.text.value;
  bool get isImage => mediaType == MediaType.image.value;
  bool get isAudio => mediaType == MediaType.audio.value;
  bool get isVideo => mediaType == MediaType.video.value;
  bool get isFile => mediaType == MediaType.file.value;
  bool get isLocation => mediaType == MediaType.location.value;
  
  bool get isGroupMessage => groupId > 0;
  
  MediaType get messageType => MediaType.fromValue(mediaType);
  MessageStatus get messageStatus => MessageStatus.fromValue(status);

  Message copyWith({
    int? messageId,
    int? senderId,
    int? receiverId,
    int? groupId,
    int? mediaType,
    String? content,
    String? mediaUrl,
    String? extra,
    int? status,
    int? createdAt,
  }) {
    return Message(
      messageId: messageId ?? this.messageId,
      senderId: senderId ?? this.senderId,
      receiverId: receiverId ?? this.receiverId,
      groupId: groupId ?? this.groupId,
      mediaType: mediaType ?? this.mediaType,
      content: content ?? this.content,
      mediaUrl: mediaUrl ?? this.mediaUrl,
      extra: extra ?? this.extra,
      status: status ?? this.status,
      createdAt: createdAt ?? this.createdAt,
    );
  }
}

@JsonSerializable()
class Conversation {
  final int peerId;
  final int groupId;
  final User? peer;
  final Group? group;
  final Message? lastMessage;
  final int unreadCount;
  final bool isPinned;
  final bool isMuted;

  Conversation({
    this.peerId = 0,
    this.groupId = 0,
    this.peer,
    this.group,
    this.lastMessage,
    this.unreadCount = 0,
    this.isPinned = false,
    this.isMuted = false,
  });

  bool get isGroup => groupId > 0;
  String get name => isGroup ? (group?.groupName ?? '') : (peer?.displayName ?? '');
  String get avatar => isGroup ? (group?.avatarUrl ?? '') : (peer?.avatarUrl ?? '');
  
  factory Conversation.fromJson(Map<String, dynamic> json) => _$ConversationFromJson(json);
  Map<String, dynamic> toJson() => _$ConversationToJson(this);
}
