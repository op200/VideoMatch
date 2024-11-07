#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "vm_type.h"

namespace vm_option
{
	enum class output_type_enum{
		nooutput, framenum
	};

	extern std::string input_video_path_1, input_video_path_2, log_path;
	extern output_type_enum output_type;
	extern uint16_t frame_scale, frame_backward;
	extern double ssim_threshold;
	extern int16_t frame_buffer_size;
	extern bool benchmark;

	void get_option(std::vector<std::string>& args);

	extern cv::VideoCapture video_cap_1, video_cap_2;
	extern fnum frame_count_1, frame_count_2, new_width, new_height;
}