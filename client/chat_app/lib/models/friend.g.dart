// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'friend.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

FriendRelation _$FriendRelationFromJson(Map<String, dynamic> json) =>
    FriendRelation(
      userId: json['user_id'] as int,
      friendId: json['friend_id'] as int,
      remark: json['remark'] as String? ?? '',
      status: json['status'] as int? ?? 0,
      createdAt: json['created_at'] as int? ?? 0,
      updatedAt: json['updated_at'] as int? ?? 0,
    );

Map<String, dynamic> _$FriendRelationToJson(FriendRelation instance) =>
    <String, dynamic>{
      'user_id': instance.userId,
      'friend_id': instance.friendId,
      'remark': instance.remark,
      'status': instance.status,
      'created_at': instance.createdAt,
      'updated_at': instance.updatedAt,
    };

Friend _$FriendFromJson(Map<String, dynamic> json) => Friend(
      user: User.fromJson(json['user'] as Map<String, dynamic>),
      relation:
          FriendRelation.fromJson(json['relation'] as Map<String, dynamic>),
    );

Map<String, dynamic> _$FriendToJson(Friend instance) => <String, dynamic>{
      'user': instance.user.toJson(),
      'relation': instance.relation.toJson(),
    };
