# CMake 规范

## 1. 适用范围

本规范适用于仓库中的所有 CMake 相关文件，包括但不限于：

- 根目录 `CMakeLists.txt`
- 子目录 `CMakeLists.txt`
- `*.cmake` 模块文件
- `CMakePresets.json`
- `CMakeUserPresets.json`

本文件只约束 CMake 组织方式、依赖表达、选项设计和构建约定，不重复定义 C++ 命名或源码风格规则。

## 2. 总体原则

### 2.1 一切围绕 target 组织

所有构建配置必须围绕具体 target 展开。
必须优先使用 `target_*` 系列接口表达 include 路径、链接依赖、编译定义和编译选项。

允许：

```cmake
target_include_directories(my_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(my_app PRIVATE my_lib)
target_compile_definitions(my_lib PRIVATE MY_LIB_ENABLE_TRACE=1)
target_compile_options(my_lib PRIVATE /W4)
```

禁止长期依赖以下全局配置方式：

- `include_directories()`
- `link_libraries()`
- `add_definitions()`
- `add_compile_options()` 被当作全仓库默认手段滥用

### 2.2 配置必须最小作用域化

依赖、宏、编译选项和 include 路径必须施加到最小必要范围。
不要为了图省事，把本应属于单个 target 的配置提升到目录级或全局级。

### 2.3 保持可读、可定位、可扩展

CMake 代码必须让评审者能够快速回答以下问题：

- 这个 target 产出什么
- 它依赖谁
- 它向外暴露什么
- 哪些配置只影响自己
- 哪些配置会传递给下游

## 3. 版本、语言与基础配置

### 3.1 顶层必须显式声明最低版本

根 `CMakeLists.txt` 必须显式写 `cmake_minimum_required(...)`。
版本选择必须与仓库中的 `CMakePresets.json` 和 CI 环境保持一致。

当前仓库统一要求 CMake 3.25 及以上。

### 3.2 顶层必须显式声明项目语言

根 `project(...)` 必须声明项目名、版本、描述和语言列表。
语言列表必须只包含当前仓库真实需要的语言。

### 3.3 C++ 标准必须集中定义

顶层必须统一设置：

- `CMAKE_CXX_STANDARD`
- `CMAKE_CXX_STANDARD_REQUIRED`
- `CMAKE_CXX_EXTENSIONS`

当前仓库默认使用 C++20，并关闭编译器扩展。

## 4. 依赖与可见性

### 4.1 必须正确使用 PRIVATE / PUBLIC / INTERFACE

依赖和属性传播范围必须精确表达：

- `PRIVATE`：仅当前 target 自己使用
- `PUBLIC`：当前 target 使用，且其使用者也需要感知
- `INTERFACE`：当前 target 自身不编译该内容，但其使用者需要继承

常见判断方式：

- 头文件中暴露的依赖通常是 `PUBLIC`
- 实现文件内部才需要的依赖通常是 `PRIVATE`
- 纯头文件库、告警集合、公共编译选项集合通常是 `INTERFACE`

### 4.2 优先链接 target，不直接传播裸变量

依赖第三方库或内部模块时，优先链接明确的 target。
不要长期依赖手工拼接库路径、全局变量或目录变量来表达传递依赖。

优先：

```cmake
target_link_libraries(my_app PRIVATE fmt::fmt my_project_core)
```

谨慎使用：

```cmake
target_link_libraries(my_app PRIVATE ${SOME_LIBRARIES})
```

### 4.3 公共头路径必须最小化

`target_include_directories()` 中的 `PUBLIC` 和 `INTERFACE` 路径必须尽量少。
不要把仅实现内部使用的目录暴露给下游 target。

## 5. Target 组织

### 5.1 一个 target 表达一个清晰职责

库目标、可执行目标、测试目标必须职责清晰。
不要把彼此独立的功能长期堆进一个超大 target。

### 5.2 共享构建属性优先抽成 INTERFACE target

跨多个 target 复用的告警级别、默认编译选项、通用定义，优先抽成 `INTERFACE` target 集中管理。

当前仓库中的 `project_warnings` 就属于这类目标。

### 5.3 目标命名必须稳定且可搜索

target 名称必须体现模块职责，避免临时、模糊或纯缩写命名。
测试 target 应与被测模块形成稳定映射，便于在 IDE、CI 和测试日志中搜索。

## 6. 源文件与目录组织

### 6.1 不使用 GLOB 自动搜集源码

