# CRDL 翻译器 (C++ Route Definition Language Compiler)

这是一个将路由定义文件 (`.crdl`) 自动生成为高性能 C++ 路由分发代码的翻译器。

## 特性

- **业务解耦**: 业务处理器函数不依赖特定的 Web 框架
- **显式契约**: 处理器函数签名由 `.crdl` 文件唯一决定
- **高性能路由**:
  - 静态路由：O(1) 哈希表查找
  - 动态路由：O(m) 段匹配（m 为路径段数）
  - 使用 Radix Tree 进行路由冲突检测
- **类型安全**: 编译时完成 URL 参数到函数参数的类型检查
- **返回值类型**: 支持 `string` 和 `json` 返回类型，自动处理响应序列化
- **表单解析**: 自动解析 `application/x-www-form-urlencoded` 表单数据
- **静态文件服务**: 支持 FILESYSTEM 路由，可使用绝对或相对路径
- **错误报告**: 详细的语法和语义错误报告

## 构建和安装

需要 C++17 和 CMake 3.10+:

### 快速开始（推荐）
```bash
# 构建项目
make build

# 运行测试
make test

# 安装到系统 (需要 root 权限)
sudo make install

# 验证安装
crdl_compiler --help
```

### 手动构建
```bash
mkdir build
cd build
cmake ..
make
```

### 安装选项
```bash
# 方法1: 使用 Makefile (推荐)
sudo make install

# 方法2: 使用 CMake
cd build
sudo make install

# 方法3: 指定自定义安装前缀
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make
sudo make install
```

### 其他命令
```bash
make clean           # 清理构建文件
make test-install    # 测试安装到临时目录
sudo make uninstall  # 从系统卸载
make help            # 显示所有可用命令
```

## 使用方法

```bash
# 编译 CRDL 文件并生成 C++ 代码
./crdl_compiler routes.crdl

# 指定输出目录
./crdl_compiler -o ./generated routes.crdl

# 只检查语法，不生成代码
./crdl_compiler --check routes.crdl

# 显示帮助
./crdl_compiler --help
```

## CRDL 语法

### 基本语法

```crdl
# 普通路由
<HTTP_METHOD> <URL_PATH> -> <HANDLER_CALL>

# 带返回值类型的路由
<HTTP_METHOD> <URL_PATH> -> <RETURN_TYPE> <HANDLER_CALL>

# 静态文件路由
FILESYSTEM <URL_PREFIX> -> <FILESYSTEM_PATH>
```

### 示例

```crdl
# 静态路由
GET /users -> get_users()
POST /users -> create_user(req)

# 参数化路由
GET /users/{int:id} -> show_user(resp, id)
GET /users/{string:username} -> show_user_by_name(username)

# 多参数路由
GET /users/{int:userId}/posts/{int:postId} -> get_post(req, userId, postId)

# 通配符路由
GET /files/{*} -> serve_static_file(req, resp)

# 带返回值类型的路由
GET /api/message -> string get_message()
GET /api/user/{int:id} -> json get_user(id)

# 静态文件服务
FILESYSTEM /static -> /var/www/static
FILESYSTEM /images -> ../images
```

### 支持的参数类型

| CRDL 类型 | C++ 类型 |
|-----------|----------|
| `string`  | `std::string` |
| `int`     | `int64_t` |
| `uint`    | `uint64_t` |
| `double`  | `double` |
| `*`       | `std::string` (通配符) |

### 预定义参数

处理器函数可以接收以下预定义参数（框架对象）：

| 参数名 | C++ 类型 | 说明 |
|--------|----------|------|
| `req`  | `photon::net::http::Request&` | HTTP 请求对象，包含请求的所有信息 |
| `resp` | `photon::net::http::Response&` | HTTP 响应对象，用于手动控制响应 |
| `query` | `const std::unordered_map<std::string, std::string>&` | URL 查询参数，自动解析（如 `?id=123&name=test`） |
| `form` | `std::unordered_map<std::string, std::string>` | 表单数据，自动解析 `application/x-www-form-urlencoded` |
| `body` | `std::string` | 原始请求体数据（字符串形式） |
| `json` | `const Json::Value&` | 自动解析的 JSON 数据（使用 JsonCpp 库） |
| `multipart` | `const MultipartForm&` | 自动解析的 `multipart/form-data`（支持文件上传） |

**示例：**
```crdl
# 使用请求对象获取请求信息
GET /info -> get_info(req)

# 使用响应对象手动控制响应（返回 void）
GET /custom -> custom_handler(resp)

# 使用查询参数（如 GET /getobj?id=123&name=test）
GET /getobj -> get_object(query)

# 使用表单数据处理 POST 请求
POST /login -> handle_login(form)

# 使用原始请求体（适用于 JSON、XML 等格式）
POST /api/raw -> process_raw(body)

# 使用自动解析的 JSON 数据
POST /api/user -> create_user(json)

# 使用 multipart/form-data（文件上传）
POST /upload -> handle_upload(multipart)

# 组合使用：body + 路径参数
POST /api/data/{int:id} -> process_data(id, body)

# 组合使用：json + 路径参数
POST /api/item/{int:id} -> update_item(id, json)

# 组合使用：query + 路径参数
GET /user/{int:id} -> get_user_details(id, query)

# 组合使用：表单 + 路径参数
POST /user/{int:id}/update -> update_user(id, form)

# 字符串返回值 + body
POST /api/echo -> string echo_body(body)

# 完整组合：请求 + 响应 + 表单
POST /upload -> handle_upload(req, resp, form)

# JSON 参数 + JSON 返回值
POST /api/process -> json process_json_data(json)
```

