import 'dart:io';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';
import 'package:path_provider/path_provider.dart';
import 'package:http/http.dart' as http;
import '../services/chat_service.dart';
import '../models/models.dart';

/// 文件类型枚举
enum FileType {
  all,
  document,
  archive,
  apk,
  audio,
  video,
  other,
}

/// 文件管理页面 - 显示所有文件消息
class FileManagerScreen extends StatefulWidget {
  const FileManagerScreen({super.key});

  @override
  State<FileManagerScreen> createState() => _FileManagerScreenState();
}

class _FileManagerScreenState extends State<FileManagerScreen> {
  List<Message> _files = [];
  List<Message> _filteredFiles = [];
  bool _isLoading = true;
  bool _hasMore = true;
  int _lastTime = 0;
  FileType _selectedType = FileType.all;
  
  // 下载状态
  final Map<String, double> _downloadProgress = {};
  final Map<String, String> _downloadedFiles = {};
  
  @override
  void initState() {
    super.initState();
    _loadFiles();
  }
  
  Future<void> _loadFiles({bool loadMore = false}) async {
    if (!mounted) return;
    
    setState(() {
      _isLoading = true;
    });
    
    try {
      final chatService = context.read<ChatService>();
      final files = await chatService.getFileMessages(
        limit: 50,
        beforeTime: loadMore ? _lastTime : 0,
      );
      
      if (!mounted) return;
      
      setState(() {
        if (loadMore) {
          _files.addAll(files);
        } else {
          _files = files;
        }
        if (files.length < 50) {
          _hasMore = false;
        }
        if (files.isNotEmpty) {
          _lastTime = files.last.createdAt;
        }
        _isLoading = false;
        _applyFilter();
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _isLoading = false;
      });
    }
  }
  
  Future<void> _refresh() async {
    _lastTime = 0;
    _hasMore = true;
    await _loadFiles();
  }
  
