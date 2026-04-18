#include "error_reporter.h"

void ErrorReporter::report_error(ErrorType type, const SourceLocation& loc,
                                const std::string& message, const std::string& source_line) {
    errors_.push_back({type, loc, message, source_line});
}

void ErrorReporter::print_errors() const {
    for (const auto& error : errors_) {
        std::cerr << error.location.filename << ":" << error.location.line << ":"
                  << error.location.column << ": " << error_type_to_string(error.type)
                  << ": " << error.message << std::endl;

        if (!error.source_line.empty()) {
            std::cerr << error.source_line << std::endl;

            if (error.location.column > 0) {
                std::string pointer(error.location.column - 1, ' ');
                pointer += "^";
                std::cerr << pointer << std::endl;
            }
        }
        std::cerr << std::endl;
    }
}

std::string ErrorReporter::error_type_to_string(ErrorType type) const {
    switch (type) {
        case ErrorType::LEXICAL_ERROR: return "error";
        case ErrorType::SYNTAX_ERROR: return "error";
        case ErrorType::SEMANTIC_ERROR: return "error";
        default: return "error";
    }
}