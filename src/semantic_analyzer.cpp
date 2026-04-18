#include "semantic_analyzer.h"
#include <unordered_map>

const std::unordered_set<std::string> SemanticAnalyzer::FRAMEWORK_OBJECTS = {
    "req", "resp", "form", "body", "json", "query", "multipart"
};

SemanticAnalyzer::SemanticAnalyzer(ErrorReporter& reporter) : error_reporter_(reporter) {}

std::unique_ptr<RadixTree> SemanticAnalyzer::analyze(const AstRoot& ast) {
    auto tree = std::make_unique<RadixTree>();

    // Validate regular routes
    for (const auto& rule : ast.rules) {
        if (validate_route_rule(rule, *tree)) {
            tree->insert(rule);
        }
    }

    // Validate filesystem routes (basic validation only)
    for (const auto& fs_route : ast.filesystem_routes) {
        // Check that URL prefix starts with /
        if (fs_route.url_prefix.empty() || fs_route.url_prefix[0] != '/') {
            report_semantic_error(fs_route.location,
                "Filesystem route URL prefix must start with '/' (got: " + fs_route.url_prefix + ")");
        }

        // Check that filesystem path is not empty (can be absolute or relative)
        if (fs_route.filesystem_path.empty()) {
            report_semantic_error(fs_route.location,
                "Filesystem path cannot be empty");
        }
    }

    return tree;
}

bool SemanticAnalyzer::validate_route_rule(const RouteRuleNode& rule, RadixTree& tree) {
    bool valid = true;

    // Check for route conflicts
    if (!check_route_conflicts(rule, tree)) {
        valid = false;
    }

    // Validate handler parameters
    if (!validate_handler_parameters(rule)) {
        valid = false;
    }

    // Validate parameter types
    if (!validate_parameter_types(rule)) {
        valid = false;
    }

    // Validate return type
    if (!validate_return_type(rule)) {
        valid = false;
    }

    return valid;
}

bool SemanticAnalyzer::validate_handler_parameters(const RouteRuleNode& rule) {
    bool valid = true;

    // Extract path parameters
    auto path_params = extract_path_parameters(rule.path_segments);

    // Check each handler argument
    for (const auto& arg : rule.handler_call.arguments) {
        if (is_framework_object(arg)) {
            // Framework objects (req, resp) are always valid
            continue;
        }

        // Non-framework arguments must be defined as path parameters
        if (path_params.count(arg) == 0) {
            report_semantic_error(rule.handler_call.location,
                                "Undefined path variable '" + arg + "' in handler parameters");
            valid = false;
        }
    }

    return valid;
}

bool SemanticAnalyzer::validate_parameter_types(const RouteRuleNode& rule) {
    bool valid = true;

    for (const auto& segment : rule.path_segments) {
        if (ast_utils::is_path_parameter(segment)) {
            const auto& param = ast_utils::get_path_parameter(segment);

            if (!is_valid_parameter_type(param.type)) {
                report_semantic_error(param.location,
                                    "Invalid parameter type '" + param.type +
                                    "'. Valid types: string, int, uint, double, *");
                valid = false;
            }

            // Check for empty parameter names
            if (param.name.empty() || param.name == "*") {
                if (param.type != "*") {  // Wildcard is allowed to have name "*"
                    report_semantic_error(param.location,
                                        "Parameter name cannot be empty");
                    valid = false;
                }
            }
        }
    }

    return valid;
}

bool SemanticAnalyzer::check_route_conflicts(const RouteRuleNode& rule, RadixTree& tree) {
    if (tree.has_conflict(rule)) {
        std::string path_str = ast_utils::path_segments_to_string(rule.path_segments);
        report_semantic_error(rule.location,
                            "Route conflict: " + rule.http_method + " " + path_str +
                            " conflicts with an existing route");
        return false;
    }
    return true;
}

std::unordered_set<std::string> SemanticAnalyzer::extract_path_parameters(const std::vector<PathSegment>& segments) {
    std::unordered_set<std::string> params;

    for (const auto& segment : segments) {
        if (ast_utils::is_path_parameter(segment)) {
            const auto& param = ast_utils::get_path_parameter(segment);
            params.insert(param.name);
        }
    }

    return params;
}

std::string SemanticAnalyzer::get_cpp_type(const std::string& crdl_type) {
    static const std::unordered_map<std::string, std::string> type_map = {
        {"string", "std::string"},
        {"int", "int64_t"},
        {"uint", "uint64_t"},
        {"double", "double"},
        {"*", "std::string"}
    };

    auto it = type_map.find(crdl_type);
    if (it != type_map.end()) {
        return it->second;
    }
    return "std::string";  // Default fallback
}

bool SemanticAnalyzer::is_framework_object(const std::string& param_name) {
    return FRAMEWORK_OBJECTS.count(param_name) > 0;
}

bool SemanticAnalyzer::is_valid_parameter_type(const std::string& type) {
    static const std::unordered_set<std::string> valid_types = {
        "string", "int", "uint", "double", "*"
    };
    return valid_types.count(type) > 0;
}

bool SemanticAnalyzer::validate_return_type(const RouteRuleNode& rule) {
    const std::string& return_type = rule.handler_call.return_type;

    // Empty return_type means void, which is always valid
    if (return_type.empty()) {
        return true;
    }

    // Check if return type is one of the supported types
    static const std::unordered_set<std::string> valid_return_types = {
        "text", "html", "json"
    };

    if (valid_return_types.count(return_type) == 0) {
        report_semantic_error(rule.handler_call.location,
                            "Invalid return type '" + return_type +
                            "'. Valid return types: text, html, json");
        return false;
    }

    return true;
}

void SemanticAnalyzer::report_semantic_error(const SourceLocation& location, const std::string& message) {
    error_reporter_.report_error(ErrorReporter::ErrorType::SEMANTIC_ERROR, location, message);
}