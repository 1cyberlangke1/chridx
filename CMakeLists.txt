cmake_minimum_required(VERSION 3.26)
project(chridx VERSION 0.1.0 LANGUAGES CXX)

# C++17：REQUIRED 让不支持的编译器直接配置失败，
# EXTENSIONS OFF 强制 -std=c++17（而不是 gnu++17）。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 给 clangd / IDE 用的 build/compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 头文件库：目前是纯模板（实现也在头里），所以只有 include 路径、没有 .cpp。
# 以后若要放非模板实现，把下面两行换成：
#   add_library(chridx STATIC src/char_index.cpp)
#   target_include_directories(chridx PUBLIC include)
add_library(chridx INTERFACE)
target_include_directories(chridx INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(CHRIDX_BUILD_TESTS "构建测试" ON)

if(CHRIDX_BUILD_TESTS)
    enable_testing()
    add_executable(test_chridx tests/test_char_index.cpp)
    target_link_libraries(test_chridx PRIVATE chridx)
    target_include_directories(test_chridx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME chridx COMMAND test_chridx)
endif()
