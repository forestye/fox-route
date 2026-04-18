## CRDL 翻译器详细设计文档

**版本**: 2.0 (整合版)

### 1\. 背景与目标

传统的 Web 框架常导致业务逻辑与 Web 层强耦合。本设计旨在通过 CRDL (C++ Route Definition Language) 翻译器，将路由定义文件自动生成为高性能、类型安全的 C++ 路由分发代码，达成以下目标：

1.  **业务解耦**：业务处理器函数保持纯粹，不依赖特定的 Web 框架实现，仅显式声明其需要的参数。
2.  **显式契约**：处理器函数签名由 `.crdl` 文件唯一决定，形成编译期强校验的契约。
3.  **高性能**：生成的 C++ 代码利用高效的 Radix Tree (基数树) 算法进行路由匹配，实现零运行时抽象开销。
4.  **类型安全**：在编译时完成 URL 参数到函数参数的类型检查与映射，避免运行时错误。

### 2\. CRDL 语言规范

每一行非注释、非空行都定义一条路由规则，基本结构如下：

```crdl
<HTTP_METHOD> <URL_PATH> -> <HANDLER_CALL>
```

  * **`<HTTP_METHOD>`**: 支持 `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`, `ANY`。
  * **`<URL_PATH>`**: 以 `/` 开头的路径模式。
      * **静态段**: 如 `/users`, `/about`。
      * **动态段 (参数)**: 格式为 `{类型:变量名}`。类型可选，默认为 `string`。
  * **`<HANDLER_CALL>`**: `函数名(参数列表)` 的形式。
      * **框架对象**: 可按需注入 `req` (`const Request&`) 和 `resp` (`Response&`)。
      * **路径参数**: 名称必须与路径中定义的变量名匹配。

**内置类型映射表**：

| CRDL 类型 | C++ 类型 |
| :--- | :--- |
| `string` | `std::string` |
| `int` | `int64_t` |
| `uint` | `uint64_t` |
| `double` | `double` |
| `*` (通配符) | `std::string` |

**示例**：

```crdl
# routes.crdl

# 接收框架对象和一个路径参数
GET /users/{int:id} -> show_user(resp, id)

# 接收两个路径参数和请求对象
GET /users/{int:userId}/posts/{int:postId} -> get_post(req, userId, postId)

# 不依赖框架对象
GET /version -> get_version()
```

### 3\. 翻译器架构

翻译器遵循经典的四阶段编译器设计。

1.  **词法分析 (Lexing)**

      * **输入**: `routes.crdl` 文件内容。
      * **输出**: Token 序列 (如 `METHOD`, `PATH_LITERAL`, `IDENTIFIER`, `ARROW` 等)。

2.  **语法分析 (Parsing)**

      * **输入**: Token 序列。
      * **输出**: 抽象语法树 (AST)。
      * **核心 AST 节点定义**：
        ```cpp
        struct PathParameterNode { std::string type; std::string name; };
        struct HandlerCallNode { std::string function_name; std::vector<std::string> arguments; };
        struct RouteRuleNode {
            std::string http_method;
            std::vector<std::variant<std::string, PathParameterNode>> path_segments;
            HandlerCallNode handler_call;
        };
        struct AstRoot { std::vector<RouteRuleNode> rules; };
        ```

3.  **语义分析与中间表示 (Semantic Analysis & IR)**

      * **输入**: AST。
      * **输出**: 一个经过验证和注解的 **Radix Tree** (作为中间表示 IR)。
      * **职责**:
          * **构建 Radix Tree**：遍历 AST 并将所有路由规则插入 Radix Tree。
          * **语义检查**：执行路由冲突检测（如 `GET /users/{id}` vs `GET /users/{slug}`），并验证处理器函数调用的参数必须来自路径定义或为框架对象 (`req`, `resp`)。
          * **信息聚合**: 将验证后的处理器函数名、参数类型、参数顺序等完整信息附加到 Radix Tree 的叶子节点上。

4.  **代码生成 (Code Generation)**

      * **输入**: 经过注解的 Radix Tree IR。
      * **输出**: `router.generated.h`, `router.generated.cpp`, `handlers.h`。

### 4\. 代码生成规范

#### 4.1. 处理器声明 (`handlers.h`)

自动生成所有处理器函数的 C++ 声明，作为开发者实现的契约。

```cpp
#pragma once
#include <string>
#include <cstdint>
#include "framework/http_types.h" // 假设框架类型在此定义

// 业务开发者需要实现的函数声明
int show_user(Response& resp, int64_t id);
int get_post(const Request& req, int64_t userId, int64_t postId);
std::string get_version();
```

#### 4.2. 路由器类声明 (`router.generated.h`)

生成 `Router` 类的声明，提供统一的路由分发接口。

```cpp
#pragma once
#include "framework/http_types.h"

class Router {
public:
    Router();
    ~Router();

    void dispatch(const Request& req, Response& resp);

private:
    struct Node; // Radix Tree 节点前向声明
    Node* root_;
};
```

#### 4.3. 路由器实现 (`router.generated.cpp`)

生成 `Router` 类的实现，包含所有路由逻辑。

  * **构造函数**: 包含在编译时静态构建 Radix Tree 的代码。
  * **`dispatch` 方法**:
    1.  解析请求的 URL 路径，并遍历 Radix Tree 进行高效匹配。
    2.  在匹配过程中，从 URL 中捕获动态段的值。
    3.  匹配成功后，查找对应 HTTP 方法的处理器。
    4.  调用处理器前，**生成类型转换代码** (如 `std::stoll`)，并用 `try-catch` 块包裹以处理转换失败的情况，若失败则自动返回 400 Bad Request。
    5.  根据 `.crdl` 中定义的顺序，调用 `handlers.h` 中对应的业务函数。
    6.  若路径或方法未匹配，则返回 404 Not Found。

### 5\. 错误处理

翻译器在编译期必须能清晰地报告错误。

  * **错误类型**:
      * **语法错误**: 不符合 CRDL 语法的行。
      * **语义错误**: 路由冲突、处理器参数未在路径中定义、未知的参数类型等。
  * **错误格式**: 错误报告应包含文件名、行号、列号及详细原因，以方便开发者定位。
    ```
    routes.crdl:12:45: error: undefined path variable 'user_id' in handler parameters
    GET /users/{int:id} -> show_user(resp, user_id)
                                             ^~~~~~~
    ```

### 6\. 命令行接口

提供一个简单的命令行工具来执行翻译过程。

  * **基本用法**：
    ```bash
    crdl_compiler [options] <input.crdl>
    ```
  * **选项**：

| 选项 | 说明 |
| :--- | :--- |
| `-o, --output <dir>` | 指定 C++ 代码的输出目录（默认：当前目录） |
| `--check` | 只检查 CRDL 文件语法和语义，不生成代码 |
| `-h, --help` | 显示帮助信息 |

### 7\. 项目结构与依赖

  * **推荐项目结构**：
    ```
    crdl_compiler/
    ├── src/         # 翻译器源码
    ├── tests/       # 测试代码
    └── CMakeLists.txt
    ```
  * **依赖**：
      * C++17 或更高版本。
      * 标准库 (如 `std::filesystem`, `std::variant` 等)。
      * 构建工具：CMake 3.10+。