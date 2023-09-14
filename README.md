
**Vulkan学习记录**
## 一、Vulkan
[Vulkan 官网](https://www.vulkan.org/)
[Vulkan 规范](https://registry.khronos.org/vulkan/specs/1.2/pdf/vkspec.pdf)
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

**shader文件必须以`vert/frag`结尾**

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
## 四、source目录结构
### 01_VulkanTutorial
[Vulkan Tutorial](https://vulkan-tutorial.com/)
[Vulkan Tutorial 翻译](https://github.com/fangcun010/VulkanTutorialCN)

- 01_helloTriangle
    Vulkan绘制一个三色三角形：
    - 创建一个 VkInstance
    - 选择一个支持 Vulkan 的图形设备 (VkPhysicalDevice)
    - 为绘制和显示操作创建 VkDevice 和 VkQueue
    - 创建一个窗口，窗口表面和交换链
    - 将交换链图像包装进 VkImageView
    - 创建一个渲染层指定渲染目标和使用方式
    - 为渲染层创建帧缓冲
    - 配置图形管线
    - 为每一个交换链图像分配指令缓冲
    - 从交换链获取图像进行绘制操作，提交图像对应的指令缓冲，返回图像到交换链
- 02_vertexBuffers
    顶点缓冲、索引缓冲的使用
- 03_uniformBuffers
    Uniform Buffers的使用，给顶点着色器传递一个MVP矩阵
- 04_textureMapping
    纹理的使用
- 05_depthBuffering
    开启深度测试
- 06_loadingModels
    加载一个模型，使用纹理、开启深度测试、传递MVP矩阵
- 07_generatingMipmaps
    细化纹理贴图 Mipmap
- 08_multiSampling
    多重采样抗锯齿
- 09_computeShader
    计算着色器的使用
### 02_SaschaWillems
[Vulkan SaschaWillems](https://github.com/SaschaWillems/Vulkan)

- 01_triangles
绘制多个三角形，创建多个`vertex buffer`和`index buffer`，在一次渲染循环中`DrawFrame()`多次调用`vkCmdDraw`或`vkCmdDrawIndexed`
- 02_dynamicUniformBuffers
`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`的使用
对不同的图元设置不同的`uniform`，比如一次渲染循环需要绘制多个三角形，每个三角形使用不同的`uniform`设置不同的颜色
- 03_pipelines
创建多个`pipeline`，每个`pipeline`对应不同的绘制模式（点、线、面），多边形模式（填充、线框）
- 04_pushconstants
类似`Uniform Buffer`，但是不需要创建`descriptor sets`，用起来和OpenGL的`uniform`一样简单，缺点是数据块大小不能太大
在`VkPipelineLayoutCreateInfo`中添加`push_constant`信息，使用`vkCmdPushConstants`提交`push_constant`数据
- 05_specializationConstants
创建多个`pipeline`，每个`pipeline`设置不同的着色器常量(`constant_id`)
`constant_id`可以在着色器编译时就将常量写入，并且不可更改，避免了着色器内部的分支展开，可以优化着色器的代码，运行时性能也更好，`uber(uniform branch)`使用`uniform`控制的分支要比`constant_id`控制慢20%-30%