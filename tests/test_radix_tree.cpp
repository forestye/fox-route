#include <iostream>
#include <cassert>
#include "../src/radix_tree.h"
#include "../src/ast.h"

void test_simple_insertion_and_lookup() {
    std::cout << "Testing simple insertion and lookup..." << std::endl;

    RadixTree tree;

    // Create a simple route: GET /users -> get_users()
    // Note: Path segments should not include leading slash for individual segments
    std::vector<PathSegment> segments = {std::string("users")};
    HandlerCallNode handler("get_users", {}, SourceLocation());
    RouteRuleNode route("GET", segments, handler, SourceLocation());

    tree.insert(route);

    // Debug: print tree structure
    tree.print_tree();

    // Test matching
    auto result = tree.match("GET", "/users");
    std::cout << "Match result for GET /users: found=" << result.found << std::endl;
    if (result.found && result.route_info) {
        std::cout << "Handler: " << result.route_info->handler_call.function_name << std::endl;
    }

    assert(result.found);
    assert(result.route_info != nullptr);
    assert(result.route_info->handler_call.function_name == "get_users");

    // Test non-matching
    auto result2 = tree.match("POST", "/users");
    assert(!result2.found);

    auto result3 = tree.match("GET", "/posts");
    assert(!result3.found);

    std::cout << "✓ Simple insertion and lookup test passed" << std::endl;
}

void test_parameterized_routes() {
    std::cout << "Testing parameterized routes..." << std::endl;

    RadixTree tree;

    // Create route: GET /users/{int:id} -> show_user(resp, id)
    std::vector<PathSegment> segments = {
        std::string("users"),
        PathParameterNode("int", "id", SourceLocation())
    };
    HandlerCallNode handler("show_user", {"resp", "id"}, SourceLocation());
    RouteRuleNode route("GET", segments, handler, SourceLocation());

    tree.insert(route);

    // Test matching with parameter
    auto result = tree.match("GET", "/users/123");
    assert(result.found);
    assert(result.route_info != nullptr);
    assert(result.route_info->handler_call.function_name == "show_user");
    assert(result.parameters.count("id") > 0);
    assert(result.parameters.at("id") == "123");

    std::cout << "✓ Parameterized routes test passed" << std::endl;
}

void test_route_conflicts() {
    std::cout << "Testing route conflicts..." << std::endl;

    RadixTree tree;

    // Insert first route
    std::vector<PathSegment> segments1 = {
        std::string("users"),
        PathParameterNode("int", "id", SourceLocation())
    };
    HandlerCallNode handler1("show_user", {"id"}, SourceLocation());
    RouteRuleNode route1("GET", segments1, handler1, SourceLocation());

    tree.insert(route1);

    // Try to insert conflicting route
    std::vector<PathSegment> segments2 = {
        std::string("users"),
        PathParameterNode("string", "slug", SourceLocation())
    };
    HandlerCallNode handler2("show_user_by_slug", {"slug"}, SourceLocation());
    RouteRuleNode route2("GET", segments2, handler2, SourceLocation());

    // This should detect a conflict since both routes have the same pattern
    bool has_conflict = tree.has_conflict(route2);
    assert(has_conflict);

    std::cout << "✓ Route conflicts test passed" << std::endl;
}

void test_wildcard_routes() {
    std::cout << "Testing wildcard routes..." << std::endl;

    RadixTree tree;

    // Create route: GET /files/{*} -> serve_file(path)
    std::vector<PathSegment> segments = {
        std::string("files"),
        PathParameterNode("*", "path", SourceLocation())
    };
    HandlerCallNode handler("serve_file", {"path"}, SourceLocation());
    RouteRuleNode route("GET", segments, handler, SourceLocation());

    tree.insert(route);

    // Test matching with wildcard
    auto result = tree.match("GET", "/files/images/logo.png");
    assert(result.found);
    assert(result.route_info != nullptr);
    assert(result.route_info->handler_call.function_name == "serve_file");
    assert(result.parameters.count("path") > 0);
    assert(result.parameters.at("path") == "images/logo.png");

    std::cout << "✓ Wildcard routes test passed" << std::endl;
}

int main() {
    std::cout << "Running Radix Tree tests..." << std::endl;

    test_simple_insertion_and_lookup();
    test_parameterized_routes();
    test_route_conflicts();
    test_wildcard_routes();

    std::cout << "All Radix Tree tests passed!" << std::endl;
    return 0;
}