
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
## 四、Vulkan绘制图像的步骤
以下步骤以01_06_loadingModels为基础
### 1.初始化GLFW并创建GLFW窗口
### 2.创建Vulkan实例用来初始化Vulkan，
- 启用全局扩展（是相对整个程序而言的，而不是某个GPU）：窗口系统扩展、调试报告扩展等
- 开启校验层
### 3.设置回调函数用来接收调试信息
### 4.创建表面Surface
- 为了在不同平台通用，可以使用GLFW创建
- Vulkan实例创建完之后选择物理设备之前必须立即创建表面，因为它会影响物理设备（GPU）的选择
### 5.选择物理设备
- 检查物理设备是否满足要求，例如：纹理的压缩、64位浮点数、多视图渲染等等
- 查找满足需求的队列簇，不同的队列簇支持不同类型的指令，例如：计算、内存传输、绘图、呈现等
- 检查扩展是否支持，例如交换链扩展：表面支持的格式、表面支持的呈现模式
### 6.创建逻辑设备
- 逻辑设备是和物理设备交互的接口
- Vulkan实例开启了校验层，此处根据需要也开启相同的校验层
- 获取指定队列簇（图形队列、呈现队列）的队列句柄，指令需要提交到队列
### 7.创建交换链
- 交换链本质上是一个图像队列，Vulkan在这个图像队列的每一个图像上绘制内容，然后将交换链中的图像显示到屏幕
- 查询交换链支持的细节
- 根据交换链细节选择合适的表面模式
- 根据交换链细节选择合适的呈现模式，代表了屏幕呈现图像的条件
- 设置交换范围，即图像的分辨率
- 指定在图像上进行的操作，例如：显示（颜色附件）、后处理等
- 获取交换链图像
### 8.创建图像视图
- 任何VkImage对象都需要通过VkImageView来绑定访问，包括处于交换链中的、处于渲染管线中的
- 交换链有几张图像就创建几个图像视图
### 9.创建渲染流程对象
- 用于渲染的帧缓冲附着，指定颜色和深度缓冲
- 设置片段着色器中的输出`layout(location = N)out`
### 10.创建着色器使用的每一个描述符集布局
- 类似OpenGL的Uniform
- 描述符池必须包含描述符绑定信息才可以分配对应的描述符集
- 和Shader中的`layout(binding = N)`关联起来
### 11.创建图形管线
完整的图形管线包括：着色器阶段、固定功能状态、管线布局、渲染流程
在Vulkan中几乎不允许对图形管线进行动态设置，也就意味着每一种状态都需要提前创建一个图形管线（填充模式、图元类型等等）
- 创建Shader（顶点、片段...），设置着色器常量、设置着色器入口函数
- 顶点信息布局（pos,color）
- 拓扑信息，指定图元类型：点、线、三角形
- 视口
- 裁剪
- 深度值范围
- 光栅化：表面剔除、正面背面设置、填充或线框
- 多重采样
- 深度、模板测试、颜色混合（每个帧缓冲可以单独设置混合）
- 动态状态（可以允许动态设置而不用重新创建图形管线）：线宽、视口大小、混合常量
- 管线布局：描述符绑定信息（Shader中使用的uniform）、push_constant等
### 12.创建指令池
- 用于管理指令缓冲对象使用的内存并负责指令缓冲对象的分配，创建指令池需要指定队列簇，指令提交时需要和创建时的队列簇保持一致。
### 13.配置深度缓冲需要的资源
- 创建一个图像（对应的图像视图也需要创建）用来缓存深度信息
### 14.创建帧缓冲对象
- 附着（颜色、深度、模板等）需要绑定到帧缓冲对象上使用
- 交换链有几张图像，就需要创建几个帧缓冲对象，交换链中的图像就是帧缓冲的颜色附着
### 15.加载纹理
- 创建Buffer将纹理像素数据映射到Buffer（暂存Buffer）
- 创建图像、分配图像内存，将Buffer（暂存Buffer）中的数据拷贝到图像
- vkCreateBuffer vkAllocateMemory vkMapMemory std::memcpy vkUnmapMemory vkCreateImage vkAllocateMemory vkCmdCopyBufferToImage
### 16.创建纹理采样器
- 指定纹理缩放时的插值方式
- 各向异性过滤（需要检查物理设备是否支持）
- 设置细化级别
- 边界颜色
### 17.创建顶点缓冲
- 使用一个临时缓冲（暂存），先将顶点数据加载到临时缓冲再复制到顶点缓冲
- vkCreateBuffer vkAllocateMemory vkMapMemory std::memcpy vkUnmapMemory vkCmdCopyBuffer
### 18.创建索引缓冲
- 类似顶点缓冲
### 19.创建Uniform Buffer
- Uniform Buffer需要频繁更新，所以此处使用暂存缓冲并不会带来性能提升
- vkCreateBuffer vkAllocateMemory
- 每一帧（GPU并行处理的帧，不是交换链的图像）都需要创建一份Uniform Buffer
### 20.创建描述符池
- 描述符池可以用来创建描述符集
### 21.创建描述符集
- 每一帧都需要一个描述符集
- 用来描述Shader中的Uniform的信息
### 22.创建指令缓冲对象
- 数量需要和交换链中的图像个数一致
- 用来记录绘制指令
### 23.创建同步对象
- 用来发送信号：图像被获取后就可以渲染、图像渲染完成后就可以呈现
- 同步对象的数量和并行帧的个数一致
### 24.渲染循环
- 等待上一次提交的指令结束执行就可以开始绘制新的一帧
- 从交换链获取一张图像
- 更新Uniform
- 提交指令缓冲给图形指令队列 vkQueueSubmit
- 使用交换链进行图像呈现 vkQueuePresentKHR
```c++
UpdateUniform()

vkBeginCommandBuffer()
  vkCmdBeginRenderPass()
    vkCmdBindPipeline()
    vkCmdSetViewport()
    vkCmdSetScissor()
    vkCmdBindVertexBuffers()
    vkCmdBindIndexBuffer()
    vkCmdBindDescriptorSets()
    vkCmdDrawIndexed()
  vkCmdEndRenderPass()
vkEndCommandBuffer()
```
### 25.重建交换链
当窗口大小改变时，交换链中的图像也需要跟着改变
- 根据新的窗口大小创建新的交换链 vkCreateSwapchainKHR
- 创建交换链中图像的视图 vkCreateImageView
- 创建新的深度图像资源：深度图像以及深度图像视图 vkCreateImage vkCreateImageView
- 创建新的帧缓冲，交换链图像和深度图像改变，相当于帧缓冲的附件改变了，帧缓冲也需要重新创建 vkCreateFramebuffer

