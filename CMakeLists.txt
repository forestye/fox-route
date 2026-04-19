cmake_minimum_required(VERSION 3.10)

project(fox-route VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

# 源文件列表
set(SOURCES
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
    src/radix_tree.cpp
    src/semantic_analyzer.cpp
    src/code_generator.cpp
    src/error_reporter.cpp
)

# 头文件列表
set(HEADERS
    src/lexer.h
    src/parser.h
    src/ast.h
    src/radix_tree.h
    src/semantic_analyzer.h
    src/code_generator.h
    src/error_reporter.h
)

# 主要可执行文件
add_executable(fox-route ${SOURCES} ${HEADERS})

# fox-route-func: 辅助工具，从 .crdl 中导出某个 handler 的函数签名
set(CRDL_FUNC_SOURCES
    src/crdl_func.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
    src/radix_tree.cpp
    src/code_generator.cpp
    src/error_reporter.cpp
)

add_executable(fox-route-func ${CRDL_FUNC_SOURCES} ${HEADERS})

# 安装配置
install(TARGETS fox-route fox-route-func
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# 安装示例文件（可选）
install(FILES example.crdl
    DESTINATION share/fox-route/examples
    COMPONENT examples
)

# 安装文档文件（可选）
install(FILES README.md spec.md
    DESTINATION share/doc/fox-route
    COMPONENT documentation
)

# 测试
enable_testing()
add_subdirectory(tests)