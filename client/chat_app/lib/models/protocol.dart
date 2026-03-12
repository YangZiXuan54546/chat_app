import 'dart:typed_data';
import 'dart:convert';

/// 消息类型枚举
enum MessageType {
  // 认证相关
  register(1),
  registerResponse(2),
  login(3),
  loginResponse(4),
  logout(5),
  logoutResponse(6),

  // 用户相关
  userInfo(10),
  userInfoResponse(11),
  userSearch(12),
  userSearchResponse(13),
  userUpdate(14),
  userUpdateResponse(15),

  // 好友相关
  friendAdd(20),
  friendAddResponse(21),
  friendAccept(22),
  friendAcceptResponse(23),
  friendReject(24),
  friendRejectResponse(25),
  friendRemove(26),
  friendRemoveResponse(27),
  friendList(28),
  friendListResponse(29),
  friendRequests(30),
  friendRequestsResponse(31),
  friendRemark(32),
  friendRemarkResponse(33),

  // 私聊消息
  privateMessage(40),
  privateMessageResponse(41),
  privateMessageAck(42),
  privateHistory(43),
  privateHistoryResponse(44),

  // 群组相关
  groupCreate(50),
  groupCreateResponse(51),
  groupJoin(52),
  groupJoinResponse(53),
  groupLeave(54),
  groupLeaveResponse(55),
  groupDismiss(56),
  groupDismissResponse(57),
  groupInfo(58),
  groupInfoResponse(59),
  groupList(60),
  groupListResponse(61),
  groupMembers(62),
  groupMembersResponse(63),
  groupAddMember(64),
  groupAddMemberResponse(65),
  groupRemoveMember(66),
  groupRemoveMemberResponse(67),
  groupSetAdmin(68),
  groupSetAdminResponse(69),
  groupTransferOwner(70),
  groupTransferOwnerResponse(71),

  // 群聊消息
  groupMessage(72),
  groupMessageResponse(73),
  groupMessageAck(74),
  groupHistory(75),
  groupHistoryResponse(76),

  // 多媒体消息
  mediaUpload(80),
  mediaUploadResponse(81),
  mediaDownload(82),
  mediaDownloadResponse(83),

  // 在线状态
  onlineStatus(90),
  onlineStatusResponse(91),

  // 心跳
  heartbeat(100),
  heartbeatResponse(101),

  // 端到端加密
  keyUpload(110),
  keyUploadResponse(111),
  keyRequest(112),
  keyResponse(113),
  encryptedMessage(114),
  encryptedMessageResponse(115),

  // 消息撤回
  messageRecall(120),
  messageRecallResponse(121),

  // 错误
  error(255);

  const MessageType(this.value);
  final int value;

  static MessageType fromValue(int value) {
    return MessageType.values.firstWhere(
      (e) => e.value == value,
      orElse: () => MessageType.error,
    );
  }
}

/// 消息头结构
class MessageHeader {
  final int length;
  final MessageType type;
  final int sequence;

  MessageHeader({
    required this.length,
    required this.type,
    required this.sequence,
  });
}

/// 协议工具类
class Protocol {
  static int _sequence = 0;

  static int get nextSequence => ++_sequence;

  /// 序列化消息 (小端序，与服务器一致)
  static Uint8List serialize(MessageType type, int sequence, Map<String, dynamic> body) {
    final bodyBytes = utf8.encode(jsonEncode(body));
    // Header: 4(length) + 1(type) + 4(sequence) = 9 bytes
    final buffer = ByteData(9 + bodyBytes.length);
    
    // 写入消息头 (小端序)
    buffer.setUint32(0, bodyBytes.length, Endian.little);
    buffer.setUint8(4, type.value);
    buffer.setUint32(5, sequence, Endian.little);
    
    // 写入消息体
    final bytes = buffer.buffer.asUint8List();
    bytes.setAll(9, bodyBytes);
    
    return bytes;
  }

  /// 解析消息头 (小端序，与服务器一致)
  static MessageHeader? parseHeader(Uint8List data) {
    if (data.length < 9) return null;
    
    final buffer = ByteData.sublistView(data);
    return MessageHeader(
      length: buffer.getUint32(0, Endian.little),
      type: MessageType.fromValue(buffer.getUint8(4)),
      sequence: buffer.getUint32(5, Endian.little),
    );
  }

  /// 解析消息体
  static Map<String, dynamic> parseBody(Uint8List data) {
    if (data.isEmpty) return {};
    try {
      return jsonDecode(utf8.decode(data)) as Map<String, dynamic>;
    } catch (e) {
      return {};
    }
  }

  /// 创建错误响应
  static Uint8List createError(int sequence, int code, String message) {
    return serialize(
      MessageType.error,
      sequence,
      {'code': code, 'message': message},
    );
  }

  /// 创建请求
  static Uint8List createRequest(MessageType type, Map<String, dynamic> body) {
    return serialize(type, nextSequence, body);
  }
}
