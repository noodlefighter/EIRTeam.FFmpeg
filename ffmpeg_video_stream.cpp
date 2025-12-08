/**************************************************************************/
/*  ffmpeg_video_stream.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             EIRTeam.FFmpeg                             */
/*                         https://ph.eirteam.moe                         */
/**************************************************************************/
/* Copyright (c) 2023-present Álex Román (EIRTeam) & contributors.        */
/*                                                                        */
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

#include "ffmpeg_video_stream.h"
#include <iterator>
#include <chrono>

#ifdef GDEXTENSION
#include "gdextension_build/gdex_print.h"
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
typedef RenderingDevice RD;
typedef RenderingServer RS;
typedef RDTextureView RDTextureViewC;
struct RDTextureFormatC {
	RenderingDevice::DataFormat format;
	int width;
	int height;
	int usage_bits;
	int depth;
	int array_layers;
	int mipmaps;

	Ref<RDTextureFormat> get_texture_format() {
		Ref<RDTextureFormat> tf;
		tf.instantiate();
		tf->set_height(height);
		tf->set_width(width);
		tf->set_usage_bits(usage_bits);
		tf->set_format(format);
		tf->set_depth(depth);
		tf->set_array_layers(array_layers);
		tf->set_mipmaps(mipmaps);
		return tf;
	}
};
RDTextureFormatC tfc_from_rdtf(Ref<RDTextureFormat> p_texture_format) {
	RDTextureFormatC tfc;
	tfc.width = p_texture_format->get_width();
	tfc.height = p_texture_format->get_height();
	tfc.usage_bits = p_texture_format->get_usage_bits();
	tfc.format = p_texture_format->get_format();
	tfc.depth = p_texture_format->get_depth();
	tfc.array_layers = p_texture_format->get_array_layers();
	tfc.mipmaps = p_texture_format->get_mipmaps();
	return tfc;
}

typedef int64_t ComputeListID;
#define TEXTURE_FORMAT_COMPAT(tf) tfc_from_rdtf(tf);
#else
#include "servers/rendering/rendering_device_binds.h"
typedef RD::TextureFormat RDTextureFormatC;
typedef RD::TextureView RDTextureViewC;
#define TEXTURE_FORMAT_COMPAT(tf) tf;
typedef RD::ComputeListID ComputeListID;
#endif

#include "tracy_import.h"
#include "yuv_to_rgb.glsl.gen.h"

#ifdef GDEXTENSION
#define FREE_RD_RID(rid) RS::get_singleton()->get_rendering_device()->free_rid(rid);
#else
#define FREE_RD_RID(rid) RS::get_singleton()->get_rendering_device()->free(rid);
#endif
void FFmpegVideoStreamPlayback::seek_into_sync() {
	decoder->seek(playback_position);
	Vector<Ref<DecodedFrame>> decoded_frames;
	for (Ref<DecodedFrame> df : available_frames) {
		decoded_frames.push_back(df);
	}
	decoder->return_frames(decoded_frames);
	available_frames.clear();
	available_audio_frames.clear();
}

double FFmpegVideoStreamPlayback::get_current_frame_time() {
	if (last_frame.is_valid()) {
		return last_frame->get_time();
	}
	return 0.0f;
}

bool FFmpegVideoStreamPlayback::check_next_frame_valid(Ref<DecodedFrame> p_decoded_frame) {
	// in the case of looping, we may start a seek back to the beginning but still receive some lingering frames from the end of the last loop. these should be allowed to continue playing.
	if (looping && Math::abs((p_decoded_frame->get_time() - decoder->get_duration()) - playback_position) < LENIENCE_BEFORE_SEEK)
		return true;

	return p_decoded_frame->get_time() <= playback_position && Math::abs(p_decoded_frame->get_time() - playback_position) < LENIENCE_BEFORE_SEEK;
}

