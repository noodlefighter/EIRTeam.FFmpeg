/**************************************************************************/
/*  player_state.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             EIRTeam.FFmpeg                             */
/*                         https://ph.eirteam.moe                         */
/**************************************************************************/
/* Copyright (c) 2023-present Álex Román (EIRTeam) & contributors.        */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "player_state.h"
#include <algorithm>

#ifdef GDEXTENSION
#include <godot_cpp/classes/os.hpp>
#else
#include "core/os/os.h"
#endif

const int MAX_RETRY_COUNT = 3;
const std::chrono::milliseconds RETRY_COOLDOWN(5000); // 5秒重试冷却时间

PlayerStateManager::PlayerStateManager() {
	current_state.store(PlayerState::STOPPED);
	last_error.store(PlayerError::NONE);
	current_position.store(0.0);
	buffering_percent.store(0.0);
	frames_decoded.store(0);
	frames_dropped.store(0);
	retry_count.store(0);
	last_error_time = std::chrono::steady_clock::now() - std::chrono::hours(1); // 初始化为很久之前
}

bool PlayerStateManager::is_valid_transition(PlayerState from, PlayerState to) {
	// 允许的状态转换矩阵
	switch (from) {
		case PlayerState::STOPPED:
			return to == PlayerState::STARTING || to == PlayerState::STOPPED || to == PlayerState::FAULTED;

		case PlayerState::STARTING:
			return to == PlayerState::PLAYING || to == PlayerState::BUFFERING || to == PlayerState::FAULTED;

		case PlayerState::PLAYING:
			return to == PlayerState::PAUSED || to == PlayerState::STOPPING ||
			       to == PlayerState::BUFFERING || to == PlayerState::SEEKING || to == PlayerState::FAULTED;

		case PlayerState::PAUSED:
			return to == PlayerState::PLAYING || to == PlayerState::STOPPING ||
			       to == PlayerState::SEEKING || to == PlayerState::FAULTED;

		case PlayerState::STOPPING:
			return to == PlayerState::STOPPED || to == PlayerState::FAULTED;

		case PlayerState::SEEKING:
			return to == PlayerState::PLAYING || to == PlayerState::BUFFERING || to == PlayerState::FAULTED;

		case PlayerState::BUFFERING:
			return to == PlayerState::PLAYING || to == PlayerState::STOPPING ||
			       to == PlayerState::PAUSED || to == PlayerState::FAULTED;

		case PlayerState::FAULTED:
			return to == PlayerState::STOPPED || to == PlayerState::STARTING;

		default:
			return false;
	}
}

void PlayerStateManager::set_state(PlayerState new_state) {
	PlayerState old_state = current_state.load();

	// 验证状态转换
	if (!is_valid_transition(old_state, new_state)) {
#ifdef GDEXTENSION
		ERR_PRINT(vformat("Invalid state transition from %d to %d", (int)old_state, (int)new_state));
#else
		ERR_PRINT(vformat("Invalid state transition from %d to %d", old_state, new_state));
#endif
		return;
	}

	current_state.store(new_state);

	// 添加状态变更事件
	PlaybackEvent event = PlaybackEvent::state_changed(new_state);
	add_event(event);

	// 如果从错误状态恢复，重置错误
	if (new_state != PlayerState::FAULTED && last_error.load() != PlayerError::NONE) {
		last_error.store(PlayerError::NONE);
		reset_retry_count();
	}
}

bool PlayerStateManager::is_playing_state() const {
	PlayerState state = current_state.load();
	return state == PlayerState::PLAYING || state == PlayerState::BUFFERING ||
	       state == PlayerState::STARTING || state == PlayerState::SEEKING;
}

bool PlayerStateManager::is_terminal_state() const {
	PlayerState state = current_state.load();
	return state == PlayerState::STOPPED || state == PlayerState::FAULTED;
}

void PlayerStateManager::set_error(PlayerError error, const String& message) {
	if (error == PlayerError::NONE) {
		last_error.store(PlayerError::NONE);
		reset_retry_count();
		return;
	}

	last_error.store(error);
	set_state(PlayerState::FAULTED);

	PlaybackEvent event = PlaybackEvent::error_occurred(error, message);
	add_event(event);

	last_error_time = std::chrono::steady_clock::now();
}

String PlayerStateManager::get_error_message() const {
	PlayerError error = last_error.load();
	switch (error) {
		case PlayerError::NONE:
			return "";
		case PlayerError::NETWORK_TIMEOUT:
			return "Network connection timeout";
		case PlayerError::DECODER_ERROR:
			return "Video decoder error";
		case PlayerError::INVALID_URI:
			return "Invalid media URI";
		case PlayerError::MEMORY_ERROR:
			return "Insufficient memory";
		case PlayerError::UNKNOWN_FORMAT:
			return "Unsupported video format";
		case PlayerError::CORRUPTED_DATA:
			return "Corrupted media data";
		case PlayerError::UNKNOWN_ERROR:
		default:
			return "Unknown error occurred";
	}
}

bool PlayerStateManager::should_retry() {
	if (retry_count.load() >= MAX_RETRY_COUNT) {
		return false;
	}

	auto now = std::chrono::steady_clock::now();
	auto time_since_error = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_error_time);

	if (time_since_error < RETRY_COOLDOWN) {
		return false;
	}

	return true;
}

void PlayerStateManager::reset_retry_count() {
	retry_count.store(0);
}

void PlayerStateManager::add_event(const PlaybackEvent& event) {
	MutexLock lock(event_queue_mutex);
	event_queue.push_back(event);
}

LocalVector<PlaybackEvent> PlayerStateManager::get_and_clear_events() {
	MutexLock lock(event_queue_mutex);
	LocalVector<PlaybackEvent> result;

	// 手动复制元素
	for (uint32_t i = 0; i < event_queue.size(); i++) {
		result.push_back(event_queue[i]);
	}

	// 清空原队列
	event_queue.clear();
	return result;
}

void PlayerStateManager::reset_performance_metrics() {
	frames_decoded.store(0);
	frames_dropped.store(0);
	current_position.store(0.0);
	buffering_percent.store(0.0);
}

// PlaybackCommandQueue 实现

void PlaybackCommandQueue::push_command(const PlaybackCommand& command) {
	MutexLock lock(queue_mutex);
	command_queue.push_back(command);
}

bool PlaybackCommandQueue::pop_command(PlaybackCommand& command) {
	MutexLock lock(queue_mutex);
	if (command_queue.is_empty()) {
		return false;
	}
	command = command_queue[0];
	// 简单移除第一个元素，使用push/pop方式
	for (uint32_t i = 1; i < command_queue.size(); i++) {
		// 创建临时LocalVector来移除第一个元素
		LocalVector<PlaybackCommand> temp;
		for (uint32_t j = 1; j < command_queue.size(); j++) {
			temp.push_back(command_queue[j]);
		}
		command_queue = temp;
		break;
	}
	return true;
}

bool PlaybackCommandQueue::has_pending_commands() const {
	MutexLock lock(queue_mutex);
	return !command_queue.is_empty();
}

void PlaybackCommandQueue::clear() {
	MutexLock lock(queue_mutex);
	command_queue.clear();
}