# RemoteControl

基于 C++ 的 Windows 远程桌面传输项目，主要实现服务端桌面采集、H.264 视频编码、TCP 自定义协议传输，以及 Qt 客户端接收与显示远程画面。

项目用于学习和实践 Windows 网络编程、DXGI Desktop Duplication 截屏、FFmpeg H.264 编解码、Qt 图形界面显示等内容。

---

## 项目功能

目前已实现：

* 基于 Winsock 的 TCP 服务端与客户端通信
* 自定义数据包协议封装
* DXGI Desktop Duplication 桌面截图
* BGRA 屏幕像素采集
* FFmpeg / libx264 H.264 编码
* H.264 数据包通过 TCP 发送
* Qt 客户端接收远程数据
* Qt 客户端解码 H.264 并显示画面
* 客户端显示帧率、分辨率、数据大小等信息

计划扩展：

* 鼠标指针绘制
* 鼠标远程控制
* 键盘远程控制
* 图像缩放传输
* 码率与帧率动态调节
* 客户端断线重连
* 多客户端连接支持

---

## 技术栈

* C++17
* CMake
* Qt 6 Widgets
* Winsock2
* DXGI Desktop Duplication
* Direct3D 11
* FFmpeg
* libx264
* vcpkg
* Visual Studio 2022 MSVC

---

## 项目结构

```txt
RemoteControl
├── common
│   ├── socket_utils.hpp      # Socket 工具类，封装 TCP 连接、收发数据包
│   ├── protocol.hpp          # 自定义通信协议
│   ├── dxgi_capture.hpp      # DXGI 桌面采集
│   ├── h264_encoder.hpp      # H.264 编码器
│   └── h264_decoder.hpp      # H.264 解码器
│
├── server
│   └── server_main.cpp       # 服务端入口
│
├── qt_client
│   ├── client_main.cpp       # Qt 客户端入口
│   ├── remote_viewer.h       # 远程画面窗口声明
│   ├── remote_viewer.cpp     # 远程画面窗口实现
│   └── qt_client.ui          # Qt Designer UI 文件
│
└── CMakeLists.txt
```

---

## 通信协议

项目使用自定义 TCP 数据包协议。

数据包由包头和包体组成：

```cpp
#pragma pack(push, 1)
struct PacketHeader
{
    int magic;
    int cmd;
    int body_len;
};
#pragma pack(pop)
```

字段说明：

| 字段       | 说明              |
| -------- | --------------- |
| magic    | 魔数，用于校验是否为合法数据包 |
| cmd      | 命令类型            |
| body_len | 包体长度            |

常用命令：

```cpp
enum Cmd
{
    CMD_SCREEN_FRAME = 200,
    CMD_VIDEO_PACKET = 210,
    CMD_MOUSE_MOVE = 300,
    CMD_MOUSE_CLICK = 301,
    CMD_KEY_DOWN = 400,
    CMD_KEY_UP = 401
};
```

当前视频流主要使用：

```cpp
CMD_VIDEO_PACKET
```

服务端将 H.264 编码后的数据作为 body 发送给客户端。

---

## 核心流程

### 服务端流程

```txt
初始化 Winsock
    ↓
创建 TCP 监听 Socket
    ↓
等待客户端连接
    ↓
初始化 DXGI 桌面采集
    ↓
初始化 H.264 编码器
    ↓
循环采集桌面画面
    ↓
BGRA 像素编码为 H.264
    ↓
通过 TCP 发送给客户端
```

### 客户端流程

```txt
初始化 Qt 窗口
    ↓
连接服务端
    ↓
接收 TCP 数据包
    ↓
判断 CMD_VIDEO_PACKET
    ↓
H.264 解码为 BGRA
    ↓
转换为 QImage / QPixmap
    ↓
显示到 QLabel
```

---

## 环境要求

### Windows

推荐系统：

```txt
Windows 10 / Windows 11
```

### Visual Studio

需要安装：

```txt
Visual Studio 2022
MSVC v143
Windows 10/11 SDK
使用 C++ 的桌面开发
```

### Qt

推荐版本：

```txt
Qt 6.8.1 MSVC2022 64-bit
```

### vcpkg

本项目使用 vcpkg 安装 FFmpeg：

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
cd C:\dev\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install ffmpeg[x264]:x64-windows
```

---

## CMake 配置

Qt Creator 中需要配置 vcpkg toolchain：

```txt
CMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
```

如果使用命令行配置：

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_PREFIX_PATH="E:/QT/6.8.1/msvc2022_64"
```

