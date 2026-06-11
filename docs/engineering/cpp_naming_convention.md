# C++ 命名规范

## 1. 适用范围

本规范适用于项目内所有 C++ 源码，包括但不限于：

- 核心模块
- 平台层
- 工具链
- 测试代码
- 示例代码

本规范中的所有条目均为强制要求。

## 2. 文档边界

本文档只定义命名规则。
除命名本身以外的源码风格规则，均以 `docs/engineering/cpp_coding_style.md` 为准。

跨文档规则只定义一次；引用时以对应文档为准。

## 3. 总体规则

项目命名风格统一为：

- 类型使用 `PascalCase`
- 非类型默认使用 `snake_case`
- 通过固定前缀区分成员、静态成员、全局变量、常量、宏
- 同类符号禁止混用多套命名风格

说明：本文档只约束“名字是什么”，不单独定义缩进、括号位置、换行和短代码块是否单行。所有格式化展示均以仓库 `.clang-format` 的实际输出为准。

## 4. 路径与文件命名

### 4.1 文件夹名

文件夹名必须使用 `snake_case`。

示例：

```text
core/
platform/
task_queue/
config_loader/
storage/
tests/
```

### 4.2 文件名

文件名必须使用 `snake_case`。

示例：

```text
task_queue.h
task_queue.cpp
config_loader.h
config_loader.cpp
session_manager.h
session_manager.cpp
```

## 5. 命名空间命名

命名空间名必须使用 `snake_case`。

示例：

```cpp
namespace proj {}

namespace proj::task_queue {}
```

## 6. 类型命名

### 6.1 通用类型

以下类型名必须使用 `PascalCase`：

- 类
- 结构体
- 枚举类型
- 联合体
- 类型别名
- `using` 别名
- 有语义名称的模板类型参数

示例：

```cpp
class TaskQueue;
struct SessionInfo;
enum class TaskState;
union ByteView;
using UserId = uint64_t;
```

### 6.2 抽象接口

抽象接口类型必须使用 `I` 前缀，后接 `PascalCase`。

示例：

```cpp
class IFileStore;
class ILogger;
class ITaskScheduler;
```

### 6.3 接口实现类

接口实现类不得使用 `I` 前缀。

实现类命名必须使用“实现标识 + 职责名”或“平台标识 + 职责名”。

示例：

```cpp
class LocalFileStore : public IFileStore;
class DefaultLogger : public ILogger;
class ThreadPoolTaskScheduler : public ITaskScheduler;
class Win32FileStore : public IFileStore;
```

### 6.4 数据结构后缀

用于描述创建参数、状态、配置、查询结果的数据结构，命名必须使用明确后缀。

允许后缀及语义：

- `Desc`：描述对象定义、创建选项或静态配置
- `State`：描述运行时状态或状态机阶段
- `Info`：描述只读事实、观测结果、元数据快照
- `Params`：描述一次调用的输入参数集合

示例：

```cpp
struct TaskDesc;
struct SessionState;
struct FileInfo;
struct ParseParams;
```

### 6.5 描述类型命名一致性

同类对象的描述类型不得混用多套后缀。

必须先确定语义，再选择唯一后缀：

- 创建选项统一使用 `Desc`，不得与 `CreateInfo`、`Options` 混用
- 运行时状态统一使用 `State`
- 只读事实或查询结果统一使用 `Info`
- 调用入参对象统一使用 `Params`

允许：

```cpp
struct CacheEntryDesc;
struct ParserDesc;
struct ParserInfo;
```

禁止：

```cpp
struct CacheEntryCreateInfo;
struct ParserDesc;
struct ParserInfo;
```

## 7. 枚举与别名命名

### 7.1 枚举类型与枚举值

枚举类型名必须使用 `PascalCase`。
枚举值必须使用 `PascalCase`。

是否必须使用 `enum class` 见 `docs/engineering/cpp_coding_style.md`。

示例：

```cpp
enum class TaskState
{
    Pending,
    Running,
    Completed
};
```

### 7.2 类型别名

类型别名和 `using` 别名必须使用 `PascalCase`。

示例：

```cpp
using UserId = uint64_t;
using Clock = std::chrono::steady_clock;
using TaskHandle = uint32_t;
```

### 7.3 句柄类型

句柄类型命名必须使用 `XxxHandle`。

示例：

```cpp
using TaskHandle = uint32_t;
using SessionHandle = uint32_t;
```