bool FFmpegVideoStreamPlayback::check_next_audio_frame_valid(Ref<DecodedAudioFrame> p_decoded_frame) {
	// in the case of looping, we may start a seek back to the beginning but still receive some lingering frames from the end of the last loop. these should be allowed to continue playing.
	if (looping && Math::abs((p_decoded_frame->get_time() - decoder->get_duration()) - playback_position) < LENIENCE_BEFORE_SEEK)
		return true;

	return p_decoded_frame->get_time() <= playback_position && Math::abs(p_decoded_frame->get_time() - playback_position) < LENIENCE_BEFORE_SEEK;
}

const char *const upd_str = "update_internal";

#define LIVE_STREAM

void FFmpegVideoStreamPlayback::update_internal(double p_delta) {
	ZoneScopedN("update_internal");

	// 使用新的状态管理器，主线程只需进行非阻塞的状态检查和渲染更新
	if (!state_manager.is_valid()) {
		return;
	}

	PlayerState current_state = state_manager->get_state();
	if (current_state == PlayerState::STOPPED || current_state == PlayerState::FAULTED) {
		return;
	}

	// 性能监控 - 确保update_internal执行时间不超过2ms
	auto update_start = std::chrono::steady_clock::now();

	// 处理状态管理器中的事件
	LocalVector<PlaybackEvent> events = state_manager->get_and_clear_events();
	for (const PlaybackEvent& event : events) {
		switch (event.type) {
			case PlaybackEventType::END_OF_STREAM:
				// 流结束事件已经由工作线程处理
				break;

			case PlaybackEventType::ERROR_OCCURRED:
				// 记录错误信息
				ERR_PRINT("Video playback error: " + event.error_message);
				break;

			default:
				break;
		}
	}

	// 非阻塞地获取解码帧
	if (decoder.is_valid() && state_manager->is_playing_state()) {
		// 获取新解码的帧，非阻塞操作
		Vector<Ref<DecodedFrame>> decoded_frames = decoder->get_decoded_frames();
		if (!decoded_frames.is_empty()) {
			MutexLock lock(frame_mutex);

			// 选择最合适的帧进行渲染
			Ref<DecodedFrame> best_frame = nullptr;
			double current_pos = state_manager->get_position();

			for (const Ref<DecodedFrame>& frame : decoded_frames) {
				if (frame.is_valid()) {
					// 找到最接近当前播放位置的帧
					if (!best_frame.is_valid() || Math::abs(frame->get_time() - current_pos) < Math::abs(best_frame->get_time() - current_pos)) {
						best_frame = frame;
					}
				}
			}

			if (best_frame.is_valid() && best_frame != last_frame) {
				// 释放上一帧
				if (last_frame.is_valid()) {
					decoder->return_frame(last_frame);
				}

				// 设置新帧
				last_frame = best_frame;
				last_frame_image = last_frame->get_image();
#ifdef FFMPEG_MT_GPU_UPLOAD
				last_frame_texture = last_frame->get_texture();
#endif

				// 更新性能指标
				state_manager->increment_frames_decoded();
				auto now = std::chrono::steady_clock::now();
				auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time);
				last_frame_time = now;

				// 简单的帧率计算
				if (frame_duration.count() > 0) {
					double fps = 1000.0 / frame_duration.count();
					// 这里可以添加FPS监控日志
				}
			}
		}

		// 处理音频帧
		Vector<Ref<DecodedAudioFrame>> audio_frames = decoder->get_decoded_audio_frames();
		if (!audio_frames.is_empty()) {
			MutexLock lock(frame_mutex);
			for (const Ref<DecodedAudioFrame>& audio_frame : audio_frames) {
				available_audio_frames.push_back(audio_frame);
			}
		}
	}

