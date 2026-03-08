import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';

class ChatScreen extends StatefulWidget {
  final int peerId;
  final String peerName;
  final bool isGroup;

  const ChatScreen({
    super.key,
    required this.peerId,
    required this.peerName,
    this.isGroup = false,
  });

  @override
  State<ChatScreen> createState() => _ChatScreenState();
}

class _ChatScreenState extends State<ChatScreen> {
  final _messageController = TextEditingController();
  final _scrollController = ScrollController();
  final _focusNode = FocusNode();
  
  bool _showEmoji = false;

  @override
  void dispose() {
    _messageController.dispose();
    _scrollController.dispose();
    _focusNode.dispose();
    super.dispose();
  }

  void _sendMessage() {
    final text = _messageController.text.trim();
    if (text.isEmpty) return;

    final chatService = context.read<ChatService>();
    
    if (widget.isGroup) {
      chatService.sendGroupMessage(widget.peerId, text);
    } else {
      chatService.sendPrivateMessage(widget.peerId, text);
    }

    _messageController.clear();
    _scrollToBottom();
  }

  void _scrollToBottom() {
    if (_scrollController.hasClients) {
      _scrollController.animateTo(
        _scrollController.position.maxScrollExtent,
        duration: const Duration(milliseconds: 300),
        curve: Curves.easeOut,
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            CircleAvatar(
              radius: 18,
              child: Text(widget.peerName[0]),
            ),
            const SizedBox(width: 12),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  widget.peerName,
                  style: const TextStyle(fontSize: 16),
                ),
                Text(
                  widget.isGroup ? '群聊' : '在线',
                  style: TextStyle(
                    fontSize: 12,
                    color: Theme.of(context).colorScheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.more_vert),
            onPressed: () {
              // 更多选项
            },
          ),
        ],
      ),
      body: Column(
        children: [
          // 消息列表
          Expanded(
            child: Consumer<ChatService>(
              builder: (context, chatService, child) {
                final messages = chatService.getMessages(
                  widget.peerId,
                  isGroup: widget.isGroup,
                );
                
                if (messages.isEmpty) {
                  return const Center(
                    child: Text('暂无消息，发送一条消息开始聊天吧'),
                  );
                }
                
                return ListView.builder(
                  controller: _scrollController,
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 16),
                  itemCount: messages.length,
                  itemBuilder: (context, index) {
                    final message = messages[index];
                    final isMe = message.senderId == chatService.currentUserId;
                    return _buildMessageItem(message, isMe);
                  },
                );
              },
            ),
          ),
          
          // 输入区域
          Container(
            decoration: BoxDecoration(
              color: Theme.of(context).colorScheme.surface,
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withOpacity(0.05),
                  blurRadius: 4,
                  offset: const Offset(0, -2),
                ),
              ],
            ),
            child: SafeArea(
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
                child: Row(
                  children: [
                    // 表情按钮
                    IconButton(
                      icon: const Icon(Icons.emoji_emotions_outlined),
                      onPressed: () {
                        setState(() {
                          _showEmoji = !_showEmoji;
                        });
                      },
                    ),
                    
                    // 输入框
                    Expanded(
                      child: TextField(
                        controller: _messageController,
                        focusNode: _focusNode,
                        decoration: InputDecoration(
                          hintText: '输入消息...',
                          border: OutlineInputBorder(
                            borderRadius: BorderRadius.circular(24),
                          ),
                          contentPadding: const EdgeInsets.symmetric(
                            horizontal: 16,
                            vertical: 8,
                          ),
                          filled: true,
                          fillColor: Theme.of(context).colorScheme.surfaceContainerHighest,
                        ),
                        maxLines: 5,
                        minLines: 1,
                        textInputAction: TextInputAction.send,
                        onSubmitted: (_) => _sendMessage(),
                      ),
                    ),
                    
                    const SizedBox(width: 8),
                    
                    // 发送按钮
                    FilledButton(
                      onPressed: _sendMessage,
                      style: FilledButton.styleFrom(
                        shape: const CircleBorder(),
                        padding: const EdgeInsets.all(12),
                      ),
                      child: const Icon(Icons.send, size: 20),
                    ),
                    
                    // 更多按钮
                    IconButton(
                      icon: const Icon(Icons.attach_file),
                      onPressed: () {
                        _showAttachmentOptions(context);
                      },
                    ),
                  ],
                ),
              ),
            ),
          ),
          
          // 表情面板
          if (_showEmoji)
            Container(
              height: 200,
              color: Theme.of(context).colorScheme.surfaceContainerHighest,
              child: Center(
                child: Text('表情面板 (可集成 emoji_picker_flutter)'),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildMessageItem(Message message, bool isMe) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: isMe ? MainAxisAlignment.end : MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          if (!isMe) ...[
            CircleAvatar(
              radius: 16,
              child: Text(widget.peerName[0]),
            ),
            const SizedBox(width: 8),
          ],
          
          Flexible(
            child: Container(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
              decoration: BoxDecoration(
                color: isMe
                  ? Theme.of(context).colorScheme.primaryContainer
                  : Theme.of(context).colorScheme.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(20).copyWith(
                  bottomLeft: isMe ? const Radius.circular(20) : const Radius.circular(4),
                  bottomRight: isMe ? const Radius.circular(4) : const Radius.circular(20),
                ),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (message.isImage)
                    ClipRRect(
                      borderRadius: BorderRadius.circular(8),
                      child: Image.network(
                        message.mediaUrl,
                        width: 200,
                        fit: BoxFit.cover,
                        errorBuilder: (_, __, ___) => const Icon(Icons.broken_image),
                      ),
                    )
                  else if (message.isFile)
                    Container(
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: Colors.white.withOpacity(0.5),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const Icon(Icons.insert_drive_file),
                          const SizedBox(width: 8),
                          Text(message.content),
                        ],
                      ),
                    )
                  else
                    Text(
                      message.content,
                      style: TextStyle(
                        color: isMe
                          ? Theme.of(context).colorScheme.onPrimaryContainer
                          : Theme.of(context).colorScheme.onSurface,
                      ),
                    ),
                ],
              ),
            ),
          ),
          
          if (isMe) ...[
            const SizedBox(width: 8),
            Icon(
              _getStatusIcon(message.status),
              size: 16,
              color: Theme.of(context).colorScheme.onSurfaceVariant,
            ),
          ],
        ],
      ),
    );
  }

  IconData _getStatusIcon(int status) {
    switch (status) {
      case 0: return Icons.access_time;
      case 1: return Icons.check;
      case 2: return Icons.done_all;
      case 3: return Icons.done_all;
      default: return Icons.error_outline;
    }
  }

  void _showAttachmentOptions(BuildContext context) {
    showModalBottomSheet(
      context: context,
      builder: (context) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.spaceAround,
            children: [
              _buildAttachmentOption(
                icon: Icons.image,
                label: '图片',
                onTap: () {
                  Navigator.pop(context);
                  // 选择图片
                },
              ),
              _buildAttachmentOption(
                icon: Icons.camera_alt,
                label: '拍照',
                onTap: () {
                  Navigator.pop(context);
                  // 拍照
                },
              ),
              _buildAttachmentOption(
                icon: Icons.videocam,
                label: '视频',
                onTap: () {
                  Navigator.pop(context);
                  // 选择视频
                },
              ),
              _buildAttachmentOption(
                icon: Icons.insert_drive_file,
                label: '文件',
                onTap: () {
                  Navigator.pop(context);
                  // 选择文件
                },
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildAttachmentOption({
    required IconData icon,
    required String label,
    VoidCallback? onTap,
  }) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 56,
              height: 56,
              decoration: BoxDecoration(
                color: Theme.of(context).colorScheme.primaryContainer,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(
                icon,
                color: Theme.of(context).colorScheme.onPrimaryContainer,
              ),
            ),
            const SizedBox(height: 8),
            Text(label),
          ],
        ),
      ),
    );
  }
}
