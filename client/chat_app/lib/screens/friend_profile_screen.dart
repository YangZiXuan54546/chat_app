import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';
import 'chat_screen.dart';

class FriendProfileScreen extends StatefulWidget {
  final Friend friend;

  const FriendProfileScreen({super.key, required this.friend});

  @override
  State<FriendProfileScreen> createState() => _FriendProfileScreenState();
}

class _FriendProfileScreenState extends State<FriendProfileScreen> {
  final TextEditingController _remarkController = TextEditingController();
  bool _isEditingRemark = false;

  @override
  void initState() {
    super.initState();
    _remarkController.text = widget.friend.relation.remark;
  }

  @override
  void dispose() {
    _remarkController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('好友详情'),
        actions: [
          PopupMenuButton<String>(
            onSelected: _handleMenuAction,
            itemBuilder: (context) => [
              const PopupMenuItem(
                value: 'remark',
                child: ListTile(
                  leading: Icon(Icons.edit),
                  title: Text('设置备注'),
                  contentPadding: EdgeInsets.zero,
                ),
              ),
              const PopupMenuItem(
                value: 'delete',
                child: ListTile(
                  leading: Icon(Icons.delete, color: Colors.red),
                  title: Text('删除好友', style: TextStyle(color: Colors.red)),
                  contentPadding: EdgeInsets.zero,
                ),
              ),
            ],
          ),
        ],
      ),
      body: Consumer<ChatService>(
        builder: (context, chatService, child) {
          // 从服务中获取最新的好友信息
          final friend = chatService.friends.firstWhere(
            (f) => f.user.userId == widget.friend.user.userId,
            orElse: () => widget.friend,
          );
          
          return SingleChildScrollView(
            child: Column(
              children: [
                // 头像和基本信息
                Container(
                  padding: const EdgeInsets.all(24),
                  child: Column(
                    children: [
                      CircleAvatar(
                        radius: 48,
                        child: Text(
                          friend.displayName[0].toUpperCase(),
                          style: const TextStyle(fontSize: 36),
                        ),
                      ),
                      const SizedBox(height: 16),
                      Text(
                        friend.displayName,
                        style: const TextStyle(
                          fontSize: 24,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      if (friend.relation.remark.isNotEmpty &&
                          friend.relation.remark != friend.user.nickname)
                        Padding(
                          padding: const EdgeInsets.only(top: 4),
                          child: Text(
                            '昵称: ${friend.user.nickname}',
                            style: TextStyle(
                              fontSize: 14,
                              color: Colors.grey[600],
                            ),
                          ),
                        ),
                      const SizedBox(height: 8),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Container(
                            width: 8,
                            height: 8,
                            decoration: BoxDecoration(
                              color: friend.user.isOnline
                                  ? Colors.green
                                  : Colors.grey,
                              borderRadius: BorderRadius.circular(4),
                            ),
                          ),
                          const SizedBox(width: 8),
                          Text(
                            friend.user.isOnline ? '在线' : '离线',
                            style: TextStyle(
                              color:
                                  friend.user.isOnline ? Colors.green : Colors.grey,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                
                const Divider(),
                
                // 详细信息
                ListTile(
                  leading: const Icon(Icons.person),
                  title: const Text('用户名'),
                  subtitle: Text(friend.user.username),
                ),
                ListTile(
                  leading: const Icon(Icons.chat_bubble_outline),
                  title: const Text('签名'),
                  subtitle: Text(
                    friend.user.signature.isNotEmpty
                        ? friend.user.signature
                        : '暂无签名',
                  ),
                ),
                
                const SizedBox(height: 16),
                
                // 操作按钮
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 24),
                  child: Row(
                    children: [
                      Expanded(
                        child: ElevatedButton.icon(
                          onPressed: () => _startChat(context, friend),
                          icon: const Icon(Icons.chat),
                          label: const Text('发消息'),
                          style: ElevatedButton.styleFrom(
                            padding: const EdgeInsets.symmetric(vertical: 12),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                
                // 备注编辑区域
                if (_isEditingRemark) ...[
                  const SizedBox(height: 24),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 24),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          '设置备注',
                          style: TextStyle(
                            fontSize: 16,
                            fontWeight: FontWeight.bold,
                            color: Colors.grey[700],
                          ),
                        ),
                        const SizedBox(height: 8),
                        TextField(
                          controller: _remarkController,
                          decoration: InputDecoration(
                            hintText: '请输入备注名',
                            border: const OutlineInputBorder(),
                            suffixIcon: IconButton(
                              icon: const Icon(Icons.clear),
                              onPressed: () => _remarkController.clear(),
                            ),
                          ),
                          maxLength: 20,
                        ),
                        const SizedBox(height: 8),
                        Row(
                          mainAxisAlignment: MainAxisAlignment.end,
                          children: [
                            TextButton(
                              onPressed: () {
                                setState(() {
                                  _isEditingRemark = false;
                                  _remarkController.text = friend.relation.remark;
                                });
                              },
                              child: const Text('取消'),
                            ),
                            const SizedBox(width: 8),
                            ElevatedButton(
                              onPressed: () => _saveRemark(context),
                              child: const Text('保存'),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ],
              ],
            ),
          );
        },
      ),
    );
  }

  void _handleMenuAction(String action) {
    switch (action) {
      case 'remark':
        setState(() {
          _isEditingRemark = true;
        });
        break;
      case 'delete':
        _showDeleteConfirmation(context);
        break;
    }
  }

  void _startChat(BuildContext context, Friend friend) {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (_) => ChatScreen(
          peerId: friend.user.userId,
          peerName: friend.displayName,
        ),
      ),
    );
  }

  void _saveRemark(BuildContext context) {
    final remark = _remarkController.text.trim();
    context.read<ChatService>().setFriendRemark(
          widget.friend.user.userId,
          remark,
        );
    setState(() {
      _isEditingRemark = false;
    });
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('备注已更新')),
    );
  }

  void _showDeleteConfirmation(BuildContext context) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('删除好友'),
        content: Text('确定要删除好友 "${widget.friend.displayName}" 吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          TextButton(
            onPressed: () {
              context.read<ChatService>().removeFriend(widget.friend.user.userId);
              Navigator.pop(context); // 关闭对话框
              Navigator.pop(context); // 返回上一页
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('好友已删除')),
              );
            },
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('删除'),
          ),
        ],
      ),
    );
  }
}
