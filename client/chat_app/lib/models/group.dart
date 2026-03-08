import 'package:json_annotation/json_annotation.dart';

part 'group.g.dart';

@JsonSerializable()
class Group {
  final int groupId;
  final String groupName;
  final String avatarUrl;
  final String description;
  final int ownerId;
  final List<int> admins;
  final List<int> members;
  final int createdAt;
  final int updatedAt;

  Group({
    required this.groupId,
    required this.groupName,
    this.avatarUrl = '',
    this.description = '',
    this.ownerId = 0,
    this.admins = const [],
    this.members = const [],
    this.createdAt = 0,
    this.updatedAt = 0,
  });

  factory Group.fromJson(Map<String, dynamic> json) => _$GroupFromJson(json);
  Map<String, dynamic> toJson() => _$GroupToJson(this);

  int get memberCount => members.length;
  
  bool isOwner(int userId) => ownerId == userId;
  bool isAdmin(int userId) => admins.contains(userId);
  bool isMember(int userId) => members.contains(userId);

  Group copyWith({
    int? groupId,
    String? groupName,
    String? avatarUrl,
    String? description,
    int? ownerId,
    List<int>? admins,
    List<int>? members,
    int? createdAt,
    int? updatedAt,
  }) {
    return Group(
      groupId: groupId ?? this.groupId,
      groupName: groupName ?? this.groupName,
      avatarUrl: avatarUrl ?? this.avatarUrl,
      description: description ?? this.description,
      ownerId: ownerId ?? this.ownerId,
      admins: admins ?? this.admins,
      members: members ?? this.members,
      createdAt: createdAt ?? this.createdAt,
      updatedAt: updatedAt ?? this.updatedAt,
    );
  }
}

@JsonSerializable()
class GroupMember {
  final int userId;
  final String nickname;
  final String avatarUrl;
  final int onlineStatus;
  final bool isOwner;
  final bool isAdmin;

  GroupMember({
    required this.userId,
    required this.nickname,
    this.avatarUrl = '',
    this.onlineStatus = 0,
    this.isOwner = false,
    this.isAdmin = false,
  });

  factory GroupMember.fromJson(Map<String, dynamic> json) => _$GroupMemberFromJson(json);
  Map<String, dynamic> toJson() => _$GroupMemberToJson(this);
}
