import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';

class CreateGroupScreen extends StatefulWidget {
  const CreateGroupScreen({super.key});

  @override
  State<CreateGroupScreen> createState() => _CreateGroupScreenState();
}

class _CreateGroupScreenState extends State<CreateGroupScreen> {
  final _formKey = GlobalKey<FormState>();
  final _nameController = TextEditingController();
  final _descriptionController = TextEditingController();
  
  final Set<int> _selectedMembers = {};
  bool _isLoading = false;

  @override
  void dispose() {
    _nameController.dispose();
    _descriptionController.dispose();
    super.dispose();
  }

  Future<void> _createGroup() async {
    if (!_formKey.currentState!.validate()) return;
    if (_selectedMembers.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('请至少选择一个群成员')),
      );
      return;
    }

    setState(() {
      _isLoading = true;
    });

    try {
      context.read<ChatService>().createGroup(
        _nameController.text,
        _descriptionController.text,
      );
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('群组创建成功')),
        );
        Navigator.pop(context);
      }
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('创建群组'),
        actions: [
          TextButton(
            onPressed: _isLoading ? null : _createGroup,
            child: _isLoading
              ? const SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              : const Text('创建'),
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Form(
          key: _formKey,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // 群组头像
              Center(
                child: Stack(
                  children: [
                    CircleAvatar(
                      radius: 50,
                      backgroundColor: Theme.of(context).colorScheme.primaryContainer,
                      child: Icon(
                        Icons.group,
                        size: 50,
                        color: Theme.of(context).colorScheme.onPrimaryContainer,
                      ),
                    ),
                    Positioned(
                      right: 0,
                      bottom: 0,
                      child: Container(
                        padding: const EdgeInsets.all(4),
                        decoration: BoxDecoration(
                          color: Theme.of(context).colorScheme.primary,
                          shape: BoxShape.circle,
                        ),
                        child: const Icon(Icons.camera_alt, size: 20, color: Colors.white),
                      ),
                    ),
                  ],
                ),
              ),
              
              const SizedBox(height: 24),
              
              // 群组名称
              TextFormField(
                controller: _nameController,
                decoration: const InputDecoration(
                  labelText: '群组名称 *',
                  prefixIcon: Icon(Icons.label_outline),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return '请输入群组名称';
                  }
                  return null;
                },
              ),
              
              const SizedBox(height: 16),
              
              // 群组描述
              TextFormField(
                controller: _descriptionController,
                decoration: const InputDecoration(
                  labelText: '群组描述',
                  prefixIcon: Icon(Icons.description_outlined),
                  alignLabelWithHint: true,
                ),
                maxLines: 3,
              ),
              
              const SizedBox(height: 24),
              
              // 选择成员
              Row(
                children: [
                  const Text(
                    '选择成员',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                  ),
                  const Spacer(),
                  Text(
                    '已选 ${_selectedMembers.length} 人',
                    style: TextStyle(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
                  ),
                ],
              ),
              
              const SizedBox(height: 12),
              
              // 好友列表
              Consumer<ChatService>(
                builder: (context, chatService, child) {
                  final friends = chatService.friends;
                  
                  if (friends.isEmpty) {
                    return const Card(
                      child: Padding(
                        padding: EdgeInsets.all(16),
                        child: Text('暂无好友，请先添加好友'),
                      ),
                    );
                  }
                  
                  return Card(
                    child: ListView.builder(
                      shrinkWrap: true,
                      physics: const NeverScrollableScrollPhysics(),
                      itemCount: friends.length,
                      itemBuilder: (context, index) {
                        final friend = friends[index];
                        final isSelected = _selectedMembers.contains(friend.user.userId);
                        
                        return CheckboxListTile(
                          value: isSelected,
                          onChanged: (checked) {
                            setState(() {
                              if (checked == true) {
                                _selectedMembers.add(friend.user.userId);
                              } else {
                                _selectedMembers.remove(friend.user.userId);
                              }
                            });
                          },
                          secondary: CircleAvatar(
                            child: Text(friend.displayName[0]),
                          ),
                          title: Text(friend.displayName),
                          subtitle: Text('@${friend.user.username}'),
                        );
                      },
                    ),
                  );
                },
              ),
            ],
          ),
        ),
      ),
    );
  }
}
