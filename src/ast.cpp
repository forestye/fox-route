#include "ast.h"

namespace ast_utils {
    std::string path_segment_to_string(const PathSegment& segment) {
        if (std::holds_alternative<std::string>(segment)) {
            return std::get<std::string>(segment);
        } else {
            const auto& param = std::get<PathParameterNode>(segment);
            return "{" + param.type + ":" + param.name + "}";
        }
    }

    std::string path_segments_to_string(const std::vector<PathSegment>& segments) {
        std::string result;
        for (const auto& segment : segments) {
            result += path_segment_to_string(segment);
        }
        return result;
    }

    bool is_path_parameter(const PathSegment& segment) {
        return std::holds_alternative<PathParameterNode>(segment);
    }

    const PathParameterNode& get_path_parameter(const PathSegment& segment) {
        return std::get<PathParameterNode>(segment);
    }
}