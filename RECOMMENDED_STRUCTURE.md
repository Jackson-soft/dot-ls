# dot-ls 推荐目录结构

## 📋 目录结构总览

```
dot-ls/
├── include/                          # 头文件目录
│   ├── protocol/                     # 协议层 (新增)
│   │   ├── lsp/                     # LSP 协议定义
│   │   │   ├── basic.hpp           # 基础类型: Position, Range, Location 等
│   │   │   ├── lifecycle.hpp       # 生命周期协议: Initialize, Shutdown
│   │   │   ├── document.hpp        # 文档同步协议: DidOpen, DidChange, DidSave
│   │   │   ├── language.hpp        # 语言特性协议: Completion, Hover, Definition
│   │   │   ├── workspace.hpp       # 工作区协议
│   │   │   └── window.hpp          # 窗口协议
│   │   └── jsonrpc/                # JSON-RPC 协议 (可选，如果不用 uranus)
│   │       ├── request.hpp
│   │       ├── response.hpp
│   │       └── notification.hpp
│   │
│   ├── domain/                       # 领域层
│   │   ├── entity/                  # 实体 (有身份标识)
│   │   │   ├── document.hpp        # 文档实体 (核心)
│   │   │   ├── symbol.hpp          # 符号实体
│   │   │   ├── workspace.hpp       # 工作区聚合根
│   │   │   └── diagnostic.hpp      # 诊断信息
│   │   │
│   │   ├── value_object/           # 值对象 (新增)
│   │   │   ├── position.hpp        # 领域位置 (区别于 LSP Position)
│   │   │   ├── range.hpp           # 领域范围
│   │   │   ├── text_edit.hpp       # 文本编辑
│   │   │   └── completion_item.hpp # 补全项
│   │   │
│   │   ├── service/                # 领域服务 (跨实体的业务逻辑)
│   │   │   ├── completion_service.hpp    # 代码补全服务
│   │   │   ├── symbol_resolver.hpp       # 符号解析服务
│   │   │   ├── rename_service.hpp        # 重命名服务
│   │   │   ├── diagnostic_service.hpp    # 诊断服务
│   │   │   └── navigation_service.hpp    # 导航服务 (跳转定义等)
│   │   │
│   │   ├── repository/             # 仓储接口 (新增)
│   │   │   ├── document_repository.hpp   # 文档仓储接口
│   │   │   ├── symbol_repository.hpp     # 符号仓储接口
│   │   │   └── workspace_repository.hpp  # 工作区仓储接口
│   │   │
│   │   └── event/                  # 领域事件 (可选，推荐)
│   │       ├── document_events.hpp       # 文档事件
│   │       ├── symbol_events.hpp         # 符号事件
│   │       └── event_bus.hpp             # 事件总线
│   │
│   ├── application/                 # 应用层 (重命名 app -> application)
│   │   ├── use_case/               # 用例 (新增)
│   │   │   ├── lifecycle/          # 生命周期用例
│   │   │   │   ├── initialize_use_case.hpp
│   │   │   │   └── shutdown_use_case.hpp
│   │   │   ├── document/           # 文档用例
│   │   │   │   ├── open_document_use_case.hpp
│   │   │   │   ├── close_document_use_case.hpp
│   │   │   │   ├── change_document_use_case.hpp
│   │   │   │   └── save_document_use_case.hpp
│   │   │   └── language/           # 语言特性用例
│   │   │       ├── completion_use_case.hpp
│   │   │       ├── rename_use_case.hpp
│   │   │       ├── goto_definition_use_case.hpp
│   │   │       └── hover_use_case.hpp
│   │   │
│   │   ├── mapper/                 # 映射器 (新增)
│   │   │   ├── document_mapper.hpp       # 领域 <-> LSP 协议转换
│   │   │   ├── symbol_mapper.hpp
│   │   │   └── diagnostic_mapper.hpp
│   │   │
│   │   ├── dto/                    # 数据传输对象 (可选)
│   │   │   └── command_dto.hpp
│   │   │
│   │   └── service/                # 应用服务 (协调器)
│   │       └── application_service.hpp   # 主应用服务
│   │
│   ├── infrastructure/              # 基础设施层 (重命名 infra -> infrastructure)
│   │   ├── transport/              # 传输层 (重命名 entity/session)
│   │   │   ├── session.hpp         # 会话接口
│   │   │   ├── stdio_session.hpp   # 标准 I/O 实现
│   │   │   └── socket_session.hpp  # Socket 实现 (未来扩展)
│   │   │
│   │   ├── persistence/            # 持久化 (新增)
│   │   │   ├── memory_document_repository.hpp    # 内存实现
│   │   │   ├── memory_symbol_repository.hpp
│   │   │   └── memory_workspace_repository.hpp
│   │   │
│   │   ├── parser/                 # 解析器 (重命名 treesitter)
│   │   │   ├── ast.hpp             # AST 定义
│   │   │   ├── parser.hpp          # 解析器接口
│   │   │   ├── dot_parser.hpp      # DOT 语言解析器
│   │   │   └── tree_sitter_adapter.hpp  # Tree-sitter 适配器
│   │   │
│   │   ├── logging/                # 日志 (新增)
│   │   │   ├── logger.hpp
│   │   │   └── log_config.hpp
│   │   │
│   │   ├── config/                 # 配置 (重命名 boot)
│   │   │   ├── app_config.hpp      # 应用配置
│   │   │   ├── server_info.hpp     # 服务器信息
│   │   │   └── bootstrap.hpp       # 启动配置
│   │   │
│   │   └── common/                 # 通用工具
│   │       ├── ring_buffer.hpp
│   │       └── thread_pool.hpp     # 线程池 (可选)
│   │
│   └── presentation/                # 表示层 (重命名 server)
│       ├── handler/                # 请求处理器 (新增)
│       │   ├── lifecycle_handler.hpp     # 生命周期处理
│       │   ├── document_handler.hpp      # 文档同步处理
│       │   └── language_handler.hpp      # 语言特性处理
│       │
│       ├── router/                 # 路由 (新增)
│       │   └── message_router.hpp        # 消息路由
│       │
│       └── server.hpp              # LSP 服务器主类
│
├── src/                             # 源文件目录
│   ├── domain/
│   │   ├── entity/
│   │   │   ├── document.cpp
│   │   │   ├── symbol.cpp
│   │   │   └── workspace.cpp
│   │   └── service/
│   │       ├── completion_service.cpp
│   │       ├── symbol_resolver.cpp
│   │       └── rename_service.cpp
│   │
│   ├── application/
│   │   ├── use_case/
│   │   │   ├── lifecycle/
│   │   │   │   └── initialize_use_case.cpp
│   │   │   ├── document/
│   │   │   │   ├── open_document_use_case.cpp
│   │   │   │   └── change_document_use_case.cpp
│   │   │   └── language/
│   │   │       └── completion_use_case.cpp
│   │   └── mapper/
│   │       ├── document_mapper.cpp
│   │       └── symbol_mapper.cpp
│   │
│   ├── infrastructure/
│   │   ├── transport/
│   │   │   └── stdio_session.cpp
│   │   ├── persistence/
│   │   │   ├── memory_document_repository.cpp
│   │   │   └── memory_symbol_repository.cpp
│   │   ├── parser/
│   │   │   └── dot_parser.cpp
│   │   └── config/
│   │       └── bootstrap.cpp
│   │
│   ├── presentation/
│   │   ├── handler/
│   │   │   ├── lifecycle_handler.cpp
│   │   │   ├── document_handler.cpp
│   │   │   └── language_handler.cpp
│   │   └── server.cpp
│   │
│   └── main.cpp                    # 程序入口
│
├── test/                           # 测试目录
│   ├── unit/                       # 单元测试
│   │   ├── domain/
│   │   │   ├── entity/
│   │   │   │   ├── document_test.cpp
│   │   │   │   └── symbol_test.cpp
│   │   │   └── service/
│   │   │       └── completion_service_test.cpp
│   │   ├── application/
│   │   │   └── use_case/
│   │   │       └── completion_use_case_test.cpp
│   │   └── infrastructure/
│   │       └── parser/
│   │           └── dot_parser_test.cpp
│   │
│   ├── integration/                # 集成测试
│   │   ├── lsp_integration_test.cpp
│   │   └── end_to_end_test.cpp
│   │
│   └── fixtures/                   # 测试数据
│       ├── sample.dot
│       └── test_workspace/
│
├── client/                         # LSP 客户端测试
│   ├── client.lua
│   └── run_test.sh
│
├── third_party/                    # 第三方库
│   └── uranus/
│
├── docs/                           # 文档 (新增)
│   ├── architecture/
│   │   ├── ddd_design.md
│   │   ├── layer_design.md
│   │   └── data_flow.md
│   ├── api/
│   │   └── lsp_protocol.md
│   └── development/
│       ├── setup.md
│       └── contributing.md
│
├── CMakeLists.txt
├── conanfile.txt
├── Makefile
└── README.org
```

