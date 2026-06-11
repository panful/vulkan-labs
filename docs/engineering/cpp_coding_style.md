# C++ Coding Style

## 1. 适用范围

本规范适用于项目内所有 C++ 源码，包括但不限于：

- 核心模块
- 平台层
- 工具链
- 测试代码
- 示例代码

本规范中的所有条目均为强制要求。

## 2. 文档边界

- 命名相关内容以 `docs/engineering/cpp_naming_convention.md` 为准

本文档统一定义以下非命名规则：

- 文件组织
- 头文件与依赖
- 命名空间使用
- 类型设计
- 声明顺序
- 语言特性使用
- 注释
- 错误处理
- 测试代码风格

## 3. 总体原则

### 3.1 代码目标

代码必须满足以下要求：

- 可读
- 可维护
- 可审查
- 可搜索
- 可重构
- 行为明确
- 风格一致

### 3.2 优先级

出现冲突时，遵循以下优先级：

1. 正确性
2. 可读性
3. 可维护性
4. 简洁性
5. 性能微优化

### 3.3 外部规范参考顺序

本文档未覆盖、表述不够细、或存在工程判断空间时，默认按以下顺序参考外部规范：

1. C++ Core Guidelines
2. 仓库现有规则、自动化工具配置与已形成的稳定局部约定
3. Google C++ Style Guide 等主流工业 C++ 规范
4. 目标平台、编译器 ABI、标准库实现和第三方库官方建议

当外部规范之间存在差异时，优先采用更符合本仓库“可读、可维护、可审查、行为明确”的写法。
不得机械搬运外部规范条文，必须结合本仓库的语言版本、工具链、性能目标和模块边界做工程化判断。

### 3.4 风格统一

同一类问题必须使用同一种写法。
禁止在项目内长期并存多套风格。

### 3.5 自动化优先

凡是可由自动化工具稳定约束的规则，统一以仓库中的工具配置为准，包括但不限于：

- `.clang-format`
- `.clang-tidy`
- `.editorconfig`

文档不重复规定可自动格式化的细节。
提交代码前必须完成格式化与静态检查。
禁止手工修改代码格式以对抗自动化工具结果。

### 3.6 文档中的格式示例以 `.clang-format` 为准

本文档中的代码示例、缩进、换行、括号位置和单行短代码块展示方式，统一遵循仓库当前 `.clang-format` 的实际输出。

若文档中的历史示例与自动格式化结果不一致，以 `.clang-format` 为准；不得以文档中的旧示例对抗自动格式化结果。

## 4. 文件组织与依赖

### 4.1 一个文件只承载一个主要职责

一个头文件和对应源文件必须围绕一个主要类型或一个清晰模块组织。
禁止在同一个文件中混入多个无关类型或无关功能。

### 4.2 声明与实现分离

声明与实现必须按职责清晰拆分：

- 头文件放声明
- 源文件放实现

以下代码除外：

- 模板定义
- 内联函数
- 必须在调用点可见的实现

### 4.3 头源文件配对

若某类型存在稳定的外部声明和非平凡实现，必须提供对应的 `.h` 与 `.cpp` 文件。
禁止把本应拆分的实现长期堆积在头文件中。

### 4.4 头文件必须自包含

每个头文件必须可以被单独包含并通过编译。
头文件不得依赖“必须先 include 另一个头文件”这种隐式前提。

### 4.5 头文件默认使用 `#pragma once`

项目内头文件默认使用：

```cpp
#pragma once
```

除非存在以下例外，否则不得新增传统 include guard 宏：

- 目标工具链或静态分析工具明确不支持 `#pragma once`
- 需要与外部公共头、生成代码或第三方兼容策略保持一致

若必须使用 include guard，则命名必须稳定、唯一，并遵循项目宏命名规范。

### 4.6 头文件不得泄漏实现细节

公共头文件不得暴露不必要的私有实现细节。
能通过前置声明、句柄、抽象接口或私有实现隐藏的内容，必须避免直接暴露在公共接口中。

### 4.7 公共头文件必须最小依赖

公共头文件必须只包含声明所必需的依赖。
重型实现依赖必须尽量留在 `.cpp` 文件中。

### 4.8 头文件和源文件中禁止广域 `using namespace`

头文件中禁止出现：

```cpp
using namespace std;
```

源文件中也禁止使用广域 `using namespace`。

### 4.9 Include 顺序以工具配置为准

头文件包含顺序、分组和排序规则统一以 `.clang-format` 为准。
代码中必须遵循自动格式化产出的 include 顺序，不得手工维持另一套规则。

### 4.10 源文件首个 include

`.cpp` 文件的第一个 include 必须是对应的 `.h` 文件。

允许：

```cpp
#include "storage/cache_store.h"
```

禁止：

```cpp
#include <vector>
#include "storage/cache_store.h"
```

### 4.11 优先使用前置声明

在不影响正确性的前提下，必须优先使用前置声明以减少头文件依赖。
头文件中不得无必要地包含重型头文件。

### 4.12 禁止依赖传递包含

代码不得依赖“某个头文件碰巧又包含了另一个头文件”的传递关系。
直接使用某个类型、函数或模板时，必须显式包含其声明所在头文件。

