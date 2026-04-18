#pragma once

#include <string>
#include <vector>
#include <iostream>

struct SourceLocation {
    std::string filename;
    size_t line;
    size_t column;

    SourceLocation(const std::string& file = "", size_t ln = 0, size_t col = 0)
        : filename(file), line(ln), column(col) {}
};

class ErrorReporter {
public:
    enum class ErrorType {
        LEXICAL_ERROR,
        SYNTAX_ERROR,
        SEMANTIC_ERROR
    };

    struct Error {
        ErrorType type;
        SourceLocation location;
        std::string message;
        std::string source_line;
    };

    void report_error(ErrorType type, const SourceLocation& loc,
                     const std::string& message, const std::string& source_line = "");

    bool has_errors() const { return !errors_.empty(); }
    const std::vector<Error>& get_errors() const { return errors_; }
    void clear() { errors_.clear(); }

    void print_errors() const;

private:
    std::vector<Error> errors_;
    std::string error_type_to_string(ErrorType type) const;
};