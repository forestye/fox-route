# 测试配置
add_executable(test_lexer test_lexer.cpp ../src/lexer.cpp ../src/error_reporter.cpp)
add_test(NAME LexerTest COMMAND test_lexer)

add_executable(test_parser test_parser.cpp ../src/lexer.cpp ../src/parser.cpp ../src/ast.cpp ../src/error_reporter.cpp)
add_test(NAME ParserTest COMMAND test_parser)

add_executable(test_radix_tree test_radix_tree.cpp ../src/radix_tree.cpp ../src/ast.cpp)
add_test(NAME RadixTreeTest COMMAND test_radix_tree)

add_executable(test_semantic_analyzer test_semantic_analyzer.cpp
    ../src/lexer.cpp
    ../src/parser.cpp
    ../src/ast.cpp
    ../src/radix_tree.cpp
    ../src/semantic_analyzer.cpp
    ../src/error_reporter.cpp)
add_test(NAME SemanticAnalyzerTest COMMAND test_semantic_analyzer)

add_executable(test_code_generator test_code_generator.cpp
    ../src/lexer.cpp
    ../src/parser.cpp
    ../src/ast.cpp
    ../src/radix_tree.cpp
    ../src/semantic_analyzer.cpp
    ../src/code_generator.cpp
    ../src/error_reporter.cpp)
add_test(NAME CodeGeneratorTest COMMAND test_code_generator)