### 4.13 项目代码必须放在项目命名空间内

所有项目代码必须放在项目命名空间内。
禁止将项目类型直接暴露在全局命名空间。

### 4.14 命名空间层级必须反映目录和模块结构

目录、模块、命名空间三者必须保持一致的语义映射。
禁止目录结构与命名空间长期脱节。

### 4.15 文件级内部符号必须放入匿名命名空间

仅供当前翻译单元使用的辅助函数、常量、状态变量，必须放入匿名命名空间。

示例：

```cpp
namespace {
constexpr size_t k_default_capacity {64};
bool g_enable_trace {false};

bool IsValidKey(std::string_view key) {
    return !key.empty();
}
```

### 4.16 公共接口必须稳定且清晰

公共头文件中的类型、函数、别名、常量，必须具备稳定且清晰的语义。
禁止把实验性实现细节直接暴露为长期公共接口。

### 4.17 公共接口必须显式说明边界

公共接口在语义不明显时，必须能明确看出以下约束：

- 生命周期
- 所有权
- 线程约束
- 失败语义
- 扩展点边界

### 4.18 跨模块依赖必须单向、可解释

模块依赖关系必须清晰、可解释、可维护。
禁止形成长期循环依赖。
若两个模块相互依赖，必须重构边界或提取公共抽象层。

## 5. 类型设计

### 5.1 `struct` 用于数据聚合

`struct` 必须优先用于：

- 纯数据对象
- 描述对象
- 状态对象
- 参数对象
- 值语义聚合体

### 5.2 `class` 用于封装与约束

需要封装、生命周期管理、资源管理、不变量维护的类型必须使用 `class`。

### 5.3 抽象接口与实现分离

抽象接口必须仅表达行为约束，不承载实现细节。

### 5.4 优先小而清晰的类型

类型必须保持职责单一。
当一个类型同时承担资源管理、流程控制、缓存、日志、配置等多种职责时，必须拆分。

### 5.5 公共类型不得暴露多余实现细节

公共类型必须只暴露调用方需要知道的语义。
能留在实现文件、私有成员或内部辅助类型中的细节，不得泄漏为公共接口。

## 6. 类组织规则

### 6.1 访问控制顺序

类内访问控制块必须按以下顺序排列：

1. `public`
2. `protected`
3. `private`

### 6.2 类内成员声明顺序

类内声明必须按以下顺序排列：

1. 类型别名 / 内部类型
2. 常量
3. 构造函数 / 析构函数
4. 拷贝 / 移动控制函数
5. 对外公开接口
6. 受保护接口
7. 私有辅助函数
8. 成员变量
9. 静态成员变量

### 6.3 成员变量必须放在 `private`

成员变量必须声明在 `private` 区域。
禁止公开可变成员变量，纯数据结构除外。

### 6.4 基类析构函数规则

只要类型会被作为基类使用，析构函数必须正确声明。
含虚函数的基类析构函数必须为虚函数。
接口类析构函数必须为 `virtual`。

## 7. 构造、析构与对象语义

### 7.1 单参数构造函数必须使用 `explicit`

所有单参数构造函数必须显式标记为：

```cpp
explicit
```

除非该类型明确设计为隐式转换类型。

### 7.2 资源拥有型类型必须明确拷贝 / 移动语义

管理资源的类型必须显式声明以下之一：

- 禁止拷贝，仅允许移动
- 自定义拷贝与移动
- 显式使用默认行为

禁止依赖隐式生成导致语义不清晰。

### 7.3 优先 RAII

资源管理必须优先使用 RAII。
禁止通过“调用者记得手动释放”作为主要资源管理模式。

### 7.4 默认成员初始化

适合作为默认值的成员必须在声明处初始化。
构造函数必须优先使用成员初始化列表。

## 8. 虚函数规则

### 8.1 基类中必须显式写 `virtual`

基类中声明虚函数时必须显式使用 `virtual`。

### 8.2 派生类中禁止重复写 `virtual`

派生类重写虚函数时不得重复写 `virtual`，必须显式使用 `override`。

允许：

```cpp
class IBase
{
public:
    virtual ~IBase() = default;
    virtual void Initialize() = 0;
};

class Derived final : public IBase
{
public:
    ~Derived() override = default;
    void Initialize() override;
};
```

禁止：

```cpp
class Derived : public IBase
{
public:
    virtual void Initialize();
};
```

### 8.3 禁止省略 `override`

所有派生类重写虚函数必须显式写 `override`。

### 8.4 禁止继续覆写时必须使用 `final`

若某个派生类或重写函数不允许继续被覆写，必须使用 `final`。

### 8.5 非扩展点禁止滥用 `virtual`

只有确实需要运行时多态时才允许引入虚函数。
禁止将普通成员函数机械性声明为虚函数。

## 9. 函数编写规则

### 9.1 函数必须保持单一职责

函数必须只完成一个清晰职责。
过长、过深、过于分支化的函数必须拆分。

### 9.2 参数个数限制

函数参数个数必须控制在可读范围内。

- 参数个数超过 5 个时，必须评估是否应封装为参数对象、配置对象或上下文对象
- 参数个数超过 7 个时，原则上禁止继续增加位置参数，除非接口签名受外部协议、标准库或第三方库约束

