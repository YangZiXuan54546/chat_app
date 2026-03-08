// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'group.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

Group _$GroupFromJson(Map<String, dynamic> json) => Group(
      groupId: json['group_id'] as int,
      groupName: json['group_name'] as String,
      avatarUrl: json['avatar_url'] as String? ?? '',
      description: json['description'] as String? ?? '',
      ownerId: json['owner_id'] as int? ?? 0,
      admins: (json['admins'] as List<dynamic>?)?.map((e) => e as int).toList() ??
          const [],
      members:
          (json['members'] as List<dynamic>?)?.map((e) => e as int).toList() ??
              const [],
      createdAt: json['created_at'] as int? ?? 0,
      updatedAt: json['updated_at'] as int? ?? 0,
    );

Map<String, dynamic> _$GroupToJson(Group instance) => <String, dynamic>{
      'group_id': instance.groupId,
      'group_name': instance.groupName,
      'avatar_url': instance.avatarUrl,
      'description': instance.description,
      'owner_id': instance.ownerId,
      'admins': instance.admins,
      'members': instance.members,
      'created_at': instance.createdAt,
      'updated_at': instance.updatedAt,
    };

GroupMember _$GroupMemberFromJson(Map<String, dynamic> json) => GroupMember(
      userId: json['user_id'] as int,
      nickname: json['nickname'] as String,
      avatarUrl: json['avatar_url'] as String? ?? '',
      onlineStatus: json['online_status'] as int? ?? 0,
      isOwner: json['is_owner'] as bool? ?? false,
      isAdmin: json['is_admin'] as bool? ?? false,
    );

Map<String, dynamic> _$GroupMemberToJson(GroupMember instance) =>
    <String, dynamic>{
      'user_id': instance.userId,
      'nickname': instance.nickname,
      'avatar_url': instance.avatarUrl,
      'online_status': instance.onlineStatus,
      'is_owner': instance.isOwner,
      'is_admin': instance.isAdmin,
    };