## 🔑 关键改进说明

### 1. **新增 `protocol/` 层**

**目的**：分离协议定义和领域模型

```cpp
// include/protocol/lsp/basic.hpp
namespace protocol::lsp {
    struct Position {  // LSP 协议的 Position
        uint64_t line;
        uint64_t character;
    };
}

// include/domain/value_object/position.hpp
namespace domain {
    class Position {  // 领域的 Position，可能有不同的行为
        size_t line;
        size_t column;
        
        auto ToLsp() const -> protocol::lsp::Position;
        static auto FromLsp(const protocol::lsp::Position&) -> Position;
    };
}
```

**迁移路径**：

- 将 `domain/model/basic.hpp` 中的 LSP 协议结构移到 `protocol/lsp/basic.hpp`
- 将 `domain/model/lifecycle.hpp` 移到 `protocol/lsp/lifecycle.hpp`
- 将 `domain/model/document.hpp` 移到 `protocol/lsp/document.hpp`
- 保留真正的领域概念在 `domain/` 下

---

### 2. **重组 `domain/` 层**

#### **entity/** - 领域实体

真正有业务逻辑的对象：

```cpp
// include/domain/entity/document.hpp
namespace domain {
class Document {
private:
    std::string uri_;           // 身份标识
    std::string content_;
    int version_;
    std::unique_ptr<AST> ast_;
    std::vector<Diagnostic> diagnostics_;

public:
    Document(std::string uri, std::string content, int version);
    
    // 领域行为
    void UpdateContent(const std::string& newContent, int newVersion);
    auto Parse() -> void;
    auto FindSymbolAt(const Position& pos) const -> std::optional<Symbol>;
    auto GetDiagnostics() const -> const std::vector<Diagnostic>&;
    auto GetCompletionsAt(const Position& pos) const -> std::vector<CompletionItem>;
    
    // Getters
    auto Uri() const -> const std::string& { return uri_; }
    auto Content() const -> const std::string& { return content_; }
    auto Version() const -> int { return version_; }
};
}
```

