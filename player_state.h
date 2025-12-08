/**************************************************************************/
/*  player_state.h                                                        */
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

#ifndef PLAYER_STATE_H
#define PLAYER_STATE_H

#ifdef GDEXTENSION

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/classes/mutex.hpp>
#include <godot_cpp/core/mutex_lock.hpp>
#include <godot_cpp/templates/local_vector.hpp>

using namespace godot;

#else

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "core/templates/local_vector.h"
#include "core/os/mutex.h"
#include "core/os/mutex_lock.h"

#endif

#include <atomic>
#include <chrono>
#include <thread>

// 播放器状态枚举
enum class PlayerState {
	STOPPED,        // 停止状态
	STARTING,       // 启动中
	PLAYING,        // 播放中
	PAUSED,         // 暂停状态
	STOPPING,       // 停止中
	SEEKING,        // 跳转中
	FAULTED,        // 错误状态
	BUFFERING       // 缓冲中
};

// 播放错误类型
enum class PlayerError {
	NONE,                   // 无错误
	NETWORK_TIMEOUT,        // 网络超时
	DECODER_ERROR,          // 解码错误
	INVALID_URI,            // 无效URI
	MEMORY_ERROR,           // 内存错误
	UNKNOWN_FORMAT,         // 未知格式
	CORRUPTED_DATA,         // 数据损坏
	UNKNOWN_ERROR           // 未知错误
};

// 播放控制命令类型
enum class PlaybackCommandType {
	PLAY,
	STOP,
	PAUSE,
	RESUME,
	SEEK,
	SET_URI,
	UPDATE_VOLUME,
	TERMINATE
};

// 播放事件类型
enum class PlaybackEventType {
	STATE_CHANGED,
	ERROR_OCCURRED,
	POSITION_UPDATED,
	BUFFERING_UPDATED,
	END_OF_STREAM
};

// 播放控制命令结构
struct PlaybackCommand {
	PlaybackCommandType type;
	double timestamp;        // 用于SEEK命令
	String uri;             // 用于SET_URI命令
	float volume;           // 用于UPDATE_VOLUME命令
	std::chrono::steady_clock::time_point created_time;

	PlaybackCommand() : type(PlaybackCommandType::TERMINATE), timestamp(0.0), volume(1.0f) {
		created_time = std::chrono::steady_clock::now();
	}

	PlaybackCommand(PlaybackCommandType t) : type(t), timestamp(0.0), volume(1.0f) {
		created_time = std::chrono::steady_clock::now();
	}

	static PlaybackCommand play() { return PlaybackCommand(PlaybackCommandType::PLAY); }
	static PlaybackCommand stop() { return PlaybackCommand(PlaybackCommandType::STOP); }
	static PlaybackCommand pause() { return PlaybackCommand(PlaybackCommandType::PAUSE); }
	static PlaybackCommand resume() { return PlaybackCommand(PlaybackCommandType::RESUME); }
	static PlaybackCommand seek(double time) {
		PlaybackCommand cmd(PlaybackCommandType::SEEK);
		cmd.timestamp = time;
		return cmd;
	}
	static PlaybackCommand set_uri(const String& new_uri) {
		PlaybackCommand cmd(PlaybackCommandType::SET_URI);
		cmd.uri = new_uri;
		return cmd;
	}
	static PlaybackCommand update_volume(float vol) {
		PlaybackCommand cmd(PlaybackCommandType::UPDATE_VOLUME);
		cmd.volume = vol;
		return cmd;
	}
	static PlaybackCommand terminate() { return PlaybackCommand(PlaybackCommandType::TERMINATE); }
};

// 播放事件结构
struct PlaybackEvent {
	PlaybackEventType type;
	PlayerState state;          // 用于STATE_CHANGED事件
	PlayerError error;          // 用于ERROR_OCCURRED事件
	double position;            // 用于POSITION_UPDATED事件
	float buffering_percent;    // 用于BUFFERING_UPDATED事件
	String error_message;       // 用于ERROR_OCCURRED事件的详细信息

	PlaybackEvent() : type(PlaybackEventType::STATE_CHANGED), state(PlayerState::STOPPED),
	                  error(PlayerError::NONE), position(0.0),
	                  buffering_percent(0.0f) {}

	PlaybackEvent(PlaybackEventType t) : type(t), state(PlayerState::STOPPED),
	                                    error(PlayerError::NONE), position(0.0),
	                                    buffering_percent(0.0f) {}

	static PlaybackEvent state_changed(PlayerState new_state) {
		PlaybackEvent event(PlaybackEventType::STATE_CHANGED);
		event.state = new_state;
		return event;
	}

	static PlaybackEvent error_occurred(PlayerError err, const String& message) {
		PlaybackEvent event(PlaybackEventType::ERROR_OCCURRED);
		event.error = err;
		event.error_message = message;
		return event;
	}

	static PlaybackEvent position_updated(double pos) {
		PlaybackEvent event(PlaybackEventType::POSITION_UPDATED);
		event.position = pos;
		return event;
	}

	static PlaybackEvent buffering_updated(float percent) {
		PlaybackEvent event(PlaybackEventType::BUFFERING_UPDATED);
		event.buffering_percent = percent;
		return event;
	}

	static PlaybackEvent end_of_stream() {
		return PlaybackEvent(PlaybackEventType::END_OF_STREAM);
	}
};

// 线程安全的播放状态管理器
class PlayerStateManager : public RefCounted {
private:
	// 原子状态，保证线程安全读取
	std::atomic<PlayerState> current_state;
	std::atomic<PlayerError> last_error;

	// 用于事件通知的互斥锁
	Mutex event_queue_mutex;
	LocalVector<PlaybackEvent> event_queue;

	// 用于性能指标的原子变量
	std::atomic<double> current_position;
	std::atomic<float> buffering_percent;
	std::atomic<uint32_t> frames_decoded;
	std::atomic<uint32_t> frames_dropped;

	// 错误恢复相关
	Mutex retry_mutex;
	std::atomic<int> retry_count;
	std::chrono::steady_clock::time_point last_error_time;

	// 状态转换验证
	bool is_valid_transition(PlayerState from, PlayerState to);

public:
	PlayerStateManager();

	// 状态管理
	void set_state(PlayerState new_state);
	PlayerState get_state() const { return current_state.load(); }
	bool is_playing_state() const;
	bool is_terminal_state() const;

	// 错误管理
	void set_error(PlayerError error, const String& message = "");
	PlayerError get_error() const { return last_error.load(); }
	String get_error_message() const;
	bool should_retry();
	void reset_retry_count();

	// 事件队列
	void add_event(const PlaybackEvent& event);
	LocalVector<PlaybackEvent> get_and_clear_events();

	// 性能指标
	void update_position(double pos) { current_position.store(pos); }
	double get_position() const { return current_position.load(); }

	void update_buffering(float percent) { buffering_percent.store(percent); }
	float get_buffering_percent() const { return buffering_percent.load(); }

	void increment_frames_decoded() { frames_decoded.fetch_add(1); }
	void increment_frames_dropped() { frames_dropped.fetch_add(1); }
	uint32_t get_frames_decoded() const { return frames_decoded.load(); }
	uint32_t get_frames_dropped() const { return frames_dropped.load(); }

	void reset_performance_metrics();
};

// 播放控制命令队列
class PlaybackCommandQueue {
private:
	Mutex queue_mutex;
	LocalVector<PlaybackCommand> command_queue;

public:
	void push_command(const PlaybackCommand& command);
	bool pop_command(PlaybackCommand& command);
	bool has_pending_commands() const;
	void clear();
};

#endif // PLAYER_STATE_H