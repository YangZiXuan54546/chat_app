import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';
import '../services/storage_service.dart';
import 'login_screen.dart';
import 'edit_profile_screen.dart';

class ProfileScreen extends StatelessWidget {
  const ProfileScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<ChatService>(
      builder: (context, chatService, child) {
        final user = chatService.currentUser;
        
        return Scaffold(
          appBar: AppBar(
            title: const Text('我的'),
            actions: [
              IconButton(
                icon: const Icon(Icons.settings),
                onPressed: () {
                  // 设置页面
                },
              ),
            ],
          ),
          body: ListView(
            children: [
              // 用户信息卡片
              Container(
                margin: const EdgeInsets.all(16),
                child: Card(
                  child: InkWell(
                    onTap: () {
                      Navigator.of(context).push(
                        MaterialPageRoute(builder: (_) => const EditProfileScreen()),
                      );
                    },
                    borderRadius: BorderRadius.circular(12),
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Row(
                        children: [
                          // 头像
                          CircleAvatar(
                            radius: 40,
                            backgroundColor: Theme.of(context).colorScheme.primaryContainer,
                            child: Text(
                              user?.displayName[0] ?? '?',
                              style: TextStyle(
                                fontSize: 32,
                                color: Theme.of(context).colorScheme.onPrimaryContainer,
                              ),
                            ),
                          ),
                          
                          const SizedBox(width: 16),
                          
                          // 用户信息
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  user?.displayName ?? '未登录',
                                  style: Theme.of(context).textTheme.titleLarge,
                                ),
                                const SizedBox(height: 4),
                                Text(
                                  user?.signature ?? '暂无签名',
                                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                    color: Theme.of(context).colorScheme.onSurfaceVariant,
                                  ),
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ],
                            ),
                          ),
                          
                          Icon(Icons.chevron_right, 
                            color: Theme.of(context).colorScheme.onSurfaceVariant),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
              
              // 功能列表
              _buildSection(
                context,
                children: [
                  _buildListItem(
                    context,
                    icon: Icons.star_outline,
                    title: '收藏',
                    onTap: () {},
                  ),
                  _buildListItem(
                    context,
                    icon: Icons.photo_library_outlined,
                    title: '相册',
                    onTap: () {},
                  ),
                  _buildListItem(
                    context,
                    icon: Icons.folder_outlined,
                    title: '文件',
                    onTap: () {},
                  ),
                ],
              ),
              
              _buildSection(
                context,
                children: [
                  _buildListItem(
                    context,
                    icon: Icons.notifications_outlined,
                    title: '通知设置',
                    onTap: () {},
                  ),
                  _buildListItem(
                    context,
                    icon: Icons.privacy_tip_outlined,
                    title: '隐私设置',
                    onTap: () {},
                  ),
                  _buildListItem(
                    context,
                    icon: Icons.palette_outlined,
                    title: '主题设置',
                    onTap: () {},
                  ),
                ],
              ),
              
              _buildSection(
                context,
                children: [
                  _buildListItem(
                    context,
                    icon: Icons.help_outline,
                    title: '帮助与反馈',
                    onTap: () {},
                  ),
                  _buildListItem(
                    context,
                    icon: Icons.info_outline,
                    title: '关于',
                    onTap: () {
                      showAboutDialog(
                        context: context,
                        applicationName: 'Chat App',
                        applicationVersion: '1.0.0',
                        applicationLegalese: '© 2024 Chat App',
                      );
                    },
                  ),
                ],
              ),
              
              // 退出登录按钮
              Container(
                margin: const EdgeInsets.all(16),
                child: OutlinedButton.icon(
                  onPressed: () => _showLogoutDialog(context, chatService),
                  icon: const Icon(Icons.logout),
                  label: const Text('退出登录'),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: Theme.of(context).colorScheme.error,
                  ),
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildSection(BuildContext context, {required List<Widget> children}) {
    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Column(
        children: [
          for (int i = 0; i < children.length; i++) ...[
            children[i],
            if (i < children.length - 1)
              Divider(
                height: 1,
                indent: 56,
                color: Theme.of(context).colorScheme.outlineVariant,
              ),
          ],
        ],
      ),
    );
  }

  Widget _buildListItem(
    BuildContext context, {
    required IconData icon,
    required String title,
    VoidCallback? onTap,
  }) {
    return ListTile(
      leading: Icon(icon),
      title: Text(title),
      trailing: Icon(
        Icons.chevron_right,
        color: Theme.of(context).colorScheme.onSurfaceVariant,
      ),
      onTap: onTap,
    );
  }

  void _showLogoutDialog(BuildContext context, ChatService chatService) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('退出登录'),
        content: const Text('确定要退出登录吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () async {
              chatService.logout();
              await StorageService().clearCurrentUser();
              
              if (context.mounted) {
                Navigator.of(context).pushAndRemoveUntil(
                  MaterialPageRoute(builder: (_) => const LoginScreen()),
                  (route) => false,
                );
              }
            },
            child: const Text('退出'),
          ),
        ],
      ),
    );
  }
}