#ifndef FFMPEG_MT_GPU_UPLOAD
	// YUV到RGB转换（主线程中快速处理）
	if (last_frame.is_valid() && last_frame_image.is_valid()) {
		if (last_frame->get_format() == FFmpegFrameFormat::YUV420P || last_frame->get_format() == FFmpegFrameFormat::YUVA420P) {
			Ref<Image> y_plane = last_frame->get_yuv_image_plane(0);
			Ref<Image> u_plane = last_frame->get_yuv_image_plane(1);
			Ref<Image> v_plane = last_frame->get_yuv_image_plane(2);
			Ref<Image> a_plane = last_frame->get_yuv_image_plane(3);

			if (y_plane.is_valid() && u_plane.is_valid() && v_plane.is_valid()) {
				yuv_converter->set_plane_image(0, y_plane);
				yuv_converter->set_plane_image(1, u_plane);
				yuv_converter->set_plane_image(2, v_plane);
				yuv_converter->set_plane_image(3, a_plane);
				yuv_converter->convert();
			}
		} else if (texture.is_valid()) {
			if (texture->get_size() != last_frame_image->get_size() || texture->get_format() != last_frame_image->get_format()) {
				ZoneNamedN(__img_update_slow, "Image update slow", true);
				texture->set_image(last_frame_image);
			} else {
				ZoneNamedN(__img_update_fast, "Image update fast", true);
				texture->update(last_frame_image);
			}
		}
	}
#endif

	// 性能监控检查
	auto update_end = std::chrono::steady_clock::now();
	auto update_duration = std::chrono::duration_cast<std::chrono::microseconds>(update_end - update_start);
	if (update_duration.count() > 2000) { // 超过2ms
		// 记录性能警告
		print_line(vformat("Warning: update_internal took %ld microseconds", update_duration.count()));
	}
}

Error FFmpegVideoStreamPlayback::load_internal() {
	if (!decoder.is_valid()) {
		return FAILED;
	}

	// 初始化解码器
	Vector2i size = decoder->get_size();
	if (decoder->get_decoder_state() == VideoDecoder::FAULTED) {
		if (state_manager.is_valid()) {
			state_manager->set_error(PlayerError::DECODER_ERROR, "Decoder initialization failed");
		}
		return FAILED;
	}

	// 创建纹理转换器
	if (decoder->get_frame_format() == FFmpegFrameFormat::YUV420P || decoder->get_frame_format() == FFmpegFrameFormat::YUVA420P) {
		yuv_converter.instantiate();
		yuv_converter->set_frame_size(size);
		yuv_texture = yuv_converter->get_output_texture();
	} else {
#ifdef GDEXTENSION
		texture = ImageTexture::create_from_image(Image::create(size.x, size.y, false, Image::FORMAT_RGBA8));
#else
		texture = ImageTexture::create_from_image(Image::create_empty(size.x, size.y, false, Image::FORMAT_RGBA8));
#endif
	}

	// 更新状态为已加载
	if (state_manager.is_valid()) {
		state_manager->set_state(PlayerState::STOPPED);
	}

	return OK;
}

Error FFmpegVideoStreamPlayback::load(Ref<FileAccess> p_file_access) {
	// 创建解码器
	decoder = Ref<VideoDecoder>(memnew(VideoDecoder(p_file_access)));
	Error result = load_internal();

	// 如果加载成功，重置状态管理器
	if (result == OK && state_manager.is_valid()) {
		state_manager->reset_performance_metrics();
	}

	return result;
}

Error FFmpegVideoStreamPlayback::load(String uri) {
	// 通过命令队列异步加载URI
	if (state_manager.is_valid()) {
		command_queue.push_command(PlaybackCommand::set_uri(uri));
		return OK;
	}

	// 如果状态管理器尚未初始化，直接创建解码器
	decoder = Ref<VideoDecoder>(memnew(VideoDecoder(uri)));
	return load_internal();
}

bool FFmpegVideoStreamPlayback::is_paused_internal() const {
	if (state_manager.is_valid()) {
		return state_manager->get_state() == PlayerState::PAUSED;
	}
	return false;
}

bool FFmpegVideoStreamPlayback::is_playing_internal() const {
	if (state_manager.is_valid()) {
		return state_manager->is_playing_state();
	}
	return false;
}

void FFmpegVideoStreamPlayback::set_paused_internal(bool p_paused) {
	if (p_paused) {
		command_queue.push_command(PlaybackCommand::pause());
	} else {
		command_queue.push_command(PlaybackCommand::resume());
	}
}

void FFmpegVideoStreamPlayback::play_internal() {
	// 异步发送播放命令到工作线程
	command_queue.push_command(PlaybackCommand::play());
}

