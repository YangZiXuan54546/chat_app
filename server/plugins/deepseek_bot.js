/**
 * DeepSeek AI 机器人插件
 * 使用 JavaScript 实现 DeepSeek API 调用
 */

var PLUGIN_META = {
    name: "DeepSeekBot",
    version: "1.0.0",
    description: "基于 DeepSeek API 的 AI 机器人",
    author: "Chat App Team"
};

// 会话存储
var sessions = {};
var MAX_CONTEXT = 10;
var MAX_CHAR_COUNT = 10000;

/**
 * 获取或创建会话
 */
function getSession(userId) {
    if (!sessions[userId]) {
        sessions[userId] = {
            messages: [],
            charCount: 0,
            createdAt: Date.now()
        };
    }
    return sessions[userId];
}

/**
 * 调用 DeepSeek API
 */
function callDeepSeek(messages, apiKey) {
    var requestBody = {
        model: "deepseek-chat",
        messages: messages,
        max_tokens: 1000,
        temperature: 0.7
    };
    
    var response = Bot.httpPost(
        "https://api.deepseek.com/v1/chat/completions",
        JSON.stringify(requestBody),
        "application/json"
    );
    
    if (response) {
        try {
            var data = JSON.parse(response);
            if (data.choices && data.choices.length > 0) {
                return data.choices[0].message.content;
            }
        } catch (e) {
            Bot.log("error", "解析 API 响应失败: " + e.message);
        }
    }
    return null;
}

/**
 * 消息处理函数
 */
function onMessage(message) {
    Bot.log("info", "DeepSeek Bot 收到消息: " + message.content);
    
    var content = message.content.trim();
    var userId = message.senderId;
    var response = { shouldReply: true };
    
    // 命令处理
    if (content.startsWith("/")) {
        var parts = content.substring(1).split(" ");
        var cmd = parts[0].toLowerCase();
        
        switch (cmd) {
            case "new":
                // 创建新会话
                sessions[userId] = {
                    messages: [],
                    charCount: 0,
                    createdAt: Date.now()
                };
                response.content = "已创建新会话。开始新的对话吧！";
                return response;
                
            case "sessions":
                // 显示会话信息
                var session = getSession(userId);
                response.content = "当前会话:\n" +
                    "消息数: " + session.messages.length + "\n" +
                    "字符数: " + session.charCount + "/" + MAX_CHAR_COUNT;
                return response;
                
            case "clear":
                sessions[userId] = null;
                response.content = "会话已清除。";
                return response;
        }
    }
    
    // 获取会话
    var session = getSession(userId);
    
    // 检查字数限制
    if (session.charCount >= MAX_CHAR_COUNT) {
        response.content = "当前会话已达到字数限制（" + MAX_CHAR_COUNT + " 字）。\n" +
            "请发送 /new 开始新会话。";
        return response;
    }
    
    // 添加用户消息到上下文
    session.messages.push({
        role: "user",
        content: content
    });
    session.charCount += content.length;
    
    // 保持上下文在限制内
    while (session.messages.length > MAX_CONTEXT) {
        session.messages.shift();
    }
    
    // 构建请求消息
    var apiMessages = [
        {
            role: "system",
            content: "你是一个友好的聊天助手。请用简洁、自然的方式回复用户的消息。回复时请使用中文。"
        }
    ];
    
    for (var i = 0; i < session.messages.length; i++) {
        apiMessages.push(session.messages[i]);
    }
    
    // 调用 DeepSeek API
    var apiKey = "sk-86c5f0d4e9b245a7b22248b2a4bded44";
    var aiResponse = callDeepSeek(apiMessages, apiKey);
    
    if (aiResponse) {
        // 添加助手回复到上下文
        session.messages.push({
            role: "assistant",
            content: aiResponse
        });
        session.charCount += aiResponse.length;
        
        response.content = aiResponse;
        
        // 检查是否接近限制
        if (session.charCount > MAX_CHAR_COUNT * 0.9) {
            response.content += "\n\n⚠️ 提示: 当前会话字数已接近限制。发送 /new 开始新会话。";
        }
    } else {
        response.content = "抱歉，AI 服务暂时不可用，请稍后再试。";
    }
    
    return response;
}