#### **value_object/** - 值对象

不可变、无身份标识的对象：

```cpp
// include/domain/value_object/completion_item.hpp
namespace domain {
struct CompletionItem {
    std::string label;
    std::string detail;
    std::string documentation;
    CompletionKind kind;
    
    auto operator==(const CompletionItem&) const -> bool = default;
};
}
```

#### **repository/** - 仓储接口

抽象数据访问：

```cpp
// include/domain/repository/document_repository.hpp
namespace domain {
class IDocumentRepository {
public:
    virtual ~IDocumentRepository() = default;
    
    virtual auto FindByUri(const std::string& uri) -> std::optional<Document> = 0;
    virtual auto FindAll() -> std::vector<Document> = 0;
    virtual void Save(const Document& doc) -> void = 0;
    virtual void Remove(const std::string& uri) -> void = 0;
};
}
```

#### **service/** - 领域服务

跨实体的复杂业务逻辑：

```cpp
// include/domain/service/completion_service.hpp
namespace domain {
class CompletionService {
private:
    std::shared_ptr<IDocumentRepository> docRepo_;
    std::shared_ptr<SymbolResolver> symbolResolver_;
    
public:
    CompletionService(
        std::shared_ptr<IDocumentRepository> docRepo,
        std::shared_ptr<SymbolResolver> resolver
    );
    
    // 复杂的补全逻辑
    auto ProvideCompletions(
        const std::string& uri,
        const Position& pos
    ) -> std::vector<CompletionItem>;
};
}
```