### 9.3 函数长度限制

函数实现应尽量控制在 80 行以内。
超过 120 行的函数原则上应重构，或在评审中明确说明其必要性与不可再拆分的原因。

以下场景允许作为受控例外，但仍必须优先保证可读性：

- 表驱动逻辑
- 协议映射或状态分发表
- 自动生成代码
- 为了保持异常安全、锁边界或事务边界而不宜拆分的实现
- 测试中为表达完整场景而保留的少量长用例

### 9.4 输出参数最小化

优先使用返回值表达结果。
仅当确有必要时才使用输出参数。

### 9.5 参数与返回值建模

函数签名必须尽量直接表达语义、所有权与失败模式。

- 除底层互操作、原地填充或性能确有必要的场景外，禁止新增普通输出参数接口
- 可缺失值优先使用 `std::optional<T>`
- 多字段结果优先返回具名结构体，禁止返回语义不清晰的长 tuple
- 错误和值共存时，必须统一使用项目约定的结果类型，如 `Result<T, E>`、`StatusOr<T>` 或等价类型
- 只读字符串参数优先使用 `std::string_view`
- C++20 下连续只读区间优先使用 `std::span<const T>`；C++17 项目必须使用等价的 span 视图类型或显式容器引用
- 使用 `std::string_view`、`std::span`、引用、裸指针等非拥有视图类型时，必须明确生命周期约束

#### 9.5.1 只读输入参数的默认规则

只读输入参数必须按“是否 cheap to copy”选择传递方式：

- 对标量、枚举、裸指针、迭代器、句柄以及其它 cheap to copy 的小型值类型，优先按值传递
- 对 `std::string_view`、`std::span`、`std::optional<T>` 这类本身就是轻量视图或轻量包装的类型，优先按值传递
- 对复制代价不明确、体积较大、含动态资源、或拷贝可能引入额外语义成本的类型，优先使用 `const T&`
- 不要把 `const T&` 当作机械默认值；对本就适合按值传递的小型类型，禁止为了“看起来更高效”而一律改成引用

工程经验上，可按以下阈值进行第一轮判断，但这些阈值不是脱离平台与 ABI 的绝对规则：

- `<= 16` 字节且 trivially copyable 的值类型，默认优先按值传递
- `> 16` 且 `< 24` 字节的值类型，需要结合寄存器传递、对齐要求、热路径频率和领域习惯判断
- `>= 24` 字节的值类型，默认优先使用 `const T&`，除非它是已知的 small value type 且 profiling 或平台约定证明按值更合适

只要类型不是 trivially copyable、带非平凡析构、持有堆资源、或复制行为不再是“廉价且稳定可预期”，即使字节数较小，也不应仅凭体积选择按值传递。

按值传递的输入参数，默认不在函数声明中写 `const T value`：

- 在声明处写 `const` 不能改变调用方语义，也不能表达额外接口约束
- 按值参数是否在函数体内被重新赋值，属于实现细节，不属于接口语义
- 若实现中确实希望禁止再次修改，可在定义体内部通过局部 `const` 绑定表达

推荐：

```cpp
void DrawPoint(Vec3f position);
void SetLabel(std::string label);
```

不推荐：

```cpp
void DrawPoint(const Vec3f position);
void SetLabel(const std::string label);
```

#### 9.5.2 small value type 按值传递例外不是例外

对于数学库、图形库、渲染库、SIMD 包装类型和其它明确设计为 small value type 的对象，应优先按值传递，只要以下条件成立之一：

- 类型语义上就是值对象，复制不改变所有权与身份
- 类型足够小，且复制成本稳定、可预期
- 类型在目标平台上天然适合寄存器传递
- 按值传递比 `const T&` 更清晰，且不会引入可见性能问题

典型例子包括但不限于：

- `vec2f`、`vec3f`、`vec4f`
- `mat3f`、`quatf`
- `Color4f`
- 轻量矩形、范围、点、尺寸、变换片段等几何值类型

若某个第三方数学类型在项目内长期被当作值对象使用，则应保持该领域内一致性，不要同一类接口一部分按值、一部分按 `const T&`。

#### 9.5.3 会被拷贝保存的输入参数优先按值

如果函数无论如何都要把传入参数拷贝或移动到成员、容器、闭包或异步任务中，则优先把该参数声明为按值传递，并在函数体内再 `std::move` 到目标位置。

推荐：

```cpp
void SetName(std::string name) {
    m_name = std::move(name);
}
```

不推荐：

```cpp
void SetName(const std::string& name) {
    m_name = name;
}
```

仅当调用点绝大多数都无法利用移动语义，且额外一次值传递明显更差时，才允许继续使用 `const T&` + 内部拷贝。

#### 9.5.4 可修改输入输出参数必须显式表达意图

需要由被调方修改且调用方对象必须存在时，使用 `T&`。

- `T&` 仅用于“必填且会被修改”的参数
- `const T&` 不得承载输出语义
- 同时承担输入和输出语义的参数必须在命名和注释中明确写清楚修改约束与调用后状态

需要表达“可空的可选参数”或“可空的输出槽位”时，使用 `T*`，并在注释中明确 `nullptr` 的语义。

