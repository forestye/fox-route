#include <iostream>
#include <cassert>
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/semantic_analyzer.h"
#include "../src/error_reporter.h"

void test_valid_routes() {
    std::cout << "Testing valid routes..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users/{int:id} -> show_user(resp, id)";

    Lexer lexer("test.crdl", source, reporter);
    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();
    assert(!reporter.has_errors());

    SemanticAnalyzer analyzer(reporter);
    auto tree = analyzer.analyze(*ast);

    assert(!reporter.has_errors());
    assert(tree != nullptr);

    std::cout << "✓ Valid routes test passed" << std::endl;
}

void test_undefined_parameter_error() {
    std::cout << "Testing undefined parameter error..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users/{int:id} -> show_user(resp, user_id)"; // user_id is not defined

    Lexer lexer("test.crdl", source, reporter);
    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();
    assert(!reporter.has_errors());

    SemanticAnalyzer analyzer(reporter);
    auto tree = analyzer.analyze(*ast);

    // Should have semantic error for undefined parameter
    assert(reporter.has_errors());

    std::cout << "✓ Undefined parameter error test passed" << std::endl;
}

void test_framework_objects() {
    std::cout << "Testing framework objects..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users -> get_users(req, resp)"; // req and resp are framework objects

    Lexer lexer("test.crdl", source, reporter);
    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();
    assert(!reporter.has_errors());

    SemanticAnalyzer analyzer(reporter);
    auto tree = analyzer.analyze(*ast);

    // Should not have errors - req and resp are valid framework objects
    assert(!reporter.has_errors());

    std::cout << "✓ Framework objects test passed" << std::endl;
}

int main() {
    std::cout << "Running Semantic Analyzer tests..." << std::endl;

    test_valid_routes();
    test_undefined_parameter_error();
    test_framework_objects();

    std::cout << "All Semantic Analyzer tests passed!" << std::endl;
    return 0;
}