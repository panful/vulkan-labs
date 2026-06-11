# Agent Guidelines

本文件是本仓库所有 AI Agent（Codex / Claude 等）的统一行为规范入口。

## 适用范围

本文件中的要求适用于本仓库内的所有代码修改。

## C++ 规则

- 修改或新增 C++ 代码时，命名规则遵循 `docs/engineering/cpp_naming_convention.md`。
- 修改或新增 C++ 代码时，源码级风格规则遵循 `docs/engineering/cpp_coding_style.md`。
- 修改或新增 `CMakeLists.txt`、`*.cmake` 或 Preset 文件时，构建规则遵循 `docs/engineering/cmake_convention.md`。
- 如果两个文档存在交叉或看起来有重叠，命名问题以 `docs/engineering/cpp_naming_convention.md` 为准，源码风格问题以 `docs/engineering/cpp_coding_style.md` 为准。

## 校验与工具

- 如果仓库中存在格式化、静态检查或其他自动化工具配置，则以仓库工具输出作为可机械执行规则的最终依据。
- 不要手工引入与仓库自动化结果相冲突的格式。

## 歧义处理

- 如果某个现有文件明显遵循一套有意为之的局部约定，且与仓库级规则冲突，则在当前任务不是统一风格的前提下，优先保持该文件内部一致性。
- 如果局部无法消解规则冲突，则优先采用更严格、且更容易评审的写法。