- 裸指针参数默认表示非拥有
- `T*` 用于可空、可缺失、数组首元素、或 C 互操作边界
- 若参数既非空又不修改对象，优先 `const T&` 或轻量类型按值；不要用裸指针模拟引用

#### 9.5.5 转移所有权与 sink 参数规则

参数语义是“被调用方接管所有权”时，签名必须显式表达 sink 语义：

- 独占所有权优先使用 `std::unique_ptr<T>` 按值传递
- 共享所有权仅在业务上确实需要共享生命周期时才使用 `std::shared_ptr<T>`，并优先按值传递
- 不要用 `const std::unique_ptr<T>&` 或 `const std::shared_ptr<T>&` 伪装所有权语义
- 仅转发、不持有对象时，不要为了省事把观察关系写成智能指针参数

#### 9.5.6 右值引用参数必须服务于明确的移动语义

`T&&` 参数只应用于以下场景：

- 完美转发模板
- 明确要求调用方传入可被消费对象的 sink 接口
- 需要区分左值和右值重载，且确有可见收益

普通业务接口不得把 `T&&` 当作“更现代的按值传递”。
若函数最终会拥有一个对象而不需要重载左值/右值，一般优先直接按值传递。

#### 9.5.7 容器与区间参数优先传视图，不传具体容器

只读字符串、字节序列、数组区间和连续容器参数，必须优先表达为视图而非具体容器类型：

- 只读文本优先 `std::string_view`
- 连续只读区间优先 `std::span<const T>`
- 连续可修改区间优先 `std::span<T>`
- 仅当接口确实依赖容器所有权、容量、分配器或节点稳定性时，才直接接收具体容器类型或容器引用

禁止为了兼容单一种调用点，把只读输入参数写成 `const std::vector<T>&`、`const std::array<T, N>&` 或 `const std::string&` 作为长期默认接口形态。

#### 9.5.8 禁止把参数传递规则绝对化

以下判断必须结合真实类型、调用频率、ABI 边界与性能数据综合决定，而不是教条套用：

- “一律用 `const T&`”
- “一律按值传递更现代”
- “所有数学类型都必须按值”
- “所有大对象都必须用引用”

若某处选择偏离默认规则，代码评审中应能给出清晰理由，例如：

- 类型是已知的 small value type
- 函数必然复制并保存参数
- 该接口位于热路径，已有 profiling 证据
- 受外部 ABI、三方库或历史接口约束

### 9.6 查询函数不得附带副作用

`GetXxx`、`IsXxx`、`HasXxx` 等查询函数不得修改可观察状态。

### 9.7 回调函数必须保持轻量

回调函数应只负责分发、封装状态或触发后续流程。
复杂逻辑必须转发到普通成员函数或独立辅助函数。

## 10. 语言特性使用规则

### 10.1 枚举必须使用 `enum class`

项目内禁止使用普通 `enum`。
所有枚举必须使用 `enum class`。

### 10.2 禁止使用 `NULL`

空指针必须统一使用：

```cpp
nullptr
```

### 10.3 禁止 C 风格类型转换并收紧 C++ cast 使用

禁止使用：

```cpp
(Type)value
```

必须根据语义选择受限的 C++ cast：

- `static_cast` 作为常规显式转换手段
- `reinterpret_cast` 仅允许在底层封装、平台适配、ABI 边界、序列化边界使用，并必须局部封装
- `const_cast` 原则上禁止，仅允许与历史接口或第三方接口互操作，且必须证明原对象并非真正的常量对象
- `dynamic_cast` 仅允许在确有运行时多态识别需求的边界使用，不得作为常规分发手段

### 10.4 禁止隐式收窄转换

禁止可能丢失精度、范围或符号语义的隐式数值转换。

涉及以下场景时，必须显式转换并证明安全：

- `size_t` 与有符号整数之间的转换
- `uint64_t` 到 `uint32_t` 等窄化整数转换
- `double` 到 `float` 等浮点窄化转换
- 枚举到底层整数类型的转换

### 10.5 禁止随意混用有符号与无符号整数

边界检查、循环、减法、比较和容器索引中，禁止随意混用 signed / unsigned。

跨类型运算必须满足以下要求：

- 类型选择可解释
- 范围安全已知
- 转换显式可见

### 10.6 优先使用 `constexpr`

编译期常量必须使用 `constexpr`。
禁止能在编译期确定却仍定义为普通变量或宏常量。

### 10.7 优先使用 `const`

不需要修改的对象、参数、引用、指针目标必须加 `const`。

### 10.8 合理使用 `noexcept`

明确不抛异常的函数必须标记 `noexcept`。
移动构造、移动赋值在满足条件时必须优先标记 `noexcept`。
析构函数不得抛异常。
项目边界函数若声明为 `noexcept`，则必须保证异常在边界内部被消化。

### 10.9 类型别名统一使用 `using`

禁止新增 `typedef`。
类型别名必须统一使用 `using`。

### 10.10 标准属性使用规则

以下场景必须优先使用标准属性而非自定义宏约定：

- 结果不得忽略的函数、工厂函数、校验结果类型应使用 `[[nodiscard]]`
- `switch` 中有意贯穿的分支必须使用 `[[fallthrough]]`
- 确实会未使用的变量或参数优先使用 `[[maybe_unused]]`