---

### 3. **新增 `application/use_case/` 层**

**目的**：用例驱动，每个用例对应一个 LSP 操作

```cpp
// include/application/use_case/language/completion_use_case.hpp
namespace application {
class CompletionUseCase {
private:
    std::shared_ptr<domain::IDocumentRepository> docRepo_;
    std::shared_ptr<domain::CompletionService> completionService_;
    std::shared_ptr<DocumentMapper> mapper_;
    
public:
    CompletionUseCase(/* 依赖注入 */);
    
    // 执行用例
    auto Execute(const protocol::lsp::CompletionParams& params)
        -> protocol::lsp::CompletionList;
};
}

// src/application/use_case/language/completion_use_case.cpp
auto CompletionUseCase::Execute(const protocol::lsp::CompletionParams& params)
    -> protocol::lsp::CompletionList 
{
    // 1. 获取文档
    auto doc = docRepo_->FindByUri(params.textDocument.uri);
    if (!doc) {
        return {};
    }
    
    // 2. 转换位置
    auto pos = mapper_->FromLsp(params.position);
    
    // 3. 调用领域服务
    auto items = completionService_->ProvideCompletions(doc->Uri(), pos);
    
    // 4. 转换回 LSP 协议
    return mapper_->ToLsp(items);
}
```

---

### 4. **新增 `application/mapper/` 层**

**目的**：协议和领域之间的转换

```cpp
// include/application/mapper/document_mapper.hpp
namespace application {
class DocumentMapper {
public:
    // LSP -> Domain
    static auto FromLsp(const protocol::lsp::Position& pos) -> domain::Position;
    static auto FromLsp(const protocol::lsp::Range& range) -> domain::Range;
    
    // Domain -> LSP
    static auto ToLsp(const domain::Position& pos) -> protocol::lsp::Position;
    static auto ToLsp(const domain::CompletionItem& item) 
        -> protocol::lsp::CompletionItem;
    static auto ToLsp(const std::vector<domain::CompletionItem>& items)
        -> protocol::lsp::CompletionList;
};
}
```

---

### 5. **重组 `infrastructure/` 层**

#### **transport/** - 传输层

将 `domain/entity/session.hpp` 移到这里：

```cpp
// include/infrastructure/transport/session.hpp
namespace infrastructure {
class ISession {
public:
    virtual ~ISession() = default;
    virtual auto Read() -> boost::asio::awaitable<std::string> = 0;
    virtual auto Write(const std::string& msg) -> boost::asio::awaitable<void> = 0;
    virtual void Close() = 0;
};

class StdioSession : public ISession {
    // 当前的 IOSession 实现
};
}
```

#### **persistence/** - 持久化

仓储的具体实现：

```cpp
// include/infrastructure/persistence/memory_document_repository.hpp
namespace infrastructure {
class MemoryDocumentRepository : public domain::IDocumentRepository {
private:
    std::unordered_map<std::string, domain::Document> store_;
    mutable std::shared_mutex mutex_;
    
public:
    auto FindByUri(const std::string& uri) -> std::optional<domain::Document> override;
    void Save(const domain::Document& doc) -> void override;
    void Remove(const std::string& uri) -> void override;
};
}
```

