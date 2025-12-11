## 1. 实现重试间隔属性
- [x] 1.1 在 `ffmpeg_video_stream.h` 中添加 `retry_interval` 成员变量
- [x] 1.2 在 `ffmpeg_video_stream.h` 中添加 `set_retry_interval()` 和 `get_retry_interval()` 方法声明
- [x] 1.3 在构造函数中初始化 `retry_interval = -1`（默认不重试）

## 2. 实现重试逻辑
- [x] 2.1 修改 `play_internal()` 方法，添加重试计时器初始化
- [x] 2.2 修改 `update_internal()` 方法，检查重试间隔并执行重试
- [x] 2.3 添加重试状态管理的成员变量

## 3. 绑定方法到脚本接口
- [x] 3.1 在 `_bind_methods()` 中绑定重试间隔设置方法

## 4. 测试和验证
- [x] 4.1 测试默认行为（retry_interval = -1 时与原行为一致）
- [x] 4.2 测试重试功能（retry_interval > 0 时自动重试）
- [x] 4.3 测试边界情况（retry_interval = 0 时立即重试）