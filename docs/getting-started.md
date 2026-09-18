# Getting Started

## Requirements

- CMake
- Ninja or Visual Studio generator
- Vulkan SDK
- A C++20 compiler

## Configure

```shell
cmake --preset msvc-debug
```

## Build

```shell
cmake --build --preset msvc-debug
```

Executables are generated under `build/<preset>/bin`.

## Compile Shaders

```shell
python scripts/compile_shaders.py
```

Compiled shader files are written to `assets/shaders`.

## Runtime Assets

Examples use the `PROJECT_ASSETS_DIR` compile definition to locate files under `assets/`, so assets do not need to be copied beside the executable.
