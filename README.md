
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

- glslc是谷歌的编译器（推荐使用）
```shell
glslc base.vert -o vert.spv
glslc base.frag -o frag.spv
```

- glslangValidator是官方的编译器
```shell
glslangValidator -V -o vert.spv base.vert
glslangValidator -V -o frag.spv base.frag
```

## 四、source目录结构
### 01_VulkanTutorial
[Vulkan Tutorial](https://vulkan-tutorial.com/)
[Vulkan Tutorial 翻译](https://github.com/fangcun010/VulkanTutorialCN)

- 01_helloTriangle
    绘制一个三色三角形，在顶点着色器中提供顶点以及颜色数据
- 02_vertexBuffers
    顶点缓冲、索引缓冲的使用
- 03_uniformBuffers
    Uniform Buffers的使用，给顶点着色器传递一个MVP矩阵
    多个 DescriptorSet 的使用
- 04_textureMapping
    纹理的使用
- 05_depthBuffering
    开启深度测试。使用步骤：创建图形管线时开启深度测试`VkPipelineDepthStencilStateCreateInfo`，创建深度测试使用的资源`VkImage VkDeviceMemory VkImageView`，设置正确的pass信息，将深度图形附加到FrameBuffer，绘制循环开始时清除深度信息即可。Z值越大，距离眼睛越远。**注意**：如果开启了背面剔除，不开启深度测试和开启深度测试时绘制的图像看起来可能是一样的。

**NDC坐标系**
|       | Vulkan | OpenGL|
|-------|--------|-------|
|+x      |从左到右   | 从左到右|
|+y      |从上到下   | 从下到上|
|+z      |从外到内   | 从外到内|
|z值范围  |[0, 1]    | [-1,1]|
|坐标系   | 右手      | 左手|

**以上只适用于OpenGL的可编程渲染管线模式，在立即渲染模式下，OpenGL使用的是右手坐标系**

- 06_loadingModels
    加载一个模型，使用纹理、开启深度测试、传递MVP矩阵
- 07_generatingMipmaps
    细化纹理贴图 Mipmap
- 08_multiSampling
    多重采样抗锯齿
- 09_computeShader
计算着色器的使用，`vkCmdDispatch`的参数表示全局工作组的大小，Shader中的`layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;`表示本地（局部）工作组的大小。两个工作组的乘积就是GPU线程的数量，这个数量应该不小于粒子的数量。
图形管线使用`vkCmdDraw`等函数提交绘制命令，计算管线使用`vkCmdDispatch`等函数提交计算命令。
**计算着色器使用时主要有以下步骤**：
创建描述符布局 vkCreateDescriptorSetLayout
创建管线 vkCreatePipelineLayout vkCreateComputePipelines
创建指令池（可选，当图形队列和计算队列不是同一个时，就需要创建用于计算着色器的指令池）vkCreateCommandPool
创建描述符集 vkAllocateDescriptorSets vkUpdateDescriptorSets
创建指令缓冲 vkAllocateCommandBuffers
创建同步对象 vkCreateSemaphore vkCreateFence
- 10_buildCommandBuffers
只创建“一次”命令缓冲，因为每一帧记录到命令缓冲中的所有命令都是一样的，所以不用每一次渲染循环都创建一次命令缓冲，只需要统一将命令全部记录到 CommandBuffer 中，然后每一次渲染循环都使用该 CommandBuffer 即可。需要保证交换链中的图像数量和GPU最大并行帧数一致，当窗口大小改变后（Framebuffer会改变）需要重新记录一次 CommandBuffer 。只要 CommandBuffer 中使用的资源以及命令没有改变就不需要重新记录。
### 02_advance
[Vulkan SaschaWillems](https://github.com/SaschaWillems/Vulkan)

- 01_triangles
绘制多个三角形，创建多个`vertex buffer`和`index buffer`，在一次渲染循环中`DrawFrame()`多次调用`vkCmdDraw`或`vkCmdDrawIndexed`
- 02_dynamicUniformBuffers
`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`的使用
对不同的图元设置不同的`uniform`，比如一次渲染循环需要绘制多个三角形（多次DrawCall），每个三角形设置不同的颜色。使用的是同一个`DynamicUniformBuffer`，每一次DrawCall通过不同的偏移值读取不同的颜色值
- 03_pipelines
创建多个`pipeline`，每个`pipeline`对应不同的绘制模式（点、线、面），多边形模式（填充、线框）。每个`pipeline`在不同的`viewport`上绘制。
- 04_pushconstants
类似`Uniform Buffer`，但是不需要创建`descriptor sets`，用起来和OpenGL的`uniform`一样简单，缺点是数据块大小不能太大
在`VkPipelineLayoutCreateInfo`中添加`push_constant`信息，使用`vkCmdPushConstants`提交`push_constant`数据
- 05_specializationConstants
创建多个`pipeline`，每个`pipeline`设置不同的着色器常量(`constant_id`)
`constant_id`可以在着色器编译时就将常量写入，并且不可更改，避免了着色器内部的分支展开，可以优化着色器的代码，运行时性能也更好，`uber(uniform branch)`使用`uniform`控制的分支要比`constant_id`控制慢20%-30%
- 06_inputAttachments
利用subpass将颜色附件、深度附件显示到屏幕，在**01_06_loadingModels**的基础上更改
- 07_imgui
ImGui的使用，如果需要添加控件，只需要修改函数`PrepareImGui()`即可，在**01_01_helloTriangle**的基础上更改。**注意**：如果使用多个subpass记得初始化ImGui时设置正确的subpass
- 08_deferredShading
使用subpass延迟着色
- 09_offscreen
RenderTarget离屏渲染，将需要绘制的场景先绘制到一个离屏的帧缓冲上，然后将这个帧缓冲的附件（VkImage）以纹理的方式绘制到屏幕上
- 10_postProcess
对离屏的帧缓冲图像进行后处理，从而实现各种特效，例如：反相、灰度、锐化、模糊、边缘检测等等
- 11_vertexAttributes
```
Interleaved: Buffer0: x0y0z0r0g0b0u0v0x1y1z1r1g1b1u1v1...
Separate   : Buffer0: x0y0z0x1y1z1... Buffer1: r0g0b0r1g1b1... Buffer2: u0v0u1v1...
```
不同的顶点属性使用不同的`VkPipelineVertexInputStateCreateInfo`，因此渲染管线需要创建两个。插入式的顶点属性只需要创建一个顶点`VkBuffer`，而分离式的顶点属性需要创建多个`VkBuffer`，例如`position、color、normal`，分离式提交顶点数据时将多个VkBuffer传给`vkCmdBindVertexBuffers`即可。
- 12_blit
**注意**：需要将`VkSwapchainCreateInfoKHR`的`imageUsage`设置为`VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT`
- 13_stencil
绘制一个小的矩形不通过模板测试，并将小矩形所在区域的模板值设置为1，再绘制一个大矩形，大矩形和小矩形重合的区域不通过模板测试，这样大矩形就只绘制一个边框。
**注意**：开启模板测试，使用的深度模板图像格式需要为：`VK_FORMAT_D32_SFLOAT_S8_UINT`，`VkImageViewCreateInfo.subresourceRange.aspectMask`的值需要设置为：`VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT`，将`VkPipelineDepthStencilStateCreateInfo`的信息添加上模板测试的参数`VkStencilOpState`即可使用模板测试。
- 14_cpuparticle
基于CPU的粒子系统。点的大小在顶点着色器中设置`gl_PointSize = 10.0;` CPU粒子系统就是使用CPU更新粒子的属性，要想在CPU端更新粒子Buffer，就需要在创建粒子Buffer时设置正确的Buffer属性`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`，或者在每次更新Buffer后，显示刷新数据到GPU
- 15_blend
颜色混合的基本使用。`VkPipelineColorBlendAttachmentState`的`blendEnable`设置为`VK_TRUE`并设置正确的混合方式即可。`VkPipelineColorBlendStateCreateInfo`可以对所有的帧缓存都设置颜色混合，如果将`logicOpEnable`设置为`VK_TRUE`，那么`VkPipelineColorBlendAttachmentState`设置的混合方式将失效。
- 16_transform
通过设置GLFW的事件回调函数，生成`View`矩阵的参数(eyePos,focalPos,viewUp)，对图元进行缩放、平移、旋转
- 17_multiThread
多线程并行生成命令缓冲区。每一个交换链（每一帧）都有一个主要命令缓冲区(PRIMARY)，主要命令缓冲区可以有多个辅助(SECONDARY)命令缓冲区。将需要绘制的多个图元分配给多个辅助命令缓冲区，每个辅助命令缓冲区并行执行。
- 18_instancing
实例化多个相同的图形，和 OpenGL 使用方式基本一致，着色器变量有一点区别：Vulkan中使用`gl_InstanceIndex` OpenGL中使用`gl_InstanceID`。Vulkan 目前不支持类似 OpenGL 函数`glVertexAttribDivisor`的功能：设置多少个实例数据更新一次属性数据，Vulkan 默认是每个实例都更新属性数据。Vulkan 的分频器目前是一个扩展功能：`VkVertexInputBindingDivisorDescriptionEXT` `VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT`
- 19_indirectdraw
间接绘制适用于处理大量动态数据的场景，不需要CPU提供顶点等数据就可以绘制（例如计算着色器生成的顶点数据，无需回传到CPU创建顶点缓冲就可以直接使用）
- 20_queryPool
使用方法和 OpenGL 基本一样，先创建`VkQueryPool`，然后再使用`vkCmdBeginQuery`和`vkCmdEndQuery`查询：遮挡、管线统计、时间戳等。
查询管线统计时，需要在创建逻辑设备时将`VkPhysicalDeviceFeatures.pipelineStatisticsQuery`设置为`VK_TRUE`，管线统计可以查询各种着色器（顶点、几何、片段、细分、计算等）的调用次数、输入输出个数等。
- 21_renderPass
renderPass 和 frameBuffer 的关系
清除深度缓冲区，指定图元始终在最上层 vkCmdClearAttachments
- 22_pipelineCache
VkPipelineCache 的使用，可以像 SPV 文件一样写入磁盘并读取，可以使用 vkMergePipelineCaches 合并多个 VkPipelineCache
- 23_textureCubeMap
立方体贴图，在 02_16_transform_TEST4 的基础上修改，如果要想实现天空盒的效果，只需要相机的观察点始终在(0,0,0)并且不响应相机的移动操作即可
### 03_computeShader
- 01_imageProcessing
使用计算着色器对图像进行处理。
`uniform image2D` 可读可写，通常用于渲染过程中动态修改纹理内容的场景
`uniform imageBuffer` 一维可读可写的图像缓冲区
`uniform sampler2D` 纹理采样器，只读
绘图、计算、传输使用不同的队列簇。
- 02_indirectDraw
使用计算着色器生成的`VkDrawIndexedIndirectCommand`绘制图形，可以在计算着色器中做：视锥剔除、LOD等，然后将绘制命令写入到buffer中，交给CPU使用间接绘制命令绘制图形。
### 04_headless
- 01_render
不创建窗口将场景绘制到一个不可见的帧缓冲区附件上。绘制十张不同角度的图片并保存为jpg图片。
- 02_compute
仅仅使用计算着色器的计算功能，将CPU的数据在GPU中计算完成后，在CPU读取结果并打印。不创建窗口，不使用图形管线。
### 05_geometryShader
- 01_geometryShader
几何着色器只需要在创建逻辑设备时，将`VkPhysicalDeviceFeatures.geometryShader`设置为`VK_TRUE`，然后在创建渲染管线时，将几何着色器加入即可。代码在`01_02_vertexBuffers`的基础上修改。
- 02_viewportArrays
使用`multiViewport`时，需要先将`VkPhysicalDeviceFeatures.multiViewport`设置为`VK_TRUE`(geometryShader也要设置)，然后在创建渲染管线时，设置正确的`VkPipelineViewportStateCreateInfo`，如果程序开启了动态视口、裁剪，还需要正确调用`vkCmdSetViewport vkCmdSetScissor`
### 06_extensions
- 01_conditionalRender
条件渲染，首先需要在创建Instance和Device时将扩展开启，然后加载`vkCmdBeginConditionalRenderingEXT vkCmdEndConditionalRenderingEXT`这两个函数指针。
- 02_multiView
注意和 05_geometryShader/02_viewportArrays 的区别：multiView 是在顶点着色器中使用`gl_ViewIndex`，viewportArrays 是在几何着色器中使用`gl_InvocationID`，并且 multiView 是扩展功能。multiView 需要先将结果离屏渲染到 VkImage 中（交换链中的图片一般不支持 Array 类型的图片），然后以纹理的方式绘制到屏幕（ multiView 的结果是`VK_IMAGE_VIEW_TYPE_2D_ARRAY`）
- 03_dynamicRendering
`vkCmdBeginRenderingKHR vkCmdEndRenderingKHR` 不需要创建 `VkRenderPass VkFrameBuffer`，使用时需要开启 Instance 和 Device 的扩展，创建管线时需要指定 VkGraphicsPipelineCreateInfo.pNext 为 VkPipelineRenderingCreateInfoKHR，绘制前和绘制后需要转换图像布局， dynamicRendering 不像 VkRenderPass 一样会对图像布局进行转换，所以需要手动转换。`vkCmdBeginRendering vkCmdEndRendering` Vulkan-1.3 开始支持，不需要开启扩展，只需要在创建实例时，将`VkApplicationInfo.apiVersion` 设置为 `VK_API_VERSION_1_3`，然后在创建逻辑设备时开启设备特性：`dynamicRendering`
### 07_Vulkan-Hpp
- 01_helloTriangle
使用 Vulkan 的C++头文件离屏绘制一个三角形并保存到图片中，只有一帧。
- 02_multiFrame
GPU 多帧并行渲染
- 03_buffer
自定义几何数据（顶点、索引）
- 04_uniform
使用描述符集（Uniform）设置 MVP 矩阵
- 05_texture
使用纹理绘制
- 06_depthBuffer
开启深度测试
- 07_swapChain
创建窗口渲染一个三角形
- 08_offscreen
离屏渲染到 vkImage 再以纹理渲染到窗口
- 09_viewer
模仿 VTK 的一个Demo
- 10_windows
多个窗口不同线程同时记录命令并提交，不同窗口使用同一个渲染管线
- 11_productConsume
生产者消费者模型，一个线程用来绘制，一个线程用来将绘制的结果保存为图片（GPU多帧并行渲染，不创建窗口）
### 08_application
- 01_shadowMap
阴影贴图实现光照阴影，先以光源视角生成一张深度图（阴影贴图），这张图记录了从光源到场景中每个可见片段的距离，再实际渲染一次场景，通过比较当前片段的深度值（光源视角的深度值），判断是否在阴影中。
- 02_hdr
高动态范围图像(High-Dynamic Range)，在 02_16_TEST5 的基础上修改，简单理解就是在离屏渲染时，将color-attachment的格式设置为float16或float32，这样就可以保存位数更大的颜色值（一般情况下是 uint_8 只有255位），然后将这个颜色附件再通过HDR算法处理一次（将float32或float16转换为uint_8）
## TODO:
pushDescriptorSet
