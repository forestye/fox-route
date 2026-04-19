#include "code_generator.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

CodeGenerator::CodeGenerator(ErrorReporter& reporter) : error_reporter_(reporter) {}

std::string CodeGenerator::get_function_signature(const RouteInfo& route_info) {
    return generate_function_signature(route_info);
}

bool CodeGenerator::generate(const AstRoot& ast, const RadixTree& tree, const std::string& output_dir) {
    try {
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_dir);

        // Generate all files
        bool success = true;
        success &= generate_handlers_h(tree, output_dir);
        success &= generate_handlers_cpp(tree, output_dir);
        success &= generate_router_h(ast, output_dir);
        success &= generate_router_cpp(ast, tree, output_dir);

        return success;
    } catch (const std::exception& e) {
        error_reporter_.report_error(ErrorReporter::ErrorType::SEMANTIC_ERROR,
                                   SourceLocation(), "Code generation failed: " + std::string(e.what()));
        return false;
    }
}

bool CodeGenerator::generate_handlers_h(const RadixTree& tree, const std::string& output_dir) {
    std::stringstream ss;

    ss << "#pragma once\n";
    ss << "#include <string>\n";
    ss << "#include <cstdint>\n";
    ss << "#include <unordered_map>\n";
    ss << "#include <vector>\n";

    // Collect all routes from the tree
    auto routes = tree.collect_all_routes();

    // Check if any route returns json or uses json parameter
    bool needs_json = false;
    bool needs_multipart = false;
    for (const auto* route_info : routes) {
        if (route_info->handler_call.return_type == "json") {
            needs_json = true;
        }
        if (std::find(route_info->handler_call.arguments.begin(),
                     route_info->handler_call.arguments.end(),
                     "json") != route_info->handler_call.arguments.end()) {
            needs_json = true;
        }
        if (std::find(route_info->handler_call.arguments.begin(),
                     route_info->handler_call.arguments.end(),
                     "multipart") != route_info->handler_call.arguments.end()) {
            needs_multipart = true;
        }
    }

    // Include json header if needed
    if (needs_json) {
        ss << "#include <json/json.h>\n";
    }

    ss << "#include \"fox-http/http_request.h\"\n";
    ss << "#include \"fox-http/http_response.h\"\n\n";

    // Add multipart data structures if needed
    if (needs_multipart) {
        ss << "// Multipart form data structures\n";
        ss << "struct MultipartFile {\n";
        ss << "    std::string field_name;      // Form field name\n";
        ss << "    std::string filename;         // Original filename\n";
        ss << "    std::string content_type;     // MIME type\n";
        ss << "    std::string data;             // File content\n";
        ss << "};\n\n";
        ss << "struct MultipartForm {\n";
        ss << "    std::unordered_map<std::string, std::string> fields;  // Regular form fields\n";
        ss << "    std::vector<MultipartFile> files;                      // Uploaded files\n";
        ss << "};\n\n";
    }

    ss << "// Business developers need to implement these function declarations\n";
    ss << "// Generated from CRDL route definitions\n\n";

    // Generate unique handler signatures
    std::unordered_set<std::string> generated_signatures;

    for (const auto* route_info : routes) {
        std::string signature = generate_function_signature(*route_info);
        if (generated_signatures.find(signature) == generated_signatures.end()) {
            ss << signature << ";\n";
            generated_signatures.insert(signature);
        }
    }

    if (routes.empty()) {
        ss << "// No routes found in CRDL file\n";
    }

    return write_file(output_dir + "/handlers.h", ss.str());
}

bool CodeGenerator::generate_handlers_cpp(const RadixTree& tree, const std::string& output_dir) {
    std::string cpp_filepath = output_dir + "/handlers.cpp";

    // Check if handlers.cpp already exists
    if (std::filesystem::exists(cpp_filepath)) {
        std::cout << "Skipped: " << cpp_filepath << " (file already exists, not overwriting)" << std::endl;
        return true;  // Return success but don't overwrite
    }

    std::stringstream ss;

    ss << "#include \"handlers.h\"\n";
    ss << "#include <iostream>\n";
    ss << "#include <string>\n";
    ss << "using namespace std;\n";
    ss << "// Implementation of handler functions\n";
    ss << "// These are template implementations - modify as needed\n\n";

    // Collect all routes from the tree
    auto routes = tree.collect_all_routes();

    // Generate template implementations for unique handlers
    std::unordered_set<std::string> generated_functions;

    for (const auto* route_info : routes) {
        std::string function_signature = generate_function_signature(*route_info);

        if (generated_functions.find(function_signature) == generated_functions.end()) {
            // Determine return type
            bool has_resp = std::find(route_info->handler_call.arguments.begin(),
                                     route_info->handler_call.arguments.end(), "resp")
                            != route_info->handler_call.arguments.end();

            ss << function_signature << " {\n";

            if (has_resp) {
                // Function uses resp parameter
                ss << "    // TODO: Implement " << route_info->handler_call.function_name << "\n";
                ss << "    resp.set_status(200);\n";
                ss << "    resp.headers().content_type(\"text/html; charset=utf-8\");\n";
                ss << "    resp.set_body(\"<html><body><h1>" << route_info->handler_call.function_name << " called</h1></body></html>\");\n";
            } else {
                // Function returns string
                ss << "    // TODO: Implement " << route_info->handler_call.function_name << "\n";

                // Add parameter info in comments
                if (!route_info->parameter_names.empty()) {
                    ss << "    // Parameters received:\n";
                    for (size_t i = 0; i < route_info->parameter_names.size(); ++i) {
                        ss << "    //   " << route_info->parameter_names[i]
                           << " (" << route_info->parameter_types[i] << ")\n";
                    }
                }

                ss << "    return \"" << route_info->handler_call.function_name << " result\";\n";
            }

            ss << "}\n\n";
            generated_functions.insert(function_signature);
        }
    }

    if (routes.empty()) {
        ss << "// No routes found in CRDL file\n";
    }

    return write_file(cpp_filepath, ss.str());
}