void FFmpegVideoStreamPlayback::stop_internal() {
	// 异步发送停止命令到工作线程
	command_queue.push_command(PlaybackCommand::stop());
}

void FFmpegVideoStreamPlayback::seek_internal(double p_time) {
	// 异步发送跳转命令到工作线程
	command_queue.push_command(PlaybackCommand::seek(p_time * 1000.0f));
}

double FFmpegVideoStreamPlayback::get_length_internal() const {
	return decoder->get_duration() / 1000.0f;
}

Ref<Texture2D> FFmpegVideoStreamPlayback::get_texture_internal() const {
#ifdef FFMPEG_MT_GPU_UPLOAD
	return last_frame_texture;
#else
	if (yuv_converter.is_valid()) {
		return yuv_converter->get_output_texture();
	}
	return texture;
#endif
}

double FFmpegVideoStreamPlayback::get_playback_position_internal() const {
	if (state_manager.is_valid()) {
		return state_manager->get_position() / 1000.0;
	}
	return playback_position / 1000.0;
}

int FFmpegVideoStreamPlayback::get_mix_rate_internal() const {
	return decoder->get_audio_mix_rate();
}

int FFmpegVideoStreamPlayback::get_channels_internal() const {
	return decoder->get_audio_channel_count();
}

FFmpegVideoStreamPlayback::FFmpegVideoStreamPlayback() {
	// 初始化状态管理器
	state_manager.instantiate();

	// 初始化性能监控时间
	last_update_time = std::chrono::steady_clock::now();
	last_frame_time = std::chrono::steady_clock::now();

	// 创建播放控制工作线程
	playback_thread = memnew(std::thread(_playback_thread_func, this));
}

FFmpegVideoStreamPlayback::~FFmpegVideoStreamPlayback() {
	// 停止播放控制工作线程
	if (playback_thread && playback_thread->joinable()) {
		// 发送终止命令
		command_queue.push_command(PlaybackCommand::terminate());

		// 等待线程结束
		playback_thread->join();
		memdelete(playback_thread);
		playback_thread = nullptr;
	}
}

void FFmpegVideoStreamPlayback::clear() {
	last_frame.unref();
	last_frame_texture.unref();
	available_frames.clear();
	available_audio_frames.clear();
	frames_processed = 0;
	playing = false;
}

YUVGPUConverter::~YUVGPUConverter() {
	for (size_t i = 0; i < std::size(yuv_planes_uniform_sets); i++) {
		if (yuv_planes_uniform_sets[i].is_valid()) {
			FREE_RD_RID(yuv_planes_uniform_sets[i]);
		}
		if (yuv_plane_textures[i].is_valid()) {
			FREE_RD_RID(yuv_plane_textures[i]);
		}
	}

	if (out_texture.is_valid() && out_texture->get_texture_rd_rid().is_valid()) {
		RID out_rid = out_texture->get_texture_rd_rid();
		out_texture->set_texture_rd_rid(RID());
		FREE_RD_RID(out_rid);
	}

	if (pipeline.is_valid()) {
		FREE_RD_RID(pipeline);
	}

	if (shader.is_valid()) {
		FREE_RD_RID(shader);
	}
}

void YUVGPUConverter::_clear_texture_internal() {
	if (out_texture.is_valid() && out_texture->get_texture_rd_rid().is_valid()) {
		RD *rd = RS::get_singleton()->get_rendering_device();
		rd->texture_clear(out_texture->get_texture_rd_rid(), Color(0, 0, 0, 0), 0, 1, 0, 1);
	}
}

