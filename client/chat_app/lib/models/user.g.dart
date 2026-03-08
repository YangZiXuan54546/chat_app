// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'user.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

User _$UserFromJson(Map<String, dynamic> json) => User(
      userId: json['user_id'] as int,
      username: json['username'] as String,
      nickname: json['nickname'] as String,
      avatarUrl: json['avatar_url'] as String? ?? '',
      signature: json['signature'] as String? ?? '',
      onlineStatus: json['online_status'] as int? ?? 0,
      createdAt: json['created_at'] as int? ?? 0,
      updatedAt: json['updated_at'] as int? ?? 0,
    );

Map<String, dynamic> _$UserToJson(User instance) => <String, dynamic>{
      'user_id': instance.userId,
      'username': instance.username,
      'nickname': instance.nickname,
      'avatar_url': instance.avatarUrl,
      'signature': instance.signature,
      'online_status': instance.onlineStatus,
      'created_at': instance.createdAt,
      'updated_at': instance.updatedAt,
    };