## 11. `auto` 使用规则

### 11.1 允许使用 `auto` 的场景

仅在以下情况允许使用 `auto`：

- 迭代器类型过长且右值表达式类型明显
- lambda
- 模板推导上下文
- 工厂函数返回类型一眼可见
- 明确提高可读性时

允许：

```cpp
auto iter = values.begin();
auto task = CreateTask();
auto callback = []() { return true; };
```

### 11.2 禁止滥用 `auto`

当类型不明显、会隐藏语义、或降低可读性时，必须写出明确类型。

禁止：

```cpp
auto count = 0;
auto enabled = true;
auto name = GetName();
```

## 12. 指针、引用与所有权

### 12.1 必须明确所有权

代码中必须能明确区分以下关系：

- 拥有对象
- 非拥有引用
- 可空观察指针
- 必须存在的引用

### 12.2 优先使用引用表达“不能为空”

当对象必须存在时，优先使用引用而非指针。

### 12.3 裸指针默认不表达所有权

裸指针默认仅表示：

- 非拥有关系
- 可空观察关系

若对象所有权由当前类型持有，必须优先使用智能指针或显式资源封装。

### 12.4 智能指针使用规则

- 独占所有权使用 `std::unique_ptr`
- 共享所有权使用 `std::shared_ptr`
- 非拥有弱引用使用 `std::weak_ptr`

禁止无理由使用 `std::shared_ptr` 代替清晰的所有权设计。
使用 `std::shared_ptr` 时必须能解释共享所有权来源与生命周期边界。
禁止无设计地形成引用环。

### 12.5 禁止业务代码直接使用裸 `new` / `delete`

业务代码、模块逻辑代码、应用层代码禁止直接使用裸 `new` / `delete`。

仅允许在以下场景中出现：

- 分配器实现
- 底层资源封装
- 容器实现
- 平台适配层

出现后必须立即封装为明确的拥有型对象，不得裸露传播。

### 12.6 容器与缓冲区类型选择

- 固定大小集合必须使用 `std::array`
- 默认禁止声明 C 风格数组（如 `T values[N]`）；仅在语言规则要求、字符串字面量初始化、或 C ABI / 系统调用 / 第三方库边界约束下允许例外
- 出现上述例外时，作用域必须尽量收敛在边界层，并在进入项目内部语义后尽快转换为 `std::array`、`std::span` 或其他明确的标准类型
- 动态集合优先使用 `std::vector`
- 稀疏键值集合优先使用语义明确的标准关联容器
- 不得为“避免写模板”而退回裸数组或手工内存管理

### 12.7 公共接口中的数组与字节序列边界

公共接口禁止无语义的“裸指针 + 长度”数组风格，除非该接口受 C ABI、系统调用或第三方库约束。

原始字节数据必须优先使用显式字节类型或明确的视图类型，不得长期以模糊的 `char*` / `void*` 表达二进制语义。

## 13. 并发与同步

### 13.1 共享可变状态必须有明确同步策略

只要状态可能被多个线程访问，就必须明确其同步方式、可见性要求和所有权边界。
禁止依赖“通常不会同时访问”的经验假设。

### 13.2 禁止数据竞争

共享可变状态不得在无同步保护的情况下被并发读写或并发写写。

### 13.3 加锁必须使用 RAII 锁对象

互斥锁的获取与释放必须通过 RAII 锁对象管理，例如 `std::lock_guard`、`std::unique_lock` 或等价封装。
禁止通过手工 `lock()` / `unlock()` 维持常规控制流。

### 13.4 多把锁必须规定固定顺序

当代码路径可能同时持有多把锁时，必须规定固定加锁顺序，防止死锁。

### 13.5 `condition_variable` 必须配合谓词循环

等待条件变量时必须使用谓词循环或等价形式，禁止假设一次唤醒即满足条件。

### 13.6 不得在持锁期间调用不受控外部代码

持锁期间不得调用回调、虚函数、第三方代码或其他不可控的外部逻辑，除非已明确证明不会造成死锁、重入或长时间阻塞。

### 13.7 `atomic` 仅用于简单同步

`std::atomic` 仅用于简单状态同步、计数或无锁原子读写。
禁止以零散原子变量替代清晰的整体并发设计。

## 14. 初始化规则

### 14.1 变量定义时必须初始化

禁止未初始化的变量。

适用范围包括但不限于：

- 局部变量
- 成员变量
- 静态成员变量
- 文件级静态变量
- 全局变量
- 指针
- 布尔值
- 计数器
- 句柄
- 容器
- 智能指针

允许：

```cpp
int count {};
Foo* foo {nullptr};
Bar value {};
std::size_t size {};
std::vector<int> values {};
std::unique_ptr<int> handle {};
```

禁止：

```cpp
int count;
Foo* foo;
std::size_t size;
std::vector<int> values;
std::unique_ptr<int> handle;
```

### 14.2 优先使用直接初始化或统一初始化

变量、成员变量、静态变量和常量在初始化时，默认必须优先使用统一初始化 `{}`
而非依赖隐式默认状态。

适用场景包括：

- 默认初始化
- 零值初始化
- 直接初始化
- 聚合类型初始化
- 容器初始化

