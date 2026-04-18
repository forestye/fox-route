#include "lexer.h"
#include <cctype>

const std::unordered_set<std::string> Lexer::HTTP_METHODS = {
    "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "ANY"
};

Lexer::Lexer(const std::string& filename, const std::string& source, ErrorReporter& reporter)
    : filename_(filename), source_(source), current_(0), line_(1), column_(1), error_reporter_(reporter) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (has_more_tokens()) {
        Token token = next_token();
        if (token.type != TokenType::INVALID) {
            tokens.push_back(token);
        }
        if (token.type == TokenType::EOF_TOKEN) {
            break;
        }
    }

    return tokens;
}

Token Lexer::next_token() {
    skip_whitespace();

    if (!has_more_tokens()) {
        return Token(TokenType::EOF_TOKEN, "", current_location());
    }

    char current_char = peek();

    // Skip comments
    if (current_char == '#') {
        skip_comment();
        return next_token();
    }

    // Newline
    if (current_char == '\n') {
        SourceLocation loc = current_location();
        advance();
        return Token(TokenType::NEWLINE, "\n", loc);
    }

    // Arrow ->
    if (current_char == '-' && peek_next() == '>') {
        SourceLocation loc = current_location();
        advance();
        advance();
        return Token(TokenType::ARROW, "->", loc);
    }

    // Left parenthesis
    if (current_char == '(') {
        SourceLocation loc = current_location();
        advance();
        return Token(TokenType::LEFT_PAREN, "(", loc);
    }

    // Right parenthesis
    if (current_char == ')') {
        SourceLocation loc = current_location();
        advance();
        return Token(TokenType::RIGHT_PAREN, ")", loc);
    }

    // Comma
    if (current_char == ',') {
        SourceLocation loc = current_location();
        advance();
        return Token(TokenType::COMMA, ",", loc);
    }

    // Path (starts with / or .)
    if (current_char == '/') {
        return read_path();
    }

    // Relative path (starts with . followed by / or .)
    if (current_char == '.') {
        char next = peek_next();
        if (next == '/' || next == '.') {
            return read_path();
        }
    }

    // Path parameter (starts with {)
    if (current_char == '{') {
        return read_path_parameter();
    }

    // Identifier or HTTP method
    if (is_alpha(current_char) || current_char == '_') {
        return read_identifier_or_method();
    }

    // Invalid character
    report_error("Unexpected character '" + std::string(1, current_char) + "'");
    advance();
    return Token(TokenType::INVALID, "", current_location());
}

char Lexer::peek() const {
    if (current_ >= source_.length()) {
        return '\0';
    }
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= source_.length()) {
        return '\0';
    }
    return source_[current_ + 1];
}

char Lexer::advance() {
    if (current_ >= source_.length()) {
        return '\0';
    }

    char ch = source_[current_++];
    if (ch == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return ch;
}

void Lexer::skip_whitespace() {
    while (has_more_tokens() && std::isspace(peek()) && peek() != '\n') {
        advance();
    }
}

void Lexer::skip_comment() {
    while (has_more_tokens() && peek() != '\n') {
        advance();
    }
}

std::string Lexer::read_while(bool (*predicate)(char)) {
    std::string result;
    while (has_more_tokens() && predicate(peek())) {
        result += advance();
    }
    return result;
}

Token Lexer::read_path() {
    SourceLocation loc = current_location();
    std::string path;

    // Read the initial character (can be '/' or '.')
    path += advance();

    // Read path segments
    while (has_more_tokens()) {
        char ch = peek();
        if (ch == ' ' || ch == '\t' || ch == '\n') {
            break;
        }
        // Stop at '->' arrow (but allow '-' in path)
        if (ch == '-' && peek_next() == '>') {
            break;
        }
        if (ch == '{') {
            // This will be handled as a separate PATH_PARAMETER token
            break;
        }
        path += advance();
    }

    return Token(TokenType::PATH_LITERAL, path, loc);
}

Token Lexer::read_identifier_or_method() {
    SourceLocation loc = current_location();
    std::string value = read_while([](char c) {
        return is_alnum(c) || c == '_';
    });

    if (HTTP_METHODS.count(value)) {
        return Token(TokenType::METHOD, value, loc);
    }

    if (value == "FILESYSTEM") {
        return Token(TokenType::FILESYSTEM, value, loc);
    }

    if (value == "text" || value == "html" || value == "json") {
        return Token(TokenType::RETURN_TYPE, value, loc);
    }

    return Token(TokenType::IDENTIFIER, value, loc);
}

Token Lexer::read_path_parameter() {
    SourceLocation loc = current_location();
    std::string value;

    // Read '{'
    if (peek() != '{') {
        report_error("Expected '{' at start of path parameter");
        return Token(TokenType::INVALID, "", loc);
    }
    value += advance();

    // Read content until '}'
    while (has_more_tokens() && peek() != '}') {
        value += advance();
    }

    // Read '}'
    if (peek() != '}') {
        report_error("Expected '}' at end of path parameter");
        return Token(TokenType::INVALID, "", loc);
    }
    value += advance();

    return Token(TokenType::PATH_PARAMETER, value, loc);
}

SourceLocation Lexer::current_location() const {
    return SourceLocation(filename_, line_, column_);
}

void Lexer::report_error(const std::string& message) {
    error_reporter_.report_error(ErrorReporter::ErrorType::LEXICAL_ERROR,
                                current_location(), message);
}

bool Lexer::is_alpha(char c) {
    return std::isalpha(c);
}

bool Lexer::is_alnum(char c) {
    return std::isalnum(c);
}

bool Lexer::is_digit(char c) {
    return std::isdigit(c);
}