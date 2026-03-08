import 'package:json_annotation/json_annotation.dart';
import 'user.dart';

part 'friend.g.dart';

enum FriendStatus {
  pending(0),
  accepted(1),
  rejected(2),
  deleted(3);

  const FriendStatus(this.value);
  final int value;

  static FriendStatus fromValue(int value) {
    return FriendStatus.values.firstWhere(
      (e) => e.value == value,
      orElse: () => FriendStatus.pending,
    );
  }
}

@JsonSerializable()
class FriendRelation {
  final int userId;
  final int friendId;
  final String remark;
  final int status;
  final int createdAt;
  final int updatedAt;

  FriendRelation({
    required this.userId,
    required this.friendId,
    this.remark = '',
    this.status = 0,
    this.createdAt = 0,
    this.updatedAt = 0,
  });

  factory FriendRelation.fromJson(Map<String, dynamic> json) => _$FriendRelationFromJson(json);
  Map<String, dynamic> toJson() => _$FriendRelationToJson(this);

  FriendStatus get friendStatus => FriendStatus.fromValue(status);
}

@JsonSerializable()
class Friend {
  final User user;
  final FriendRelation relation;

  Friend({
    required this.user,
    required this.relation,
  });

  factory Friend.fromJson(Map<String, dynamic> json) => _$FriendFromJson(json);
  Map<String, dynamic> toJson() => _$FriendToJson(this);

  String get displayName => relation.remark.isNotEmpty ? relation.remark : user.displayName;
}
