#include "radix_tree.h"
#include <iostream>
#include <sstream>
#include <algorithm>

RadixTree::RadixTree() : root_(std::make_unique<RadixTreeNode>()) {}

RadixTree::~RadixTree() = default;

void RadixTree::insert(const RouteRuleNode& route) {
    insert_recursive(root_.get(), route.path_segments, 0, route);
}

void RadixTree::insert_recursive(RadixTreeNode* node, const std::vector<PathSegment>& segments,
                                size_t segment_index, const RouteRuleNode& route) {
    // If we've processed all segments, add the handler to this node
    if (segment_index >= segments.size()) {
        // Collect parameter information
        std::vector<std::string> param_names;
        std::vector<std::string> param_types;

        for (const auto& segment : segments) {
            if (ast_utils::is_path_parameter(segment)) {
                const auto& param = ast_utils::get_path_parameter(segment);
                param_names.push_back(param.name);
                param_types.push_back(param.type);
            }
        }

        RouteInfo route_info(route.http_method, route.handler_call, param_names, param_types);
        node->add_handler(route.http_method, route_info);
        return;
    }

    const PathSegment& current_segment = segments[segment_index];

    if (ast_utils::is_path_parameter(current_segment)) {
        const auto& param = ast_utils::get_path_parameter(current_segment);

        // Handle wildcard parameters
        if (param.type == "*") {
            if (!node->wildcard_child) {
                node->wildcard_child = std::make_unique<RadixTreeNode>(
                    "*", true, param.type, param.name
                );
            }
            insert_recursive(node->wildcard_child.get(), segments, segment_index + 1, route);
        } else {
            // Handle regular parameters
            if (!node->parameter_child) {
                node->parameter_child = std::make_unique<RadixTreeNode>(
                    "{" + param.type + ":" + param.name + "}", true, param.type, param.name
                );
            }
            insert_recursive(node->parameter_child.get(), segments, segment_index + 1, route);
        }
    } else {
        // Handle static segments
        const std::string& static_segment = std::get<std::string>(current_segment);

        if (node->children.count(static_segment) == 0) {
            node->children[static_segment] = std::make_unique<RadixTreeNode>(static_segment);
        }

        insert_recursive(node->children[static_segment].get(), segments, segment_index + 1, route);
    }
}

RadixTree::MatchResult RadixTree::match(const std::string& method, const std::string& path) const {
    std::string normalized = normalize_path(path);
    std::vector<std::string> path_parts = split_path(normalized);
    std::unordered_map<std::string, std::string> parameters;

    return match_recursive(root_.get(), method, path_parts, 0, parameters);
}

RadixTree::MatchResult RadixTree::match_recursive(RadixTreeNode* node, const std::string& method,
                                                 const std::vector<std::string>& path_parts, size_t part_index,
                                                 std::unordered_map<std::string, std::string>& parameters) const {
    // If we've consumed all path parts
    if (part_index >= path_parts.size()) {
        if (node->has_handler(method)) {
            return MatchResult(method, &node->get_handler(method), parameters);
        }
        return MatchResult();
    }

    const std::string& current_part = path_parts[part_index];

    // Try static children first
    auto static_it = node->children.find(current_part);
    if (static_it != node->children.end()) {
        auto result = match_recursive(static_it->second.get(), method, path_parts, part_index + 1, parameters);
        if (result.found) {
            return result;
        }
    }

    // Try parameter child
    if (node->parameter_child) {
        std::string old_value;
        bool had_old_value = false;

        if (parameters.count(node->parameter_child->parameter_name)) {
            old_value = parameters[node->parameter_child->parameter_name];
            had_old_value = true;
        }

        parameters[node->parameter_child->parameter_name] = current_part;

        auto result = match_recursive(node->parameter_child.get(), method, path_parts, part_index + 1, parameters);
        if (result.found) {
            return result;
        }

        // Restore old value or remove
        if (had_old_value) {
            parameters[node->parameter_child->parameter_name] = old_value;
        } else {
            parameters.erase(node->parameter_child->parameter_name);
        }
    }

    // Try wildcard child (matches remaining path)
    if (node->wildcard_child) {
        std::string remaining_path;
        for (size_t i = part_index; i < path_parts.size(); ++i) {
            if (!remaining_path.empty()) remaining_path += "/";
            remaining_path += path_parts[i];
        }

        parameters[node->wildcard_child->parameter_name] = remaining_path;

        if (node->wildcard_child->has_handler(method)) {
            return MatchResult(method, &node->wildcard_child->get_handler(method), parameters);
        }
    }

    return MatchResult();
}

bool RadixTree::has_conflict(const RouteRuleNode& route) const {
    return check_conflict_recursive(root_.get(), route.path_segments, 0, route.http_method);
}

