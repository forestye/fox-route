#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "code_generator.h"
#include "error_reporter.h"

struct CommandLineArgs {
    std::string input_file;
    std::string output_dir = ".";
    bool check_only = false;
    bool show_help = false;
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input.crdl>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <dir>    Specify C++ code output directory (default: current directory)\n";
    std::cout << "  --check              Only check CRDL file syntax and semantics, don't generate code\n";
    std::cout << "  -h, --help           Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " routes.crdl\n";
    std::cout << "  " << program_name << " -o ./generated routes.crdl\n";
    std::cout << "  " << program_name << " --check routes.crdl\n";
}

CommandLineArgs parse_arguments(int argc, char* argv[]) {
    CommandLineArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
        } else if (arg == "--check") {
            args.check_only = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a directory argument\n";
                args.show_help = true;
                return args;
            }
            args.output_dir = argv[++i];
        } else if (arg.front() == '-') {
            std::cerr << "Error: Unknown option " << arg << "\n";
            args.show_help = true;
            return args;
        } else {
            if (args.input_file.empty()) {
                args.input_file = arg;
            } else {
                std::cerr << "Error: Multiple input files not supported\n";
                args.show_help = true;
                return args;
            }
        }
    }

    if (args.input_file.empty() && !args.show_help) {
        std::cerr << "Error: No input file specified\n";
        args.show_help = true;
    }

    return args;
}

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

int main(int argc, char* argv[]) {
    CommandLineArgs args = parse_arguments(argc, argv);

    if (args.show_help) {
        print_usage(argv[0]);
        return args.input_file.empty() ? 1 : 0;
    }

    try {
        // Read input file
        std::string source = read_file(args.input_file);
        ErrorReporter error_reporter;

        // Lexical analysis
        std::cout << "Lexical analysis..." << std::endl;
        Lexer lexer(args.input_file, source, error_reporter);
        auto tokens = lexer.tokenize();

        if (error_reporter.has_errors()) {
            error_reporter.print_errors();
            return 1;
        }

        std::cout << "Found " << tokens.size() << " tokens" << std::endl;

        // Syntax analysis
        std::cout << "Syntax analysis..." << std::endl;
        Parser parser(std::move(tokens), error_reporter);
        auto ast = parser.parse();

        if (error_reporter.has_errors()) {
            error_reporter.print_errors();
            return 1;
        }

        std::cout << "Parsed " << ast->rules.size() << " route rules" << std::endl;

        // Semantic analysis
        std::cout << "Semantic analysis..." << std::endl;
        SemanticAnalyzer analyzer(error_reporter);
        auto radix_tree = analyzer.analyze(*ast);

        if (error_reporter.has_errors()) {
            error_reporter.print_errors();
            return 1;
        }

        std::cout << "Semantic analysis completed successfully" << std::endl;


        // If check-only mode, stop here
        if (args.check_only) {
            std::cout << "✓ CRDL file is valid" << std::endl;
            return 0;
        }

        // Code generation
        std::cout << "Generating C++ code..." << std::endl;
        CodeGenerator generator(error_reporter);
        bool success = generator.generate(*ast, *radix_tree, args.output_dir);

        if (!success || error_reporter.has_errors()) {
            error_reporter.print_errors();
            return 1;
        }

        std::cout << "✓ Code generation completed successfully" << std::endl;
        std::cout << "Generated files in: " << std::filesystem::absolute(args.output_dir).string() << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}