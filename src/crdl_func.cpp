#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "lexer.h"
#include "parser.h"
#include "code_generator.h"
#include "error_reporter.h"

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }
    return content;
}

std::string extract_function_name(const std::string& input) {
    std::string func_name = input;

    // If it has a file extension, remove it
    size_t dot_pos = func_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        func_name = func_name.substr(0, dot_pos);
    }

    return func_name;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <route_file.crdl> <function_name_or_file>\n";
        std::cerr << "Examples:\n";
        std::cerr << "  " << argv[0] << " test.crdl show_user\n";
        std::cerr << "  " << argv[0] << " test.crdl show_user.html\n";
        return 1;
    }

    std::string route_file = argv[1];
    std::string function_input = argv[2];
    std::string target_function = extract_function_name(function_input);

    try {
        // Read and parse the CRDL file
        std::string content = read_file(route_file);
        ErrorReporter error_reporter;

        // Lexical analysis
        Lexer lexer(route_file, content, error_reporter);
        auto tokens = lexer.tokenize();

        if (error_reporter.has_errors()) {
            error_reporter.print_errors();
            return 1;
        }

        // Syntax analysis
        Parser parser(std::move(tokens), error_reporter);
        auto ast = parser.parse();

        if (error_reporter.has_errors()) {
            error_reporter.print_errors();
            return 1;
        }

        // Find the target function in the routes
        for (const auto& rule : ast->rules) {
            if (rule.handler_call.function_name == target_function) {
                // Create a temporary RouteInfo to generate signature
                std::vector<std::string> param_names;
                std::vector<std::string> param_types;

                for (const auto& segment : rule.path_segments) {
                    if (ast_utils::is_path_parameter(segment)) {
                        const auto& param = ast_utils::get_path_parameter(segment);
                        param_names.push_back(param.name);
                        param_types.push_back(param.type);
                    }
                }

                RouteInfo route_info(rule.http_method, rule.handler_call, param_names, param_types);

                // Generate function signature
                CodeGenerator generator(error_reporter);
                std::string signature = generator.get_function_signature(route_info);

                // Output without newlines
                std::cout << signature;
                return 0;
            }
        }

        // Function not found
        std::cerr << "Function '" << target_function << "' not found in " << route_file << std::endl;
        return 1;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}