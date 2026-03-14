import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:cached_network_image/cached_network_image.dart';
import '../services/chat_service.dart';
import '../models/models.dart';

/// 收藏消息列表页面
class FavoritesScreen extends StatefulWidget {
  const FavoritesScreen({super.key});

  @override
  State<FavoritesScreen> createState() => _FavoritesScreenState();
}

class _FavoritesScreenState extends State<FavoritesScreen> {
  @override
  void initState() {
    super.initState();
    // 加载收藏列表
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<ChatService>().loadFavorites();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('我的收藏'),
        centerTitle: true,
      ),
      body: Consumer<ChatService>(
        builder: (context, chatService, child) {
          final favorites = chatService.favorites;
          
          if (favorites.isEmpty) {
            return const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(
                    Icons.star_outline,
                    size: 64,
                    color: Colors.grey,
                  ),
                  SizedBox(height: 16),
                  Text(
                    '暂无收藏消息',
                    style: TextStyle(
                      color: Colors.grey,
                      fontSize: 16,
                    ),
                  ),
                ],
              ),
            );
          }
          
          return ListView.builder(
            itemCount: favorites.length,
            itemBuilder: (context, index) {
              final favorite = favorites[index];
              return _buildFavoriteItem(favorite, chatService);
            },
          );
        },
      ),
    );
  }

  Widget _buildFavoriteItem(Favorite favorite, ChatService chatService) {
    return Dismissible(
      key: Key('${favorite.messageId}_${favorite.messageType}'),
      direction: DismissDirection.endToStart,
      background: Container(
        alignment: Alignment.centerRight,
        padding: const EdgeInsets.only(right: 20),
        color: Colors.red,
        child: const Icon(
          Icons.delete,
          color: Colors.white,
        ),
      ),
      confirmDismiss: (direction) async {
        return await showDialog(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('取消收藏'),
            content: const Text('确定要取消收藏这条消息吗？'),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, false),
                child: const Text('取消'),
              ),
              TextButton(
                onPressed: () => Navigator.pop(context, true),
                child: const Text('确定'),
              ),
            ],
          ),
        );
      },
      onDismissed: (direction) async {
        await chatService.removeFavorite(
          favorite.messageId,
          favorite.messageType,
        );
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('已取消收藏')),
          );
        }
      },
      child: ListTile(
        leading: CircleAvatar(
          backgroundImage: favorite.senderAvatar.isNotEmpty
              ? CachedNetworkImageProvider(favorite.senderAvatar)
              : null,
          child: favorite.senderAvatar.isEmpty
              ? Text(favorite.displayName.isNotEmpty 
                  ? favorite.displayName[0].toUpperCase() 
                  : '?')
              : null,
        ),
        title: Row(
          children: [
            Expanded(
              child: Text(
                favorite.displayName,
                style: const TextStyle(fontWeight: FontWeight.w500),
              ),
            ),
            Text(
              favorite.formattedTime,
              style: TextStyle(
                fontSize: 12,
                color: Colors.grey[600],
              ),
            ),
          ],
        ),
        subtitle: _buildMessageContent(favorite),
        isThreeLine: favorite.isImage,
        onTap: () => _showFavoriteDetail(favorite),
      ),
    );
  }

  Widget _buildMessageContent(Favorite favorite) {
    if (favorite.isImage) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SizedBox(height: 8),
          ClipRRect(
            borderRadius: BorderRadius.circular(8),
            child: CachedNetworkImage(
              imageUrl: _fixMediaUrl(favorite.mediaUrl),
              width: 120,
              height: 120,
              fit: BoxFit.cover,
              placeholder: (context, url) => Container(
                width: 120,
                height: 120,
                color: Colors.grey[200],
                child: const Center(
                  child: CircularProgressIndicator(strokeWidth: 2),
                ),
              ),
              errorWidget: (context, url, error) => Container(
                width: 120,
                height: 120,
                color: Colors.grey[200],
                child: const Icon(Icons.broken_image, color: Colors.grey),
              ),
            ),
          ),
        ],
      );
    } else if (favorite.isFile) {
      return Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.insert_drive_file, color: Colors.grey[600], size: 20),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              favorite.content,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      );
    } else {
      return Text(
        favorite.content,
        maxLines: 2,
        overflow: TextOverflow.ellipsis,
      );
    }
  }
  
  String _fixMediaUrl(String url) {
    if (url.contains("localhost")) {
      return url.replaceFirst(RegExp(r"http://localhost:\d+"), "http://10.0.2.2:8889");
    }
    return url;
  }

  void _showFavoriteDetail(Favorite favorite) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (context) => DraggableScrollableSheet(
        initialChildSize: 0.5,
        maxChildSize: 0.9,
        minChildSize: 0.3,
        expand: false,
        builder: (context, scrollController) => Container(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  CircleAvatar(
                    backgroundImage: favorite.senderAvatar.isNotEmpty
                        ? CachedNetworkImageProvider(favorite.senderAvatar)
                        : null,
                    child: favorite.senderAvatar.isEmpty
                        ? Text(favorite.displayName.isNotEmpty 
                            ? favorite.displayName[0].toUpperCase() 
                            : '?')
                        : null,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          favorite.displayName,
                          style: const TextStyle(
                            fontWeight: FontWeight.w500,
                            fontSize: 16,
                          ),
                        ),
                        Text(
                          favorite.formattedTime,
                          style: TextStyle(
                            fontSize: 12,
                            color: Colors.grey[600],
                          ),
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    icon: const Icon(Icons.delete_outline),
                    onPressed: () async {
                      final confirm = await showDialog(
                        context: context,
                        builder: (context) => AlertDialog(
                          title: const Text('取消收藏'),
                          content: const Text('确定要取消收藏这条消息吗？'),
                          actions: [
                            TextButton(
                              onPressed: () => Navigator.pop(context, false),
                              child: const Text('取消'),
                            ),
                            TextButton(
                              onPressed: () => Navigator.pop(context, true),
                              child: const Text('确定'),
                            ),
                          ],
                        ),
                      );
                      
                      if (confirm == true && mounted) {
                        Navigator.pop(context);
                        await context.read<ChatService>().removeFavorite(
                          favorite.messageId,
                          favorite.messageType,
                        );
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('已取消收藏')),
                        );
                      }
                    },
                  ),
                ],
              ),
              const Divider(height: 24),
              Expanded(
                child: SingleChildScrollView(
                  controller: scrollController,
                  child: _buildDetailContent(favorite),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildDetailContent(Favorite favorite) {
    if (favorite.isImage) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Center(
            child: CachedNetworkImage(
              imageUrl: _fixMediaUrl(favorite.mediaUrl),
              fit: BoxFit.contain,
              placeholder: (context, url) => const Center(
                child: CircularProgressIndicator(),
              ),
              errorWidget: (context, url, error) => Container(
                height: 200,
                color: Colors.grey[200],
                child: const Center(
                  child: Icon(Icons.broken_image, size: 48, color: Colors.grey),
                ),
              ),
            ),
          ),
        ],
      );
    } else if (favorite.isFile) {
      return Row(
        children: [
          Icon(Icons.insert_drive_file, size: 48, color: Colors.grey[600]),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  favorite.content,
                  style: const TextStyle(fontSize: 16),
                ),
              ],
            ),
          ),
        ],
      );
    } else {
      return Text(
        favorite.content,
        style: const TextStyle(fontSize: 16, height: 1.5),
      );
    }
  }
}
