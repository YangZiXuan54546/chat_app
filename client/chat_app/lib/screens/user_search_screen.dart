import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';

class UserSearchScreen extends StatefulWidget {
  const UserSearchScreen({super.key});

  @override
  State<UserSearchScreen> createState() => _UserSearchScreenState();
}

class _UserSearchScreenState extends State<UserSearchScreen> {
  final _searchController = TextEditingController();
  List<dynamic> _searchResults = [];
  bool _isSearching = false;

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  void _search() {
    final keyword = _searchController.text.trim();
    if (keyword.isEmpty) return;

    setState(() {
      _isSearching = true;
    });

    context.read<ChatService>().searchUsers(keyword);
    
    // 模拟搜索延迟
    Future.delayed(const Duration(seconds: 1), () {
      if (mounted) {
        setState(() {
          _isSearching = false;
          _searchResults = []; // 实际从 chatService 获取
        });
      }
    });
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
            onPressed: _search,
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

    if (_searchResults.isEmpty) {
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
      itemCount: _searchResults.length,
      itemBuilder: (context, index) {
        final user = _searchResults[index];
        return ListTile(
          leading: CircleAvatar(
            child: Text(user['nickname']?[0] ?? user['username']?[0] ?? '?'),
          ),
          title: Text(user['nickname'] ?? user['username']),
          subtitle: Text('@${user['username']}'),
          trailing: OutlinedButton(
            onPressed: () {
              _showAddFriendDialog(context, user);
            },
            child: const Text('添加'),
          ),
        );
      },
    );
  }

  void _showAddFriendDialog(BuildContext context, dynamic user) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('添加好友'),
        content: Text('确定要添加 ${user['nickname'] ?? user['username']} 为好友吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () {
              context.read<ChatService>().addFriend(user['user_id']);
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('好友请求已发送')),
              );
            },
            child: const Text('添加'),
          ),
        ],
      ),
    );
  }
}
