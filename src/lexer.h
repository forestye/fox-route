#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include "error_reporter.h"

enum class TokenType {
    METHOD,          // GET, POST, PUT, DELETE, PATCH, OPTIONS, ANY
    FILESYSTEM,      // FILESYSTEM
    RETURN_TYPE,     // string, json
    PATH_LITERAL,    // /users, /posts
    PATH_PARAMETER,  // {int:id}, {string:name}, {*}
    ARROW,           // ->
    IDENTIFIER,      // function names, parameter names
    LEFT_PAREN,      // (
    RIGHT_PAREN,     // )
    COMMA,           // ,
    NEWLINE,         // \n
    EOF_TOKEN,       // End of file
    INVALID          // Invalid token
};

struct Token {
    TokenType type;
    std::string value;
    SourceLocation location;

    Token(TokenType t, const std::string& v, const SourceLocation& loc)
        : type(t), value(v), location(loc) {}
};

class Lexer {
public:
    explicit Lexer(const std::string& filename, const std::string& source, ErrorReporter& reporter);

    std::vector<Token> tokenize();
    Token next_token();
    bool has_more_tokens() const { return current_ < source_.length(); }

private:
    std::string filename_;
    std::string source_;
    size_t current_;
    size_t line_;
    size_t column_;
    ErrorReporter& error_reporter_;

    static const std::unordered_set<std::string> HTTP_METHODS;

    char peek() const;
    char peek_next() const;
    char advance();
    void skip_whitespace();
    void skip_comment();
    std::string read_while(bool (*predicate)(char));

    Token read_path();
    Token read_identifier_or_method();
    Token read_path_parameter();

    SourceLocation current_location() const;
    void report_error(const std::string& message);

    static bool is_alpha(char c);
    static bool is_alnum(char c);
    static bool is_digit(char c);
};