句柄变量名必须按普通变量规则命名：

```cpp
TaskHandle task_handle;
SessionHandle session_handle;
```

## 8. 函数命名

### 8.1 普通函数

普通函数、成员函数、静态函数、工具函数、工厂函数必须使用 `PascalCase`。

示例：

```cpp
void Initialize();
void Shutdown();
void LoadConfig();
void StartWorker();
```

### 8.2 查询函数

查询函数必须使用清晰语义前缀：

- `Get`
- `Is`
- `Has`
- `Can`
- `Should`
- `Supports`

示例：

```cpp
size_t GetSize() const;
bool IsReady() const;
bool HasPendingTask() const;
bool SupportsHotReload() const;
```

### 8.3 回调函数

回调函数名必须统一使用 `OnXxx` 形式。

示例：

```cpp
void OnConfigChanged();
void OnTaskCompleted();
void OnConnectionClosed();
```

### 8.4 文件级辅助函数

文件级辅助函数，包括匿名命名空间中的内部辅助函数，必须按普通函数规则命名，即使用 `PascalCase`。

示例：

```cpp
namespace {
bool IsValidPath(std::string_view path);
size_t CountPendingTasks(...);
}
```

## 9. 变量命名

### 9.1 普通变量、局部变量、参数

普通变量、局部变量、函数参数必须使用 `snake_case`。

示例：

```cpp
size_t task_count {0};
double timeout_seconds {0.0};
const TaskDesc& task_desc
```

### 9.2 成员变量

成员变量必须使用 `m_snake_case`。

示例：

```cpp
TaskQueue* m_queue {nullptr};
size_t m_task_count {0};
bool m_is_initialized {false};
```

### 9.3 静态成员变量

静态成员变量必须使用 `s_snake_case`。

示例：

```cpp
static size_t s_instance_count;
static bool s_enable_metrics;
```

### 9.4 全局变量

全局变量必须使用 `g_snake_case`。

示例：

```cpp
ILogger* g_logger {nullptr};
bool g_enable_logging {false};
```

### 9.5 文件级状态变量

文件级全局静态变量，包括匿名命名空间中的文件级状态变量，必须使用 `g_snake_case`。

示例：

```cpp
namespace {
bool g_enable_trace {false};
size_t g_cached_thread_count {0};
}
```

### 9.6 常量

具名常量必须使用 `k_snake_case`。

适用对象包括：

- `constexpr`
- `constinit`
- 文件级常量
- 类内静态常量
- 模块内命名常量

示例：

```cpp
constexpr size_t k_default_capacity {64};
constexpr uint32_t k_invalid_task_id {0xffffffffu};
```

### 9.7 匿名命名空间常量

匿名命名空间中的具名常量必须使用 `k_snake_case`。

示例：

```cpp
namespace {
constexpr size_t k_default_alignment {64};
}
```

### 9.8 布尔变量

布尔变量名必须直接表达布尔语义，并显式区分“当前状态”与“配置开关”。

推荐前缀：

- `is_`：当前状态
- `has_`：拥有或存在
- `can_`：能力或许可
- `should_`：策略判断或建议动作
- `enable_`：配置开关、输入选项、参数意图
- `enabled_`：某能力已被开启的结果状态

约束：

- 运行时状态优先使用 `is_`、`has_`、`can_`
- 配置对象、参数对象中的开关优先使用 `enable_`
- 表达“已经启用”的派生状态时使用 `enabled_`
- 禁止使用不带语义前缀的裸布尔名

示例：

```cpp
bool is_ready        = false;
bool has_value       = true;
bool can_retry       = true;
bool should_reload   = false;
bool enable_logging  = true;
bool logging_enabled = false;
bool m_is_running    = false;
```

## 10. 禁止匈牙利命名和类型前缀变量名

变量名禁止包含类型前缀。

以下写法全部禁止：

```cpp
double d_value {};
int i_index {};
int* p_buffer {};
float f_timeout {};
bool b_ready {};
char* psz_name {};
```

必须写为：

```cpp
double value {};
int index {};
int* buffer {};
float timeout_seconds {};
bool is_ready {};
char* name {};
```

### 10.1 指针变量

指针变量不得使用 `p_`、`ptr_` 作为类型前缀式命名规则。
指针变量名必须按普通变量规则命名。

允许：

