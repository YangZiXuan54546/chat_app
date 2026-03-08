import 'package:intl/intl.dart';

class DateTimeUtils {
  static final DateFormat _timeFormat = DateFormat('HH:mm');
  static final DateFormat _dateFormat = DateFormat('MM-dd');
  static final DateFormat _dateTimeFormat = DateFormat('MM-dd HH:mm');
  static final DateFormat _fullFormat = DateFormat('yyyy-MM-dd HH:mm');

  /// 格式化消息时间
  static String formatMessageTime(int timestamp) {
    final dateTime = DateTime.fromMillisecondsSinceEpoch(timestamp * 1000);
    final now = DateTime.now();
    final today = DateTime(now.year, now.month, now.day);
    final messageDate = DateTime(dateTime.year, dateTime.month, dateTime.day);

    if (messageDate == today) {
      return _timeFormat.format(dateTime);
    } else if (now.difference(messageDate).inDays < 7) {
      return _dateTimeFormat.format(dateTime);
    } else {
      return _fullFormat.format(dateTime);
    }
  }

  /// 格式化相对时间
  static String formatRelativeTime(int timestamp) {
    final dateTime = DateTime.fromMillisecondsSinceEpoch(timestamp * 1000);
    final now = DateTime.now();
    final difference = now.difference(dateTime);

    if (difference.inSeconds < 60) {
      return '刚刚';
    } else if (difference.inMinutes < 60) {
      return '${difference.inMinutes}分钟前';
    } else if (difference.inHours < 24) {
      return '${difference.inHours}小时前';
    } else if (difference.inDays < 7) {
      return '${difference.inDays}天前';
    } else if (difference.inDays < 30) {
      return '${difference.inDays ~/ 7}周前';
    } else if (difference.inDays < 365) {
      return '${difference.inDays ~/ 30}个月前';
    } else {
      return _fullFormat.format(dateTime);
    }
  }
}
