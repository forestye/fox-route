# fox-route

把 CRDL（一种简洁的路由 DSL）编译成 C++ `Router` 类，配合
[fox-http](https://github.com/forestye/fox-http) 使用。

## 做什么

写一个 `.crdl` 文件：

```crdl
GET  /users -> text get_users()
GET  /users/{int:id} -> show_user(resp, id)
POST /users -> json create_user(json)
GET  /files/{*} -> serve_file(req, resp)
FILESYSTEM /static -> ./public
```

跑 `fox-route -o build/ routes.crdl`，得到：

- `handlers.h` —— 你要实现的 handler 函数声明
- `handlers.cpp` —— 骨架实现（首次生成时），之后不覆盖
- `router.generated.h` / `router.generated.cpp` —— 一个 `Router` 类继承
  `fox::http::HttpHandler`，内部做路径匹配 + 参数转换 + 返回值序列化

Router 插进 fox-http：

```cpp
#include "fox-http/http_server.h"
#include "router.generated.h"

int main() {
    Router r;
    fox::http::HttpServer server(8080);
    server.set_handler(&r);
    return server.run();
}
```

---

## 为什么

- **业务代码不绑 HTTP 框架**：你写的 handler 函数签名由 `.crdl` 决定，比如
  `std::string get_users()`、`Json::Value create_user(const Json::Value&)`，
  完全不见 `HttpRequest` / `HttpResponse`，除非你显式在 CRDL 里要 `req` / `resp`。
- **类型安全**：URL 上的 `{int:id}` 在 C++ 侧就是 `int64_t id`，类型匹配由
  编译器检查，不是请求到了才 runtime parse。
- **零手写路由表**：静态路由 O(1) 哈希查找，动态路由按段数分桶线性匹配，
  都自动生成。
- **返回值自动处理**：`text` / `html` / `json` 返回类型自动包装成响应，
  Content-Type 和序列化都不用写。

---

## 依赖

- C++17 编译器
- CMake 3.10+
- 生成的代码依赖 fox-http（不直接依赖）

---

## 构建

```bash
git clone git@github.com:forestye/fox-route.git
cd fox-route
cmake -S . -B build
cmake --build build -j
# 产物：./build/fox-route, ./build/fox-route-func
# 可选安装：sudo cmake --install build
```

装好后（或把 build 目录加入 PATH）：

```bash
fox-route --help
```

---

## CRDL 语法

### 基本形式

```
<METHOD>   <PATH>  ->  [<RETURN_TYPE>]  <HANDLER_CALL>
FILESYSTEM <PATH>  ->  <LOCAL_DIR>
```

`<METHOD>`：`GET` / `POST` / `PUT` / `DELETE` / `PATCH` / `HEAD` / `OPTIONS` / `ANY`
（`ANY` 会展开成所有 HTTP 方法的注册）。

### 路径模式

| 形式 | 含义 | 例子 |
|---|---|---|
| 纯文本 | 静态路径 | `/users` |
| `{<type>:<name>}` | 类型化参数段 | `/users/{int:id}` |
| `{*}` | 尾部通配（匹配剩余任意后缀） | `/files/{*}` |

支持的参数类型（与生成的 C++ 类型对应）：

| CRDL 类型 | C++ 类型 |
|---|---|
| `string` | `std::string` |
| `int` | `int64_t` |
| `uint` | `uint64_t` |
| `double` | `double` |
| `*` | `std::string`（通配后缀） |

### Handler 参数（Framework Objects）

Handler 可以接 URL 参数，也可以接下列"框架参数"——生成器会帮你准备好：

| 名字 | C++ 类型 | 作用 |
|---|---|---|
| `req` | `fox::http::HttpRequest&` | 原始请求对象（读 header、路径、method 等） |
| `resp` | `fox::http::HttpResponse&` | 原始响应对象（手动控制响应） |
| `query` | `const std::unordered_map<std::string, std::string>&` | URL query 参数，自动解码 |
| `form` | `std::unordered_map<std::string, std::string>` | `application/x-www-form-urlencoded` 解析结果 |
| `body` | `std::string` | 原始请求体 |
| `json` | `const Json::Value&` | 自动解析好的 JsonCpp 对象（体不合法时自动返回 400） |
| `multipart` | `const MultipartForm&` | 解析好的 multipart/form-data，支持文件上传 |

组合用法示例：

```crdl
GET  /health -> text ok()
GET  /user/{int:id} -> show_user(resp, id)                   # 路径参数 + resp
POST /api/user/{int:id} -> json update(id, json)             # 路径 + JSON 体 + JSON 返回
POST /login -> handle_login(form, resp)                      # 表单 + resp
GET  /search -> json search(query)                           # query 参数
POST /upload -> text handle_upload(multipart)                # 文件上传
```

约束：`json` / `form` / `body` / `multipart` 互斥（都从请求体读），同一个
handler 只能用其一。

### 返回值类型

| CRDL | C++ 返回 | Content-Type | 说明 |
|---|---|---|---|
| `text` | `std::string` | `text/plain; charset=utf-8` | 纯文本 |
| `html` | `std::string` | `text/html; charset=utf-8` | HTML |
| `json` | `Json::Value` | `application/json; charset=utf-8` | 自动序列化 |
| （不写） | `void` | — | 你自己往 `resp` 写 |

### FILESYSTEM 静态文件

```crdl
FILESYSTEM /static -> /var/www/static
FILESYSTEM /images -> ./public/images
```

生成器会在 Router 里内嵌一个简单文件服务器：URL 前缀对应本地目录，按扩展名
设置 MIME（html/css/js/json/txt/png/jpg/gif/svg/ico 等），拒绝 `..` 路径穿越。

> 当前实现不支持 Last-Modified / If-Modified-Since / Range。足够用静态资源
> demo，生产跑大文件建议前面加 nginx。

### 完整示例

见 [`example.crdl`](example.crdl)。

---

## 用法

```bash
# 生成 .h / .cpp 到当前目录
fox-route routes.crdl

# 指定输出目录
fox-route -o build/generated routes.crdl

# 只检查语法，不落盘
fox-route --check routes.crdl

# 查看全部选项
fox-route --help
```

### `fox-route-func` 辅助工具

`fox-route-func` 从 CRDL 文件里抽取某个 handler 的 C++ 函数签名，方便喂给
[`fox-page`](https://github.com/forestye/fox-page) 的 `--func` 参数：

```bash
fox-route-func routes.crdl show_user
# → void show_user(fox::http::HttpResponse& resp, int64_t id)
```

配合 fox-page：

```bash
fox-page user.html -o gen/ --func "$(fox-route-func routes.crdl user)"
```

这样模板渲染函数和 CRDL 声明的签名完全一致，路由直接调用即可。

---

## 生成代码示例

输入：

```crdl
GET /hello -> text hello()
GET /user/{int:id} -> show_user(resp, id)
POST /echo -> text echo(body)
```

生成的 `handlers.h`（节选）：

```cpp
#pragma once
#include "fox-http/http_request.h"
#include "fox-http/http_response.h"
#include <cstdint>
#include <string>

std::string hello();
void show_user(fox::http::HttpResponse& resp, int64_t id);
std::string echo(std::string body);
```

生成的 `Router`（节选）：

```cpp
class Router : public fox::http::HttpHandler {
public:
    Router();
    void handle(fox::http::HttpRequest& req,
                fox::http::HttpResponse& resp) override;
    // 内部：static_routes_ + dynamic_routes_by_segment_count_
};
```

`Router::handle` 的逻辑：

1. 如果 URL 前缀命中某个 `FILESYSTEM` 映射 → 服务本地文件，返回
2. 静态路由精确匹配 → 直接分派
3. 动态路由按段数分桶线性匹配 → 类型检查、转换、分派
4. 以上都没中 → 404

---

## 和 fox-http / fox-page 的集成

典型 CMake（在下游应用的 `CMakeLists.txt` 里）：

```cmake
# 找工具链
find_program(FOX_ROUTE      NAMES fox-route      HINTS ../fox-route/build)
find_program(FOX_ROUTE_FUNC NAMES fox-route-func HINTS ../fox-route/build)
find_program(FOX_PAGE       NAMES fox-page       HINTS ../fox-page/build)

# 生成 handlers.h / router.generated.{h,cpp}
set(ROUTES ${CMAKE_SOURCE_DIR}/routes.crdl)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/handlers.h
           ${CMAKE_BINARY_DIR}/router.generated.cpp
           ${CMAKE_BINARY_DIR}/router.generated.h
    DEPENDS ${ROUTES}
    COMMAND ${FOX_ROUTE} -o ${CMAKE_BINARY_DIR} ${ROUTES}
)

# 生成页面（每个 .html → 一个 .cpp），签名从 CRDL 抽
foreach(page index user profile)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/pages/${page}.cpp
        DEPENDS ${CMAKE_SOURCE_DIR}/pages/${page}.html ${ROUTES}
        COMMAND sh -c "${FOX_PAGE} ${CMAKE_SOURCE_DIR}/pages/${page}.html \
                      -o ${CMAKE_BINARY_DIR}/pages \
                      --func \"$(${FOX_ROUTE_FUNC} ${ROUTES} ${page})\""
    )
    list(APPEND PAGE_CPP ${CMAKE_BINARY_DIR}/pages/${page}.cpp)
endforeach()

add_executable(my_app
    main.cpp
    handlers.cpp
    ${CMAKE_BINARY_DIR}/router.generated.cpp
    ${PAGE_CPP}
)
target_include_directories(my_app PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(my_app PRIVATE fox-http::fox-http)
```

---

## 项目结构

```
fox-route/
├── src/
│   ├── main.cpp                  # CLI 入口
│   ├── lexer.{h,cpp}             # 词法分析
│   ├── parser.{h,cpp}            # 语法分析（.crdl → AST）
│   ├── ast.{h,cpp}               # AST 结构
│   ├── semantic_analyzer.{h,cpp} # 语义检查 + RadixTree 构建
│   ├── radix_tree.{h,cpp}        # 路由冲突检测
│   ├── code_generator.{h,cpp}    # 生成 C++ 代码
│   ├── crdl_func.cpp             # fox-route-func 工具入口
│   └── error_reporter.{h,cpp}
├── tests/                        # gtest 单元测试
├── example.crdl                  # 完整语法示例
├── spec.md                       # CRDL 语言规范
└── CMakeLists.txt
```

---

## 姊妹项目

- [fox-http](https://github.com/forestye/fox-http) —— 运行时 HTTP 服务器库
- [fox-page](https://github.com/forestye/fox-page) —— HTML 模板 → C++
  代码生成器；和 fox-route 共享 handler 签名