```cpp
TaskQueue* queue = nullptr;
ILogger* logger  = nullptr;
```

禁止：

```cpp
TaskQueue* p_queue  = nullptr;
ILogger* ptr_logger = nullptr;
```

## 11. 模板命名

### 11.1 模板类型参数

模板类型参数命名必须满足以下规则之一：

- 简单泛型参数使用单字母大写：`T`、`U`、`K`、`V`
- 有明确语义的参数使用 `PascalCase`

示例：

```cpp
template<typename T>
class RingBuffer;

template<typename ValueType>
class CacheStore;
```

### 11.2 非类型模板参数

非类型模板参数必须使用 `PascalCase`。

示例：

```cpp
template<typename T, size_t Capacity>
class FixedQueue;
```

## 12. 宏命名

所有宏必须使用统一项目前缀 + 全大写 + 下划线分词。

统一形式：

```text
PROJ_ALL_CAPS
```

示例：

```cpp
#define PROJ_ASSERT(expr)
#define PROJ_CHECK(expr)
#define PROJ_LOG_INFO(...)
#define PROJ_UNUSED(x)
#define PROJ_ENABLE_TRACING 1
```

以下写法全部禁止：

```cpp
#define ASSERT(expr)
#define CHECK(expr)
#define LOG_INFO(...)
#define UNUSED(x)
```

## 13. 缩写规则

### 13.1 允许保留的常见缩写

行业公认、稳定、可搜索的缩写允许保留原语义，包括但不限于：

- `CPU`
- `GPU`
- `API`
- `ID`
- `UUID`
- `URL`
- `HTTP`
- `TCP`
- `UDP`
- `JSON`
- `XML`
- `SQL`
- `UTF8`
- `ASCII`
- `HDR`
- `SRGB`
- `AABB`

命名写法必须统一遵循以下规则，缩写在 `PascalCase` 中按普通单词处理，在 `snake_case` 中统一转为小写单词。

- `PascalCase` 中使用 `HttpRequest`、`SqlClient`、`UuidGenerator`、`ApiClient`
- `snake_case` 中使用 `http_request`、`sql_client`、`uuid_generator`、`api_client`
- 宏仍使用全大写形式，例如 `PROJ_HTTP_CLIENT`

类型中示例：

```cpp
class UuidGenerator;
struct HttpRequest;
class SqlClient;
class ApiClient;
```

变量中示例：

```cpp
uint64_t object_id;
bool use_hdr;
std::string api_key;
```

### 13.2 禁止随意缩写

以下写法禁止：

```cpp
cfg_mgr
req_ctx
tmp_buf
svc_impl
```

必须使用完整、稳定、可读的单词。

## 14. 项目前缀规则

### 14.1 文件名

文件名不得添加项目名、公司名或模块缩写前缀。

禁止：

```text
proj_task_queue.h
corp_config_loader.h
svc_session_manager.h
```

必须写为：

```text
task_queue.h
config_loader.h
session_manager.h
```

### 14.2 类型名

项目内部 C++ 类型名不得添加项目名、公司名前缀。

禁止：

```cpp
class ProjTaskQueue;
class CorpConfigLoader;
class SvcSessionManager;
```

必须写为：

```cpp
class TaskQueue;
class ConfigLoader;
class SessionManager;
```

### 14.3 例外

只有宏使用统一项目前缀 `PROJ_`。

## 15. 命名语义补充约定

### 15.1 优先使用领域与生态中的标准术语

命名必须优先采用本领域、公认生态或上游框架中的标准术语。
若项目建立在 Qt、图形库、网络协议栈或业务领域模型之上，命名应尽量与官方文档和团队长期术语保持一致。

禁止为了“看起来简短”而随意自造缩写或替换成熟术语。
若术语已经在行业内长期稳定使用，则应直接沿用。

### 15.2 避免模糊、泛化和无职责信息的名称

命名必须尽量表达“它是什么”或“它负责什么”。
禁止在缺少上下文限定时使用信息量过低的名称，例如：

- `data`
- `thing`
- `item`
- `object`
- `manager`
- `handler`
- `processor`

以下场景允许使用相关词汇：

- 作为明确语义后缀的一部分，如 `FileInfo`
- 作为组合名称的一部分，如 `request_data`
- 当该名称在架构中已有稳定且被普遍接受的专业含义

若使用 `Manager`、`Handler`、`Processor` 等职责型后缀，必须能从完整名字中看出边界与职责，例如 `ConnectionManager`、`RequestHandler`。

