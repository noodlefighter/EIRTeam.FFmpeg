## ADDED Requirements

### Requirement: 线程安全的播放控制
视频播放器 SHALL 在独立的工作线程中执行所有可能导致阻塞的播放操作，确保主线程保持响应性。

#### Scenario: 异步播放启动
- **WHEN** 用户调用play()方法
- **THEN** 播放控制命令被加入到工作线程队列
- **AND** 主线程立即返回，不等待播放开始
- **AND** 播放状态通过异步事件通知主线程

#### Scenario: 播放状态查询
- **WHEN** 主线程查询播放状态（playing/paused/stopped）
- **THEN** 状态查询立即返回当前状态
- **AND** 不需要等待工作线程响应
- **AND** 状态数据通过线程安全的机制共享

### Requirement: 响应式错误处理
播放器 SHALL 实现超时机制和错误恢复策略，防止异常情况下的无限等待和UI卡死。

#### Scenario: 解码超时处理
- **WHEN** 解码操作超过预设超时时间（5秒）
- **THEN** 播放器自动停止解码
- **AND** 生成ERROR状态事件
- **AND** 主线程显示错误信息而不卡死

#### Scenario: 网络流连接失败
- **WHEN** 网络流连接超时或失败
- **THEN** 播放器重试连接最多3次
- **AND** 失败后进入ERROR状态
- **AND** 提供详细的错误信息和恢复建议

### Requirement: 优化的主线程更新
主线程的update循环 SHALL 仅包含非阻塞的渲染和状态更新操作，所有耗时操作都应在工作线程完成。

#### Scenario: 高频更新循环
- **WHEN** Godot每帧调用update_internal()方法
- **THEN** 方法执行时间不超过2毫秒
- **AND** 不包含任何I/O或锁等待操作
- **AND** 仅处理状态检查和纹理更新

#### Scenario: 帧缓冲管理
- **WHEN** 工作线程产生新的解码帧
- **THEN** 通过无锁队列传递给主线程
- **AND** 主线程快速复制帧数据到GPU纹理
- **AND** 释放工作线程的帧缓冲资源

### Requirement: 事件驱动的状态通知
播放器 SHALL 使用事件机制主动通知状态变更，而不是依赖主线程轮询检查。

#### Scenario: 播放开始通知
- **WHEN** 工作线程成功开始播放
- **THEN** 生成PLAYING状态事件
- **AND** 主线程接收事件并更新UI状态
- **AND** 触发相应的回调函数

#### Scenario: 播放结束通知
- **WHEN** 视频播放完成或流结束
- **THEN** 生成FINISHED状态事件
- **AND** 主线程更新播放状态为STOPPED
- **AND** 如果启用循环，自动重新开始播放

### Requirement: 播放控制命令队列
播放器 SHALL 提供线程安全的命令队列系统，确保播放控制的原子性和一致性。

#### Scenario: 播放控制命令
- **WHEN** 用户调用播放控制方法（play/pause/stop/seek）
- **THEN** 命令被序列化到工作线程队列
- **AND** 按照FIFO顺序执行
- **AND** 支持同步和异步两种执行模式

#### Scenario: 紧急停止处理
- **WHEN** 用户调用stop()方法或播放器销毁
- **THEN** 立即中断工作线程的当前操作
- **AND** 清理所有待处理的命令
- **AND** 安全释放所有资源