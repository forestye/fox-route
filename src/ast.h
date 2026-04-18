#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include "error_reporter.h"

struct PathParameterNode {
    std::string type;        // "int", "string", "uint", "double", "*" (defaults to "string")
    std::string name;        // variable name
    SourceLocation location;

    PathParameterNode(const std::string& t, const std::string& n, const SourceLocation& loc)
        : type(t), name(n), location(loc) {}
};

struct HandlerCallNode {
    std::string return_type;       // "string", "json", or "" (void/no return)
    std::string function_name;
    std::vector<std::string> arguments;
    SourceLocation location;

    HandlerCallNode(const std::string& name, const std::vector<std::string>& args, const SourceLocation& loc)
        : return_type(""), function_name(name), arguments(args), location(loc) {}

    HandlerCallNode(const std::string& ret_type, const std::string& name, const std::vector<std::string>& args, const SourceLocation& loc)
        : return_type(ret_type), function_name(name), arguments(args), location(loc) {}
};

using PathSegment = std::variant<std::string, PathParameterNode>;

struct RouteRuleNode {
    std::string http_method;
    std::vector<PathSegment> path_segments;
    HandlerCallNode handler_call;
    SourceLocation location;

    RouteRuleNode(const std::string& method, const std::vector<PathSegment>& segments,
                  const HandlerCallNode& handler, const SourceLocation& loc)
        : http_method(method), path_segments(segments), handler_call(handler), location(loc) {}
};

struct FilesystemRouteNode {
    std::string url_prefix;        // e.g., "/images"
    std::string filesystem_path;   // e.g., "/var/www/images"
    SourceLocation location;

    FilesystemRouteNode(const std::string& prefix, const std::string& fs_path, const SourceLocation& loc)
        : url_prefix(prefix), filesystem_path(fs_path), location(loc) {}
};

struct AstRoot {
    std::vector<RouteRuleNode> rules;
    std::vector<FilesystemRouteNode> filesystem_routes;

    AstRoot() = default;
    explicit AstRoot(const std::vector<RouteRuleNode>& r) : rules(r) {}
};

// Utility functions for AST manipulation
namespace ast_utils {
    std::string path_segment_to_string(const PathSegment& segment);
    std::string path_segments_to_string(const std::vector<PathSegment>& segments);
    bool is_path_parameter(const PathSegment& segment);
    const PathParameterNode& get_path_parameter(const PathSegment& segment);
}