void YUVGPUConverter::_ensure_pipeline() {
	if (pipeline.is_valid()) {
		return;
	}

	RD *rd = RS::get_singleton()->get_rendering_device();

#ifdef GDEXTENSION

	Ref<RDShaderSource> shader_source;
	shader_source.instantiate();
	// Ugly hack to skip the #[compute] in the header, because parse_versions_from_text is not available through GDNative
	shader_source->set_stage_source(RenderingDevice::ShaderStage::SHADER_STAGE_COMPUTE, yuv_to_rgb_shader_glsl + 10);
	Ref<RDShaderSPIRV> shader_spirv = rd->shader_compile_spirv_from_source(shader_source);

#else

	Ref<RDShaderFile> shader_file;
	shader_file.instantiate();
	Error err = shader_file->parse_versions_from_text(yuv_to_rgb_shader_glsl);
	if (err != OK) {
		print_line("Something catastrophic happened, call eirexe");
	}
	Vector<RD::ShaderStageSPIRVData> shader_spirv = shader_file->get_spirv_stages();

#endif
	shader = rd->shader_create_from_spirv(shader_spirv);
	pipeline = rd->compute_pipeline_create(shader);
}

Error YUVGPUConverter::_ensure_plane_textures() {
	RD *rd = RS::get_singleton()->get_rendering_device();
	for (size_t i = 0; i < std::size(yuv_plane_textures); i++) {
		if (yuv_plane_textures[i].is_valid()) {
			RDTextureFormatC format = TEXTURE_FORMAT_COMPAT(rd->texture_get_format(yuv_plane_textures[i]));

			int desired_frame_width = i == 0 || i == 3 ? frame_size.width : Math::ceil(frame_size.width / 2.0f);
			int desired_frame_height = i == 0 || i == 3 ? frame_size.height : Math::ceil(frame_size.height / 2.0f);

			if (static_cast<int>(format.width) == desired_frame_width && static_cast<int>(format.height) == desired_frame_height) {
				continue;
			}
			continue;
		}

		// Texture didn't exist or was invalid, re-create it

		// free existing texture if needed
		if (yuv_plane_textures[i].is_valid()) {
			FREE_RD_RID(yuv_plane_textures[i]);
		}

		RDTextureFormatC new_format;
		new_format.format = RenderingDevice::DATA_FORMAT_R8_UNORM;
		// chroma planes are half the size of the luma plane
		new_format.width = i == 0 || i == 3 ? frame_size.width : Math::ceil(frame_size.width / 2.0f);
		new_format.height = i == 0 || i == 3 ? frame_size.height : Math::ceil(frame_size.height / 2.0f);
		new_format.depth = 1;
		new_format.array_layers = 1;
		new_format.mipmaps = 1;
		new_format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;

#ifdef GDEXTENSION
		Ref<RDTextureFormat> new_format_c = new_format.get_texture_format();
		Ref<RDTextureViewC> texture_view;
		texture_view.instantiate();
#else
		RD::TextureFormat new_format_c = new_format;
		RDTextureViewC texture_view;
#endif
		yuv_plane_textures[i] = rd->texture_create(new_format_c, texture_view);

		if (yuv_planes_uniform_sets[i].is_valid()) {
			FREE_RD_RID(yuv_planes_uniform_sets[i]);
		}

		yuv_planes_uniform_sets[i] = _create_uniform_set(yuv_plane_textures[i]);
	}

	return OK;
}

Error YUVGPUConverter::_ensure_output_texture() {
	_ensure_pipeline();
	RD *rd = RS::get_singleton()->get_rendering_device();
	if (!out_texture.is_valid()) {
		out_texture.instantiate();
	}

	if (out_texture->get_texture_rd_rid().is_valid()) {
		RDTextureFormatC format = TEXTURE_FORMAT_COMPAT(rd->texture_get_format(out_texture->get_texture_rd_rid()));
		if (static_cast<int>(format.width) == frame_size.width && static_cast<int>(format.height) == frame_size.height) {
			return OK;
		}
	}

	if (out_texture->get_texture_rd_rid().is_valid()) {
		FREE_RD_RID(out_texture->get_texture_rd_rid());
	}

	RDTextureFormatC out_texture_format;
	out_texture_format.format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
	out_texture_format.width = frame_size.width;
	out_texture_format.height = frame_size.height;
	out_texture_format.depth = 1;
	out_texture_format.array_layers = 1;
	out_texture_format.mipmaps = 1;
	// RD::TEXTURE_USAGE_CAN_UPDATE_BIT not needed since we won't update it from the CPU
	out_texture_format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;

#ifdef GDEXTENSION
	Ref<RDTextureView> texture_view;
	texture_view.instantiate();
	Ref<RDTextureFormat> out_texture_format_c = out_texture_format.get_texture_format();
#else
	RD::TextureFormat out_texture_format_c = out_texture_format;
	RD::TextureView texture_view;
#endif
	out_texture->set_texture_rd_rid(rd->texture_create(out_texture_format_c, texture_view));
	rd->texture_clear(out_texture->get_texture_rd_rid(), Color(0, 0, 0, 0), 0, 1, 0, 1);

	if (out_uniform_set.is_valid()) {
		FREE_RD_RID(out_uniform_set);
	}
	out_uniform_set = _create_uniform_set(out_texture->get_texture_rd_rid());
	return OK;
}