bool CodeGenerator::generate_router_h(const AstRoot& ast, const std::string& output_dir) {
    std::stringstream ss;

    ss << "#pragma once\n\n";
    ss << "#include \"fox-http/http_handler.h\"\n";
    ss << "#include \"fox-http/http_request.h\"\n";
    ss << "#include \"fox-http/http_response.h\"\n";
    ss << "#include <memory>\n";
    ss << "#include <string>\n";
    ss << "#include <unordered_map>\n";
    ss << "#include <vector>\n\n";

    // Forward declarations
    ss << "struct PathSegment;\n";
    ss << "struct RouteHandler;\n";
    ss << "struct DynamicRoute;\n\n";

    ss << "class Router : public fox::http::HttpHandler {\n";
    ss << "public:\n";
    ss << "    Router();\n";
    ss << "    ~Router();\n\n";
    ss << "    void handle(fox::http::HttpRequest& req,\n";
    ss << "                fox::http::HttpResponse& resp) override;\n\n";
    ss << "private:\n";
    ss << "    // Static routes: path -> (method -> handler)\n";
    ss << "    std::unordered_map<std::string, std::unordered_map<int, RouteHandler*>> static_routes_;\n\n";
    ss << "    // Dynamic routes: grouped by segment count for fast filtering\n";
    ss << "    std::unordered_map<size_t, std::vector<DynamicRoute*>> dynamic_routes_by_segment_count_;\n\n";
    ss << "    // Storage for all route handlers\n";
    ss << "    std::vector<std::unique_ptr<RouteHandler>> handlers_;\n";
    ss << "    std::vector<std::unique_ptr<DynamicRoute>> dynamic_routes_;\n\n";

    // Add filesystem handlers if needed
    if (!ast.filesystem_routes.empty()) {
        ss << "    // Filesystem routes (simple local-disk mount)\n";
        ss << "    struct FilesystemRoute {\n";
        ss << "        std::string url_prefix;\n";
        ss << "        std::string local_path;\n";
        ss << "    };\n";
        ss << "    std::vector<FilesystemRoute> filesystem_routes_;\n";
        ss << "    void serve_filesystem_file(const FilesystemRoute& route,\n";
        ss << "                               std::string_view url_path,\n";
        ss << "                               fox::http::HttpResponse& resp);\n\n";
    }

    ss << "    // Helper methods\n";
    ss << "    std::vector<std::string> split_path(std::string_view path) const;\n";
    ss << "    bool match_segments(const std::vector<std::string>& path_parts,\n";
    ss << "                       const std::vector<PathSegment>& segments,\n";
    ss << "                       std::unordered_map<std::string, std::string>& params) const;\n";
    ss << "    void dispatch_handler(const RouteHandler& handler,\n";
    ss << "                          fox::http::HttpRequest& req,\n";
    ss << "                          fox::http::HttpResponse& resp,\n";
    ss << "                          const std::unordered_map<std::string, std::string>& params);\n";
    ss << "};\n";

    return write_file(output_dir + "/router.generated.h", ss.str());
}