  FileType _getFileType(String fileName) {
    final ext = fileName.split('.').last.toLowerCase();
    
    if (['pdf', 'doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'txt', 'rtf'].contains(ext)) {
      return FileType.document;
    }
    if (['zip', 'rar', '7z', 'tar', 'gz'].contains(ext)) {
      return FileType.archive;
    }
    if (ext == 'apk') {
      return FileType.apk;
    }
    if (['mp3', 'wav', 'flac', 'aac', 'ogg'].contains(ext)) {
      return FileType.audio;
    }
    if (['mp4', 'avi', 'mkv', 'mov', 'wmv'].contains(ext)) {
      return FileType.video;
    }
    return FileType.other;
  }
  
  IconData _getFileIcon(FileType type) {
    switch (type) {
      case FileType.document:
        return Icons.description_outlined;
      case FileType.archive:
        return Icons.folder_zip_outlined;
      case FileType.apk:
        return Icons.android_outlined;
      case FileType.audio:
        return Icons.audiotrack_outlined;
      case FileType.video:
        return Icons.video_file_outlined;
      default:
        return Icons.insert_drive_file_outlined;
    }
  }
  
  Color _getFileColor(FileType type) {
    switch (type) {
      case FileType.document:
        return Colors.blue;
      case FileType.archive:
        return Colors.orange;
      case FileType.apk:
        return Colors.green;
      case FileType.audio:
        return Colors.purple;
      case FileType.video:
        return Colors.red;
      default:
        return Colors.grey;
    }
  }
  
  void _applyFilter() {
    if (_selectedType == FileType.all) {
      _filteredFiles = List.from(_files);
    } else {
      _filteredFiles = _files.where((file) {
        return _getFileType(file.content) == _selectedType;
      }).toList();
    }
  }
  
  String _formatFileSize(int? bytes) {
    if (bytes == null || bytes <= 0) return '未知';
    
    const units = ['B', 'KB', 'MB', 'GB'];
    int unitIndex = 0;
    double size = bytes.toDouble();
    
    while (size >= 1024 && unitIndex < units.length - 1) {
      size /= 1024;
      unitIndex++;
    }
    
    return '${size.toStringAsFixed(1)} ${units[unitIndex]}';
  }
  
  String _fixMediaUrl(String url) {
    if (url.contains("localhost")) {
      return url.replaceFirst(RegExp(r"http://localhost:\d+"), "http://10.0.2.2:8889");
    }
    return url;
  }
  
  Future<void> _downloadFile(Message message) async {
    if (_downloadProgress.containsKey(message.mediaUrl)) return;
    
    final url = _fixMediaUrl(message.mediaUrl);
    final fileName = message.content;
    
    setState(() {
      _downloadProgress[message.mediaUrl] = 0;
    });
    
    try {
      final directory = await getExternalStorageDirectory();
      if (directory == null) {
        throw Exception('无法访问存储目录');
      }
      
      final downloadsDir = Directory('${directory.path}/downloads');
      if (!downloadsDir.existsSync()) {
        downloadsDir.createSync(recursive: true);
      }
      
      final savePath = '${downloadsDir.path}/$fileName';
      
      final response = await http.get(Uri.parse(url));
      
      if (response.statusCode == 200) {
        final savedFile = File(savePath);
        await savedFile.writeAsBytes(response.bodyBytes);
        
        setState(() {
          _downloadProgress.remove(message.mediaUrl);
          _downloadedFiles[fileName] = savePath;
        });
        
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('文件已保存'),
              action: SnackBarAction(
                label: '打开',
                onPressed: () => _openFile(savePath),
              ),
            ),
          );
        }
      } else {
        throw Exception('HTTP ${response.statusCode}');
      }
    } catch (e) {
      setState(() {
        _downloadProgress.remove(message.mediaUrl);
      });
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('下载失败: $e')),
        );
      }
    }
  }
  
  Future<void> _openFile(String path) async {
    try {
      final uri = Uri.parse('file://$path');
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
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('文件管理 (${_filteredFiles.length})'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _refresh,
            tooltip: '刷新',
          ),
        ],
      ),
      body: Column(
        children: [
          _buildFilterTabs(),
          Expanded(child: _buildBody()),
        ],
      ),
    );
  }
  
  Widget _buildFilterTabs() {
    return Container(
      height: 48,
      padding: const EdgeInsets.symmetric(horizontal: 8),
      child: ListView(
        scrollDirection: Axis.horizontal,
        children: FileType.values.map((type) {
          final isSelected = _selectedType == type;
          final count = type == FileType.all 
              ? _files.length 
              : _files.where((f) => _getFileType(f.content) == type).length;
          
          return Padding(
            padding: const EdgeInsets.symmetric(horizontal: 4),
            child: FilterChip(
              label: Text('${_getTypeName(type)} ($count)'),
              selected: isSelected,
              onSelected: (_) {
                setState(() {
                  _selectedType = type;
                  _applyFilter();
                });
              },
            ),
          );
        }).toList(),
      ),
    );
  }
  
  String _getTypeName(FileType type) {
    switch (type) {
      case FileType.all:
        return '全部';
      case FileType.document:
        return '文档';
      case FileType.archive:
        return '压缩包';
      case FileType.apk:
        return 'APK';
      case FileType.audio:
        return '音频';
      case FileType.video:
        return '视频';
      case FileType.other:
        return '其他';
    }
  }
  
  Widget _buildBody() {
    if (_isLoading && _files.isEmpty) {
      return const Center(child: CircularProgressIndicator());
    }
    
    if (_filteredFiles.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.folder_off_outlined, size: 64, color: Theme.of(context).colorScheme.outline),
            const SizedBox(height: 16),
            Text('暂无文件', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant)),
          ],
        ),
      );
    }
    
    return NotificationListener<ScrollNotification>(
      onNotification: (notification) {
        if (notification is ScrollEndNotification &&
            notification.metrics.pixels >= notification.metrics.maxScrollExtent - 200 &&
            _hasMore &&
            !_isLoading) {
          _loadFiles(loadMore: true);
        }
        return false;
      },
      child: RefreshIndicator(
        onRefresh: _refresh,
        child: ListView.builder(
          padding: const EdgeInsets.all(8),
          itemCount: _filteredFiles.length + (_hasMore ? 1 : 0),
          itemBuilder: (context, index) {
            if (index == _filteredFiles.length) {
              return const Center(child: Padding(padding: EdgeInsets.all(16), child: CircularProgressIndicator()));
            }
            
            final file = _filteredFiles[index];
            return _buildFileItem(file);
          },
        ),
      ),
    );
  }
  
  Widget _buildFileItem(Message file) {
    final fileName = file.content;
    final fileType = _getFileType(fileName);
    final fileIcon = _getFileIcon(fileType);
    final fileColor = _getFileColor(fileType);
    final isDownloading = _downloadProgress.containsKey(file.mediaUrl);
    final isDownloaded = _downloadedFiles.containsKey(fileName);
    
    return Card(
      margin: const EdgeInsets.symmetric(vertical: 4),
      child: InkWell(
        onTap: isDownloading ? null : () => _downloadFile(file),
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Row(
            children: [
              Container(
                width: 48,
                height: 48,
                decoration: BoxDecoration(
                  color: fileColor.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Icon(fileIcon, color: fileColor, size: 28),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(fileName, style: const TextStyle(fontWeight: FontWeight.w500), maxLines: 1, overflow: TextOverflow.ellipsis),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Text(_formatFileSize(file.mediaSize), style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                        const SizedBox(width: 8),
                        Text('·', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant)),
                        const SizedBox(width: 8),
                        Text(_getSourceText(file), style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                      ],
                    ),
                  ],
                ),
              ),
              if (isDownloading)
                const SizedBox(width: 24, height: 24, child: CircularProgressIndicator(strokeWidth: 2))
              else if (isDownloaded)
                IconButton(
                  icon: const Icon(Icons.open_in_new),
                  onPressed: () => _openFile(_downloadedFiles[fileName]!),
                  tooltip: '打开文件',
                )
              else
                IconButton(
                  icon: const Icon(Icons.download),
                  onPressed: () => _downloadFile(file),
                  tooltip: '下载文件',
                ),
            ],
          ),
        ),
      ),
    );
  }
  
  String _getSourceText(Message message) {
    final time = DateTime.fromMillisecondsSinceEpoch(message.createdAt);
    final timeStr = '${time.month}/${time.day} ${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}';
    
    if (message.groupId > 0) {
      return '群聊 · $timeStr';
    } else {
      return '私聊 · $timeStr';
    }
  }
}
