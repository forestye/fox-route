#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>
#include "radix_tree.h"
#include "error_reporter.h"

class CodeGenerator {
public:
    explicit CodeGenerator(ErrorReporter& reporter);

    // Generate all output files
    bool generate(const AstRoot& ast, const RadixTree& tree, const std::string& output_dir);

    // Public utility method for external tools
    std::string get_function_signature(const RouteInfo& route_info);

private:
    ErrorReporter& error_reporter_;

    // Generation methods
    bool generate_handlers_h(const RadixTree& tree, const std::string& output_dir);
    bool generate_handlers_cpp(const RadixTree& tree, const std::string& output_dir);
    bool generate_router_h(const AstRoot& ast, const std::string& output_dir);
    bool generate_router_cpp(const AstRoot& ast, const RadixTree& tree, const std::string& output_dir);

    // Helper methods
    std::string get_cpp_type(const std::string& crdl_type);
    std::string generate_function_signature(const RouteInfo& route_info);
    std::string generate_route_regex(const std::string& path_pattern);
    std::string generate_radix_tree_construction(const RadixTree& tree);
    std::string generate_dispatch_method(const RadixTree& tree);
    std::string generate_parameter_conversion(const std::string& param_name, const std::string& param_type);
    std::string generate_handler_call(const RouteInfo& route_info);

    void collect_all_handlers(const RadixTreeNode* node, std::unordered_map<std::string, const RouteInfo*>& handlers);

    bool write_file(const std::string& filepath, const std::string& content);
    std::string escape_string_literal(const std::string& str);
    std::string escape_regex_char(char c);
};