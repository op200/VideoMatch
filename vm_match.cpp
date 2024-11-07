#include "vm_match.h"

#include <opencv2/opencv.hpp>
#include <format>
#include <map>
#include <algorithm>

#include "vm_option.h"
#include "vm_log.h"

namespace vm_match
{
	int32_t* match_frame_list;
	std::map<fnum, cv::Mat> frame_buffer_map;
	fnum frame_buffer_back, frame_read_pos;
	fnum video_frame_num_1, video_frame_num_2;

	bool _read_frame(cv::Mat& frame_2, cv::Mat& frame_2_resize){
		bool isread = vm_option::video_cap_2.read(frame_2);
		if(vm_option::frame_scale != 1){
			cv::resize(frame_2, frame_2_resize,
					   cv::Size(vm_option::new_width, vm_option::new_height),
					   0, 0, cv::INTER_NEAREST);
		}
		return isread;
	}
	bool _read_frame(cv::Mat& frame_2){
		bool isread = vm_option::video_cap_2.read(frame_2);
		if(vm_option::frame_scale != 1){
			cv::Mat frame_2_resize;
			cv::resize(frame_2, frame_2_resize,
					   cv::Size(vm_option::new_width, vm_option::new_height),
					   0, 0, cv::INTER_NEAREST);
			frame_2 = frame_2_resize;
		}
		return isread;
	}
	bool _read_frame(cv::VideoCapture cap, cv::Mat& frame_2){
		bool isread = cap.read(frame_2);
		if(vm_option::frame_scale != 1){
			cv::Mat frame_2_resize;
			cv::resize(frame_2, frame_2_resize,
					   cv::Size(vm_option::new_width, vm_option::new_height),
					   0, 0, cv::INTER_NEAREST);
			frame_2 = frame_2_resize;
		}
		return isread;
	}

	void _get_new_buffer(){
		frame_buffer_map.clear();
		vm_option::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, video_frame_num_2);
		for(fnum i=video_frame_num_2;
			i<video_frame_num_2+vm_option::frame_buffer_size && i<vm_option::frame_count_2;
			++i){
			cv::Mat frame;
			_read_frame(frame, frame_buffer_map[i]);
		}
		frame_read_pos = std::min(video_frame_num_2+vm_option::frame_buffer_size, vm_option::frame_count_2);
		frame_buffer_back = frame_read_pos - 1;
	}

	bool _get_frame_2(cv::Mat& frame_2, const fnum& frame_num){
		if(!vm_option::frame_buffer_size)
			return _read_frame(frame_2);


		// video_frame_num_2 > 边界 ? 移动buffer
		if(video_frame_num_2 > frame_buffer_back)
			_get_new_buffer();

		// 读取位置超出buffer ? 单独读取 : 读buffer
		if(frame_num > frame_buffer_back){
			if(frame_num == frame_read_pos){
				++frame_read_pos;
				return _read_frame(frame_2);
			}
			else{
				vm_option::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, frame_num);
				frame_read_pos = frame_num+1;
				return _read_frame(frame_2);
			}
		}
		else{
			frame_2 = frame_buffer_map[frame_num];
			return true;
		}
	}

	double compare_ssim(const cv::Mat& frame_1, const cv::Mat& frame_2){
		cv::Mat frame_1_gray, frame_2_gray;
		cv::cvtColor(frame_1, frame_1_gray, cv::COLOR_BGR2GRAY);
		cv::cvtColor(frame_2, frame_2_gray, cv::COLOR_BGR2GRAY);

		cv::Mat validImage1, validImage2;
		frame_1_gray.convertTo(validImage1, CV_32F); //数据类型转换为 float,防止后续计算出现错误
		frame_2_gray.convertTo(validImage2, CV_32F);

		cv::Mat image1_1 = validImage1.mul(validImage1); //图像乘积
		cv::Mat image2_2 = validImage2.mul(validImage2);
		cv::Mat image1_2 = validImage1.mul(validImage2);

		cv::Mat gausBlur1, gausBlur2, gausBlur12;
		GaussianBlur(validImage1, gausBlur1, cv::Size(11, 11), 1.5); //高斯卷积核计算图像均值
		GaussianBlur(validImage2, gausBlur2, cv::Size(11, 11), 1.5);
		GaussianBlur(image1_2, gausBlur12, cv::Size(11, 11), 1.5);

		cv::Mat imageAvgProduct = gausBlur1.mul(gausBlur2); //均值乘积
		cv::Mat u1Squre = gausBlur1.mul(gausBlur1); //各自均值的平方
		cv::Mat u2Squre = gausBlur2.mul(gausBlur2);

		cv::Mat imageConvariance, imageVariance1, imageVariance2;
		cv::Mat squreAvg1, squreAvg2;
		GaussianBlur(image1_1, squreAvg1, cv::Size(11, 11), 1.5); //图像平方的均值
		GaussianBlur(image2_2, squreAvg2, cv::Size(11, 11), 1.5);

		imageConvariance = gausBlur12 - gausBlur1.mul(gausBlur2);// 计算协方差
		imageVariance1 = squreAvg1 - gausBlur1.mul(gausBlur1); //计算方差
		imageVariance2 = squreAvg2 - gausBlur2.mul(gausBlur2);
		cv::MatExpr member = ((2 * gausBlur1.mul(gausBlur2) + 6.5025).mul(2 * imageConvariance + 58.5225));
		cv::MatExpr denominator = ((u1Squre + u2Squre + 6.5025).mul(imageVariance1 + imageVariance2 + 58.5225));
		cv::Mat ssim;
		cv::divide(member, denominator, ssim);
		return cv::mean(ssim)[0];
	}

	bool frame_cmp(const cv::Mat& frame_1, const cv::Mat& frame_2){
		return compare_ssim(frame_1, frame_2) >= vm_option::ssim_threshold;
	}

	void do_match(){
		match_frame_list = new int32_t[vm_option::frame_count_1];
		video_frame_num_1 = video_frame_num_2 = 0;
		cv::Mat frame_1, frame_2;

		// 初始化buffer
		if(vm_option::frame_buffer_size)
			_get_new_buffer();

		// 读取并对比
		vm_option::video_cap_1.set(cv::CAP_PROP_POS_FRAMES, 0);
		for(; video_frame_num_1 < vm_option::frame_count_1; ++video_frame_num_1){
			_read_frame(vm_option::video_cap_1, frame_1);

			if(!vm_option::frame_buffer_size)
				vm_option::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, video_frame_num_2);
			bool is_not_finded = true;
			for(fnum i = video_frame_num_2;
				i <= video_frame_num_2+vm_option::frame_backward && i < vm_option::frame_count_2;
				++i){
				bool isread = _get_frame_2(frame_2, i);
				if(!isread)vm_log::error(video_frame_num_1+" "+video_frame_num_2);
				if(frame_cmp(frame_1, frame_2)){
					match_frame_list[video_frame_num_1] = i;
					video_frame_num_2 = i+1;
					is_not_finded = false;
					break;
				}
			}
			if(is_not_finded)
				match_frame_list[video_frame_num_1] = -1, ++video_frame_num_2;
			vm_log::change_title(std::format(R"({0} / {1})", video_frame_num_1, vm_option::frame_count_1));
		}

		vm_option::video_cap_1.release();
		vm_option::video_cap_2.release();
	}
}