RID YUVGPUConverter::_create_uniform_set(const RID &p_texture_rd_rid) {
#ifdef GDEXTENSION
	Ref<RDUniform> uniform;
	uniform.instantiate();
	uniform->set_binding(0);
	uniform->set_uniform_type(RD::UNIFORM_TYPE_IMAGE);
	uniform->add_id(p_texture_rd_rid);
	TypedArray<RDUniform> uniforms;
	uniforms.push_back(uniform);
#else
	RD::Uniform uniform;
	uniform.uniform_type = RD::UNIFORM_TYPE_IMAGE;
	uniform.binding = 0;
	uniform.append_id(p_texture_rd_rid);
	Vector<RD::Uniform> uniforms;
	uniforms.push_back(uniform);
#endif
	return RS::get_singleton()->get_rendering_device()->uniform_set_create(uniforms, shader, 0);
}

void YUVGPUConverter::_upload_plane_images() {
	for (size_t i = 0; i < std::size(yuv_plane_images); i++) {
		ERR_CONTINUE_MSG(!yuv_plane_images[i].is_valid() && i != 3, vformat("YUV plane %d was missing, cannot upload texture data.", (int)i));
		if (!yuv_plane_images[i].is_valid()) {
			continue;
		}
		RS::get_singleton()->get_rendering_device()->texture_update(yuv_plane_textures[i], 0, yuv_plane_images[i]->get_data());
	}
}

void YUVGPUConverter::set_plane_image(int p_plane_idx, Ref<Image> p_image) {
	if (!p_image.is_valid()) {
		yuv_plane_images[p_plane_idx] = p_image;
		return;
	}
	ERR_FAIL_COND(!p_image.is_valid());
	ERR_FAIL_INDEX((size_t)p_plane_idx, std::size(yuv_plane_images));
	// Sanity checks
	int desired_frame_width = p_plane_idx == 0 || p_plane_idx == 3 ? frame_size.width : Math::ceil(frame_size.width / 2.0f);
	int desired_frame_height = p_plane_idx == 0 || p_plane_idx == 3 ? frame_size.height : Math::ceil(frame_size.height / 2.0f);
	ERR_FAIL_COND_MSG(p_image->get_width() != desired_frame_width, vformat("Wrong YUV plane width for plane %d, expected %d got %d", p_plane_idx, desired_frame_width, p_image->get_width()));
	ERR_FAIL_COND_MSG(p_image->get_height() != desired_frame_height, vformat("Wrong YUV plane height for plane %, expected %d got %d", p_plane_idx, desired_frame_height, p_image->get_height()));
	ERR_FAIL_COND_MSG(p_image->get_format() != Image::FORMAT_R8, "Wrong image format, expected R8");
	yuv_plane_images[p_plane_idx] = p_image;
}

Vector2i YUVGPUConverter::get_frame_size() const { return frame_size; }

void YUVGPUConverter::set_frame_size(const Vector2i &p_frame_size) {
	ERR_FAIL_COND_MSG(p_frame_size.x == 0, "Frame size cannot be zero!");
	ERR_FAIL_COND_MSG(p_frame_size.y == 0, "Frame size cannot be zero!");
	frame_size = p_frame_size;

	yuv_plane_images[0].unref();
	yuv_plane_images[1].unref();
	yuv_plane_images[2].unref();
}

void YUVGPUConverter::convert() {
	RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &YUVGPUConverter::_convert_internal));
}

