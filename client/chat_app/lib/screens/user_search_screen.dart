import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';

class UserSearchScreen extends StatefulWidget {
  const UserSearchScreen({super.key});

  @override
  State<UserSearchScreen> createState() => _UserSearchScreenState();
}

class _UserSearchScreenState extends State<UserSearchScreen> {
  final _searchController = TextEditingController();
  bool _isSearching = false;

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  Future<void> _search() async {
    final keyword = _searchController.text.trim();
    if (keyword.isEmpty) return;

    setState(() {
      _isSearching = true;
    });

    context.read<ChatService>().searchUsers(keyword);
    
    // 等待搜索结果
    await Future.delayed(const Duration(milliseconds: 500));
    
    if (mounted) {
      setState(() {
        _isSearching = false;
      });
    }
  }

  Future<void> _addFriend(BuildContext context, User user) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('添加好友'),
        content: Text('确定要添加 ${user.nickname} 为好友吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('添加'),
          ),
        ],
      ),
    );

    if (confirmed == true && mounted) {
      final success = await context.read<ChatService>().addFriend(user.userId);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(success ? '好友请求已发送' : '发送失败，请稍后重试'),
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: TextField(
          controller: _searchController,
          decoration: const InputDecoration(
            hintText: '搜索用户',
            border: InputBorder.none,
          ),
          autofocus: true,
          textInputAction: TextInputAction.search,
          onSubmitted: (_) => _search(),
        ),
        actions: [
          TextButton(
            onPressed: _isSearching ? null : _search,
            child: const Text('搜索'),
          ),
        ],
      ),
      body: _buildBody(),
    );
  }

  Widget _buildBody() {
    if (_isSearching) {
      return const Center(
        child: CircularProgressIndicator(),
      );
    }

    if (_searchController.text.isEmpty) {
      return const Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.search, size: 64, color: Colors.grey),
            SizedBox(height: 16),
            Text('输入关键词搜索用户', style: TextStyle(color: Colors.grey)),
          ],
        ),
      );
    }

    return Consumer<ChatService>(
      builder: (context, chatService, child) {
        final results = chatService.searchResults;
        
        if (results.isEmpty) {
          return const Center(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(Icons.person_off, size: 64, color: Colors.grey),
                SizedBox(height: 16),
                Text('未找到相关用户', style: TextStyle(color: Colors.grey)),
              ],
            ),
          );
        }

        return ListView.builder(
          itemCount: results.length,
          itemBuilder: (context, index) {
            final user = results[index];
            final isSelf = user.userId == chatService.currentUserId;
            final isFriend = chatService.friends.any((f) => f.user.userId == user.userId);
            
            return ListTile(
              leading: CircleAvatar(
                child: Text(user.nickname.isNotEmpty ? user.nickname[0] : '?'),
              ),
              title: Text(user.nickname.isNotEmpty ? user.nickname : user.username),
              subtitle: Text('@${user.username}'),
              trailing: isSelf
                  ? const Chip(label: Text('自己'))
                  : isFriend
                      ? const Chip(label: Text('已添加'))
                      : OutlinedButton(
                          onPressed: () => _addFriend(context, user),
                          child: const Text('添加'),
                        ),
            );
          },
        );
      },
    );
  }
}