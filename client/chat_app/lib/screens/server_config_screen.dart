import 'package:flutter/material.dart';
import '../services/storage_service.dart';

class ServerConfigScreen extends StatefulWidget {
  const ServerConfigScreen({super.key});

  @override
  State<ServerConfigScreen> createState() => _ServerConfigScreenState();
}

class _ServerConfigScreenState extends State<ServerConfigScreen> {
  final _formKey = GlobalKey<FormState>();
  final _hostController = TextEditingController();
  final _portController = TextEditingController();
  
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    final storage = StorageService();
    _hostController.text = storage.serverHost;
    _portController.text = storage.serverPort.toString();
  }

  @override
  void dispose() {
    _hostController.dispose();
    _portController.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    if (!_formKey.currentState!.validate()) return;
    
    setState(() {
      _isLoading = true;
    });
    
    try {
      await StorageService().saveServerConfig(
        _hostController.text,
        int.parse(_portController.text),
      );
      
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('服务器设置已保存')),
      );
      Navigator.of(context).pop();
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
        title: const Text('服务器设置'),
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: Form(
            key: _formKey,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Info card
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Row(
                      children: [
                        Icon(Icons.info_outline, 
                          color: Theme.of(context).colorScheme.primary),
                        const SizedBox(width: 12),
                        const Expanded(
                          child: Text(
                            '配置聊天服务器的连接信息',
                            style: TextStyle(fontSize: 14),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                
                const SizedBox(height: 24),
                
                // Host
                TextFormField(
                  controller: _hostController,
                  decoration: const InputDecoration(
                    labelText: '服务器地址',
                    prefixIcon: Icon(Icons.dns_outlined),
                    helperText: 'IP地址或域名',
                  ),
                  keyboardType: TextInputType.url,
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return '请输入服务器地址';
                    }
                    return null;
                  },
                ),
                
                const SizedBox(height: 16),
                
                // Port
                TextFormField(
                  controller: _portController,
                  decoration: const InputDecoration(
                    labelText: '端口',
                    prefixIcon: Icon(Icons.settings_ethernet),
                    helperText: '默认 8888',
                  ),
                  keyboardType: TextInputType.number,
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return '请输入端口';
                    }
                    final port = int.tryParse(value);
                    if (port == null || port < 1 || port > 65535) {
                      return '请输入有效的端口号 (1-65535)';
                    }
                    return null;
                  },
                ),
                
                const SizedBox(height: 32),
                
                // Save button
                FilledButton(
                  onPressed: _isLoading ? null : _save,
                  child: _isLoading
                    ? const SizedBox(
                        width: 24,
                        height: 24,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Text('保存'),
                ),
                
                const SizedBox(height: 16),
                
                // Reset button
                OutlinedButton(
                  onPressed: () {
                    _hostController.text = '10.0.2.2';
                    _portController.text = '8888';
                  },
                  child: const Text('恢复默认'),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