bool CodeGenerator::generate_router_cpp(const AstRoot& ast, const RadixTree& tree, const std::string& output_dir) {
    std::stringstream ss;

    ss << "#include \"router.generated.h\"\n";
    ss << "#include \"handlers.h\"\n";
    ss << "#include \"fox-http/http_util.h\"\n";
    ss << "#include <unordered_map>\n";
    ss << "#include <string>\n";
    ss << "#include <vector>\n";
    ss << "#include <memory>\n";
    ss << "#include <sstream>\n";
    ss << "#include <stdexcept>\n";
    ss << "#include <iostream>\n";
    // Filesystem route support: read files from local disk.
    if (!ast.filesystem_routes.empty()) {
        ss << "#include <filesystem>\n";
        ss << "#include <fstream>\n";
    }

    // Collect routes with their path patterns for generating dispatch logic
    auto routes_with_paths = tree.collect_routes_with_paths();

    // Check if any route returns json or uses json/multipart parameter
    bool needs_json = false;
    bool needs_multipart = false;
    for (const auto& route_with_path : routes_with_paths) {
        if (route_with_path.route_info->handler_call.return_type == "json") {
            needs_json = true;
        }
        if (std::find(route_with_path.route_info->handler_call.arguments.begin(),
                     route_with_path.route_info->handler_call.arguments.end(),
                     "json") != route_with_path.route_info->handler_call.arguments.end()) {
            needs_json = true;
        }
        if (std::find(route_with_path.route_info->handler_call.arguments.begin(),
                     route_with_path.route_info->handler_call.arguments.end(),
                     "multipart") != route_with_path.route_info->handler_call.arguments.end()) {
            needs_multipart = true;
        }
    }

    // Include json header if needed
    if (needs_json) {
        ss << "#include <json/json.h>\n";
    }

    ss << "\n";
    ss << "using namespace std;\n";
    ss << "using fox::http::HttpRequest;\n";
    ss << "using fox::http::HttpResponse;\n";
    ss << "using fox::http::util::parse_form_urlencoded;\n\n";

    // Add multipart parser function if needed
    if (needs_multipart) {
        // Note: MultipartFile and MultipartForm structures are defined in handlers.h
        // Add multipart parser function
        ss << "// Parse multipart/form-data\n";
        ss << "MultipartForm parse_multipart(const string& body, const string& boundary) {\n";
        ss << "    MultipartForm result;\n";
        ss << "    string delimiter = \"--\" + boundary;\n";
        ss << "    string end_delimiter = delimiter + \"--\";\n";
        ss << "    \n";
        ss << "    size_t pos = 0;\n";
        ss << "    while (pos < body.size()) {\n";
        ss << "        // Find next part\n";
        ss << "        size_t part_start = body.find(delimiter, pos);\n";
        ss << "        if (part_start == string::npos) break;\n";
        ss << "        part_start += delimiter.size();\n";
        ss << "        \n";
        ss << "        // Skip CRLF after delimiter\n";
        ss << "        if (part_start + 2 <= body.size() && body.substr(part_start, 2) == \"\\r\\n\") {\n";
        ss << "            part_start += 2;\n";
        ss << "        }\n";
        ss << "        \n";
        ss << "        // Check for end delimiter\n";
        ss << "        if (body.substr(part_start - delimiter.size() - 2, end_delimiter.size()) == end_delimiter) {\n";
        ss << "            break;\n";
        ss << "        }\n";
        ss << "        \n";
        ss << "        // Find end of headers\n";
        ss << "        size_t headers_end = body.find(\"\\r\\n\\r\\n\", part_start);\n";
        ss << "        if (headers_end == string::npos) break;\n";
        ss << "        \n";
        ss << "        string headers = body.substr(part_start, headers_end - part_start);\n";
        ss << "        size_t data_start = headers_end + 4;\n";
        ss << "        \n";
        ss << "        // Find next delimiter\n";
        ss << "        size_t data_end = body.find(\"\\r\\n\" + delimiter, data_start);\n";
        ss << "        if (data_end == string::npos) break;\n";
        ss << "        \n";
        ss << "        string data = body.substr(data_start, data_end - data_start);\n";
        ss << "        \n";
        ss << "        // Parse headers\n";
        ss << "        string field_name, filename, content_type;\n";
        ss << "        istringstream header_stream(headers);\n";
        ss << "        string header_line;\n";
        ss << "        while (getline(header_stream, header_line)) {\n";
        ss << "            if (header_line.back() == '\\r') header_line.pop_back();\n";
        ss << "            \n";
        ss << "            if (header_line.find(\"Content-Disposition:\") == 0) {\n";
        ss << "                // Extract field name\n";
        ss << "                size_t name_pos = header_line.find(\"name=\\\"\");\n";
        ss << "                if (name_pos != string::npos) {\n";
        ss << "                    size_t name_start = name_pos + 6;\n";
        ss << "                    size_t name_end = header_line.find(\"\\\"\", name_start);\n";
        ss << "                    field_name = header_line.substr(name_start, name_end - name_start);\n";
        ss << "                }\n";
        ss << "                // Extract filename if present\n";
        ss << "                size_t file_pos = header_line.find(\"filename=\\\"\");\n";
        ss << "                if (file_pos != string::npos) {\n";
        ss << "                    size_t file_start = file_pos + 10;\n";
        ss << "                    size_t file_end = header_line.find(\"\\\"\", file_start);\n";
        ss << "                    filename = header_line.substr(file_start, file_end - file_start);\n";
        ss << "                }\n";
        ss << "            } else if (header_line.find(\"Content-Type:\") == 0) {\n";
        ss << "                content_type = header_line.substr(14);\n";
        ss << "                // Trim whitespace\n";
        ss << "                size_t first = content_type.find_first_not_of(\" \\t\");\n";
        ss << "                if (first != string::npos) content_type = content_type.substr(first);\n";
        ss << "            }\n";
        ss << "        }\n";
        ss << "        \n";
        ss << "        // Store data\n";
        ss << "        if (!filename.empty()) {\n";
        ss << "            // This is a file\n";
        ss << "            MultipartFile file;\n";
        ss << "            file.field_name = field_name;\n";
        ss << "            file.filename = filename;\n";
        ss << "            file.content_type = content_type;\n";
        ss << "            file.data = data;\n";
        ss << "            result.files.push_back(file);\n";
        ss << "        } else {\n";
        ss << "            // This is a regular field\n";
        ss << "            result.fields[field_name] = data;\n";
        ss << "        }\n";
        ss << "        \n";
        ss << "        pos = data_end + 2;\n";
        ss << "    }\n";
        ss << "    \n";
        ss << "    return result;\n";
        ss << "}\n\n";
    }

    // Generate data structures
    ss << "// Path segment: either static or parameter\n";
    ss << "struct PathSegment {\n";
    ss << "    bool is_static;\n";
    ss << "    string static_value;  // If static segment\n";
    ss << "    string param_type;    // If parameter: int, uint, double, string, *\n";
    ss << "    string param_name;    // Parameter name\n";
    ss << "};\n\n";

    ss << "// Route handler information\n";
    ss << "struct RouteHandler {\n";
    ss << "    string handler_name;\n";
    ss << "    vector<string> handler_args;\n";
    ss << "    vector<string> param_names;\n";
    ss << "    vector<string> param_types;\n";
    ss << "    bool uses_form;\n";
    ss << "    bool uses_body;\n";
    ss << "    bool uses_json;\n";
    ss << "    bool uses_query;\n";
    ss << "    bool uses_multipart;\n";
    ss << "    string return_type;  // \"text\", \"html\", \"json\", or \"\" (void)\n";
    ss << "};\n\n";

    ss << "// Dynamic route with segments\n";
    ss << "struct DynamicRoute {\n";
    ss << "    HttpRequest::Method method;\n";
    ss << "    vector<PathSegment> segments;\n";
    ss << "    RouteHandler* handler;\n";
    ss << "};\n\n";

    // Type conversion functions
    ss << "namespace {\n";
    ss << "    int64_t convert_to_int64(const string& str) {\n";
    ss << "        return stoll(str);\n";
    ss << "    }\n\n";
    ss << "    uint64_t convert_to_uint64(const string& str) {\n";
    ss << "        return stoull(str);\n";
    ss << "    }\n\n";
    ss << "    double convert_to_double(const string& str) {\n";
    ss << "        return stod(str);\n";
    ss << "    }\n\n";

    ss << "    // Validate parameter type and return true if valid\n";
    ss << "    bool validate_param_type(const string& value, const string& type) {\n";
    ss << "        if (type == \"int\" || type == \"uint\") {\n";
    ss << "            for (char c : value) {\n";
    ss << "                if (!isdigit(c)) return false;\n";
    ss << "            }\n";
    ss << "            return !value.empty();\n";
    ss << "        } else if (type == \"double\") {\n";
    ss << "            try { stod(value); return true; } catch (...) { return false; }\n";
    ss << "        }\n";
    ss << "        return true;  // string and wildcard always valid\n";
    ss << "    }\n";
    ss << "}\n\n";

    // Constructor: build route tables
    ss << "Router::Router() {\n";
    ss << "    // Build route tables from CRDL definitions\n";
    ss << "    // Routes are separated into static (O(1) lookup) and dynamic (segment-based matching)\n\n";

    // First pass: create all handlers
    size_t handler_idx = 0;
    for (const auto& route_with_path : routes_with_paths) {
        const RouteInfo* route = route_with_path.route_info;
        bool needs_form = std::find(route->handler_call.arguments.begin(),
                                   route->handler_call.arguments.end(), "form")
                         != route->handler_call.arguments.end();
        bool needs_body = std::find(route->handler_call.arguments.begin(),
                                   route->handler_call.arguments.end(), "body")
                         != route->handler_call.arguments.end();
        bool needs_json = std::find(route->handler_call.arguments.begin(),
                                   route->handler_call.arguments.end(), "json")
                         != route->handler_call.arguments.end();
        bool needs_query = std::find(route->handler_call.arguments.begin(),
                                    route->handler_call.arguments.end(), "query")
                          != route->handler_call.arguments.end();
        bool needs_multipart = std::find(route->handler_call.arguments.begin(),
                                        route->handler_call.arguments.end(), "multipart")
                              != route->handler_call.arguments.end();

        ss << "    handlers_.push_back(make_unique<RouteHandler>(RouteHandler{\n";
        ss << "        \"" << route->handler_call.function_name << "\",\n";
        ss << "        {";
        for (size_t i = 0; i < route->handler_call.arguments.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << route->handler_call.arguments[i] << "\"";
        }
        ss << "},\n";
        ss << "        {";
        for (size_t i = 0; i < route->parameter_names.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << route->parameter_names[i] << "\"";
        }
        ss << "},\n";
        ss << "        {";
        for (size_t i = 0; i < route->parameter_types.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << route->parameter_types[i] << "\"";
        }
        ss << "},\n";
        ss << "        " << (needs_form ? "true" : "false") << ",\n";
        ss << "        " << (needs_body ? "true" : "false") << ",\n";
        ss << "        " << (needs_json ? "true" : "false") << ",\n";
        ss << "        " << (needs_query ? "true" : "false") << ",\n";
        ss << "        " << (needs_multipart ? "true" : "false") << ",\n";
        ss << "        \"" << route->handler_call.return_type << "\"\n";
        ss << "    }));\n";
    }
    ss << "\n";

    // Second pass: categorize routes
    handler_idx = 0;
    for (const auto& route_with_path : routes_with_paths) {
        const RouteInfo* route = route_with_path.route_info;
        const std::string& path_pattern = route_with_path.path_pattern;

        // Check if this is a static route (no parameters)
        bool is_static = route->parameter_names.empty();

        // "ANY" in CRDL means "match any HTTP method". Expand it into all
        // supported methods for both static and dynamic route registration.
        static const std::vector<std::string> all_methods = {
            "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"
        };
        std::vector<std::string> methods_to_register;
        if (route->http_method == "ANY") {
            methods_to_register = all_methods;
        } else {
            methods_to_register.push_back(route->http_method);
        }

        if (is_static) {
            // Static route: add to hash table (once per method).
            for (const auto& m : methods_to_register) {
                ss << "    static_routes_[\"" << path_pattern << "\"][(int) HttpRequest::Method::" << m
                   << "] = handlers_[" << handler_idx << "].get();\n";
            }
        } else {
            // Dynamic route: build segments once, register route per method.
            // All method-specific copies share the same handler.
            ss << "    {\n";
            ss << "        auto route = make_unique<DynamicRoute>();\n";
            ss << "        route->method = HttpRequest::Method::" << methods_to_register.front() << ";\n";
            ss << "        route->handler = handlers_[" << handler_idx << "].get();\n";

            // Parse path pattern into segments
            ss << "        // Path: " << path_pattern << "\n";
            std::vector<std::string> segments;
            std::string current_seg;
            bool in_param = false;

            for (char c : path_pattern) {
                if (c == '{') {
                    if (!current_seg.empty() && current_seg != "/") {
                        segments.push_back(current_seg);
                        current_seg.clear();
                    }
                    in_param = true;
                    current_seg = "{";
                } else if (c == '}' && in_param) {
                    current_seg += "}";
                    segments.push_back(current_seg);
                    current_seg.clear();
                    in_param = false;
                } else {
                    current_seg += c;
                }
            }
            if (!current_seg.empty() && current_seg != "/") {
                segments.push_back(current_seg);
            }

            // Generate segment parsing code
            // Split static segments by '/' to match split_path behavior
            size_t total_segment_count = 0;
            for (const auto& seg : segments) {
                if (seg.front() == '{') {
                    // Parameter segment
                    size_t colon_pos = seg.find(':');
                    std::string param_type = seg.substr(1, colon_pos - 1);
                    std::string param_name = seg.substr(colon_pos + 1, seg.length() - colon_pos - 2);

                    ss << "        route->segments.push_back(PathSegment{false, \"\", \""
                       << param_type << "\", \"" << param_name << "\"});\n";
                    total_segment_count++;
                } else {
                    // Static segment - split by '/' to match split_path behavior
                    std::vector<std::string> static_parts;
                    std::string part;
                    for (char c : seg) {
                        if (c == '/') {
                            if (!part.empty()) {
                                static_parts.push_back(part);
                                part.clear();
                            }
                        } else {
                            part += c;
                        }
                    }
                    if (!part.empty()) {
                        static_parts.push_back(part);
                    }

                    // Generate a PathSegment for each static part
                    for (const auto& static_part : static_parts) {
                        ss << "        route->segments.push_back(PathSegment{true, \""
                           << static_part << "\", \"\", \"\"});\n";
                        total_segment_count++;
                    }
                }
            }

            ss << "        dynamic_routes_by_segment_count_[" << total_segment_count
               << "].push_back(route.get());\n";
            ss << "        dynamic_routes_.push_back(std::move(route));\n";
            // For ANY methods, clone the same route under every other method.
            for (std::size_t mi = 1; mi < methods_to_register.size(); ++mi) {
                ss << "        {\n";
                ss << "            auto alt = make_unique<DynamicRoute>();\n";
                ss << "            alt->method = HttpRequest::Method::" << methods_to_register[mi] << ";\n";
                ss << "            alt->handler = handlers_[" << handler_idx << "].get();\n";
                ss << "            alt->segments = dynamic_routes_.back()->segments;\n";
                ss << "            dynamic_routes_by_segment_count_[" << total_segment_count
                   << "].push_back(alt.get());\n";
                ss << "            dynamic_routes_.push_back(std::move(alt));\n";
                ss << "        }\n";
            }
            ss << "    }\n";
        }

        handler_idx++;
    }

    // Add filesystem routes initialization
    if (!ast.filesystem_routes.empty()) {
        ss << "\n    // Initialize filesystem routes (url prefix -> local directory).\n";
        for (const auto& fs_route : ast.filesystem_routes) {
            ss << "    filesystem_routes_.push_back({\""
               << fs_route.url_prefix << "\", \"" << fs_route.filesystem_path << "\"});\n";
        }
    }

    ss << "}\n\n";

    ss << "Router::~Router() = default;\n\n";

    if (!ast.filesystem_routes.empty()) {
        // Generate the inline filesystem handler.
        ss << "void Router::serve_filesystem_file(const FilesystemRoute& route,\n";
        ss << "                                  string_view url_path,\n";
        ss << "                                  HttpResponse& resp) {\n";
        ss << "    // Strip the URL prefix, prepend local_path. Reject '..' traversal.\n";
        ss << "    string rel(url_path.substr(route.url_prefix.size()));\n";
        ss << "    while (!rel.empty() && rel.front() == '/') rel.erase(rel.begin());\n";
        ss << "    if (rel.find(\"..\") != string::npos) {\n";
        ss << "        resp.set_status(403);\n";
        ss << "        resp.set_body(\"Forbidden\");\n";
        ss << "        return;\n";
        ss << "    }\n";
        ss << "    filesystem::path full = filesystem::path(route.local_path) / rel;\n";
        ss << "    if (!filesystem::exists(full) || filesystem::is_directory(full)) {\n";
        ss << "        resp.set_status(404);\n";
        ss << "        resp.set_body(\"Not Found\");\n";
        ss << "        return;\n";
        ss << "    }\n";
        ss << "    ifstream ifs(full, ios::binary);\n";
        ss << "    if (!ifs) {\n";
        ss << "        resp.set_status(404);\n";
        ss << "        resp.set_body(\"Not Found\");\n";
        ss << "        return;\n";
        ss << "    }\n";
        ss << "    stringstream buf;\n";
        ss << "    buf << ifs.rdbuf();\n";
        ss << "    // Simple extension-based Content-Type.\n";
        ss << "    auto ext = full.extension().string();\n";
        ss << "    string ct = \"application/octet-stream\";\n";
        ss << "    if (ext == \".html\" || ext == \".htm\") ct = \"text/html; charset=utf-8\";\n";
        ss << "    else if (ext == \".css\") ct = \"text/css; charset=utf-8\";\n";
        ss << "    else if (ext == \".js\") ct = \"application/javascript; charset=utf-8\";\n";
        ss << "    else if (ext == \".json\") ct = \"application/json; charset=utf-8\";\n";
        ss << "    else if (ext == \".txt\") ct = \"text/plain; charset=utf-8\";\n";
        ss << "    else if (ext == \".png\") ct = \"image/png\";\n";
        ss << "    else if (ext == \".jpg\" || ext == \".jpeg\") ct = \"image/jpeg\";\n";
        ss << "    else if (ext == \".gif\") ct = \"image/gif\";\n";
        ss << "    else if (ext == \".svg\") ct = \"image/svg+xml\";\n";
        ss << "    else if (ext == \".ico\") ct = \"image/x-icon\";\n";
        ss << "    resp.set_status(200);\n";
        ss << "    resp.headers().content_type(ct);\n";
        ss << "    resp.set_body(buf.str());\n";
        ss << "}\n\n";
    }

    // Helper method: split path
    ss << "vector<string> Router::split_path(string_view path) const {\n";
    ss << "    vector<string> parts;\n";
    ss << "    string current;\n";
    ss << "    for (char c : path) {\n";
    ss << "        if (c == '/') {\n";
    ss << "            if (!current.empty()) {\n";
    ss << "                parts.push_back(current);\n";
    ss << "                current.clear();\n";
    ss << "            }\n";
    ss << "        } else {\n";
    ss << "            current += c;\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "    if (!current.empty()) parts.push_back(current);\n";
    ss << "    return parts;\n";
    ss << "}\n\n";

    // Helper method: match segments
    ss << "bool Router::match_segments(const vector<string>& path_parts,\n";
    ss << "                           const vector<PathSegment>& segments,\n";
    ss << "                           unordered_map<string, string>& params) const {\n";
    ss << "    size_t path_idx = 0;\n";
    ss << "    for (const auto& seg : segments) {\n";
    ss << "        if (seg.is_static) {\n";
    ss << "            // Match static segment (already normalized without slashes)\n";
    ss << "            if (path_idx >= path_parts.size() || path_parts[path_idx] != seg.static_value) {\n";
    ss << "                return false;\n";
    ss << "            }\n";
    ss << "            path_idx++;\n";
    ss << "        } else {\n";
    ss << "            // Match parameter segment\n";
    ss << "            if (path_idx >= path_parts.size()) return false;\n";
    ss << "            const string& value = path_parts[path_idx];\n";
    ss << "            // Validate type\n";
    ss << "            if (!validate_param_type(value, seg.param_type)) return false;\n";
    ss << "            params[seg.param_name] = value;\n";
    ss << "            path_idx++;\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "    return path_idx == path_parts.size();  // Must consume all path parts\n";
    ss << "}\n\n";

    // Helper method: dispatch handler
    ss << "void Router::dispatch_handler(const RouteHandler& handler,\n";
    ss << "                              HttpRequest& req, HttpResponse& resp,\n";
    ss << "                              const unordered_map<string, string>& params) {\n";
    ss << "    try {\n";
    ss << "        // Body is already eagerly read by Connection.\n";
    ss << "        string body(req.body());\n";
    ss << "        unordered_map<string, string> form;\n";
    ss << "        Json::Value json_value;\n";
    if (needs_multipart) {
        ss << "        MultipartForm multipart;\n";
    }
    ss << "        if (handler.uses_form) {\n";
    ss << "            form = parse_form_urlencoded(body);\n";
    ss << "        }\n";
    ss << "        if (handler.uses_json) {\n";
    ss << "            Json::CharReaderBuilder reader_builder;\n";
    ss << "            string errors;\n";
    ss << "            istringstream body_stream(body);\n";
    ss << "            if (!Json::parseFromStream(reader_builder, body_stream, &json_value, &errors)) {\n";
    ss << "                resp.set_status(400);\n";
    ss << "                resp.set_body(\"Bad Request: Invalid JSON - \" + errors);\n";
    ss << "                return;\n";
    ss << "            }\n";
    ss << "        }\n";
    if (needs_multipart) {
        ss << "        if (handler.uses_multipart) {\n";
        ss << "            string content_type(req.header(\"Content-Type\"));\n";
        ss << "            size_t boundary_pos = content_type.find(\"boundary=\");\n";
        ss << "            if (boundary_pos != string::npos) {\n";
        ss << "                string boundary = content_type.substr(boundary_pos + 9);\n";
        ss << "                multipart = parse_multipart(body, boundary);\n";
        ss << "            } else {\n";
        ss << "                resp.set_status(400);\n";
        ss << "                resp.set_body(\"Bad Request: Missing boundary in Content-Type\");\n";
        ss << "                return;\n";
        ss << "            }\n";
        ss << "        }\n";
    }
    ss << "        unordered_map<string, string> query;\n";
    ss << "        if (handler.uses_query) {\n";
    ss << "            const auto& qmap = req.queries();\n";
    ss << "            query.insert(qmap.begin(), qmap.end());\n";
    ss << "        }\n\n";

    ss << "        // Dispatch to specific handler\n";
    for (const auto& route_with_path : routes_with_paths) {
        const RouteInfo* route = route_with_path.route_info;
        ss << "        if (handler.handler_name == \"" << route->handler_call.function_name << "\") {\n";

        const std::string& return_type = route->handler_call.return_type;

        // Call handler and capture return value if needed
        if (!return_type.empty()) {
            ss << "            auto result = ";
        } else {
            ss << "            ";
        }

        ss << route->handler_call.function_name << "(";

        for (size_t i = 0; i < route->handler_call.arguments.size(); ++i) {
            if (i > 0) ss << ", ";
            const std::string& arg = route->handler_call.arguments[i];

            if (arg == "req") {
                ss << "req";
            } else if (arg == "resp") {
                ss << "resp";
            } else if (arg == "form") {
                ss << "form";
            } else if (arg == "body") {
                ss << "body";
            } else if (arg == "json") {
                ss << "json_value";
            } else if (arg == "query") {
                ss << "query";
            } else if (arg == "multipart") {
                ss << "multipart";
            } else {
                // Find parameter type and convert
                auto it = std::find(route->parameter_names.begin(),
                                  route->parameter_names.end(), arg);
                if (it != route->parameter_names.end()) {
                    size_t idx = std::distance(route->parameter_names.begin(), it);
                    const std::string& type = route->parameter_types[idx];

                    if (type == "int") {
                        ss << "convert_to_int64(params.at(\"" << arg << "\"))";
                    } else if (type == "uint") {
                        ss << "convert_to_uint64(params.at(\"" << arg << "\"))";
                    } else if (type == "double") {
                        ss << "convert_to_double(params.at(\"" << arg << "\"))";
                    } else {
                        ss << "params.at(\"" << arg << "\")";
                    }
                }
            }
        }
        ss << ");\n";

        // Handle return value based on type
        if (return_type == "text") {
            ss << "            resp.set_status(200);\n";
            ss << "            resp.headers().content_type(\"text/plain; charset=utf-8\");\n";
            ss << "            resp.set_body(std::move(result));\n";
        } else if (return_type == "html") {
            ss << "            resp.set_status(200);\n";
            ss << "            resp.headers().content_type(\"text/html; charset=utf-8\");\n";
            ss << "            resp.set_body(std::move(result));\n";
        } else if (return_type == "json") {
            ss << "            Json::StreamWriterBuilder writer;\n";
            ss << "            string json_str = Json::writeString(writer, result);\n";
            ss << "            resp.set_status(200);\n";
            ss << "            resp.headers().content_type(\"application/json; charset=utf-8\");\n";
            ss << "            resp.set_body(std::move(json_str));\n";
        }

        ss << "            return;\n";
        ss << "        }\n";
    }
    ss << "    } catch (const exception& e) {\n";
    ss << "        resp.set_status(400);\n";
    ss << "        resp.set_body(\"Bad Request: \" + string(e.what()));\n";
    ss << "        return;\n";
    ss << "    }\n";
    ss << "    return;\n";
    ss << "}\n\n";

    // Main handle method
    ss << "void Router::handle(HttpRequest& req, HttpResponse& resp) {\n";
    ss << "    string_view path = req.path();\n";
    ss << "    HttpRequest::Method method = req.method();\n\n";

    // Add filesystem route checking first
    if (!ast.filesystem_routes.empty()) {
        ss << "    // 0. Check filesystem routes first.\n";
        ss << "    for (const auto& fs_route : filesystem_routes_) {\n";
        ss << "        if (path == fs_route.url_prefix ||\n";
        ss << "            (path.size() > fs_route.url_prefix.size() &&\n";
        ss << "             path.compare(0, fs_route.url_prefix.size(), fs_route.url_prefix) == 0 &&\n";
        ss << "             path[fs_route.url_prefix.size()] == '/')) {\n";
        ss << "            serve_filesystem_file(fs_route, path, resp);\n";
        ss << "            return;\n";
        ss << "        }\n";
        ss << "    }\n\n";
    }

    ss << "    // 1. Static routes.\n";
    ss << "    auto static_it = static_routes_.find(string(path));\n";
    ss << "    if (static_it != static_routes_.end()) {\n";
    ss << "        auto method_it = static_it->second.find((int) method);\n";
    ss << "        if (method_it != static_it->second.end()) {\n";
    ss << "            dispatch_handler(*method_it->second, req, resp, {});\n";
    ss << "            return;\n";
    ss << "        }\n";
    ss << "    }\n\n";

    ss << "    // 2. Dynamic routes.\n";
    ss << "    vector<string> path_parts = split_path(path);\n";
    ss << "    auto dynamic_it = dynamic_routes_by_segment_count_.find(path_parts.size());\n";
    ss << "    if (dynamic_it != dynamic_routes_by_segment_count_.end()) {\n";
    ss << "        for (const auto* route : dynamic_it->second) {\n";
    ss << "            if (route->method != method) continue;\n";
    ss << "            unordered_map<string, string> params;\n";
    ss << "            if (match_segments(path_parts, route->segments, params)) {\n";
    ss << "                dispatch_handler(*route->handler, req, resp, params);\n";
    ss << "                return;\n";
    ss << "            }\n";
    ss << "        }\n";
    ss << "    }\n\n";

    ss << "    // 3. No match — 404.\n";
    ss << "    resp.set_status(404);\n";
    ss << "    resp.set_body(\"Not Found\");\n";
    ss << "}\n";

    return write_file(output_dir + "/router.generated.cpp", ss.str());
}

