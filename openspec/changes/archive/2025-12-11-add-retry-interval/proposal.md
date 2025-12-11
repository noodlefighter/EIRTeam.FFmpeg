## Why
当前视频播放功能在解码器失败时（`FAULTED`状态）或流结束时（`END_OF_STREAM`状态）会直接停止播放，没有自动重试机制。这可能导致：
1. 网络抖动或临时性问题导致的播放中断无法自动恢复
2. RTSP等流媒体连接断开后无法自动重新连接
3. 本地视频文件播放结束后无法自动重新播放（循环播放需求）

## What Changes
- 为 `FFmpegVideoStreamPlayback` 类添加重试机制，包含 `retry_timer` 变量和 `RETRY_INTERVAL_MS` 常量（10秒间隔）
- 当解码器处于 `FAULTED` 或 `END_OF_STREAM` 状态时，启动重试计时器
- 重试时间到达后，完全重新启动解码器进程（适用于RTSP流重连）或重新开始播放（适用于本地文件循环）
- 修改 `update_internal()` 方法添加重试状态检测和逻辑
- 增强 `VideoDecoder::stop_decoding()` 方法，添加完整的FFmpeg资源清理，确保重新启动时能正确重新初始化
- 修改 `play_internal()` 方法，移除FAULTED状态限制，添加重试计时器重置

## Impact
- **影响规范**: video-playback
- **影响代码**: `ffmpeg_video_stream.cpp`, `ffmpeg_video_stream.h`, `video_decoder.cpp`, `video_decoder.h`
- **向后兼容**: 新增功能，不影响现有行为
- **稳定性提升**: 修复了END_OF_STREAM状态重试时的崩溃问题
- **功能增强**: 支持RTSP流自动重连和本地文件自动重播