生产代码和测试代码的源文件列表必须显式写在 `add_library()`、`add_executable()` 或 `target_sources()` 中。

禁止使用以下方式长期维护目标源文件：

```cmake
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS *.cpp *.h)
```

显式列文件虽然更啰嗦，但在大型项目中更利于审查、增量构建诊断和变更追踪。

### 6.2 按目录边界拆分 CMakeLists

当模块已形成清晰目录边界时，应通过子目录 `CMakeLists.txt` 拆分构建逻辑。
根 `CMakeLists.txt` 负责顶层策略、全局选项和子目录编排，不承担所有模块的具体实现细节。

### 6.3 不在 CMake 中偷偷改源目录内容

生成文件、配置产物和中间文件必须写入构建目录或明确的输出目录。
禁止让常规配置流程直接回写源码目录中的普通源文件。

## 7. 选项与可选依赖

### 7.1 可选功能必须显式建模为 option

可选能力、实验开关和第三方依赖开关必须通过 `option(...)` 明确建模，不要依赖隐式环境状态。

选项命名应满足：

- 语义完整
- 默认值清晰
- 打开和关闭后的行为可预测

### 7.2 可选第三方依赖必须优雅降级

新增可选第三方依赖时，应先定义 option，再执行 `find_package(...)` 或等价探测。
若依赖未安装且该能力是可选项，应关闭该能力并输出明确的 `message(STATUS ...)`，保证未安装依赖的开发者仍可配置和构建主工程。

这也是当前仓库已有规则的延伸。

### 7.3 避免把本地环境差异编码进仓库默认配置

不要在仓库默认 CMake 配置中硬编码个人机器路径、私有 SDK 目录或只适用于单一开发机的变量。
这类差异应放入本地 preset、环境变量或开发机专属覆盖配置。

## 8. Preset 与构建目录

### 8.1 优先使用 CMake Presets

仓库统一使用 `CMakePresets.json` 描述标准构建入口。
新增构建变体时，优先扩展 preset，而不是要求开发者手工记忆复杂命令。

### 8.2 构建目录必须与源码目录分离

必须坚持 out-of-source build。
构建目录应由 preset 统一管理，避免在源码目录散落临时产物。

当前仓库约定 `binaryDir` 为：

```text
build/<preset-name>
```

### 8.3 Preset 层次应简洁可复用

多个 preset 之间的公共配置应抽到隐藏 base preset。
平台、编译器、构建类型和附加能力应按继承关系拆分，避免重复拷贝大段配置。

## 9. 测试接入

### 9.1 测试能力通过 CTest 集成

需要测试时，顶层应使用 `include(CTest)`，并通过 `BUILD_TESTING` 控制测试目录是否参与构建。

### 9.2 测试 target 必须显式注册

测试可执行程序创建后，必须使用 `add_test(...)` 显式注册到 CTest。
测试名应稳定、可读、可搜索。

### 9.3 测试依赖只链接最小必要目标

测试 target 应优先链接被测模块，而不是重复拼接其底层依赖。
这样可以减少测试和生产目标之间的漂移。

## 10. 编译选项与工具接入

### 10.1 编译器相关选项必须集中管理

编译器相关告警和行为开关应按编译器分支集中管理，例如 `if(MSVC) ... else() ...`。
不要把同一套告警参数零散复制到多个 target。

### 10.2 静态检查工具接入必须可开关

像 `clang-tidy` 这类工具应通过显式 option 或 preset 控制，而不是默认强绑在所有开发者配置上。
启用后若工具缺失，应报出清晰错误。

当前仓库的 `ENABLE_CLANG_TIDY` 就采用了这种做法。

## 11. 消息与失败策略

### 11.1 错误必须尽早暴露

当缺少必需编译器、必需依赖或配置前提不满足时，应尽早 `message(FATAL_ERROR ...)`，避免把问题拖到编译阶段才暴露。

### 11.2 状态信息必须清晰且克制

对可选依赖关闭、构建模式变化、关键路径选择等信息，应使用 `message(STATUS ...)` 提示。
禁止输出大量噪声日志，影响开发者定位真正的问题。

## 12. 代码示例

```cmake
cmake_minimum_required(VERSION 3.25)

project(
  render_engine
  VERSION 0.1.0
  DESCRIPTION "A Vulkan render engine for CAD, BIM, and point cloud workloads"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Enable building tests" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy during compilation" OFF)
include(CTest)

add_library(project_warnings INTERFACE)

add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
```