std::string CodeGenerator::get_cpp_type(const std::string& crdl_type) {
    static const std::unordered_map<std::string, std::string> type_map = {
        {"string", "std::string"},
        {"int", "int64_t"},
        {"uint", "uint64_t"},
        {"double", "double"},
        {"*", "std::string"}
    };

    auto it = type_map.find(crdl_type);
    return (it != type_map.end()) ? it->second : "std::string";
}

std::string CodeGenerator::generate_function_signature(const RouteInfo& route_info) {
    std::stringstream ss;

    // Determine return type based on explicit return_type or whether resp is used
    const std::string& return_type = route_info.handler_call.return_type;

    if (!return_type.empty()) {
        // Explicit return type specified
        if (return_type == "text" || return_type == "html") {
            ss << "std::string ";
        } else if (return_type == "json") {
            ss << "Json::Value ";
        }
    } else {
        // No explicit return type - use legacy behavior
        bool has_resp = std::find(route_info.handler_call.arguments.begin(),
                                 route_info.handler_call.arguments.end(), "resp")
                        != route_info.handler_call.arguments.end();

        if (has_resp) {
            ss << "void ";  // If resp is used, function handles response itself
        } else {
            ss << "std::string ";  // Otherwise, return string response
        }
    }

    ss << route_info.handler_call.function_name << "(";

    bool first = true;

    // Add handler call arguments with their types
    for (const auto& arg : route_info.handler_call.arguments) {
        if (!first) ss << ", ";
        first = false;

        if (arg == "req") {
            ss << "fox::http::HttpRequest& " << arg;
        } else if (arg == "resp") {
            ss << "fox::http::HttpResponse& " << arg;
        } else if (arg == "form") {
            ss << "std::unordered_map<std::string, std::string> " << arg;
        } else if (arg == "body") {
            ss << "std::string " << arg;
        } else if (arg == "json") {
            ss << "const Json::Value& " << arg;
        } else if (arg == "query") {
            ss << "const std::unordered_map<std::string, std::string>& " << arg;
        } else if (arg == "multipart") {
            ss << "const MultipartForm& " << arg;
        } else {
            // Find the parameter type
            auto param_it = std::find(route_info.parameter_names.begin(),
                                    route_info.parameter_names.end(), arg);
            if (param_it != route_info.parameter_names.end()) {
                size_t index = std::distance(route_info.parameter_names.begin(), param_it);
                std::string cpp_type = get_cpp_type(route_info.parameter_types[index]);
                ss << cpp_type << " " << arg;
            } else {
                ss << "std::string " << arg; // Fallback
            }
        }
    }

    ss << ")";
    return ss.str();
}