#### **parser/** - 解析器

Tree-sitter 相关：

```cpp
// include/infrastructure/parser/dot_parser.hpp
namespace infrastructure {
class DotParser {
private:
    TSParser* parser_;
    
public:
    DotParser();
    ~DotParser();
    
    auto Parse(const std::string& source) -> std::unique_ptr<AST>;
    auto Update(AST* tree, const std::string& newSource) 
        -> std::unique_ptr<AST>;
};
}
```

---

### 6. **重组 `presentation/` 层**

#### **handler/** - 处理器

每个处理器负责一类 LSP 请求：

```cpp
// include/presentation/handler/language_handler.hpp
namespace presentation {
class LanguageHandler {
private:
    std::shared_ptr<application::CompletionUseCase> completionUseCase_;
    std::shared_ptr<application::RenameUseCase> renameUseCase_;
    
public:
    LanguageHandler(/* 依赖注入 */);
    
    auto HandleCompletion(const nlohmann::json& params) -> nlohmann::json;
    auto HandleRename(const nlohmann::json& params) -> nlohmann::json;
    auto HandleGotoDefinition(const nlohmann::json& params) -> nlohmann::json;
    auto HandleHover(const nlohmann::json& params) -> nlohmann::json;
};
}
```

#### **server.hpp** - 主服务器

简化为路由和协调：

```cpp
// include/presentation/server.hpp
namespace presentation {
class Server {
private:
    std::unique_ptr<infrastructure::ISession> session_;
    std::unique_ptr<LifecycleHandler> lifecycleHandler_;
    std::unique_ptr<DocumentHandler> documentHandler_;
    std::unique_ptr<LanguageHandler> languageHandler_;
    
public:
    Server(/* 依赖注入 */);
    
    auto Run() -> boost::cobalt::task<void>;
    
private:
    auto Dispatch(const uranus::jsonrpc::Request& req) -> nlohmann::json;
};
}
```

---

## 🚀 迁移步骤

### 阶段 1：协议分离（1-2 天）

1. 创建 `include/protocol/lsp/` 目录
2. 将 `domain/model/*.hpp` 中的 LSP 协议结构移到 `protocol/lsp/`
3. 更新所有引用

### 阶段 2：领域重构（3-5 天）

1. 创建 `domain/entity/document.hpp` 并实现核心逻辑
2. 创建 `domain/repository/` 接口
3. 创建 `domain/service/completion_service.hpp` 等
4. 将空的 service 实现迁移为真正的业务逻辑

### 阶段 3：应用层（2-3 天）

1. 创建 `application/use_case/` 结构
2. 创建 `application/mapper/` 实现协议转换
3. 将 `app/app.hpp` 的逻辑拆分到各个 use case

### 阶段 4：基础设施（1-2 天）

1. 将 `domain/entity/session.hpp` 移到 `infrastructure/transport/`
2. 实现 `infrastructure/persistence/` 的仓储
3. 重组 parser 相关代码

### 阶段 5：表示层（1-2 天）

1. 创建 `presentation/handler/` 处理器
2. 简化 `server.hpp`
3. 更新 `main.cpp`

---

## 📊 依赖关系图

