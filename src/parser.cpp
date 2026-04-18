#include "parser.h"
#include <sstream>
#include <algorithm>
#include <unordered_set>

Parser::Parser(std::vector<Token> tokens, ErrorReporter& reporter)
    : tokens_(std::move(tokens)), current_(0), error_reporter_(reporter) {}

std::unique_ptr<AstRoot> Parser::parse() {
    auto root = std::make_unique<AstRoot>();

    while (!is_at_end()) {
        // Skip newlines
        if (match(TokenType::NEWLINE)) {
            continue;
        }

        if (current_token().type == TokenType::EOF_TOKEN) {
            break;
        }

        // Check if this is a filesystem route
        if (check(TokenType::FILESYSTEM)) {
            auto fs_route = parse_filesystem_route();
            if (fs_route) {
                root->filesystem_routes.push_back(*fs_route);
            }
        } else {
            // Regular route rule
            auto rule = parse_route_rule();
            if (rule) {
                root->rules.push_back(*rule);
            }
        }

        // Synchronize on error
        if (error_reporter_.has_errors()) {
            synchronize();
        }
    }

    return root;
}

std::unique_ptr<RouteRuleNode> Parser::parse_route_rule() {
    SourceLocation rule_location = current_token().location;

    // Parse HTTP method
    if (!check(TokenType::METHOD)) {
        report_error("Expected HTTP method (GET, POST, PUT, DELETE, PATCH, OPTIONS, ANY)");
        return nullptr;
    }
    std::string http_method = current_token().value;
    advance();

    // Parse path
    auto path_segments = parse_path();
    if (path_segments.empty() && error_reporter_.has_errors()) {
        return nullptr;
    }

    // Parse arrow
    if (!match(TokenType::ARROW)) {
        report_error("Expected '->' after path");
        return nullptr;
    }

    // Parse handler call
    auto handler_call = parse_handler_call();
    if (handler_call.function_name.empty() && error_reporter_.has_errors()) {
        return nullptr;
    }

    // Expect newline or EOF
    if (!match(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
        report_error("Expected newline or end of file after route rule");
    }

    return std::make_unique<RouteRuleNode>(http_method, path_segments, handler_call, rule_location);
}

std::unique_ptr<FilesystemRouteNode> Parser::parse_filesystem_route() {
    SourceLocation rule_location = current_token().location;

    // Parse FILESYSTEM keyword
    if (!match(TokenType::FILESYSTEM)) {
        report_error("Expected FILESYSTEM keyword");
        return nullptr;
    }

    // Parse URL prefix (must be a path)
    if (!check(TokenType::PATH_LITERAL)) {
        report_error("Expected URL prefix path after FILESYSTEM (e.g., /images)");
        return nullptr;
    }
    std::string url_prefix = current_token().value;
    advance();

    // Parse arrow
    if (!match(TokenType::ARROW)) {
        report_error("Expected '->' after URL prefix");
        return nullptr;
    }

    // Parse filesystem path (can be PATH_LITERAL or IDENTIFIER for relative paths)
    if (!check(TokenType::PATH_LITERAL) && !check(TokenType::IDENTIFIER)) {
        report_error("Expected filesystem path after '->' (e.g., /var/www/images or ../images)");
        return nullptr;
    }
    std::string filesystem_path = current_token().value;
    advance();

    // For IDENTIFIER paths, continue reading if followed by '/' for paths like "data/files"
    while (check(TokenType::PATH_LITERAL)) {
        filesystem_path += current_token().value;
        advance();
    }

    // Expect newline or EOF
    if (!match(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
        report_error("Expected newline or end of file after filesystem route");
    }

    return std::make_unique<FilesystemRouteNode>(url_prefix, filesystem_path, rule_location);
}

std::vector<PathSegment> Parser::parse_path() {
    std::vector<PathSegment> segments;

    while (check(TokenType::PATH_LITERAL) || check(TokenType::PATH_PARAMETER)) {
        if (check(TokenType::PATH_LITERAL)) {
            segments.push_back(current_token().value);
            advance();
        } else if (check(TokenType::PATH_PARAMETER)) {
            Token param_token = current_token();
            advance();

            PathParameterNode param = parse_path_parameter(param_token.value, param_token.location);
            segments.push_back(param);
        }
    }

    if (segments.empty()) {
        report_error("Expected path after HTTP method");
    }

    return segments;
}

PathParameterNode Parser::parse_path_parameter(const std::string& param_string, const SourceLocation& location) {
    // Parse {type:name} or {name} or {*}
    if (param_string.length() < 3 || param_string.front() != '{' || param_string.back() != '}') {
        error_reporter_.report_error(ErrorReporter::ErrorType::SYNTAX_ERROR, location,
                                   "Invalid path parameter format: " + param_string);
        return PathParameterNode("string", "", location);
    }

    std::string content = param_string.substr(1, param_string.length() - 2);

    // Handle wildcard {*}
    if (content == "*") {
        return PathParameterNode("*", "*", location);
    }

    // Split by ':'
    auto colon_pos = content.find(':');
    if (colon_pos == std::string::npos) {
        // No type specified, default to string
        return PathParameterNode("string", content, location);
    }

    std::string type = content.substr(0, colon_pos);
    std::string name = content.substr(colon_pos + 1);

    // Validate type
    static const std::unordered_set<std::string> valid_types = {
        "string", "int", "uint", "double", "*"
    };

    if (valid_types.count(type) == 0) {
        error_reporter_.report_error(ErrorReporter::ErrorType::SYNTAX_ERROR, location,
                                   "Invalid parameter type '" + type + "'. Valid types: string, int, uint, double, *");
        return PathParameterNode("string", name, location);
    }

    if (name.empty()) {
        error_reporter_.report_error(ErrorReporter::ErrorType::SYNTAX_ERROR, location,
                                   "Parameter name cannot be empty");
        return PathParameterNode(type, "unnamed", location);
    }

    return PathParameterNode(type, name, location);
}

HandlerCallNode Parser::parse_handler_call() {
    SourceLocation call_location = current_token().location;

    // Check for optional return type (string or json)
    std::string return_type;
    if (check(TokenType::RETURN_TYPE)) {
        return_type = current_token().value;
        advance();
    }

    if (!check(TokenType::IDENTIFIER) && !check(TokenType::RETURN_TYPE)) {
        report_error("Expected function name");
        return HandlerCallNode(return_type, "", {}, call_location);
    }

    std::string function_name = current_token().value;
    advance();

    if (!match(TokenType::LEFT_PAREN)) {
        report_error("Expected '(' after function name");
        return HandlerCallNode(return_type, function_name, {}, call_location);
    }

    auto arguments = parse_argument_list();

    if (!match(TokenType::RIGHT_PAREN)) {
        report_error("Expected ')' after argument list");
        return HandlerCallNode(return_type, function_name, arguments, call_location);
    }

    return HandlerCallNode(return_type, function_name, arguments, call_location);
}

std::vector<std::string> Parser::parse_argument_list() {
    std::vector<std::string> arguments;

    if (check(TokenType::RIGHT_PAREN)) {
        return arguments; // Empty argument list
    }

    do {
        if (!check(TokenType::IDENTIFIER) && !check(TokenType::RETURN_TYPE)) {
            report_error("Expected argument name");
            return arguments;
        }

        arguments.push_back(current_token().value);
        advance();

    } while (match(TokenType::COMMA));

    return arguments;
}

const Token& Parser::current_token() const {
    if (current_ >= tokens_.size()) {
        static Token eof_token(TokenType::EOF_TOKEN, "", SourceLocation());
        return eof_token;
    }
    return tokens_[current_];
}

const Token& Parser::peek_token() const {
    if (current_ + 1 >= tokens_.size()) {
        static Token eof_token(TokenType::EOF_TOKEN, "", SourceLocation());
        return eof_token;
    }
    return tokens_[current_ + 1];
}

bool Parser::is_at_end() const {
    return current_ >= tokens_.size() || current_token().type == TokenType::EOF_TOKEN;
}

void Parser::advance() {
    if (!is_at_end()) {
        current_++;
    }
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    return current_token().type == type;
}

void Parser::report_error(const std::string& message) {
    report_error_at_token(current_token(), message);
}

void Parser::report_error_at_token(const Token& token, const std::string& message) {
    error_reporter_.report_error(ErrorReporter::ErrorType::SYNTAX_ERROR,
                               token.location, message);
}

void Parser::synchronize() {
    advance();

    while (!is_at_end()) {
        if (current_token().type == TokenType::NEWLINE) {
            return;
        }

        switch (current_token().type) {
            case TokenType::METHOD:
                return;
            default:
                break;
        }

        advance();
    }
}