### 15.3 函数名必须体现动作语义

在遵守本文大小写规则的前提下，函数名必须清楚表达“做什么”。
优先使用具体动词或动宾结构，例如：

- `LoadConfig`
- `ComputeBounds`
- `SerializeTask`
- `ValidateRequest`

应尽量避免长期大量出现语义过弱的函数名，例如：

- `Process`
- `Handle`
- `DoWork`

若函数本身就是统一分发入口、事件入口或框架回调，则可保留与语境一致的命名。

### 15.4 变量名应显式表达单位、状态与集合语义

带单位的数据必须尽量在名称中体现单位，避免调用方误用。

示例：

- `timeout_ms`
- `size_bytes`
- `angle_rad`

集合名称必须显式体现“这是多个对象”。
可以使用复数或稳定后缀，但全项目必须统一一种风格。

允许：

- `users`
- `task_list`
- `entry_map`

### 15.5 目录和文件名必须体现模块职责

目录名和文件名不仅要满足 `snake_case`，还必须反映模块边界或主要职责。
禁止长期使用缺乏语义的信息垃圾桶名称，例如：

- `misc`
- `other`
- `stuff`

文件名应优先与主要类型或主要功能对应，例如：

- `image_renderer.cpp`
- `order_repository.h`
- `task_scheduler.cpp`

### 15.6 老代码命名迁移必须按模块渐进进行

所有新增代码必须立即遵循本文命名规则。
历史代码的修正必须按模块、子目录或单一职责范围逐步推进，避免一次性全库大改名。

每次命名迁移完成后，必须保证：

- 引用已同步更新
- 构建可通过
- 测试可通过
- 改动范围便于审查与回滚

## 16. 命名速查表

| 对象类别 | 强制命名规则 | 示例 |
| --- | --- | --- |
| 文件夹 | `snake_case` | `task_queue` |
| 文件名 | `snake_case` | `config_loader.h` |
| 命名空间 | `snake_case` | `storage` |
| 类 / 结构体 / 联合体 | `PascalCase` | `SessionManager` |
| 抽象接口 | `IPascalCase` | `IFileStore` |
| 枚举类型 | `PascalCase` | `TaskState` |
| 枚举值 | `PascalCase` | `Completed` |
| 类型别名 / `using` | `PascalCase` | `TaskHandle` |
| 函数 | `PascalCase` | `LoadConfig` |
| 回调函数 | `OnXxx` | `OnTaskCompleted` |
| 普通变量 / 参数 / 局部变量 | `snake_case` | `task_count` |
| 成员变量 | `m_snake_case` | `m_task_count` |
| 静态成员变量 | `s_snake_case` | `s_instance_count` |
| 全局变量 | `g_snake_case` | `g_logger` |
| 常量 | `k_snake_case` | `k_default_capacity` |
| 宏 | `PROJ_ALL_CAPS` | `PROJ_ASSERT` |

## 17. 标准示例

```cpp
#include <cstdint>
#include <string>

namespace proj::storage {
constexpr size_t k_default_cache_size = 64;

enum class EntryState { Pending, Ready, Failed };

struct CacheEntryDesc {
    std::string key;
    EntryState initial_state = EntryState::Pending;
    bool enable_persistence  = false;
};

using EntryHandle = uint32_t;

class ICacheStore {
public:
    virtual ~ICacheStore() = default;

    virtual bool Initialize()                                         = 0;
    virtual void Shutdown()                                           = 0;
    virtual EntryHandle CreateEntry(const CacheEntryDesc& entry_desc) = 0;
    virtual void OnEntryChanged()                                     = 0;
};

class LocalCacheStore final : public ICacheStore {
public:
    bool Initialize() override;
    void Shutdown() override;
    EntryHandle CreateEntry(const CacheEntryDesc& entry_desc) override;
    void OnEntryChanged() override;

private:
    void CreateDefaultEntries();

private:
    size_t m_entry_count  = 0;
    bool m_is_initialized = false;

    static size_t s_instance_count;
};
}  // namespace proj::storage

namespace {
constexpr size_t k_default_alignment = 64;
bool g_enable_logging                = false;

bool IsValidKey(const std::string& key) { return !key.empty(); }
}  // namespace

#define PROJ_ASSERT(expr)
#define PROJ_ENABLE_LOGGING 1
```
