## ADDED Requirements

### Requirement: 自动重试播放功能
系统 SHALL 支持在播放失败时自动重试播放。

#### Scenario: 设置重试间隔为正值
- **WHEN** 用户将 `retry_interval` 设置为大于等于0的毫秒数
- **AND** 播放过程中解码器进入 `FAULTED` 状态
- **THEN** 系统 SHALL 在指定间隔后自动尝试重新开始播放

#### Scenario: 设置重试间隔为-1（默认）
- **WHEN** 用户将 `retry_interval` 设置为-1或使用默认值
- **AND** 播放过程中解码器进入 `FAULTED` 状态
- **THEN** 系统 SHALL 保持原有行为，直接停止播放不进行重试

#### Scenario: 重试间隔为0
- **WHEN** 用户将 `retry_interval` 设置为0
- **AND** 播放过程中解码器进入 `FAULTED` 状态
- **THEN** 系统 SHALL 在下一个更新周期立即尝试重新开始播放

## MODIFIED Requirements

### Requirement: 播放状态管理
系统 SHALL 管理播放状态并处理解码器故障。

#### Scenario: 解码器故障时的重试逻辑
- **WHEN** 播放过程中解码器状态变为 `FAULTED`
- **AND** `retry_interval` 不为-1
- **THEN** 系统 SHALL 停止当前播放，启动重试计时器，并在间隔后重新调用 `play_internal()`
- **AND** 重试期间 SHALL 保持 `playing = true` 状态，避免外部误认为播放已停止

#### Scenario: 重试成功
- **WHEN** 重试播放成功且解码器状态恢复正常
- **THEN** 系统 SHALL 清除重试状态，恢复正常播放流程