### 描述符集、描述符池、描述符绑定信息、UniformBuffer
- 以下三个值(N)共同指定了描述符集的绑定点
  - VkWriteDescriptorSet.dstBinding = N;
  - VkDescriptorSetLayoutBinding.binding = N;
  - 着色器中的 layout(binding = N) uniform name{vec4 transform;};
- 描述符集布局
  - vkCreateDescriptorSetLayout 创建描述符集布局，指定着色器的哪个阶段都有那些Uniform，比如顶点着色器阶段有MVP矩阵，片段着色器阶段有采样器，还指定了着色器的绑定点
  - VkDescriptorSetLayoutBinding 指定类型：UniformBuffer还是Sampler，绑定点（和Shader对应），着色器阶段（顶点、片段）
  - VkPipelineLayoutCreateInfo 创建管线布局时会用到描述符集布局
- 描述符池
  - vkCreateDescriptorPool 指定可以创建X个Y类型的描述符集，比如创建2个UniformBuffer类型的描述符集，或者创建1个Sampler类型的描述符集
  - VkDescriptorPoolSize 指定对应的描述符池可以分配的各种类型的描述符数量
  - VkDescriptorPoolCreateInfo.maxSets 指定最大可分配的描述符集（不是描述符）数量
- 描述符集
  - vkAllocateDescriptorSets 创建描述符集，将描述符集布局和描述符集关联起来
  - vkUpdateDescriptorSets 将描述符集和UniformBuffer或采样器关联起来 
  - VkDescriptorBufferInfo 指定具体哪一个UniformBuffer(VkBuffer)，以及Buffer大小
  - VkDescriptorImageInfo  指定采样器（VkSampler），图像视图（VkImageView）
