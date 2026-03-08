cmake_minimum_required(VERSION 3.16)
project(chat_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(mysqlclient REQUIRED)

# nlohmann_json - 使用 FetchContent
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# asio - 使用 FetchContent
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

# 服务器可执行文件
add_executable(chat_server
    src/main.cpp
    src/server.cpp
    src/session.cpp
    src/database.cpp
    src/user_manager.cpp
    src/message_manager.cpp
    src/group_manager.cpp
    src/friend_manager.cpp
)

target_include_directories(chat_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/asio/include
)

target_link_libraries(chat_server PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    mysqlclient
    nlohmann_json::nlohmann_json
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

# 安装
install(TARGETS chat_server DESTINATION bin)

# 打包
set(CPACK_PACKAGE_NAME "chat-server")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "DEB;RPM;TGZ")
include(CPack)
