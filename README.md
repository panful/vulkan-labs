
**Vulkan学习记录**
## 一、Vulkan
[Vulkan 官网](https://www.vulkan.org/)
[Vulkan 规范](https://registry.khronos.org/vulkan/specs/1.2/pdf/vkspec.pdf)
[Vulkan Tutorial](https://vulkan-tutorial.com/)
[Vulkan SaschaWillems](https://github.com/SaschaWillems/Vulkan)
## 二、Vulkan使用
### 1. Windows下使用Vulkan
[vulkan_sdk for Windows](https://vulkan.lunarg.com/sdk/home#windows)

下载sdk之后安装即可使用

### 2. Linux下使用Vulkan
[Ubuntu 22.04 (Jammy Jellyfish) 安装vulkan_sdk](https://vulkan.lunarg.com/doc/sdk/1.3.236.0/linux/getting_started_ubuntu.html)

```shell
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
sudo apt update
sudo apt install vulkan-sdk
```
如果`sudo apt install vulkan-sdk`报错，检查前两行命令是否执行成功，成功后再次update再安装

## 三、编译Shader

**shader文件必须以vert/frag结尾**

glslc是谷歌的编译器（推荐使用）
```shell
glslc base.vert -o vert.spv
glslc base.frag -o frag.spv
```

glslangValidator是官方的编译器
```shell
glslangValidator -V -o vert.spv base.vert
glslangValidator -V -o frag.spv base.frag
```