- 描述符
  - VkWriteDescriptorSet 指定绑定点（和Shader对应），类型（UniformBuffer、Sampler），数据（VkDescriptorBufferInfo VkDescriptorImageInfo）

描述符可以简单理解为GLSL中的`uniform sampler`等。描述符集可以简单理解为一整个渲染（计算）管线使用的所有`uniform sampler`等，每使用一次管线，需要使用`vkCmdBindDescriptorSets`绑定一次描述符集。
### Buffer和Image
- 整体步骤：
  - 1. 创建暂存Buffer，申请内存，绑定Buffer和内存
  - 2. 映射内存到CPU，填充内存
  - 3. 创建真正的Buffer，申请内存，绑定Buffer和内存
  - 4. 拷贝暂存Buffer到真正的Buffer
  - 5. 销毁暂存Buffer、释放暂存Buffer对应的内存
- VertexBuffer
  - vkCreateBuffer          创建VkBuffer，是一个暂存VkBuffer（CPU可见）
  - vkAllocateMemory        申请内存VkDeviceMemory
  - vkBindBufferMemory      将VkBuffer和VkDeviceMemory绑定
  - vkMapMemory             将VkDeviceMemory和主机内存(void*)建立映射关系
  - std::memcpy             拷贝实际的顶点数据
  - vkUnmapMemory           结束内存映射
  - vkCreateBuffer          创建真正的顶点缓冲（显卡读取更快）
  - vkAllocateMemory        申请内存
  - vkBindBufferMemory      将缓冲对象和内存绑定
  - vkCmdCopyBuffer         将暂存缓冲中的数据拷贝到真正的缓冲
  - vkDestroyBuffer         释放暂存缓冲VkBuffer
  - vkFreeMemory            释放暂存缓冲对应的内存
- IndexBuffer
  和顶点缓冲步骤完全一致
- UniformBuffer使用步骤
  - vkCreateBuffer          创建VkBuffer，因为UniformBuffer需要频繁更新所以没必要使用暂存缓冲
  - vkAllocateMemory        申请内存VkDeviceMemory
  - vkBindBufferMemory      将VkBuffer和VkDeviceMemory绑定
  - vkMapMemory             将VkDeviceMemory和主机内存(void*)建立映射关系
- 纹理使用步骤
  - vkCreateBuffer          创建VkBuffer，这里只是一个暂存VkBuffer，需要使用图像数据填充它，这样效率会高很多
  - vkAllocateMemory        申请内存VkDeviceMemory
  - vkBindBufferMemory      将VkBuffer和VkDeviceMemory绑定
  - vkMapMemory             将VkDeviceMemory和主机内存(void*)建立映射关系
  - std::memcpy             拷贝实际的像素数据
  - vkUnmapMemory
  - vkCreateImage           创建图像
  - vkAllocateMemory
  - vkBindImageMemory
  - vkCmdPipelineBarrier    图像布局变换
  - vkCmdCopyBufferToImage  将VkBuffer中的暂存数据拷贝到VkImage，就是将纹理的像素数据拷贝到VkImage
  - vkCmdPipelineBarrier    图像布局变换
  - vkDestroyBuffer         释放暂存缓冲VkBuffer
  - vkFreeMemory            释放暂存缓冲对应的内存
  - vkCreateImageView       创建图像视图，图像只能通过图像视图访问
  - vkCreateSampler         创建采样器