std::string CodeGenerator::generate_route_regex(const std::string& path_pattern) {
    // Convert CRDL path pattern like "/users/{int:id}/posts/{string:title}"
    // into a regex pattern like "^/users/([0-9]+)/posts/([^/]+)$"

    std::string regex_pattern = "^";

    size_t pos = 0;
    while (pos < path_pattern.length()) {
        if (path_pattern[pos] == '{') {
            // Find the end of the parameter
            size_t end_pos = path_pattern.find('}', pos);
            if (end_pos == std::string::npos) {
                // Malformed parameter, treat as literal
                regex_pattern += escape_regex_char(path_pattern[pos]);
                pos++;
                continue;
            }

            // Extract parameter definition like "int:id"
            std::string param_def = path_pattern.substr(pos + 1, end_pos - pos - 1);
            size_t colon_pos = param_def.find(':');

            if (colon_pos != std::string::npos) {
                std::string param_type = param_def.substr(0, colon_pos);

                // Generate appropriate regex for parameter type
                if (param_type == "int") {
                    regex_pattern += "([0-9]+)";  // Positive integers
                } else if (param_type == "uint") {
                    regex_pattern += "([0-9]+)";  // Positive integers (same as int for URL matching)
                } else if (param_type == "double") {
                    regex_pattern += "([0-9]*\\.?[0-9]+)";  // Decimal numbers
                } else if (param_type == "string") {
                    regex_pattern += "([^/]+)";  // Any non-slash characters
                } else if (param_type == "*") {
                    regex_pattern += "(.*)";  // Wildcard matches everything
                } else {
                    // Unknown type, treat as string
                    regex_pattern += "([^/]+)";
                }
            } else {
                // No colon found, treat as string parameter
                regex_pattern += "([^/]+)";
            }

            pos = end_pos + 1;
        } else {
            // Regular character, escape if necessary for regex
            regex_pattern += escape_regex_char(path_pattern[pos]);
            pos++;
        }
    }

    regex_pattern += "$";
    return regex_pattern;
}