bool RadixTree::check_conflict_recursive(RadixTreeNode* node, const std::vector<PathSegment>& segments,
                                       size_t segment_index, const std::string& method) const {
    if (segment_index >= segments.size()) {
        return node->has_handler(method);
    }

    const PathSegment& current_segment = segments[segment_index];

    if (ast_utils::is_path_parameter(current_segment)) {
        const auto& param = ast_utils::get_path_parameter(current_segment);

        if (param.type == "*") {
            if (node->wildcard_child) {
                return check_conflict_recursive(node->wildcard_child.get(), segments, segment_index + 1, method);
            }
        } else {
            if (node->parameter_child) {
                return check_conflict_recursive(node->parameter_child.get(), segments, segment_index + 1, method);
            }
        }
    } else {
        const std::string& static_segment = std::get<std::string>(current_segment);
        auto it = node->children.find(static_segment);
        if (it != node->children.end()) {
            return check_conflict_recursive(it->second.get(), segments, segment_index + 1, method);
        }
    }

    return false;
}

void RadixTree::print_tree() const {
    std::cout << "Radix Tree Structure:" << std::endl;
    print_tree_recursive(root_.get(), "");
}

void RadixTree::print_tree_recursive(RadixTreeNode* node, const std::string& prefix) const {
    if (!node) return;

    std::cout << prefix << "Node: '" << node->path_segment << "'";
    if (node->is_parameter) {
        std::cout << " [PARAM: " << node->parameter_type << ":" << node->parameter_name << "]";
    }
    std::cout << std::endl;

    if (!node->route_handlers.empty()) {
        std::cout << prefix << "  Handlers: ";
        for (const auto& handler : node->route_handlers) {
            std::cout << handler.first << " ";
        }
        std::cout << std::endl;
    }

    for (const auto& child : node->children) {
        print_tree_recursive(child.second.get(), prefix + "  ");
    }

    if (node->parameter_child) {
        print_tree_recursive(node->parameter_child.get(), prefix + "  ");
    }

    if (node->wildcard_child) {
        print_tree_recursive(node->wildcard_child.get(), prefix + "  ");
    }
}

std::vector<std::string> RadixTree::split_path(const std::string& path) const {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;

    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    return parts;
}

std::string RadixTree::normalize_path(const std::string& path) const {
    if (path.empty() || path[0] != '/') {
        return "/" + path;
    }

    // Remove trailing slash if not root
    if (path.length() > 1 && path.back() == '/') {
        return path.substr(0, path.length() - 1);
    }

    return path;
}

std::vector<const RouteInfo*> RadixTree::collect_all_routes() const {
    std::vector<const RouteInfo*> routes;
    collect_routes_recursive(root_.get(), routes);
    return routes;
}

void RadixTree::collect_routes_recursive(RadixTreeNode* node, std::vector<const RouteInfo*>& routes) const {
    if (!node) return;

    // Collect all handlers from this node
    for (const auto& handler : node->route_handlers) {
        routes.push_back(&handler.second);
    }

    // Recurse into children
    for (const auto& child : node->children) {
        collect_routes_recursive(child.second.get(), routes);
    }

    if (node->parameter_child) {
        collect_routes_recursive(node->parameter_child.get(), routes);
    }

    if (node->wildcard_child) {
        collect_routes_recursive(node->wildcard_child.get(), routes);
    }
}

std::vector<RouteWithPath> RadixTree::collect_routes_with_paths() const {
    std::vector<RouteWithPath> routes;
    collect_routes_with_paths_recursive(root_.get(), "", routes);
    return routes;
}

void RadixTree::collect_routes_with_paths_recursive(RadixTreeNode* node, const std::string& current_path,
                                                  std::vector<RouteWithPath>& routes) const {
    if (!node) return;

    // Build the current path for this node
    std::string path = current_path;
    if (!node->path_segment.empty()) {
        if (node->is_parameter) {
            // Format as {type:name}
            // Check if path already ends with slash to avoid double slashes
            if (!path.empty() && path.back() != '/') {
                path += "/";
            }
            path += "{" + node->parameter_type + ":" + node->parameter_name + "}";
        } else {
            // For static segments, path_segment already contains the slash
            // Just append it directly (it's stored as '/segment/' format)
            path += node->path_segment;
        }
    }

    // If this node has handlers, collect them with the current path
    for (const auto& handler : node->route_handlers) {
        std::string final_path = path.empty() ? "/" : path;
        // Remove trailing slash if it's not the root path
        if (final_path.length() > 1 && final_path.back() == '/') {
            final_path = final_path.substr(0, final_path.length() - 1);
        }
        routes.emplace_back(&handler.second, final_path);
    }

    // Recurse into children
    for (const auto& child : node->children) {
        collect_routes_with_paths_recursive(child.second.get(), path, routes);
    }

    if (node->parameter_child) {
        collect_routes_with_paths_recursive(node->parameter_child.get(), path, routes);
    }

    if (node->wildcard_child) {
        collect_routes_with_paths_recursive(node->wildcard_child.get(), path, routes);
    }
}