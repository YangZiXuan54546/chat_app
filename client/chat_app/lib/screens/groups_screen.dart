import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';
import 'create_group_screen.dart';
import 'chat_screen.dart';

class GroupsScreen extends StatelessWidget {
  const GroupsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('群组'),
        actions: [
          IconButton(
            icon: const Icon(Icons.group_add),
            onPressed: () {
              Navigator.of(context).push(
                MaterialPageRoute(builder: (_) => const CreateGroupScreen()),
              );
            },
          ),
        ],
      ),
      body: Consumer<ChatService>(
        builder: (context, chatService, child) {
          // 获取群组列表
          final groups = chatService.groups;
          
          if (groups.isEmpty) {
            return Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.group_outlined, size: 64, color: Colors.grey),
                  const SizedBox(height: 16),
                  const Text('暂无群组', style: TextStyle(color: Colors.grey)),
                  const SizedBox(height: 16),
                  FilledButton.icon(
                    onPressed: () {
                      Navigator.of(context).push(
                        MaterialPageRoute(builder: (_) => const CreateGroupScreen()),
                      );
                    },
                    icon: const Icon(Icons.add),
                    label: const Text('创建群组'),
                  ),
                ],
              ),
            );
          }
          
          return ListView.builder(
            itemCount: groups.length,
            itemBuilder: (context, index) {
              final group = groups[index];
              return ListTile(
                leading: CircleAvatar(
                  backgroundColor: Theme.of(context).colorScheme.secondaryContainer,
                  child: Icon(
                    Icons.group,
                    color: Theme.of(context).colorScheme.onSecondaryContainer,
                  ),
                ),
                title: Text(group.groupName),
                subtitle: Text('${group.memberCount}人'),
                onTap: () {
                  Navigator.of(context).push(
                    MaterialPageRoute(
                      builder: (_) => ChatScreen(
                        peerId: group.groupId,
                        peerName: group.groupName,
                        isGroup: true,
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
          Navigator.of(context).push(
            MaterialPageRoute(builder: (_) => const CreateGroupScreen()),
          );
        },
        child: const Icon(Icons.add),
      ),
    );
  }
}