std::string CodeGenerator::escape_regex_char(char c) {
    // Escape special regex characters
    switch (c) {
        case '.': return "\\.";
        case '^': return "\\^";
        case '$': return "\\$";
        case '*': return "\\*";
        case '+': return "\\+";
        case '?': return "\\?";
        case '[': return "\\[";
        case ']': return "\\]";
        case '(': return "\\(";
        case ')': return "\\)";
        case '{': return "\\{";
        case '}': return "\\}";
        case '|': return "\\|";
        case '\\': return "\\\\";
        default: return std::string(1, c);
    }
}

void CodeGenerator::collect_all_handlers(const RadixTreeNode* node,
                                       std::unordered_map<std::string, const RouteInfo*>& handlers) {
    // This is a placeholder - we'd need access to the actual tree structure
    // to implement this properly
}

bool CodeGenerator::write_file(const std::string& filepath, const std::string& content) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            error_reporter_.report_error(ErrorReporter::ErrorType::SEMANTIC_ERROR,
                                       SourceLocation(), "Cannot open file for writing: " + filepath);
            return false;
        }

        file << content;
        file.close();

        std::cout << "Generated: " << filepath << std::endl;
        return true;
    } catch (const std::exception& e) {
        error_reporter_.report_error(ErrorReporter::ErrorType::SEMANTIC_ERROR,
                                   SourceLocation(), "Error writing file " + filepath + ": " + e.what());
        return false;
    }
}

std::string CodeGenerator::escape_string_literal(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}