### 图形管线、帧缓冲、附件
- 图形管线：
  - 着色器 VkShaderModule
  - 顶点信息 VkPipelineVertexInputStateCreateInfo 顶点的布局以及顶点着色器的绑定点
  - 拓扑信息 VkPipelineInputAssemblyStateCreateInfo 图元类型：点、线、三角形
  - 视口（裁剪）VkPipelineViewportStateCreateInfo
  - 光栅化（正反面、面剔除、填充模式）VkPipelineRasterizationStateCreateInfo
  - 多重采样 VkPipelineMultisampleStateCreateInfo
  - 深度模板测试 VkPipelineDepthStencilStateCreateInfo 比较方式、是否只读等
  - 颜色混合 VkPipelineColorBlendAttachmentState 
  - 动态状态（视口大小、线宽、颜色混合常量等等）VkPipelineDynamicStateCreateInfo
  - 管线布局（描述符集布局、着色器常量push_constant）VkPipelineLayoutCreateInfo
  - 渲染流程 VkRenderPass
- 渲染流程（RenderPass）
  - 附件描述 VkAttachmentDescription 附件的布局、操作方式、格式等，并不指定具体的附件
  - 子流程 VkSubpassDescription 设置子流程引用的附件，在这里绑定片段着色器的输出：layout(location = N) out vec4 fragColor;
  - 子流程依赖的关系 VkSubpassDependency
- 帧缓冲
  - 将颜色附件、深度附件等设置给帧缓冲，附件就是图像视图

### 渲染循环
以下命令都是异步执行，因此每次渲染循环开始时都应该判断上一次提交的指令是否执行结束，否则GPU需要处理的指令会大量堆积
- vkAcquireNextImageKHR “获取到图像”后发送一个信号
- vkQueueSubmit 收到“获取图像成功的信号”后开始执行绘制命令，绘制完成后发送“绘制完成的信号”
- vkQueuePresentKHR 收到“绘制完成”的信号后将该图像呈现到屏幕

录制绘制指令
- 开始记录指令          vkBeginCommandBuffer()
- 开始一个渲染流程      vkCmdBeginRenderPass()
- 绑定图形管线          vkCmdBindPipeline()
- 设置视口              vkCmdSetViewport()
- 设置裁剪              vkCmdSetScissor()
- 绑定顶点缓冲          vkCmdBindVertexBuffers()
- 绑定索引缓冲          vkCmdBindIndexBuffer()
- 绑定描述符集          vkCmdBindDescriptorSets()
- 执行绘制              vkCmdDrawIndexed()
- 结束这次渲染流程       vkCmdEndRenderPass()
- 结束记录指令          vkEndCommandBuffer()

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
- 04_textureMapping
    纹理的使用
- 05_depthBuffering
    开启深度测试。使用步骤：创建图形管线时开启深度测试`VkPipelineDepthStencilStateCreateInfo`，创建深度测试使用的资源`VkImage VkDeviceMemory VkImageView`，设置正确的pass信息，将深度图形附加到FrameBuffer，绘制循环开始时清除深度信息即可。Z值越大，距离眼睛越远。**注意**：如果开启了背面剔除，不开启深度测试和开启深度测试时绘制的图像看起来可能是一样的。
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
### 03_computeShader
- 01_imageProcessing
使用计算着色器对图像进行处理。
`uniform image2D` 可读可写，通常用于渲染过程中动态修改纹理内容的场景
`uniform imageBuffer` 一维可读可写的图像缓冲区
`uniform sampler2D` 纹理采样器，只读
绘图、计算、传输使用不同的队列簇。
- 02_indirectDraw
使用计算着色器生成的`VkDrawIndexedIndirectCommand`绘制图形，可以在计算着色器中做：视锥剔除、LOD等，然后将绘制命令写入到buffer中，交给CPU使用间接绘制命令绘制图形。
