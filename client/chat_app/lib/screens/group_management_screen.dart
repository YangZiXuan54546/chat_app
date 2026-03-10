import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../models/models.dart';

class GroupManagementScreen extends StatefulWidget {
  final int groupId;
  final String groupName;

  const GroupManagementScreen({
    super.key,
    required this.groupId,
    required this.groupName,
  });

  @override
  State<GroupManagementScreen> createState() => _GroupManagementScreenState();
}

class _GroupManagementScreenState extends State<GroupManagementScreen> {
  List<GroupMember> _members = [];
  bool _isLoading = true;
  int? _currentUserId;
  Group? _group;

  @override
  void initState() {
    super.initState();
    _loadData();
  }

  void _loadData() {
    final chatService = context.read<ChatService>();
    _currentUserId = chatService.currentUserId;
    _group = chatService.groups.firstWhere(
      (g) => g.groupId == widget.groupId,
      orElse: () => Group(groupId: widget.groupId, groupName: widget.groupName),
    );
    
    // 请求获取群成员
    chatService.getGroupMembers(widget.groupId);
    
    // 模拟加载
    Future.delayed(const Duration(milliseconds: 500), () {
      if (mounted) {
        setState(() {
          _isLoading = false;
        });
      }
    });
  }

  bool get _isOwner => _group?.isOwner(_currentUserId ?? 0) ?? false;
  bool get _isAdmin => _group?.isAdmin(_currentUserId ?? 0) ?? false;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.groupName),
        actions: [
          PopupMenuButton<String>(
            onSelected: _handleMenuAction,
            itemBuilder: (context) => [
              if (_isOwner) ...[
                const PopupMenuItem(
                  value: 'dismiss',
                  child: ListTile(
                    leading: Icon(Icons.delete_forever, color: Colors.red),
                    title: Text('解散群组', style: TextStyle(color: Colors.red)),
                  ),
                ),
              ],
              if (!_isOwner) ...[
                const PopupMenuItem(
                  value: 'leave',
                  child: ListTile(
                    leading: Icon(Icons.exit_to_app, color: Colors.orange),
                    title: Text('退出群组', style: TextStyle(color: Colors.orange)),
                  ),
                ),
              ],
            ],
          ),
        ],
      ),
      body: _isLoading
          ? const Center(child: CircularProgressIndicator())
          : Consumer<ChatService>(
              builder: (context, chatService, child) {
                final group = chatService.groups.firstWhere(
                  (g) => g.groupId == widget.groupId,
                  orElse: () => Group(groupId: widget.groupId, groupName: widget.groupName),
                );
                _group = group;
                
                return Column(
                  children: [
                    // 群组信息卡片
                    _buildGroupInfoCard(group),
                    
                    // 成员列表标题
                    Padding(
                      padding: const EdgeInsets.all(16),
                      child: Row(
                        children: [
                          Text(
                            '群成员 (${group.members.length})',
                            style: Theme.of(context).textTheme.titleMedium,
                          ),
                          const Spacer(),
                          if (_isOwner || _isAdmin)
                            TextButton.icon(
                              icon: const Icon(Icons.person_add),
                              label: const Text('邀请'),
                              onPressed: () => _showInviteDialog(),
                            ),
                        ],
                      ),
                    ),
                    
                    // 成员列表
                    Expanded(
                      child: _buildMembersList(chatService),
                    ),
                  ],
                );
              },
            ),
    );
  }

  Widget _buildGroupInfoCard(Group group) {
    return Card(
      margin: const EdgeInsets.all(16),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                CircleAvatar(
                  radius: 32,
                  child: Text(group.groupName[0], style: const TextStyle(fontSize: 24)),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        group.groupName,
                        style: Theme.of(context).textTheme.titleLarge,
                      ),
                      const SizedBox(height: 4),
                      Text(
                        group.description.isNotEmpty ? group.description : '暂无群简介',
                        style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              children: [
                Chip(
                  label: Text('${group.members.length} 人'),
                  avatar: const Icon(Icons.people, size: 16),
                ),
                if (_isOwner)
                  const Chip(
                    label: Text('群主'),
                    avatar: Icon(Icons.star, size: 16),
                    backgroundColor: Colors.amber,
                  )
                else if (_isAdmin)
                  const Chip(
                    label: Text('管理员'),
                    avatar: Icon(Icons.shield, size: 16),
                    backgroundColor: Colors.blue,
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildMembersList(ChatService chatService) {
    // 构建成员列表
    final members = <GroupMember>[];
    
    // 从群组信息获取成员
    if (_group != null) {
      for (final memberId in _group!.members) {
        final user = chatService.searchResults.firstWhere(
          (u) => u.userId == memberId,
          orElse: () => User(userId: memberId, username: '用户$memberId', nickname: '用户$memberId'),
        );
        
        members.add(GroupMember(
          userId: memberId,
          nickname: user.nickname.isNotEmpty ? user.nickname : user.username,
          avatarUrl: user.avatarUrl,
          onlineStatus: user.onlineStatus,
          isOwner: _group!.ownerId == memberId,
          isAdmin: _group!.admins.contains(memberId),
        ));
      }
    }
    
    // 排序：群主 > 管理员 > 普通成员
    members.sort((a, b) {
      if (a.isOwner) return -1;
      if (b.isOwner) return 1;
      if (a.isAdmin && !b.isAdmin) return -1;
      if (!a.isAdmin && b.isAdmin) return 1;
      return 0;
    });

    if (members.isEmpty) {
      return const Center(child: Text('暂无成员'));
    }

    return ListView.builder(
      itemCount: members.length,
      itemBuilder: (context, index) {
        final member = members[index];
        return _buildMemberItem(member, chatService);
      },
    );
  }

  Widget _buildMemberItem(GroupMember member, ChatService chatService) {
    return ListTile(
      leading: Stack(
        children: [
          CircleAvatar(
            child: Text(member.nickname[0]),
          ),
          if (member.isOwner || member.isAdmin)
            Positioned(
              right: 0,
              bottom: 0,
              child: Container(
                padding: const EdgeInsets.all(2),
                decoration: BoxDecoration(
                  color: member.isOwner ? Colors.amber : Colors.blue,
                  shape: BoxShape.circle,
                ),
                child: Icon(
                  member.isOwner ? Icons.star : Icons.shield,
                  size: 12,
                  color: Colors.white,
                ),
              ),
            ),
        ],
      ),
      title: Text(member.nickname),
      subtitle: Text(_getMemberRole(member)),
      trailing: _buildMemberActions(member, chatService),
    );
  }

  String _getMemberRole(GroupMember member) {
    if (member.isOwner) return '群主';
    if (member.isAdmin) return '管理员';
    return '成员';
  }

  Widget? _buildMemberActions(GroupMember member, ChatService chatService) {
    // 不能对自己操作
    if (member.userId == _currentUserId) return null;
    
    // 只有群主可以操作
    if (!_isOwner) return null;
    
    // 不能对群主操作
    if (member.isOwner) return null;

    return PopupMenuButton<String>(
      onSelected: (value) => _handleMemberAction(value, member, chatService),
      itemBuilder: (context) => [
        if (!member.isAdmin)
          const PopupMenuItem(
            value: 'set_admin',
            child: ListTile(
              leading: Icon(Icons.shield),
              title: Text('设为管理员'),
            ),
          )
        else
          const PopupMenuItem(
            value: 'remove_admin',
            child: ListTile(
              leading: Icon(Icons.shield_outlined),
              title: Text('取消管理员'),
            ),
          ),
        const PopupMenuItem(
          value: 'transfer_owner',
          child: ListTile(
            leading: Icon(Icons.star),
            title: Text('转让群主'),
          ),
        ),
        const PopupMenuItem(
          value: 'remove',
          child: ListTile(
            leading: Icon(Icons.person_remove, color: Colors.red),
            title: Text('移出群聊', style: TextStyle(color: Colors.red)),
          ),
        ),
      ],
    );
  }

  void _handleMenuAction(String action) {
    switch (action) {
      case 'leave':
        _showLeaveConfirmation();
        break;
      case 'dismiss':
        _showDismissConfirmation();
        break;
    }
  }

  void _handleMemberAction(String action, GroupMember member, ChatService chatService) {
    switch (action) {
      case 'set_admin':
        chatService.setGroupAdmin(widget.groupId, member.userId, true);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('已将 ${member.nickname} 设为管理员')),
        );
        break;
      case 'remove_admin':
        chatService.setGroupAdmin(widget.groupId, member.userId, false);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('已取消 ${member.nickname} 的管理员身份')),
        );
        break;
      case 'transfer_owner':
        _showTransferOwnerConfirmation(member, chatService);
        break;
      case 'remove':
        _showRemoveMemberConfirmation(member, chatService);
        break;
    }
  }

  void _showLeaveConfirmation() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('退出群组'),
        content: Text('确定要退出群组 "${widget.groupName}" 吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () {
              context.read<ChatService>().leaveGroup(widget.groupId);
              Navigator.pop(context);
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('已退出群组')),
              );
            },
            child: const Text('退出'),
          ),
        ],
      ),
    );
  }

  void _showDismissConfirmation() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('解散群组'),
        content: Text('确定要解散群组 "${widget.groupName}" 吗？此操作不可撤销。'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: Colors.red),
            onPressed: () {
              context.read<ChatService>().dismissGroup(widget.groupId);
              Navigator.pop(context);
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('群组已解散')),
              );
            },
            child: const Text('解散'),
          ),
        ],
      ),
    );
  }

  void _showTransferOwnerConfirmation(GroupMember member, ChatService chatService) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('转让群主'),
        content: Text('确定要将群主转让给 ${member.nickname} 吗？转让后您将成为普通成员。'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () {
              chatService.transferGroupOwner(widget.groupId, member.userId);
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text('已将群主转让给 ${member.nickname}')),
              );
            },
            child: const Text('转让'),
          ),
        ],
      ),
    );
  }

  void _showRemoveMemberConfirmation(GroupMember member, ChatService chatService) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('移出群聊'),
        content: Text('确定要将 ${member.nickname} 移出群聊吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: Colors.red),
            onPressed: () {
              chatService.removeGroupMember(widget.groupId, member.userId);
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text('已将 ${member.nickname} 移出群聊')),
              );
            },
            child: const Text('移出'),
          ),
        ],
      ),
    );
  }

  void _showInviteDialog() {
    // 显示好友列表供选择邀请
    showDialog(
      context: context,
      builder: (context) {
        final chatService = context.read<ChatService>();
        final friends = chatService.friends;
        
        return AlertDialog(
          title: const Text('邀请好友'),
          content: SizedBox(
            width: double.maxFinite,
            height: 300,
            child: ListView.builder(
              itemCount: friends.length,
              itemBuilder: (context, index) {
                final friend = friends[index];
                return ListTile(
                  leading: CircleAvatar(
                    child: Text(friend.user.nickname[0]),
                  ),
                  title: Text(friend.user.nickname),
                  onTap: () {
                    chatService.inviteGroupMembers(widget.groupId, [friend.user.userId]);
                    Navigator.pop(context);
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(content: Text('已邀请 ${friend.user.nickname}')),
                    );
                  },
                );
              },
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('取消'),
            ),
          ],
        );
      },
    );
  }
}
