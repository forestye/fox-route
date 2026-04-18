#pragma once

#include <memory>
#include <unordered_set>
#include "ast.h"
#include "radix_tree.h"
#include "error_reporter.h"

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(ErrorReporter& reporter);

    // Analyze AST and build verified Radix Tree
    std::unique_ptr<RadixTree> analyze(const AstRoot& ast);

private:
    ErrorReporter& error_reporter_;
    static const std::unordered_set<std::string> FRAMEWORK_OBJECTS;

    // Validation methods
    bool validate_route_rule(const RouteRuleNode& rule, RadixTree& tree);
    bool validate_handler_parameters(const RouteRuleNode& rule);
    bool validate_parameter_types(const RouteRuleNode& rule);
    bool validate_return_type(const RouteRuleNode& rule);
    bool check_route_conflicts(const RouteRuleNode& rule, RadixTree& tree);

    // Helper methods
    std::unordered_set<std::string> extract_path_parameters(const std::vector<PathSegment>& segments);
    std::string get_cpp_type(const std::string& crdl_type);
    bool is_framework_object(const std::string& param_name);
    bool is_valid_parameter_type(const std::string& type);

    void report_semantic_error(const SourceLocation& location, const std::string& message);
};