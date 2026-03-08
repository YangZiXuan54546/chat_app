import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';
import 'chat_screen.dart';

class ChatsScreen extends StatelessWidget {
  const ChatsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('消息'),
        actions: [
          IconButton(
            icon: const Icon(Icons.search),
            onPressed: () {
              // 搜索
            },
          ),
        ],
      ),
      body: Consumer<ChatService>(
        builder: (context, chatService, child) {
          final conversations = chatService.conversations;
          
          if (conversations.isEmpty) {
            return const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(Icons.chat_bubble_outline, size: 64, color: Colors.grey),
                  SizedBox(height: 16),
                  Text('暂无消息', style: TextStyle(color: Colors.grey)),
                ],
              ),
            );
          }
          
          return ListView.builder(
            itemCount: conversations.length,
            itemBuilder: (context, index) {
              final conv = conversations[index];
              return ListTile(
                leading: CircleAvatar(
                  child: Text(conv.name[0]),
                ),
                title: Text(conv.name),
                subtitle: Text(
                  conv.lastMessage?.content ?? '',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                trailing: conv.unreadCount > 0
                  ? Container(
                      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                      decoration: BoxDecoration(
                        color: Theme.of(context).colorScheme.primary,
                        borderRadius: BorderRadius.circular(12),
                      ),
                      child: Text(
                        conv.unreadCount > 99 ? '99+' : conv.unreadCount.toString(),
                        style: const TextStyle(color: Colors.white, fontSize: 12),
                      ),
                    )
                  : null,
                onTap: () {
                  Navigator.of(context).push(
                    MaterialPageRoute(
                      builder: (_) => ChatScreen(
                        peerId: conv.groupId > 0 ? conv.groupId : conv.peerId,
                        peerName: conv.name,
                        isGroup: conv.groupId > 0,
                      ),
                    ),
                  );
                },
              );
            },
          );
        },
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () {
          // 新建聊天
        },
        child: const Icon(Icons.add_comment),
      ),
    );
  }
}
