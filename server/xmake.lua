-- xmake.lua for chat_app server
-- 更快的增量编译

set_project("chat_server")
set_version("1.0.0")
set_languages("c++17")

add_rules("mode.debug", "mode.release")

-- Termux 路径
local TERMUX_PREFIX = "/data/data/com.termux/files/usr"

-- 添加系统库路径
add_includedirs(TERMUX_PREFIX .. "/include")
add_linkdirs(TERMUX_PREFIX .. "/lib")

-- 通用依赖
add_packages("nlohmann_json", "openssl", "mariadb", "libcurl", "libmicrohttpd")

-- 通用编译选项
add_syslinks("pthread", "dl", "m")
add_defines("ASIO_STANDALONE", "ASIO_CPP14")

-- 主服务器
target("chat_server")
    set_kind("binary")
    add_files("src/main.cpp", "src/server.cpp", "src/session.cpp", 
              "src/database.cpp", "src/database_pool.cpp",
              "src/user_manager.cpp", "src/message_manager.cpp",
              "src/group_manager.cpp", "src/friend_manager.cpp",
              "src/deepseek_client.cpp", "src/bot_manager.cpp",
              "src/js_bot.cpp", "src/fcm_manager.cpp", "src/jpush_manager.cpp",
              "src/thread_pool_v2.cpp", "src/http_gateway.cpp")
    add_includedirs("include")
    add_links("mariadb", "curl", "microhttpd", "ssl", "crypto", "qjs")

-- 媒体服务器
target("media_server")
    set_kind("binary")
    add_files("src/media_server.cpp")
    add_links("microhttpd")

-- 网关服务器 (WebSocket + HTTP)
target("gateway_server")
    set_kind("binary")
    add_files("src/gateway_main.cpp", "src/websocket_server.cpp", 
              "src/http_gateway.cpp", "src/database.cpp", "src/database_pool.cpp",
              "src/user_manager.cpp", "src/message_manager.cpp",
              "src/group_manager.cpp", "src/friend_manager.cpp",
              "src/deepseek_client.cpp", "src/bot_manager.cpp",
              "src/js_bot.cpp", "src/fcm_manager.cpp", "src/jpush_manager.cpp",
              "src/thread_pool_v2.cpp")
    add_includedirs("include")
    add_links("mariadb", "curl", "microhttpd", "ssl", "crypto", "qjs")
