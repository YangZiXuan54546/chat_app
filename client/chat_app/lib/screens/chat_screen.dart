import 'dart:io';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:image_picker/image_picker.dart';
import 'package:cached_network_image/cached_network_image.dart';
import 'package:file_picker/file_picker.dart';
import 'package:url_launcher/url_launcher.dart';
import 'package:path_provider/path_provider.dart';
import 'package:path/path.dart' as path;
import '../services/chat_service.dart';
import '../models/models.dart';
import 'group_management_screen.dart';

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
  final _imagePicker = ImagePicker();
  
  bool _showEmoji = false;
  bool _isUploading = false;

  @override
  void initState() {
    super.initState();
    // 设置当前聊天界面状态（避免收到消息时显示通知）
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<ChatService>().setCurrentChatScreen(widget.peerId, true);
    });
    _loadMessages();
  }

  Future<void> _loadMessages() async {
    final chatService = context.read<ChatService>();
    
    // 先从本地加载消息（离线可用）
    await chatService.loadLocalMessages(widget.peerId, isGroup: widget.isGroup);
    
    // 然后从服务器加载最新消息
    chatService.loadHistory(widget.peerId, isGroup: widget.isGroup);
    
    // 滚动到底部
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _scrollToBottom();
    });
  }

  @override
  void dispose() {
    // 清除当前聊天界面状态
    context.read<ChatService>().setCurrentChatScreen(0, false);
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
          if (widget.isGroup)
            IconButton(
              icon: const Icon(Icons.group),
              onPressed: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                    builder: (context) => GroupManagementScreen(
                      groupId: widget.peerId,
                      groupName: widget.peerName,
                    ),
                  ),
                );
              },
            ),
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
          // 上传进度指示器
          if (_isUploading)
            LinearProgressIndicator(
              backgroundColor: Theme.of(context).colorScheme.surfaceContainerHighest,
              valueColor: AlwaysStoppedAnimation<Color>(
                Theme.of(context).colorScheme.primary,
              ),
            ),
          
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
                    GestureDetector(
                      onTap: () => _showImagePreview(message.mediaUrl),
                      child: ClipRRect(
                        borderRadius: BorderRadius.circular(8),
                        child: _buildImageWidget(message.mediaUrl),
                      ),
                    )
                  else if (message.isFile)
                    GestureDetector(
                      onTap: () => _downloadFile(message),
                      child: Container(
                        padding: const EdgeInsets.all(12),
                        decoration: BoxDecoration(
                          color: Colors.white.withOpacity(0.5),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(
                              _getFileIcon(message.content),
                              size: 40,
                              color: Theme.of(context).colorScheme.primary,
                            ),
                            const SizedBox(width: 12),
                            Flexible(
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    message.content,
                                    style: const TextStyle(
                                      fontWeight: FontWeight.w500,
                                    ),
                                    maxLines: 2,
                                    overflow: TextOverflow.ellipsis,
                                  ),
                                  const SizedBox(height: 4),
                                  Text(
                                    '点击下载',
                                    style: TextStyle(
                                      fontSize: 12,
                                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                                    ),
                                  ),
                                ],
                              ),
                            ),
                            const SizedBox(width: 8),
                            Icon(
                              Icons.download,
                              size: 20,
                              color: Theme.of(context).colorScheme.primary,
                            ),
                          ],
                        ),
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
  
  /// 构建图片组件
  Widget _buildImageWidget(String url) {
    if (url.isEmpty) {
      return const Icon(Icons.broken_image, size: 100);
    }
    
    // 检查是否是网络URL
    if (url.startsWith('http://') || url.startsWith('https://')) {
      return CachedNetworkImage(
        imageUrl: url,
        width: 200,
        fit: BoxFit.cover,
        placeholder: (context, url) => Container(
          width: 200,
          height: 150,
          color: Colors.grey[200],
          child: const Center(
            child: CircularProgressIndicator(),
          ),
        ),
        errorWidget: (context, url, error) => Container(
          width: 200,
          height: 150,
          color: Colors.grey[200],
          child: const Icon(Icons.broken_image, size: 50),
        ),
      );
    } else {
      // 本地文件
      return Image.file(
        File(url),
        width: 200,
        fit: BoxFit.cover,
        errorBuilder: (_, __, ___) => const Icon(Icons.broken_image, size: 100),
      );
    }
  }
  
  /// 显示图片预览
  void _showImagePreview(String url) {
    if (url.isEmpty) return;
    
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (context) => _ImagePreviewScreen(url: url),
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
                  _pickImage(ImageSource.gallery);
                },
              ),
              _buildAttachmentOption(
                icon: Icons.camera_alt,
                label: '拍照',
                onTap: () {
                  Navigator.pop(context);
                  _pickImage(ImageSource.camera);
                },
              ),
              _buildAttachmentOption(
                icon: Icons.videocam,
                label: '视频',
                onTap: () {
                  Navigator.pop(context);
                  // 选择视频 - 待实现
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('视频功能即将上线')),
                  );
                },
              ),
              _buildAttachmentOption(
                icon: Icons.insert_drive_file,
                label: '文件',
                onTap: () {
                  Navigator.pop(context);
                  _pickFile();
                },
              ),
            ],
          ),
        ),
      ),
    );
  }
  
  /// 选择图片并发送
  Future<void> _pickImage(ImageSource source) async {
    try {
      final XFile? image = await _imagePicker.pickImage(
        source: source,
        maxWidth: 1920,
        maxHeight: 1920,
        imageQuality: 85,
      );
      
      if (image == null) return;
      
      final chatService = context.read<ChatService>();
      
      // 显示上传进度
      setState(() {
        _isUploading = true;
      });
      
      // 发送图片消息
      final success = await chatService.sendImageMessage(
        widget.peerId,
        File(image.path),
        isGroup: widget.isGroup,
      );
      
      setState(() {
        _isUploading = false;
      });
      
      if (!success) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(chatService.uploadError ?? '发送图片失败'),
              backgroundColor: Colors.red,
            ),
          );
        }
      } else {
        _scrollToBottom();
      }
    } catch (e) {
      setState(() {
        _isUploading = false;
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('选择图片失败: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }
  
  /// 选择文件并发送
  Future<void> _pickFile() async {
    try {
      final result = await FilePicker.platform.pickFiles(
        type: FileType.any,
        allowCompression: false,
      );
      
      if (result == null || result.files.isEmpty) return;
      
      final file = result.files.first;
      if (file.path == null) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('无法获取文件路径'),
              backgroundColor: Colors.red,
            ),
          );
        }
        return;
      }
      
      // 检查文件大小 (限制10MB)
      final fileObj = File(file.path!);
      final fileSize = await fileObj.length();
      if (fileSize > 10 * 1024 * 1024) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('文件大小超出限制 (最大 10MB)，当前: ${ChatService.formatFileSize(fileSize)}'),
              backgroundColor: Colors.red,
            ),
          );
        }
        return;
      }
      
      final chatService = context.read<ChatService>();
      
      // 显示上传进度
      setState(() {
        _isUploading = true;
      });
      
      // 发送文件消息
      final success = await chatService.sendFileMessage(
        widget.peerId,
        fileObj,
        file.name,
        isGroup: widget.isGroup,
      );
      
      setState(() {
        _isUploading = false;
      });
      
      if (!success) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(chatService.uploadError ?? '发送文件失败'),
              backgroundColor: Colors.red,
            ),
          );
        }
      } else {
        _scrollToBottom();
      }
    } catch (e) {
      setState(() {
        _isUploading = false;
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('选择文件失败: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }
  
  /// 下载文件
  Future<void> _downloadFile(Message message) async {
    if (message.mediaUrl.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('文件链接无效')),
      );
      return;
    }
    
    try {
      // 显示下载中提示
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('正在下载文件...')),
      );
      
      // 获取下载目录
      final downloadDir = await getExternalStorageDirectory() ?? 
                          await getApplicationDocumentsDirectory();
      
      // 从 URL 提取文件名或使用消息内容
      String fileName = message.content;
      if (fileName.isEmpty) {
        fileName = path.basename(Uri.parse(message.mediaUrl).path);
      }
      
      final localPath = path.join(downloadDir.path, 'downloads', fileName);
      
      // 确保下载目录存在
      final downloadFolder = Directory(path.dirname(localPath));
      if (!await downloadFolder.exists()) {
        await downloadFolder.create(recursive: true);
      }
      
      // 使用 http 下载文件
      final uri = Uri.parse(message.mediaUrl);
      final request = await HttpClient().getUrl(uri);
      final response = await request.close();
      
      if (response.statusCode == 200) {
        final file = File(localPath);
        await file.writeAsBytes(await response.toList().then((list) => list.expand((x) => x).toList()));
        
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('文件已保存到: $localPath'),
              action: SnackBarAction(
                label: '打开',
                onPressed: () => _openFile(localPath),
              ),
            ),
          );
        }
      } else {
        throw Exception('HTTP ${response.statusCode}');
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('下载失败: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }
  
  /// 打开文件
  Future<void> _openFile(String filePath) async {
    try {
      final uri = Uri.file(filePath);
      if (await canLaunchUrl(uri)) {
        await launchUrl(uri);
      } else {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('无法打开此文件类型')),
          );
        }
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('打开文件失败: $e')),
        );
      }
    }
  }
  
  /// 根据文件名获取图标
  IconData _getFileIcon(String fileName) {
    final extension = path.extension(fileName).toLowerCase();
    switch (extension) {
      case '.pdf':
        return Icons.picture_as_pdf;
      case '.doc':
      case '.docx':
        return Icons.description;
      case '.xls':
      case '.xlsx':
        return Icons.table_chart;
      case '.ppt':
      case '.pptx':
        return Icons.slideshow;
      case '.zip':
      case '.rar':
      case '.7z':
        return Icons.folder_zip;
      case '.mp3':
      case '.wav':
      case '.flac':
        return Icons.audio_file;
      case '.mp4':
      case '.avi':
      case '.mkv':
        return Icons.video_file;
      case '.txt':
        return Icons.article;
      case '.json':
      case '.xml':
      case '.html':
      case '.css':
      case '.js':
      case '.dart':
      case '.py':
      case '.java':
      case '.cpp':
      case '.h':
        return Icons.code;
      default:
        return Icons.insert_drive_file;
    }
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

/// 图片预览屏幕
class _ImagePreviewScreen extends StatelessWidget {
  final String url;
  
  const _ImagePreviewScreen({required this.url});
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        foregroundColor: Colors.white,
      ),
      body: Center(
        child: InteractiveViewer(
          minScale: 0.5,
          maxScale: 4.0,
          child: url.startsWith('http://') || url.startsWith('https://')
              ? CachedNetworkImage(
                  imageUrl: url,
                  fit: BoxFit.contain,
                  placeholder: (context, url) => const CircularProgressIndicator(),
                  errorWidget: (context, url, error) => const Icon(
                    Icons.broken_image,
                    color: Colors.white,
                    size: 100,
                  ),
                )
              : Image.file(
                  File(url),
                  fit: BoxFit.contain,
                  errorBuilder: (_, __, ___) => const Icon(
                    Icons.broken_image,
                    color: Colors.white,
                    size: 100,
                  ),
                ),
        ),
      ),
    );
  }
}
