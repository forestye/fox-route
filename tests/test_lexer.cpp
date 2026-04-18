#include <iostream>
#include <cassert>
#include "../src/lexer.h"
#include "../src/error_reporter.h"

void test_simple_tokens() {
    std::cout << "Testing simple tokens..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users -> get_users()";
    Lexer lexer("test.crdl", source, reporter);

    auto tokens = lexer.tokenize();

    assert(!reporter.has_errors());
    assert(tokens.size() >= 6); // METHOD, PATH, ARROW, IDENTIFIER, LPAREN, RPAREN, EOF

    assert(tokens[0].type == TokenType::METHOD);
    assert(tokens[0].value == "GET");

    assert(tokens[1].type == TokenType::PATH_LITERAL);
    assert(tokens[1].value == "/users");

    assert(tokens[2].type == TokenType::ARROW);
    assert(tokens[2].value == "->");

    assert(tokens[3].type == TokenType::IDENTIFIER);
    assert(tokens[3].value == "get_users");

    std::cout << "✓ Simple tokens test passed" << std::endl;
}

void test_path_parameters() {
    std::cout << "Testing path parameters..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users/{int:id}/posts/{string:slug} -> get_post(id, slug)";
    Lexer lexer("test.crdl", source, reporter);

    auto tokens = lexer.tokenize();

    assert(!reporter.has_errors());

    // Find path parameter tokens
    bool found_int_param = false;
    bool found_string_param = false;

    for (const auto& token : tokens) {
        if (token.type == TokenType::PATH_PARAMETER) {
            if (token.value == "{int:id}") {
                found_int_param = true;
            } else if (token.value == "{string:slug}") {
                found_string_param = true;
            }
        }
    }

    assert(found_int_param);
    assert(found_string_param);

    std::cout << "✓ Path parameters test passed" << std::endl;
}

void test_comments() {
    std::cout << "Testing comments..." << std::endl;

    ErrorReporter reporter;
    std::string source = "# This is a comment\nGET /users -> get_users()\n# Another comment";
    Lexer lexer("test.crdl", source, reporter);

    auto tokens = lexer.tokenize();

    assert(!reporter.has_errors());

    // Comments should be filtered out
    bool found_comment = false;
    for (const auto& token : tokens) {
        if (token.value.find('#') != std::string::npos) {
            found_comment = true;
            break;
        }
    }

    assert(!found_comment);

    std::cout << "✓ Comments test passed" << std::endl;
}

int main() {
    std::cout << "Running Lexer tests..." << std::endl;

    test_simple_tokens();
    test_path_parameters();
    test_comments();

    std::cout << "All Lexer tests passed!" << std::endl;
    return 0;
}