推荐：

```cpp
int count {};
bool enable_logging {false};
std::string name {"demo"};
std::vector<int> values {1, 2, 3};
DemoState state {};
```

不推荐：

```cpp
int count = 0;
bool enable_logging = false;
std::string name = "demo";
DemoState state;
```

允许例外：

- 为了调用构造函数或工厂函数，使用 `()` 或 `=` 明显更清晰时
- 移动构造、拷贝构造等必须使用初始化列表语法的场景
- 历史接口、第三方接口或宏约束导致无法自然使用 `{}` 的场景

即使存在允许例外，也必须保证初始化语义显式、可读、可审查。

对于以下这类本身就具备稳定默认状态的类型，即使“不写也通常能工作”，也必须优先显式写出 `{}`
初始化，禁止依赖读者推断其默认构造语义：

- `std::string`
- `std::string_view`
- `std::optional<T>`
- 标准容器
- 智能指针
- 轻量值语义包装类型

推荐：

```cpp
std::string name {};
std::string_view key {};
std::optional<uint32_t> entry_id {};
std::vector<int> values {};
std::unique_ptr<Foo> impl {};
```

不推荐：

```cpp
std::string name;
std::string_view key;
std::optional<uint32_t> entry_id;
std::vector<int> values;
std::unique_ptr<Foo> impl;
```

### 14.3 布尔、计数器、句柄、指针必须显式初始化

布尔值、计数器、句柄和指针必须显式初始化，禁止依赖“稍后再赋值”的模糊约定。

推荐：

```cpp
bool is_ready {false};
std::size_t retry_count {};
TaskHandle task_handle {};
Foo* foo {nullptr};
```

禁止：

```cpp
bool is_ready;
std::size_t retry_count;
TaskHandle task_handle;
Foo* foo;
```

### 14.4 成员变量必须在声明处显式初始化

只要成员存在稳定、合理的默认值，就必须在声明处显式初始化，默认优先使用 `{}`
形式。

推荐：

```cpp
class RenderEngine {
private:
    EngineCreateInfo m_create_info {};
    RuntimeInfo m_runtime_info {};
    IEngineLogger* m_logger {nullptr};
    bool m_is_initialized {false};
    std::size_t m_frame_count {};
};
```

若成员必须通过构造函数参数初始化，也建议先提供安全默认值，再由构造函数初始化列表
覆盖。

若成员类型具备稳定默认状态，则该规则同样适用于标准库类型和轻量包装类型，不得仅因为“默认构造本来就是空值”
而省略初始化。例如：

```cpp
class CacheStore {
private:
    std::string m_name {};
    std::string_view m_debug_label {};
    std::optional<uint32_t> m_cached_entry_id {};
    std::unique_ptr<int> m_impl {};
};
```

### 14.5 静态成员变量、文件级变量与全局变量必须显式初始化

静态成员变量、文件级静态变量和全局变量必须显式初始化。

推荐：

```cpp
inline static std::size_t s_instance_count {};

namespace {
bool g_enable_logging {false};
constexpr std::size_t k_default_capacity {64};
}
```

若因兼容性或工程结构需要采用类外定义，则必须在定义处显式初始化：

```cpp
std::size_t RenderEngine::s_instance_count = 0;
```

禁止仅声明而不定义初始化值，或依赖读者推断其零初始化语义。

## 15. 条件语句与循环

### 15.1 必须始终使用花括号

`if`、`else`、`for`、`while`、`do while` 的语句体必须始终使用花括号。
即使语句体只有一行，也不得省略。
若 `.clang-format` 将短小的带花括号语句体压成单行，视为符合规范。

允许：

```cpp
if (is_ready) {
    return true;
}
```

禁止：

```cpp
if (is_ready) return true;
```

### 15.2 条件表达式必须清晰

复杂条件必须拆分为具名布尔变量。
禁止堆叠难以阅读的长条件。

### 15.3 禁止在条件中混入复杂副作用

条件判断中不得塞入大量赋值、资源修改或流程控制逻辑。

### 15.4 `switch` 必须保持穷尽与可审查

针对 `enum class` 的 `switch` 必须穷尽处理所有有效枚举值。

为了让新增枚举值能尽早暴露问题，优先避免机械性 `default` 分支。
仅在确有必要处理未知值、外部脏数据或兼容旧协议时才允许使用 `default`。

## 16. 错误处理

### 16.1 必须显式处理错误路径

可能失败的操作必须有清晰错误处理路径。
禁止默默吞掉失败。

### 16.2 受控异常策略

项目默认不鼓励业务代码随意抛异常。
允许在局部实现层使用异常表达真正异常的失败，例如构造失败、资源获取失败、无法在当前层合理处理的不变量破坏。

必须遵循以下规则：

- 禁止使用异常表达常规业务分支
- 禁止异常跨越线程入口、模块公共 API、动态库导出接口、C 接口、回调入口、`noexcept` 函数、析构函数边界传播
- 边界层必须捕获异常并转换为稳定错误表示，或记录后终止
- 标准库或第三方库内部异常不得作为业务流程依赖

### 16.3 返回值语义必须明确

函数返回值必须表达明确语义，例如：

