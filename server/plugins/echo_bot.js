/**
 * JavaScript 机器人插件示例
 * 
 * 这个插件展示了如何使用 JavaScript 编写机器人功能
 */

// 插件元数据
var PLUGIN_META = {
    name: "EchoBot",
    version: "1.0.0",
    description: "一个简单的回声机器人示例",
    author: "Chat App Team"
};

/**
 * 消息处理函数
 * 当机器人收到消息时会调用此函数
 * 
 * @param {Object} message - 消息对象
 *   - messageId: 消息ID
 *   - senderId: 发送者ID
 *   - senderName: 发送者名称
 *   - receiverId: 接收者ID
 *   - content: 消息内容
 *   - mediaType: 媒体类型
 *   - mediaUrl: 媒体URL
 *   - createdAt: 创建时间戳
 *   - isGroup: 是否群消息
 *   - groupId: 群ID (如果是群消息)
 *   - mentionedUsers: 被@的用户ID数组 (群消息)
 * 
 * @returns {Object} 响应对象
 *   - content: 回复内容
 *   - shouldReply: 是否回复 (默认 true)
 */
function onMessage(message) {
    Bot.log("info", "收到消息: " + message.content);
    
    // 检查是否是 @我的消息
    var botId = message.receiverId;
    var isMentioned = false;
    
    if (message.isGroup && message.mentionedUsers) {
        for (var i = 0; i < message.mentionedUsers.length; i++) {
            if (message.mentionedUsers[i] == botId) {
                isMentioned = true;
                break;
            }
        }
    }
    
    // 处理不同类型的消息
    var content = message.content.trim();
    var response = {
        shouldReply: true
    };
    
    // 命令处理
    if (content.startsWith("/")) {
        var parts = content.substring(1).split(" ");
        var cmd = parts[0].toLowerCase();
        
        switch (cmd) {
            case "help":
                response.content = "可用命令:\n" +
                    "/help - 显示帮助\n" +
                    "/time - 显示当前时间\n" +
                    "/echo <text> - 回显文本\n" +
                    "/info - 显示用户信息";
                break;
                
            case "time":
                var now = new Date();
                response.content = "当前时间: " + now.toLocaleString("zh-CN");
                break;
                
            case "echo":
                if (parts.length > 1) {
                    response.content = parts.slice(1).join(" ");
                } else {
                    response.content = "用法: /echo <text>";
                }
                break;
                
            case "info":
                response.content = "发送者: " + message.senderName + 
                    "\nID: " + message.senderId +
                    "\n时间: " + new Date(message.createdAt * 1000).toLocaleString("zh-CN");
                break;
                
            default:
                response.content = "未知命令: " + cmd + "\n输入 /help 查看可用命令";
        }
    } else if (!message.isGroup || isMentioned) {
        // 私聊消息或群消息中@我的消息
        response.content = "你好 " + message.senderName + "！我是 JavaScript 机器人。\n" +
            "输入 /help 查看可用命令。";
    } else {
        // 群消息但未@我，不回复
        response.shouldReply = false;
    }
    
    return response;
}
