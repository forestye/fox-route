#include <iostream>
#include <cassert>
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/error_reporter.h"

void test_simple_route_parsing() {
    std::cout << "Testing simple route parsing..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users -> get_users()";
    Lexer lexer("test.crdl", source, reporter);

    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();

    assert(!reporter.has_errors());
    assert(ast != nullptr);
    assert(ast->rules.size() == 1);

    const auto& rule = ast->rules[0];
    assert(rule.http_method == "GET");
    assert(rule.path_segments.size() == 1);
    assert(std::get<std::string>(rule.path_segments[0]) == "/users");
    assert(rule.handler_call.function_name == "get_users");
    assert(rule.handler_call.arguments.empty());

    std::cout << "✓ Simple route parsing test passed" << std::endl;
}

void test_parameterized_route_parsing() {
    std::cout << "Testing parameterized route parsing..." << std::endl;

    ErrorReporter reporter;
    std::string source = "GET /users/{int:id} -> show_user(resp, id)";
    Lexer lexer("test.crdl", source, reporter);

    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();

    assert(!reporter.has_errors());
    assert(ast != nullptr);
    assert(ast->rules.size() == 1);

    const auto& rule = ast->rules[0];
    assert(rule.http_method == "GET");
    assert(rule.path_segments.size() == 2);

    // Check path segments
    assert(std::get<std::string>(rule.path_segments[0]) == "/users/");

    const auto& param = std::get<PathParameterNode>(rule.path_segments[1]);
    assert(param.type == "int");
    assert(param.name == "id");

    // Check handler call
    assert(rule.handler_call.function_name == "show_user");
    assert(rule.handler_call.arguments.size() == 2);
    assert(rule.handler_call.arguments[0] == "resp");
    assert(rule.handler_call.arguments[1] == "id");

    std::cout << "✓ Parameterized route parsing test passed" << std::endl;
}

void test_multiple_routes_parsing() {
    std::cout << "Testing multiple routes parsing..." << std::endl;

    ErrorReporter reporter;
    std::string source =
        "GET /users -> get_users()\n"
        "POST /users -> create_user(req)\n"
        "GET /users/{int:id} -> show_user(resp, id)\n";

    Lexer lexer("test.crdl", source, reporter);

    auto tokens = lexer.tokenize();
    assert(!reporter.has_errors());

    Parser parser(std::move(tokens), reporter);
    auto ast = parser.parse();

    assert(!reporter.has_errors());
    assert(ast != nullptr);
    assert(ast->rules.size() == 3);

    // Check first route
    assert(ast->rules[0].http_method == "GET");
    assert(ast->rules[0].handler_call.function_name == "get_users");

    // Check second route
    assert(ast->rules[1].http_method == "POST");
    assert(ast->rules[1].handler_call.function_name == "create_user");

    // Check third route
    assert(ast->rules[2].http_method == "GET");
    assert(ast->rules[2].handler_call.function_name == "show_user");

    std::cout << "✓ Multiple routes parsing test passed" << std::endl;
}

int main() {
    std::cout << "Running Parser tests..." << std::endl;

    test_simple_route_parsing();
    test_parameterized_route_parsing();
    test_multiple_routes_parsing();

    std::cout << "All Parser tests passed!" << std::endl;
    return 0;
}