- 成功 / 失败
- 有效 / 无效
- 对象 / 空对象
- 状态码
- 结果类型

### 16.4 断言与运行时错误必须区分

- 断言用于程序员错误、不变量破坏、非法状态
- 正常运行时失败必须走正式错误处理逻辑

禁止使用断言替代正常错误处理。

## 17. 宏使用规则

### 17.1 宏必须最小化

除以下场景外，禁止新增普通宏：

- 条件编译
- 平台配置
- 日志
- 断言
- 少量无法由语言特性替代的封装

### 17.2 能不用宏就不用宏

能使用以下方式时，必须禁止用宏替代：

- `constexpr`
- `inline`
- 模板
- `enum class`
- 普通函数

### 17.3 宏命名以命名规范为准

所有宏命名必须遵循 `docs/engineering/cpp_naming_convention.md`。
禁止无前缀宏。

## 18. 注释规范

### 18.1 注释必须说明“为什么”

注释必须优先说明：

- 为什么这样实现
- 为什么需要这个约束
- 为什么不能改成别的方式
- 代码背后的边界条件与背景

禁止重复代码本身已清楚表达的事实。

禁止：

```cpp
// Increment count
++count;
```

允许：

```cpp
// 必须在提交前递增，否则缓存键会与上一轮结果冲突。
++count;
```

### 18.2 公共接口必须具备必要说明

公共类型、公共函数、跨模块接口，在语义不明显时必须补充注释，说明：

- 功能
- 参数语义
- 返回值语义
- 生命周期约束
- 线程约束
- 所有权约束

使用 `std::string_view`、`std::span`、引用、裸指针等非拥有视图类型时，必须明确注明生命周期约束。

### 18.3 公共接口优先使用 Doxygen 风格注释

公共类型、公共函数、跨模块接口、对外暴露的抽象接口，优先使用 Doxygen 风格注释。
统一优先使用 `///` 单行风格，而非块注释风格。

推荐形式：

```cpp
/// @brief 创建并初始化缓存存储。
/// @details 调用成功后对象进入可用状态；失败时返回 `false`，对象保持未初始化状态。
/// @param config_desc 缓存存储的配置描述对象。
/// @return 初始化是否成功。
bool Initialize(const CacheStoreDesc& config_desc);
```

最低要求：

- `@brief`：必须概括接口职责
- `@details`：在语义、边界、失败模式、线程约束不明显时必须补充
- `@param`：参数语义不直观、存在约束或生命周期要求时必须补充
- `@return`：返回值不是显而易见的成功/失败语义时必须补充

以下场景必须优先补充 `@details`：

- 生命周期有要求
- 所有权转移不明显
- 线程安全有前提
- 调用顺序有约束
- 失败后对象状态不显然
- 与外部资源、系统状态或协议交互

禁止在同一项目中长期混用多套公开接口注释风格。

### 18.4 内部函数和内部接口注释以简洁为先

仅在当前文件、当前类或当前模块内部使用的函数、类型和辅助逻辑，不要求机械补齐完整 Doxygen 标签。
内部实现优先使用简洁注释，重点解释：

- 为什么存在这段逻辑
- 哪个边界条件不明显
- 为什么不能直接改成更直观的写法

内部辅助函数、局部变量附近注释应尽量短小，避免把实现细节逐行翻译成自然语言。

推荐示例：

```cpp
// 在提交前统一规整名称，避免同一逻辑路径产生重复缓存键。
NormalizeTaskName(task_name);
```

```cpp
// 这里必须保留旧协议分支；上游仍有存量数据使用该编码格式。
if (header.version == 1) {
    return ParseLegacyHeader(header);
}
```

### 18.5 公共接口注释建议模板

当接口会被其他模块、其他团队或外部调用方使用时，推荐优先按以下形式书写：

```cpp
/// @brief 创建任务对象并返回任务信息。
/// @details
/// - 成功时返回有效任务信息。
/// - 失败时返回无效结果，对象保持原有状态不变。
/// - 调用方必须保证 `task_name` 在调用期间保持有效。
/// @param task_name 任务名称，不允许为空。
/// @param enable_logging 是否启用任务日志。
/// @return 创建后的任务信息；失败时 `is_valid` 为 `false`。
TaskInfo CreateTask(std::string_view task_name, bool enable_logging);
```

对于公共类或抽象接口，推荐至少描述职责边界和使用前提：

```cpp
/// @brief 提供任务存储的统一抽象接口。
/// @details
/// - 实现类必须明确线程安全语义。
/// - 除非文档另有说明，接口本身不转移传入对象所有权。
/// - 初始化成功后方可调用创建和查询接口。
class ITaskStore {
public:
    virtual ~ITaskStore() = default;
};
```

### 18.6 注释内容必须面向调用方和维护者

公共接口注释必须站在调用方视角，回答“我该怎么用、什么时候会失败、有哪些边界”；
内部实现注释必须站在维护者视角，回答“为什么这么做、哪些地方不能轻易改”。

禁止出现以下低价值注释：

- 只把函数名翻译成自然语言
- 只把 if / for / return 的表面行为重复一遍
- 只描述“做了什么”而不解释原因或边界

### 18.7 参数、返回值与失败语义的注释要求