void YUVGPUConverter::_convert_internal() {
	// First we must ensure everything we need exists
	_ensure_pipeline();
	_ensure_plane_textures();
	_ensure_output_texture();
	_upload_plane_images();

	RD *rd = RS::get_singleton()->get_rendering_device();

	push_constant.use_alpha = yuv_plane_images[3].is_valid() ? 1 : 0;

	PackedByteArray push_constant_data;
	push_constant_data.resize(sizeof(push_constant));
	memcpy(push_constant_data.ptrw(), &push_constant, push_constant_data.size());

	ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, pipeline);

#ifdef GDEXTENSION
	rd->compute_list_set_push_constant(compute_list, push_constant_data, push_constant_data.size());
#else
	rd->compute_list_set_push_constant(compute_list, push_constant_data.ptr(), push_constant_data.size());
#endif
	rd->compute_list_bind_uniform_set(compute_list, yuv_planes_uniform_sets[0], 0);
	rd->compute_list_bind_uniform_set(compute_list, yuv_planes_uniform_sets[1], 1);
	rd->compute_list_bind_uniform_set(compute_list, yuv_planes_uniform_sets[2], 2);
	rd->compute_list_bind_uniform_set(compute_list, yuv_planes_uniform_sets[3], 3);
	rd->compute_list_bind_uniform_set(compute_list, out_uniform_set, 4);
	rd->compute_list_dispatch(compute_list, Math::ceil(frame_size.x / 8.0f), Math::ceil(frame_size.y / 8.0f), 1);
	rd->compute_list_end();
}

Ref<Texture2D> YUVGPUConverter::get_output_texture() const {
	const_cast<YUVGPUConverter *>(this)->_ensure_output_texture();
	return out_texture;
}

void YUVGPUConverter::clear_output_texture() {
	RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &YUVGPUConverter::_clear_texture_internal));
}

YUVGPUConverter::YUVGPUConverter() {
	out_texture.instantiate();
}

// FFmpegVideoStreamPlayback 工作线程函数实现
void FFmpegVideoStreamPlayback::_playback_thread_func(void *userdata) {
	FFmpegVideoStreamPlayback *playback = static_cast<FFmpegVideoStreamPlayback*>(userdata);

	while (!playback->playback_thread_abort.is_set()) {
		// 处理命令队列
		playback->_process_commands();

		// 更新解码器状态
		playback->_update_decoder_state();

		// 处理解码器事件
		playback->_handle_decoder_events();

		// 短暂休眠以避免CPU占用过高
		OS::get_singleton()->delay_usec(1000); // 1ms
	}
}

void FFmpegVideoStreamPlayback::_process_commands() {
	PlaybackCommand command;
	while (command_queue.pop_command(command)) {
		_process_playback_command(command);
	}
}