```
┌─────────────────────────────────────────────────────────┐
│                    main.cpp                              │
│                  (程序入口)                               │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│               presentation/                              │
│  ┌────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │  Server    │→ │  Handlers    │→ │  Use Cases     │  │
│  └────────────┘  └──────────────┘  └────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│               application/                               │
│  ┌────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ Use Cases  │→ │  Mappers     │→ │  Domain        │  │
│  └────────────┘  └──────────────┘  └────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│                  domain/                                 │
│  ┌────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │  Entities  │  │  Services    │  │  Repositories  │  │
│  │            │  │              │  │  (Interface)   │  │
│  └────────────┘  └──────────────┘  └────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│            infrastructure/                               │
│  ┌────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │ Transport  │  │ Persistence  │  │  Parser        │  │
│  │            │  │ (Impl)       │  │                │  │
│  └────────────┘  └──────────────┘  └────────────────┘  │
└─────────────────────────────────────────────────────────┘
                       ▲
                       │
┌──────────────────────┴──────────────────────────────────┐
│                  protocol/                               │
│               (LSP 协议定义)                              │
└─────────────────────────────────────────────────────────┘
```

**依赖规则**：

- ✅ 外层可以依赖内层
- ❌ 内层不能依赖外层
- ✅ domain 层通过接口依赖 infrastructure
- ✅ 所有层都可以依赖 protocol（只是数据结构）

---

## 🎯 核心原则

1. **依赖倒置**：domain 定义接口，infrastructure 实现
2. **单一职责**：每个类只做一件事
3. **开闭原则**：通过接口扩展，不修改现有代码
4. **协议分离**：LSP 协议不污染领域模型
5. **用例驱动**：每个 LSP 操作对应一个 use case

---

## 📝 实践示例：完整的补全流程

```cpp
// 1. presentation/server.hpp - 接收请求
auto Server::Dispatch(const Request& req) -> json {
    if (req.Method() == "textDocument/completion") {
        return languageHandler_->HandleCompletion(req.Params());
    }
}

// 2. presentation/handler/language_handler.cpp - 处理请求
auto LanguageHandler::HandleCompletion(const json& params) -> json {
    auto lspParams = params.get<protocol::lsp::CompletionParams>();
    auto result = completionUseCase_->Execute(lspParams);
    return nlohmann::json(result);
}

// 3. application/use_case/language/completion_use_case.cpp - 用例编排
auto CompletionUseCase::Execute(const protocol::lsp::CompletionParams& params)
    -> protocol::lsp::CompletionList 
{
    // 获取文档
    auto doc = docRepo_->FindByUri(params.textDocument.uri);
    
    // 转换协议
    auto pos = mapper_->FromLsp(params.position);
    
    // 调用领域服务
    auto items = completionService_->ProvideCompletions(doc->Uri(), pos);
    
    // 转换回协议
    return mapper_->ToLsp(items);
}

// 4. domain/service/completion_service.cpp - 领域逻辑
auto CompletionService::ProvideCompletions(
    const string& uri, 
    const Position& pos
) -> vector<CompletionItem> 
{
    // 获取文档
    auto doc = docRepo_->FindByUri(uri);
    
    // 解析语法
    auto node = doc->GetAST()->FindNodeAt(pos);
    
    // 符号解析
    auto scope = symbolResolver_->GetScopeAt(uri, pos);
    
    // 生成补全
    vector<CompletionItem> items;
    for (auto& sym : scope->GetVisibleSymbols()) {
        if (sym.MatchesPrefix(node->GetText())) {
            items.push_back(CreateCompletionItem(sym));
        }
    }
    
    return items;
}

// 5. infrastructure/persistence/memory_document_repository.cpp - 数据访问
auto MemoryDocumentRepository::FindByUri(const string& uri) 
    -> optional<Document> 
{
    shared_lock lock(mutex_);
    if (auto it = store_.find(uri); it != store_.end()) {
        return it->second;
    }
    return nullopt;
}
```

---

## ✅ 目录结构优点总结

1. **清晰的职责分离**：每层、每目录职责明确
2. **易于测试**：依赖注入，可 mock 各层
3. **便于扩展**：新增功能只需添加新的 use case
4. **协议独立**：可轻松支持多种协议
5. **符合 DDD**：真正的领域驱动设计
6. **可维护性高**：代码组织清晰，易于理解

这个结构可以随着项目发展逐步演进，不需要一次性全部重构！

