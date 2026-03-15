import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:cached_network_image/cached_network_image.dart';
import 'package:photo_view/photo_view.dart';
import 'package:photo_view/photo_view_gallery.dart';
import '../services/chat_service.dart';
import '../services/storage_service.dart';
import '../models/models.dart';

/// 相册页面 - 显示所有图片消息
class GalleryScreen extends StatefulWidget {
  const GalleryScreen({super.key});

  @override
  State<GalleryScreen> createState() => _GalleryScreenState();
}

class _GalleryScreenState extends State<GalleryScreen> {
  List<Message> _images = [];
  bool _isLoading = true;
  bool _hasMore = true;
  int _lastTime = 0;
  
  @override
  void initState() {
    super.initState();
    _loadImages();
  }
  
  Future<void> _loadImages({bool loadMore = false}) async {
    if (!mounted) return;
    
    setState(() {
      _isLoading = true;
    });
    
    try {
      final chatService = context.read<ChatService>();
      final images = await chatService.getImageMessages(
        limit: 50,
        beforeTime: loadMore ? _lastTime : 0,
      );
      
      if (!mounted) return;
      
      setState(() {
        if (loadMore) {
          _images.addAll(images);
        } else {
          _images = images;
        }
        if (images.length < 50) {
          _hasMore = false;
        }
        if (images.isNotEmpty) {
          _lastTime = images.last.createdAt;
        }
        _isLoading = false;
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
    await _loadImages();
  }
  
  void _openGallery(int initialIndex) {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (_) => _GalleryViewScreen(
          images: _images,
          initialIndex: initialIndex,
        ),
      ),
    );
  }
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('相册 (${_images.length})'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _refresh,
            tooltip: '刷新',
          ),
        ],
      ),
      body: _buildBody(),
    );
  }
  
  Widget _buildBody() {
    if (_isLoading && _images.isEmpty) {
      return const Center(
        child: CircularProgressIndicator(),
      );
    }
    
    if (_images.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              Icons.photo_library_outlined,
              size: 64,
              color: Theme.of(context).colorScheme.outline,
            ),
            const SizedBox(height: 16),
            Text(
              '暂无图片',
              style: TextStyle(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
                fontSize: 16,
              ),
            ),
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
          _loadImages(loadMore: true);
        }
        return false;
      },
      child: RefreshIndicator(
        onRefresh: _refresh,
        child: GridView.builder(
          padding: const EdgeInsets.all(4),
          gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: 3,
            crossAxisSpacing: 4,
            mainAxisSpacing: 4,
          ),
          itemCount: _images.length + (_hasMore ? 1 : 0),
          itemBuilder: (context, index) {
            if (index == _images.length) {
              return const Center(
                child: Padding(
                  padding: EdgeInsets.all(16),
                  child: CircularProgressIndicator(),
                ),
              );
            }
            
            final image = _images[index];
            return _buildImageItem(image, index);
          },
        ),
      ),
    );
  }
  
  Widget _buildImageItem(Message image, int index) {
    final mediaUrl = _fixMediaUrl(image.mediaUrl);
    
    return GestureDetector(
      onTap: () => _openGallery(index),
      child: Hero(
        tag: 'gallery_image_${image.messageId}',
        child: ClipRRect(
          borderRadius: BorderRadius.circular(4),
          child: Stack(
            fit: StackFit.expand,
            children: [
              CachedNetworkImage(
                imageUrl: mediaUrl,
                fit: BoxFit.cover,
                placeholder: (context, url) => Container(
                  color: Theme.of(context).colorScheme.surfaceContainerHighest,
                  child: const Center(
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
                ),
                errorWidget: (context, url, error) => Container(
                  color: Theme.of(context).colorScheme.surfaceContainerHighest,
                  child: Icon(
                    Icons.broken_image_outlined,
                    color: Theme.of(context).colorScheme.outline,
                  ),
                ),
              ),
              Positioned(
                bottom: 0,
                left: 0,
                right: 0,
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 2),
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: [
                        Colors.transparent,
                        Colors.black.withOpacity(0.6),
                      ],
                    ),
                  ),
                  child: Text(
                    _getSourceText(image),
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 10,
                    ),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
  
  String _fixMediaUrl(String url) {
    return StorageService().fixMediaUrl(url);
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

/// 图片查看页面
class _GalleryViewScreen extends StatefulWidget {
  final List<Message> images;
  final int initialIndex;
  
  const _GalleryViewScreen({
    required this.images,
    required this.initialIndex,
  });
  
  @override
  State<_GalleryViewScreen> createState() => _GalleryViewScreenState();
}

class _GalleryViewScreenState extends State<_GalleryViewScreen> {
  late PageController _pageController;
  late int _currentIndex;
  
  @override
  void initState() {
    super.initState();
    _currentIndex = widget.initialIndex;
    _pageController = PageController(initialPage: widget.initialIndex);
  }
  
  @override
  void dispose() {
    _pageController.dispose();
    super.dispose();
  }
  
  String _fixMediaUrl(String url) {
    return StorageService().fixMediaUrl(url);
  }
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        foregroundColor: Colors.white,
        title: Text(
          '${_currentIndex + 1} / ${widget.images.length}',
          style: const TextStyle(color: Colors.white),
        ),
      ),
      body: PhotoViewGallery.builder(
        scrollPhysics: const BouncingScrollPhysics(),
        builder: (context, index) {
          final image = widget.images[index];
          return PhotoViewGalleryPageOptions(
            imageProvider: CachedNetworkImageProvider(_fixMediaUrl(image.mediaUrl)),
            initialScale: PhotoViewComputedScale.contained,
            minScale: PhotoViewComputedScale.contained,
            maxScale: PhotoViewComputedScale.covered * 3,
            heroAttributes: PhotoViewHeroAttributes(
              tag: 'gallery_image_${image.messageId}',
            ),
          );
        },
        itemCount: widget.images.length,
        loadingBuilder: (context, event) => Center(
          child: SizedBox(
            width: 20,
            height: 20,
            child: CircularProgressIndicator(
              value: event == null
                  ? null
                  : event.cumulativeBytesLoaded / (event.expectedTotalBytes ?? 1),
              strokeWidth: 2,
              color: Colors.white,
            ),
          ),
        ),
        pageController: _pageController,
        onPageChanged: (index) {
          setState(() {
            _currentIndex = index;
          });
        },
      ),
    );
  }
}