void FFmpegVideoStreamPlayback::_process_playback_command(const PlaybackCommand& command) {
	switch (command.type) {
		case PlaybackCommandType::PLAY:
			if (state_manager->get_state() == PlayerState::STOPPED || state_manager->get_state() == PlayerState::FAULTED) {
				state_manager->set_state(PlayerState::STARTING);

				if (decoder.is_valid()) {
					playback_position = 0.0;
					decoder->seek(0.0, true);
					decoder->start_decoding();
					state_manager->set_state(PlayerState::PLAYING);
				} else {
					state_manager->set_error(PlayerError::INVALID_URI, "No media loaded");
				}
			}
			break;

		case PlaybackCommandType::STOP:
			if (state_manager->is_playing_state()) {
				state_manager->set_state(PlayerState::STOPPING);

				if (decoder.is_valid()) {
					decoder->seek(0.0, true);
				}

				// 清理帧缓冲区
				{
					MutexLock lock(frame_mutex);
					available_frames.clear();
					available_audio_frames.clear();
					last_frame.unref();
					last_frame_texture.unref();
					last_frame_image.unref();
				}

				if (yuv_converter.is_valid()) {
					yuv_converter->clear_output_texture();
				}

				playback_position = 0.0;
				texture.unref();
				frames_processed = 0;

				state_manager->set_state(PlayerState::STOPPED);
			}
			break;

		case PlaybackCommandType::PAUSE:
			if (state_manager->get_state() == PlayerState::PLAYING) {
				state_manager->set_state(PlayerState::PAUSED);
			}
			break;

		case PlaybackCommandType::RESUME:
			if (state_manager->get_state() == PlayerState::PAUSED) {
				state_manager->set_state(PlayerState::PLAYING);
			}
			break;

		case PlaybackCommandType::SEEK:
			if (decoder.is_valid() && state_manager->is_playing_state()) {
				state_manager->set_state(PlayerState::SEEKING);

				// 清理当前帧缓冲区
				{
					MutexLock lock(frame_mutex);
					available_frames.clear();
					available_audio_frames.clear();
				}

				playback_position = command.timestamp;
				decoder->seek(command.timestamp, true);
				just_seeked = true;

				state_manager->set_state(PlayerState::PLAYING);
			}
			break;

		case PlaybackCommandType::SET_URI:
			// 停止当前播放
			_process_playback_command(PlaybackCommand::stop());

			// 设置新的URI
			if (decoder.is_valid()) {
				decoder->return_frames(decoder->get_decoded_frames());
				decoder.unref();
			}

			// 使用URI构造新的decoder
			decoder = Ref<VideoDecoder>(memnew(VideoDecoder(command.uri)));
			if (decoder->get_decoder_state() != VideoDecoder::FAULTED) {
				state_manager->set_state(PlayerState::STOPPED);
			} else {
				state_manager->set_error(PlayerError::INVALID_URI, "Failed to load media: " + command.uri);
			}
			break;

		case PlaybackCommandType::TERMINATE:
			_process_playback_command(PlaybackCommand::stop());
			playback_thread_abort.set();
			break;

		default:
			break;
	}
}

void FFmpegVideoStreamPlayback::_update_decoder_state() {
	if (!decoder.is_valid()) {
		return;
	}

	VideoDecoder::DecoderState decoder_state = decoder->get_decoder_state();
	PlayerState current_state = state_manager->get_state();

	switch (decoder_state) {
		case VideoDecoder::FAULTED:
			if (current_state != PlayerState::FAULTED) {
				state_manager->set_error(PlayerError::DECODER_ERROR, "Video decoder faulted");
			}
			break;

		case VideoDecoder::END_OF_STREAM:
			if (current_state == PlayerState::PLAYING) {
				// 添加流结束事件
				PlaybackEvent event = PlaybackEvent::end_of_stream();
				state_manager->add_event(event);

				if (looping) {
					// 循环播放
					_process_playback_command(PlaybackCommand::seek(0.0));
				} else {
					_process_playback_command(PlaybackCommand::stop());
				}
			}
			break;

		case VideoDecoder::RUNNING:
			// 更新播放位置
			if (current_state == PlayerState::PLAYING && decoder->get_decoder_state() == VideoDecoder::RUNNING) {
				double last_frame_time = decoder->get_last_decoded_frame_time();
				if (last_frame_time >= 0) {
					playback_position = last_frame_time;
					state_manager->update_position(playback_position);
				}
			}
			break;

		default:
			break;
	}
}

void FFmpegVideoStreamPlayback::_handle_decoder_events() {
	// 检查是否需要重试
	PlayerError error = state_manager->get_error();
	if (error != PlayerError::NONE && state_manager->should_retry()) {
		_process_playback_command(PlaybackCommand::play());
	}

	// 处理缓冲区状态
	if (decoder.is_valid()) {
		Vector<Ref<DecodedFrame>> decoded_frames = decoder->get_decoded_frames();
		bool is_buffering = decoded_frames.is_empty() && state_manager->get_state() == PlayerState::PLAYING;

		if (is_buffering) {
			state_manager->set_state(PlayerState::BUFFERING);
			state_manager->update_buffering(0.0f);
		} else if (state_manager->get_state() == PlayerState::BUFFERING && !decoded_frames.is_empty()) {
			state_manager->set_state(PlayerState::PLAYING);
			state_manager->update_buffering(100.0f);
		}
	}
}
