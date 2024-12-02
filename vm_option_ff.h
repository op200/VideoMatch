#pragma once

extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <string>
#include <vector>

#include "vm_type.h"

namespace vm_option_ff
{
	enum class output_type_enum{
		nooutput, framenum
	};

	extern std::string input_video_path_1, input_video_path_2, log_path;
	extern output_type_enum output_type;
	extern uint16_t frame_scale;
	extern double ssim_threshold;
	extern int16_t frame_forward;
	extern bool benchmark, debug;
	extern std::string hwaccel;

	extern int8_t video_stream_index_1, video_stream_index_2;
	extern AVFormatContext *formatContext_1, *formatContext_2;
	extern AVCodecContext *codecContext_1, *codecContext_2;

	void get_option(std::vector<std::string>& args);

	extern fnum frame_count_1, frame_count_2;
	extern uint32_t new_width, new_height;
}