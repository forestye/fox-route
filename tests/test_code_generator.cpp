#include <iostream>
#include <cassert>
#include <filesystem>
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/semantic_analyzer.h"
#include "../src/code_generator.h"
#include "../src/error_reporter.h"

void test_code_generation() {
    std::cout << "Testing code generation..." << std::endl;

    ErrorReporter reporter;
    std::string source =
        "GET /users -> get_users()\n"
        "GET /users/{int:id} -> show_user(resp, id)\n";

    Lexer lexer("test.crdl", source, reporter);
    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();
    assert(!reporter.has_errors());

    SemanticAnalyzer analyzer(reporter);
    auto tree = analyzer.analyze(*ast);
    assert(!reporter.has_errors());

    // Create temporary output directory
    std::string output_dir = "/tmp/crdl_test_output";
    std::filesystem::create_directories(output_dir);

    CodeGenerator generator(reporter);
    bool success = generator.generate(*ast, *tree, output_dir);

    assert(success);
    assert(!reporter.has_errors());

    // Check that files were created
    assert(std::filesystem::exists(output_dir + "/handlers.h"));
    assert(std::filesystem::exists(output_dir + "/router.generated.h"));
    assert(std::filesystem::exists(output_dir + "/router.generated.cpp"));

    // Clean up
    std::filesystem::remove_all(output_dir);

    std::cout << "✓ Code generation test passed" << std::endl;
}

int main() {
    std::cout << "Running Code Generator tests..." << std::endl;

    test_code_generation();

    std::cout << "All Code Generator tests passed!" << std::endl;
    return 0;
}