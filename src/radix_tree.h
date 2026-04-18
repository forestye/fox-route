#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "ast.h"

struct RouteInfo {
    std::string http_method;
    HandlerCallNode handler_call;
    std::vector<std::string> parameter_names;   // Names of path parameters in order
    std::vector<std::string> parameter_types;   // Types of path parameters in order

    RouteInfo() : handler_call("", {}, SourceLocation()) {}

    RouteInfo(const std::string& method, const HandlerCallNode& handler,
              const std::vector<std::string>& param_names,
              const std::vector<std::string>& param_types)
        : http_method(method), handler_call(handler),
          parameter_names(param_names), parameter_types(param_types) {}
};

// Structure to store route info along with its reconstructed path pattern
struct RouteWithPath {
    const RouteInfo* route_info;
    std::string path_pattern;  // Original path pattern like "/users/{int:id}"

    RouteWithPath(const RouteInfo* info, const std::string& pattern)
        : route_info(info), path_pattern(pattern) {}
};

class RadixTreeNode {
public:
    std::string path_segment;                                    // The path segment this node represents
    bool is_parameter;                                          // True if this is a parameter node
    std::string parameter_type;                                 // Type if this is a parameter ("int", "string", etc.)
    std::string parameter_name;                                 // Name if this is a parameter

    std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> children;  // Static children
    std::unique_ptr<RadixTreeNode> parameter_child;                            // Dynamic parameter child
    std::unique_ptr<RadixTreeNode> wildcard_child;                            // Wildcard child (matches any remaining path)

    std::unordered_map<std::string, RouteInfo> route_handlers;  // HTTP method -> handler info

    RadixTreeNode(const std::string& segment = "", bool is_param = false,
                  const std::string& param_type = "", const std::string& param_name = "")
        : path_segment(segment), is_parameter(is_param),
          parameter_type(param_type), parameter_name(param_name) {}

    bool is_leaf() const {
        return !route_handlers.empty();
    }

    bool has_handler(const std::string& method) const {
        return route_handlers.count(method) > 0;
    }

    const RouteInfo& get_handler(const std::string& method) const {
        return route_handlers.at(method);
    }

    void add_handler(const std::string& method, const RouteInfo& info) {
        route_handlers[method] = info;
    }
};

class RadixTree {
public:
    RadixTree();
    ~RadixTree();

    // Insert a route into the tree
    void insert(const RouteRuleNode& route);

    // Find a route that matches the given path
    struct MatchResult {
        bool found;
        std::string http_method;
        const RouteInfo* route_info;
        std::unordered_map<std::string, std::string> parameters;  // parameter_name -> value

        MatchResult() : found(false), route_info(nullptr) {}
        MatchResult(const std::string& method, const RouteInfo* info,
                   const std::unordered_map<std::string, std::string>& params)
            : found(true), http_method(method), route_info(info), parameters(params) {}
    };

    MatchResult match(const std::string& method, const std::string& path) const;

    // Check for route conflicts
    bool has_conflict(const RouteRuleNode& route) const;

    // Debug: print tree structure
    void print_tree() const;

    // Collect all route information for code generation
    std::vector<const RouteInfo*> collect_all_routes() const;

    // Collect all routes with their path patterns for code generation
    std::vector<RouteWithPath> collect_routes_with_paths() const;

private:
    std::unique_ptr<RadixTreeNode> root_;

    // Helper methods
    void insert_recursive(RadixTreeNode* node, const std::vector<PathSegment>& segments,
                         size_t segment_index, const RouteRuleNode& route);

    MatchResult match_recursive(RadixTreeNode* node, const std::string& method,
                               const std::vector<std::string>& path_parts, size_t part_index,
                               std::unordered_map<std::string, std::string>& parameters) const;

    bool check_conflict_recursive(RadixTreeNode* node, const std::vector<PathSegment>& segments,
                                 size_t segment_index, const std::string& method) const;

    void print_tree_recursive(RadixTreeNode* node, const std::string& prefix) const;
    void collect_routes_recursive(RadixTreeNode* node, std::vector<const RouteInfo*>& routes) const;
    void collect_routes_with_paths_recursive(RadixTreeNode* node, const std::string& current_path,
                                           std::vector<RouteWithPath>& routes) const;

    std::vector<std::string> split_path(const std::string& path) const;
    std::string normalize_path(const std::string& path) const;
};