**MultipartForm 数据结构：**
```cpp
struct MultipartFile {
    std::string field_name;      // 表单字段名
    std::string filename;         // 原始文件名
    std::string content_type;     // MIME 类型
    std::string data;             // 文件内容
};

struct MultipartForm {
    std::unordered_map<std::string, std::string> fields;  // 普通表单字段
    std::vector<MultipartFile> files;                      // 上传的文件
};
```

**注意事项：**
- `query` 参数会自动从 URL 中解析查询字符串（`?key1=value1&key2=value2`）
- `query` 参数可以与路径参数、`json`、`body` 等任意组合使用
- `json` 参数会自动解析请求体为 JSON 对象（使用 JsonCpp 库）
- 如果请求体不是有效的 JSON，会自动返回 400 错误
- `multipart` 参数会自动解析 `multipart/form-data` 格式，支持文件上传
- 如果缺少 boundary 或格式错误，会自动返回 400 错误
- `json`、`form`、`body`、`multipart` 参数不能同时使用（它们都需要读取请求体）

### 返回值类型

处理器函数可以指定返回值类型，编译器会自动处理响应的构建：

| 返回类型 | C++ 返回类型 | Content-Type | 说明 |
|----------|--------------|--------------|------|
| `text` | `std::string` | `text/plain; charset=utf-8` | 返回纯文本 |
| `html` | `std::string` | `text/html; charset=utf-8` | 返回 HTML 页面 |
| `json`   | `Json::Value` | `application/json; charset=utf-8` | 返回 JSON（自动序列化） |
| 无类型   | `void` | - | 函数自己处理响应（向后兼容） |

**语法：**
```crdl
<HTTP_METHOD> <URL_PATH> -> <RETURN_TYPE> <HANDLER_CALL>
```

**示例：**
```crdl
# 返回纯文本（自动设置 Content-Type: text/plain）
GET /api/message -> text get_message()

# 返回 HTML 页面（自动设置 Content-Type: text/html）
GET /page -> html render_page()

# 返回 JSON 对象（自动序列化并设置 Content-Type: application/json）
GET /api/user/{int:id} -> json get_user(id)

# 文本返回值 + 路径参数
GET /api/greet/{string:name} -> text greet(name)

# HTML 返回值 + 路径参数
GET /hello/{string:name} -> html greet_user(name)

# JSON 返回值 + 请求对象
GET /api/data -> json get_data(req)

# 不指定返回类型（手动控制响应，向后兼容）
GET /hello -> hello(resp)
```

**生成的函数签名：**
```cpp
// handlers.h
std::string get_message();           // text 返回类型
std::string render_page();           // html 返回类型
Json::Value get_user(int64_t id);    // json 返回类型
std::string greet(std::string name); // text 返回类型
std::string greet_user(std::string name); // html 返回类型
Json::Value get_data(photon::net::http::Request& req); // json 返回类型
void hello(photon::net::http::Response& resp); // void（无返回类型）
```

### FILESYSTEM 路由

支持静态文件服务，可以使用绝对路径或相对路径：

**语法：**
```crdl
FILESYSTEM <URL_PREFIX> -> <FILESYSTEM_PATH>
```

**示例：**
```crdl
# 绝对路径
FILESYSTEM /static -> /var/www/static

# 相对路径（相对于程序运行目录）
FILESYSTEM /images -> ../images
FILESYSTEM /assets -> ./assets
FILESYSTEM /files -> data/files
```

**说明：**
- URL_PREFIX：访问路径前缀，必须以 `/` 开头
- FILESYSTEM_PATH：文件系统路径，支持绝对路径和相对路径
- 使用 Photon 的 `fs_handler` 处理静态文件请求

## 生成的文件

编译器会生成三个文件：

1. **`handlers.h`**: 处理器函数声明
2. **`router.generated.h`**: Router 类声明
3. **`router.generated.cpp`**: Router 类实现

## 运行测试

```bash
cd build
make test
```

## 项目结构

```
crdl_compiler/
├── src/                    # 源代码
│   ├── main.cpp           # 主程序
│   ├── lexer.h/cpp        # 词法分析器
│   ├── parser.h/cpp       # 语法分析器
│   ├── ast.h/cpp          # AST 定义
│   ├── radix_tree.h/cpp   # Radix Tree 实现
│   ├── semantic_analyzer.h/cpp  # 语义分析器
│   ├── code_generator.h/cpp     # 代码生成器
│   └── error_reporter.h/cpp     # 错误报告器
├── tests/                 # 测试
├── example.crdl          # CRDL 语法示例
├── spec.md               # 设计文档
└── CMakeLists.txt        # 构建配置
```

## 示例使用

参见 `example.crdl` 文件，包含了各种路由定义的示例。