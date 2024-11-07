#pragma once

#include <opencv2/opencv.hpp>

#include "vm_type.h"

namespace vm_match
{
	extern int32_t* match_frame_list;

	void do_match();

	/*bool frame_cmp(const cv::Mat& frame_1, const cv::Mat& frame_2);

	double compare_ssim(const cv::Mat& frame_1, const cv::Mat& frame_2);*/
}