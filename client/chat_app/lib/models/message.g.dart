// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'message.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

Message _$MessageFromJson(Map<String, dynamic> json) => Message(
      messageId: json['message_id'] as int? ?? 0,
      senderId: json['sender_id'] as int,
      receiverId: json['receiver_id'] as int? ?? 0,
      groupId: json['group_id'] as int? ?? 0,
      mediaType: json['media_type'] as int? ?? 0,
      content: json['content'] as String? ?? '',
      mediaUrl: json['media_url'] as String? ?? '',
      extra: json['extra'] as String? ?? '',
      status: json['status'] as int? ?? 0,
      createdAt: json['created_at'] as int? ?? 0,
    );

Map<String, dynamic> _$MessageToJson(Message instance) => <String, dynamic>{
      'message_id': instance.messageId,
      'sender_id': instance.senderId,
      'receiver_id': instance.receiverId,
      'group_id': instance.groupId,
      'media_type': instance.mediaType,
      'content': instance.content,
      'media_url': instance.mediaUrl,
      'extra': instance.extra,
      'status': instance.status,
      'created_at': instance.createdAt,
    };

Conversation _$ConversationFromJson(Map<String, dynamic> json) => Conversation(
      peerId: json['peer_id'] as int? ?? 0,
      groupId: json['group_id'] as int? ?? 0,
      peer: json['peer'] == null
          ? null
          : User.fromJson(json['peer'] as Map<String, dynamic>),
      group: json['group'] == null
          ? null
          : Group.fromJson(json['group'] as Map<String, dynamic>),
      lastMessage: json['last_message'] == null
          ? null
          : Message.fromJson(json['last_message'] as Map<String, dynamic>),
      unreadCount: json['unread_count'] as int? ?? 0,
      isPinned: json['is_pinned'] as bool? ?? false,
      isMuted: json['is_muted'] as bool? ?? false,
    );

Map<String, dynamic> _$ConversationToJson(Conversation instance) =>
    <String, dynamic>{
      'peer_id': instance.peerId,
      'group_id': instance.groupId,
      'peer': instance.peer?.toJson(),
      'group': instance.group?.toJson(),
      'last_message': instance.lastMessage?.toJson(),
      'unread_count': instance.unreadCount,
      'is_pinned': instance.isPinned,
      'is_muted': instance.isMuted,
    };
