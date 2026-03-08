import 'package:json_annotation/json_annotation.dart';

part 'user.g.dart';

@JsonSerializable()
class User {
  final int userId;
  final String username;
  final String nickname;
  final String avatarUrl;
  final String signature;
  final int onlineStatus;
  final int createdAt;
  final int updatedAt;

  User({
    required this.userId,
    required this.username,
    required this.nickname,
    this.avatarUrl = '',
    this.signature = '',
    this.onlineStatus = 0,
    this.createdAt = 0,
    this.updatedAt = 0,
  });

  factory User.fromJson(Map<String, dynamic> json) => _$UserFromJson(json);
  Map<String, dynamic> toJson() => _$UserToJson(this);

  bool get isOnline => onlineStatus == 1;
  
  String get displayName => nickname.isNotEmpty ? nickname : username;

  User copyWith({
    int? userId,
    String? username,
    String? nickname,
    String? avatarUrl,
    String? signature,
    int? onlineStatus,
    int? createdAt,
    int? updatedAt,
  }) {
    return User(
      userId: userId ?? this.userId,
      username: username ?? this.username,
      nickname: nickname ?? this.nickname,
      avatarUrl: avatarUrl ?? this.avatarUrl,
      signature: signature ?? this.signature,
      onlineStatus: onlineStatus ?? this.onlineStatus,
      createdAt: createdAt ?? this.createdAt,
      updatedAt: updatedAt ?? this.updatedAt,
    );
  }
}
