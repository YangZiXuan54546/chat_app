import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/chat_service.dart';

class RegisterScreen extends StatefulWidget {
  const RegisterScreen({super.key});

  @override
  State<RegisterScreen> createState() => _RegisterScreenState();
}

class _RegisterScreenState extends State<RegisterScreen> {
  final _formKey = GlobalKey<FormState>();
  final _usernameController = TextEditingController();
  final _passwordController = TextEditingController();
  final _confirmPasswordController = TextEditingController();
  final _nicknameController = TextEditingController();
  
  bool _isLoading = false;
  bool _obscurePassword = true;
  bool _obscureConfirmPassword = true;
  String? _error;
  bool _success = false;

  @override
  void dispose() {
    _usernameController.dispose();
    _passwordController.dispose();
    _confirmPasswordController.dispose();
    _nicknameController.dispose();
    super.dispose();
  }

  Future<void> _register() async {
    if (!_formKey.currentState!.validate()) return;

    setState(() {
      _isLoading = true;
      _error = null;
      _success = false;
    });

    try {
      final chatService = context.read<ChatService>();
      
      // 确保已连接
      if (!chatService.isConnected) {
        setState(() {
          _error = '未连接到服务器，请先配置服务器地址';
          _isLoading = false;
        });
        return;
      }
      
      final success = await chatService.register(
        _usernameController.text,
        _passwordController.text,
        _nicknameController.text.isEmpty 
          ? _usernameController.text 
          : _nicknameController.text,
      );
      
      if (success) {
        setState(() {
          _success = true;
        });
      } else {
        setState(() {
          _error = chatService.registerError ?? '注册失败';
        });
      }
    } catch (e) {
      setState(() {
        _error = e.toString();
      });
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
        title: const Text('注册'),
      ),
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 400),
              child: _success 
                ? _buildSuccessView() 
                : _buildRegisterForm(),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildSuccessView() {
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Container(
          width: 100,
          height: 100,
          decoration: BoxDecoration(
            color: Theme.of(context).colorScheme.primaryContainer,
            shape: BoxShape.circle,
          ),
          child: Icon(
            Icons.check,
            size: 50,
            color: Theme.of(context).colorScheme.primary,
          ),
        ),
        const SizedBox(height: 24),
        Text(
          '注册成功！',
          style: Theme.of(context).textTheme.headlineMedium?.copyWith(
            fontWeight: FontWeight.bold,
          ),
        ),
        const SizedBox(height: 8),
        Text(
          '您现在可以使用您的账户登录',
          style: Theme.of(context).textTheme.bodyLarge?.copyWith(
            color: Theme.of(context).colorScheme.onSurfaceVariant,
          ),
          textAlign: TextAlign.center,
        ),
        const SizedBox(height: 32),
        FilledButton.icon(
          onPressed: () {
            Navigator.of(context).pop();
          },
          icon: const Icon(Icons.login),
          label: const Text('去登录'),
        ),
      ],
    );
  }

  Widget _buildRegisterForm() {
    return Form(
      key: _formKey,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Title
          Text(
            '创建新账户',
            style: Theme.of(context).textTheme.headlineMedium?.copyWith(
              fontWeight: FontWeight.bold,
            ),
            textAlign: TextAlign.center,
          ),
          
          const SizedBox(height: 8),
          
          Text(
            '填写以下信息注册',
            style: Theme.of(context).textTheme.bodyLarge?.copyWith(
              color: Theme.of(context).colorScheme.onSurfaceVariant,
            ),
            textAlign: TextAlign.center,
          ),
          
          const SizedBox(height: 32),
          
          // Error message
          if (_error != null)
            Container(
              padding: const EdgeInsets.all(12),
              margin: const EdgeInsets.only(bottom: 16),
              decoration: BoxDecoration(
                color: Theme.of(context).colorScheme.errorContainer,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Row(
                children: [
                  Icon(Icons.error_outline, 
                    color: Theme.of(context).colorScheme.error),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      _error!,
                      style: TextStyle(
                        color: Theme.of(context).colorScheme.onErrorContainer,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          
          // Username
          TextFormField(
            controller: _usernameController,
            decoration: const InputDecoration(
              labelText: '用户名 *',
              prefixIcon: Icon(Icons.person_outline),
              helperText: '用户名将用于登录',
            ),
            textInputAction: TextInputAction.next,
            validator: (value) {
              if (value == null || value.isEmpty) {
                return '请输入用户名';
              }
              if (value.length < 3) {
                return '用户名至少3个字符';
              }
              if (value.length > 20) {
                return '用户名最多20个字符';
              }
              return null;
            },
          ),
          
          const SizedBox(height: 16),
          
          // Nickname
          TextFormField(
            controller: _nicknameController,
            decoration: const InputDecoration(
              labelText: '昵称',
              prefixIcon: Icon(Icons.badge_outlined),
              helperText: '可选，默认为用户名',
            ),
            textInputAction: TextInputAction.next,
          ),
          
          const SizedBox(height: 16),
          
          // Password
          TextFormField(
            controller: _passwordController,
            decoration: InputDecoration(
              labelText: '密码 *',
              prefixIcon: const Icon(Icons.lock_outline),
              suffixIcon: IconButton(
                icon: Icon(
                  _obscurePassword ? Icons.visibility : Icons.visibility_off,
                ),
                onPressed: () {
                  setState(() {
                    _obscurePassword = !_obscurePassword;
                  });
                },
              ),
              helperText: '至少6个字符',
            ),
            obscureText: _obscurePassword,
            textInputAction: TextInputAction.next,
            validator: (value) {
              if (value == null || value.isEmpty) {
                return '请输入密码';
              }
              if (value.length < 6) {
                return '密码至少6个字符';
              }
              return null;
            },
          ),
          
          const SizedBox(height: 16),
          
          // Confirm Password
          TextFormField(
            controller: _confirmPasswordController,
            decoration: InputDecoration(
              labelText: '确认密码 *',
              prefixIcon: const Icon(Icons.lock_outline),
              suffixIcon: IconButton(
                icon: Icon(
                  _obscureConfirmPassword ? Icons.visibility : Icons.visibility_off,
                ),
                onPressed: () {
                  setState(() {
                    _obscureConfirmPassword = !_obscureConfirmPassword;
                  });
                },
              ),
            ),
            obscureText: _obscureConfirmPassword,
            textInputAction: TextInputAction.done,
            validator: (value) {
              if (value == null || value.isEmpty) {
                return '请确认密码';
              }
              if (value != _passwordController.text) {
                return '两次输入的密码不一致';
              }
              return null;
            },
          ),
          
          const SizedBox(height: 32),
          
          // Register button
          FilledButton(
            onPressed: _isLoading ? null : _register,
            child: _isLoading
              ? const SizedBox(
                  width: 24,
                  height: 24,
                  child: CircularProgressIndicator(
                    strokeWidth: 2,
                    color: Colors.white,
                  ),
                )
              : const Text('注册', style: TextStyle(fontSize: 16)),
          ),
          
          const SizedBox(height: 16),
          
          // Login link
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(
                '已有账户？',
                style: TextStyle(
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
              ),
              TextButton(
                onPressed: () {
                  Navigator.of(context).pop();
                },
                child: const Text('登录'),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