构建：

```powershell
cmake --build build --config Debug
```

---

## CMakeLists 示例

```cmake
cmake_minimum_required(VERSION 3.10.0)

project(RemoteControl VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(FFMPEG REQUIRED)

qt_standard_project_setup()

set(VCPKG_INSTALLED_DIR "C:/dev/vcpkg/installed/x64-windows")

add_executable(RemoteServer
    server/server_main.cpp
)

target_include_directories(RemoteServer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${VCPKG_INSTALLED_DIR}/include
)

target_link_directories(RemoteServer
    PRIVATE
    $<$<CONFIG:Debug>:${VCPKG_INSTALLED_DIR}/debug/lib>
    $<$<NOT:$<CONFIG:Debug>>:${VCPKG_INSTALLED_DIR}/lib>
)

target_link_libraries(RemoteServer
    PRIVATE
    ws2_32
    d3d11
    dxgi
    avcodec
    avutil
    swscale
)

qt_add_executable(RemoteQtClient
    qt_client/client_main.cpp
    qt_client/remote_viewer.h
    qt_client/remote_viewer.cpp
    qt_client/qt_client.ui
)

target_include_directories(RemoteQtClient
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${VCPKG_INSTALLED_DIR}/include
)

target_link_directories(RemoteQtClient
    PRIVATE
    $<$<CONFIG:Debug>:${VCPKG_INSTALLED_DIR}/debug/lib>
    $<$<NOT:$<CONFIG:Debug>>:${VCPKG_INSTALLED_DIR}/lib>
)

target_link_libraries(RemoteQtClient
    PRIVATE
    Qt6::Widgets
    ws2_32
    avcodec
    avutil
    swscale
)
```

---

## 运行方式

### 1. 启动服务端

先运行：

```txt
RemoteServer.exe
```

服务端会监听端口：

```txt
9999
```

并等待客户端连接。

### 2. 启动客户端

再运行：

```txt
RemoteQtClient.exe
```

客户端连接服务端后，会接收远程桌面 H.264 视频流并显示。

---

## 当前测试结果

已完成以下验证：

* DXGI 能成功采集桌面画面
* libx264 编码器能正常初始化
* 服务端能生成 `test.h264`
* `test.h264` 可以使用 `ffplay` 正常播放
* 服务端可以通过 TCP 发送 H.264 packet
* 客户端可以接收 H.264 packet 并打印数据大小

---

## 常见问题

### 1. 找不到 `avcodec.lib`

需要确认 FFmpeg 库目录是否加入 CMake：

```cmake
target_link_directories(RemoteQtClient
    PRIVATE
    C:/dev/vcpkg/installed/x64-windows/debug/lib
    C:/dev/vcpkg/installed/x64-windows/lib
)
```

并链接：

```cmake
avcodec
avutil
swscale
```

### 2. 找不到 `type_traits`

这是 MSVC 标准库头文件。一般是 Qt Creator 没有正确使用 MSVC Kit。

解决方法：

* 使用 `Desktop Qt 6.x MSVC2022 64bit` Kit
* 不要使用 `Imported Kit`
* 删除旧 build 目录后重新配置
* 确认 Visual Studio 安装了“使用 C++ 的桌面开发”

### 3. Qt 无法直接显示 H.264

`QImage::fromData()` 不能直接显示 H.264。

错误方式：

```cpp
QImage::fromData(h264Data);
```

正确方式：

```txt
H.264 packet
-> FFmpeg 解码
-> BGRA pixels
-> QImage
-> QLabel 显示
```

### 4. DXGI 截图不显示鼠标

DXGI Desktop Duplication 默认画面中不一定包含鼠标指针。

可以通过：

```txt
GetFramePointerShape
```

获取鼠标形状，再手动绘制到 BGRA 图像中。

---

## 后续优化方向

* 添加鼠标指针绘制
* 添加鼠标与键盘远程控制
* 添加画面缩放
* 添加帧率控制
* 添加码率调节
* 添加断线重连
* 添加多线程编码与发送
* 优化客户端解码显示延迟
* 增加日志系统
* 增加配置文件

---

## 项目说明

本项目主要用于学习 C++ 网络编程、Windows 桌面采集、视频编码传输和 Qt 客户端显示。当前仍处于开发阶段，功能会持续完善。
