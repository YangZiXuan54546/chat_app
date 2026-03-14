cmake_minimum_required(VERSION 3.16)
project(chat_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)
pkg_check_modules(CURL REQUIRED libcurl)

# 使用系统安装的 nlohmann-json 和 asio
find_package(nlohmann_json 3.2.0 REQUIRED)
find_path(ASIO_INCLUDE_DIR NAMES asio.hpp PATHS /usr/include)

# QuickJS 库
set(QUICKJS_INCLUDE_DIR /usr/local/include/quickjs)
set(QUICKJS_LIBRARY /usr/local/lib/quickjs/libquickjs.a)

# 服务器可执行文件
add_executable(chat_server
    src/main.cpp
    src/server.cpp
    src/session.cpp
    src/database.cpp
    src/database_pool.cpp
    src/user_manager.cpp
    src/message_manager.cpp
    src/group_manager.cpp
    src/friend_manager.cpp
    src/deepseek_client.cpp
    src/bot_manager.cpp
    src/js_bot.cpp
    src/fcm_manager.cpp
    src/jpush_manager.cpp
    src/thread_pool_v2.cpp
)

target_include_directories(chat_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ASIO_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${QUICKJS_INCLUDE_DIR}
)

target_link_libraries(chat_server PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MYSQL_LIBRARIES}
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${QUICKJS_LIBRARY}
    dl
    m
)

target_compile_definitions(chat_server PRIVATE
    ASIO_STANDALONE
    ASIO_CPP14
)

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chat_server PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
    )
endif()

# 媒体服务器可执行文件
pkg_check_modules(MHD REQUIRED libmicrohttpd)

add_executable(media_server
    src/media_server.cpp
)

target_include_directories(media_server PRIVATE
    ${MHD_INCLUDE_DIRS}
)

target_link_libraries(media_server PRIVATE
    ${MHD_LIBRARIES}
)

# 安装
install(TARGETS chat_server media_server DESTINATION bin)

# 打包
set(CPACK_PACKAGE_NAME "chat-server")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "DEB;RPM;TGZ")
include(CPack)