满足以下任一条件时，必须补充 `@param` / `@return` / `@details`：

- 参数含义不直观
- 参数存在合法范围、单位、编码或格式约束
- 参数或返回值存在生命周期要求
- 返回值包含失败语义、空值语义或部分成功语义
- 接口会修改对象状态、缓存、外部资源或全局状态

对于具备错误码、状态对象、可选返回值或结果对象的接口，必须明确说明：

- 成功条件
- 失败条件
- 失败后的对象状态
- 调用方是否需要重试、清理或补偿

### 18.8 注释必须与代码保持同步

代码语义、参数含义、线程模型、失败行为、所有权关系发生变化时，相关注释必须同步修改。
过期注释、误导性注释与错误代码同等对待。

禁止保留以下注释：

- 已不再反映当前行为的历史说明
- 与实现相矛盾的参数或返回值说明
- 已被重构废弃但未删除的旧约束描述

### 18.9 禁止注释掉代码作为长期保留手段

禁止通过大段注释掉旧代码、旧分支或旧实现来充当版本管理。
需要保留历史实现时，应依赖版本控制系统，而不是长期保留注释代码。

仅允许在极短期、明确带归属和清理计划的过渡场景下临时保留，并必须配套 `TODO(owner): ...` 或 `FIXME(owner): ...`。

### 18.10 注释风格必须稳定统一

面向 API 文档生成或公共接口说明时，统一优先使用：

```cpp
/// @brief ...
/// @details ...
```

面向内部实现说明时，统一优先使用：

```cpp
// ...
```

除非工具链明确要求，否则不鼓励新增混杂风格，例如同一文件内长期混用多种块注释、星号对齐注释、Doxygen 块注释和随意格式的行注释。

### 18.11 TODO / FIXME / HACK 必须规范书写

必须统一格式：

```cpp
// TODO(owner): ...
// FIXME(owner): ...
// HACK(owner): ...
```

禁止写无归属、无上下文的临时标记。

## 19. 日志与断言

### 19.1 日志必须服务于定位问题

日志必须提供足够上下文。
禁止输出无关键值、无语义的信息性噪音。

### 19.2 断言必须表达不可接受状态

断言仅用于：

- 不变量破坏
- 理论上不应发生的状态
- 调用方违反前置条件

### 19.3 日志和断言不得隐藏副作用

传入日志宏、断言宏的表达式不得依赖副作用保证程序逻辑正确。

禁止：

```cpp
PROJ_ASSERT(AdvanceState());
```

## 20. 测试代码规则

### 20.1 测试代码必须遵循同一风格

测试代码不得自成一套风格。
命名、注释、类型组织、语言特性使用必须遵循本规范及相关文档。

### 20.2 测试类型命名必须表达角色

测试夹具、假实现、桩对象、Mock 类型命名必须清晰表达角色。

允许：

- `TaskQueueTest`
- `FakeFileStore`
- `MockLogger`

## 21. 标准示例

```cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace proj::storage {
enum class LookupResult { Hit, Miss };

struct CacheEntryDesc {
    uint64_t size {0};
    bool enable_persistence {false};
};

class ICacheStore {
public:
    virtual ~ICacheStore() = default;

    virtual bool Initialize()                                                   = 0;
    virtual void Shutdown()                                                     = 0;
    [[nodiscard]] virtual LookupResult GetLookupResult(uint32_t entry_id) const = 0;
    virtual bool HasEntry(uint32_t entry_id) const                              = 0;
    virtual void OnEntryChanged()                                               = 0;
};

class LocalCacheStore final : public ICacheStore {
public:
    static constexpr uint32_t k_invalid_entry_id = 0xffffffffu;

public:
    LocalCacheStore()           = default;
    ~LocalCacheStore() override = default;

    LocalCacheStore(const LocalCacheStore&)                = delete;
    LocalCacheStore& operator=(const LocalCacheStore&)     = delete;
    LocalCacheStore(LocalCacheStore&&) noexcept            = default;
    LocalCacheStore& operator=(LocalCacheStore&&) noexcept = default;

    bool Initialize() override;
    void Shutdown() override;
    [[nodiscard]] LookupResult GetLookupResult(uint32_t entry_id) const override;
    bool HasEntry(uint32_t entry_id) const override;
    void OnEntryChanged() override;

    [[nodiscard]] std::optional<uint32_t> TryFindEntry(std::string_view key) const;
    [[nodiscard]] uint32_t CreateEntry(const CacheEntryDesc& entry_desc);
    [[nodiscard]] const std::string& GetName() const noexcept;
    void UpdateEntries(std::span<const uint32_t> entry_ids);

private:
    void CreateDefaultEntries();
    bool IsValidEntryId(uint32_t entry_id) const noexcept;

private:
    std::vector<uint32_t> m_entry_ids;
    std::string m_name;
    bool m_is_initialized {false};
    std::unique_ptr<int> m_impl;

    static uint32_t s_instance_count;
};
}  // namespace proj::storage

namespace {
constexpr uint32_t k_default_capacity {64};

bool g_enable_logging {false};

bool IsValidName(std::string_view name) { return !name.empty(); }
}  // namespace

#define PROJ_ASSERT(expr)
#define PROJ_LOG_INFO(...)
```
