import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';
import 'chat_screen.dart';
import 'user_search_screen.dart';
import 'friend_profile_screen.dart';

class ContactsScreen extends StatefulWidget {
  const ContactsScreen({super.key});

  @override
  State<ContactsScreen> createState() => _ContactsScreenState();
}

class _ContactsScreenState extends State<ContactsScreen> {
  @override
  void initState() {
    super.initState();
    // 加载好友列表
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<ChatService>();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('通讯录'),
        actions: [
          IconButton(
            icon: const Icon(Icons.person_add),
            onPressed: () {
              Navigator.of(context).push(
                MaterialPageRoute(builder: (_) => const UserSearchScreen()),
              );
            },
          ),
        ],
      ),
      body: Consumer<ChatService>(
        builder: (context, chatService, child) {
          final friends = chatService.friends;
          final requests = chatService.friendRequests;
          
          return CustomScrollView(
            slivers: [
              // 好友请求
              if (requests.isNotEmpty)
                SliverToBoxAdapter(
                  child: Card(
                    margin: const EdgeInsets.all(8),
                    child: ListTile(
                      leading: Badge(
                        label: Text(requests.length.toString()),
                        child: const Icon(Icons.person_add),
                      ),
                      title: const Text('好友请求'),
                      subtitle: Text('${requests.length}个待处理'),
                      trailing: const Icon(Icons.chevron_right),
                      onTap: () {
                        _showFriendRequests(context, requests);
                      },
                    ),
                  ),
                ),
              
              // 好友列表标题
              const SliverToBoxAdapter(
                child: Padding(
                  padding: EdgeInsets.fromLTRB(16, 16, 16, 8),
                  child: Text(
                    '好友列表',
                    style: TextStyle(
                      fontSize: 14,
                      fontWeight: FontWeight.bold,
                      color: Colors.grey,
                    ),
                  ),
                ),
              ),
              
              // 好友列表
              friends.isEmpty
                ? const SliverFillRemaining(
                    child: Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Icon(Icons.people_outline, size: 64, color: Colors.grey),
                          SizedBox(height: 16),
                          Text('暂无好友', style: TextStyle(color: Colors.grey)),
                        ],
                      ),
                    ),
                  )
                : SliverList(
                    delegate: SliverChildBuilderDelegate(
                      (context, index) {
                        final friend = friends[index];
                        return ListTile(
                          leading: GestureDetector(
                            onTap: () {
                              Navigator.of(context).push(
                                MaterialPageRoute(
                                  builder: (_) => FriendProfileScreen(friend: friend),
                                ),
                              );
                            },
                            child: CircleAvatar(
                              child: Text(friend.displayName[0]),
                            ),
                          ),
                          title: Text(friend.displayName),
                          subtitle: friend.user.signature.isNotEmpty
                            ? Text(
                                friend.user.signature,
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                              )
                            : null,
                          trailing: friend.user.isOnline
                            ? Container(
                                width: 10,
                                height: 10,
                                decoration: BoxDecoration(
                                  color: Colors.green,
                                  borderRadius: BorderRadius.circular(5),
                                ),
                              )
                            : null,
                          onTap: () {
                            Navigator.of(context).push(
                              MaterialPageRoute(
                                builder: (_) => ChatScreen(
                                  peerId: friend.user.userId,
                                  peerName: friend.displayName,
                                ),
                              ),
                            );
                          },
                          onLongPress: () {
                            _showFriendOptions(context, friend);
                          },
                        );
                      },
                      childCount: friends.length,
                    ),
                  ),
            ],
          );
        },
      ),
    );
  }

  void _showFriendRequests(BuildContext context, List<Friend> requests) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (context) => DraggableScrollableSheet(
        initialChildSize: 0.5,
        maxChildSize: 0.8,
        minChildSize: 0.3,
        expand: false,
        builder: (context, scrollController) => Column(
          children: [
            Padding(
              padding: const EdgeInsets.all(16),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text(
                    '好友请求',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                  IconButton(
                    icon: const Icon(Icons.close),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            const Divider(height: 1),
            Expanded(
              child: ListView.builder(
                controller: scrollController,
                itemCount: requests.length,
                itemBuilder: (context, index) {
                  final request = requests[index];
                  return ListTile(
                    leading: CircleAvatar(
                      child: Text(request.user.displayName[0]),
                    ),
                    title: Text(request.user.displayName),
                    subtitle: Text(request.user.username),
                    trailing: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        IconButton(
                          icon: const Icon(Icons.check, color: Colors.green),
                          onPressed: () {
                            context.read<ChatService>().acceptFriend(request.user.userId);
                          },
                        ),
                        IconButton(
                          icon: const Icon(Icons.close, color: Colors.red),
                          onPressed: () {
                            context.read<ChatService>().rejectFriend(request.user.userId);
                          },
                        ),
                      ],
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _showFriendOptions(BuildContext context, Friend friend) {
    showModalBottomSheet(
      context: context,
      builder: (context) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              leading: const Icon(Icons.person),
              title: const Text('查看详情'),
              onTap: () {
                Navigator.pop(context);
                Navigator.of(context).push(
                  MaterialPageRoute(
                    builder: (_) => FriendProfileScreen(friend: friend),
                  ),
                );
              },
            ),
            ListTile(
              leading: const Icon(Icons.chat),
              title: const Text('发消息'),
              onTap: () {
                Navigator.pop(context);
                Navigator.of(context).push(
                  MaterialPageRoute(
                    builder: (_) => ChatScreen(
                      peerId: friend.user.userId,
                      peerName: friend.displayName,
                    ),
                  ),
                );
              },
            ),
            ListTile(
              leading: const Icon(Icons.delete, color: Colors.red),
              title: const Text('删除好友', style: TextStyle(color: Colors.red)),
              onTap: () {
                Navigator.pop(context);
                _showDeleteConfirmation(context, friend);
              },
            ),
          ],
        ),
      ),
    );
  }

  void _showDeleteConfirmation(BuildContext context, Friend friend) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('删除好友'),
        content: Text('确定要删除好友 "${friend.displayName}" 吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          TextButton(
            onPressed: () {
              context.read<ChatService>().removeFriend(friend.user.userId);
              Navigator.pop(context);
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
