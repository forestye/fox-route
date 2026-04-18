#pragma once

#include <memory>
#include "lexer.h"
#include "ast.h"
#include "error_reporter.h"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, ErrorReporter& reporter);

    std::unique_ptr<AstRoot> parse();

private:
    std::vector<Token> tokens_;
    size_t current_;
    ErrorReporter& error_reporter_;

    const Token& current_token() const;
    const Token& peek_token() const;
    bool is_at_end() const;
    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;

    void report_error(const std::string& message);
    void report_error_at_token(const Token& token, const std::string& message);

    std::unique_ptr<RouteRuleNode> parse_route_rule();
    std::unique_ptr<FilesystemRouteNode> parse_filesystem_route();
    std::vector<PathSegment> parse_path();
    PathParameterNode parse_path_parameter(const std::string& param_string, const SourceLocation& location);
    HandlerCallNode parse_handler_call();
    std::vector<std::string> parse_argument_list();

    void synchronize();
};