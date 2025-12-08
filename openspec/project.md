# Project Context

## Purpose
EIRTeam.FFmpeg 是一个为 Godot Engine 4.1+ 开发的 GDExtension 视频解码库，使用 FFmpeg 库提供强大的媒体播放功能。该项目的目标是为 Godot 开发者提供高性能、跨平台的视频播放解决方案，支持本地和网络媒体资源（包括 RTSP 流）。

主要功能包括：
- 支持多种视频格式和编解码器（包括 H.264）
- 本地文件播放和网络流媒体支持（feature-uri 分支特性）
- 跨平台兼容性（Linux、Windows、Android、macOS）
- GPU 加速的 YUV 到 RGB 转换
- 与 Godot 原生 VideoStream API 集成

## Tech Stack
- **核心语言**: C++
- **构建系统**: SCons + Makefile + Just
- **主要依赖**:
  - FFmpeg 6.1 (libavcodec, libavformat, libavutil, libswscale, libswresample, libavfilter)
  - Godot-cpp GDExtension framework
- **目标平台**: Linux (x86_64), Windows (x86_64), Android (arm64-v8a, arm-v7a), macOS
- **交叉编译工具链**: MinGW-w64 (Windows)
- **着色器**: GLSL (YUV 到 RGB 转换)
- **许可证**: MIT License

## Project Conventions

### Code Style
- 遵循 Godot 引擎的 C++ 编码规范
- 使用 Godot 命名约定（snake_case 用于变量和函数，PascalCase 用于类名）
- 文件头包含完整的 MIT 许可证声明和版权信息
- 使用 RAII 和现代 C++ 特性
- 错误处理优先使用 Godot 的错误机制

### Architecture Patterns
- **GDExtension 模式**: 使用 Godot 的 GDExtension API 与引擎集成
- **组件化设计**: 分离 FFmpeg 包装器、视频流处理、帧管理等功能
- **平台抽象**: 通过 SCons 构建系统处理平台差异
- **资源管理**: 使用 Godot 的引用计数系统管理内存
- **异步处理**: 支持多线程视频解码和处理（通过 FFMPEG_MT_GPU_UPLOAD）

### Testing Strategy
- 目前主要依赖手动测试和集成测试
- 测试涵盖不同平台的构建过程
- 支持多种视频格式和网络流的播放测试
- 性能测试重点关注解码效率和内存使用

### Git Workflow
- **主分支**: master (稳定版本)
- **开发分支**: feature-uri (添加网络流支持)
- **提交信息**: 使用中英文混合，主要使用中文描述功能变更
- **子模块**: 使用 git submodule 管理 FFmpeg 相关依赖

## Domain Context
**多媒体处理领域**:
- FFmpeg API 的使用需要理解多媒体容器格式、编解码器和流处理
- Godot 引擎的渲染管线和资源系统集成
- 跨平台编译和部署的复杂性
- 网络流媒体协议（RTSP、HTTP Live Streaming 等）

**GDExtension 开发**:
- Godot 4.x 的 GDExtension 架构和 API
- 与 Godot 引擎的生命周期管理
- 原生扩展的打包和分发机制

## Important Constraints
- **许可证合规**: FFmpeg 使用 LGPL 许可证，需要注意专利编码器（如 H.264）的地域法律限制
- **平台限制**: 目前只支持 x86_64 架构的 Linux 和 Windows
- **依赖管理**: 需要 FFmpeg 预构建库或从源码构建
- **性能要求**: 视频解码需要优化 CPU 和 GPU 使用率
- **内存管理**: 严格的内存管理以避免泄漏，特别是在长时间播放时

## External Dependencies
- **FFmpeg**: 核心多媒体处理库
  - 静态库用于编译时链接
  - 动态库用于运行时加载
- **Godot Engine**: 4.1+ 版本
- **MinGW-w64**: Windows 交叉编译工具链
- **Python 3**: 构建脚本依赖
- **SCons**: 主要构建系统

**网络资源依赖** (feature-uri 分支):
- 支持 RTSP、HTTP 等网络协议
- 需要处理网络延迟和连接稳定性问题
