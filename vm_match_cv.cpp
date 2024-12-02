#include "vm_match_cv.h"

#include <format>
#include <map>
#include <algorithm>

#include "vm_option_cv.h"
#include "vm_log.h"

namespace vm_match_cv
{
	fnum* match_frame_list;
	std::map<fnum, cv::Mat> frame_buffer_map;
	fnum frame_buffer_back, frame_read_pos;
	fnum video_frame_num_1, video_frame_num_2;

	bool _read_frame(cv::Mat& frame_2, cv::Mat& frame_2_resize){
		bool isread = vm_option_cv::video_cap_2.read(frame_2);

		if(vm_option_cv::frame_scale != 1){
			cv::resize(frame_2, frame_2_resize,
					   cv::Size(vm_option_cv::new_width, vm_option_cv::new_height),
					   0, 0, cv::INTER_NEAREST);
		}
		else
			frame_2_resize = frame_2;

		if(vm_option_cv::debug && frame_2_resize.empty())
			vm_log::error("vm_match::_read_frame(cv::Mat& frame_2, cv::Mat& frame_2_resize): read an empty frame in video 2");

		return isread;
	}
	bool _read_frame(cv::Mat& frame_2){
		bool isread = vm_option_cv::video_cap_2.read(frame_2);

		if(vm_option_cv::debug && frame_2.empty())
			vm_log::error("vm_match::_read_frame(cv::Mat& frame_2): read an empty frame in video 2");

		if(vm_option_cv::frame_scale != 1){
			cv::Mat frame_2_resize;
			cv::resize(frame_2, frame_2_resize,
					   cv::Size(vm_option_cv::new_width, vm_option_cv::new_height),
					   0, 0, cv::INTER_NEAREST);
			frame_2 = frame_2_resize;
		}
		return isread;
	}
	bool _read_frame(cv::VideoCapture& cap, cv::Mat& frame){
		bool isread = cap.read(frame);

		if(vm_option_cv::debug && frame.empty()){
			uint8_t cap_num = 1;
			if(&cap == &vm_option_cv::video_cap_2)
				cap_num = 2;
			vm_log::error(std::format("vm_match::_read_frame(cv::VideoCapture& cap, cv::Mat& frame): read an empty frame in video {0}", cap_num));
		}

		if(vm_option_cv::frame_scale != 1){
			cv::Mat frame_resize;
			cv::resize(frame, frame_resize,
					   cv::Size(vm_option_cv::new_width, vm_option_cv::new_height),
					   0, 0, cv::INTER_NEAREST);
			frame = frame_resize;
		}
		return isread;
	}

	void _get_new_buffer(){
		frame_buffer_map.clear();
		vm_option_cv::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, video_frame_num_2);
		for(fnum i = video_frame_num_2;
			i < video_frame_num_2 + vm_option_cv::frame_buffer_size && i < vm_option_cv::frame_count_2;
			++i){
			cv::Mat frame;
			_read_frame(frame, frame_buffer_map[i]);

			if(vm_option_cv::debug && frame.empty())
				vm_log::error(std::format("vm_match::_get_new_buffer: The frame is empty: frame num {0} in video 2", i));
			if(vm_option_cv::debug && frame_buffer_map[i].empty())
				vm_log::error(std::format("vm_match::_get_new_buffer: The frame is empty: frame num {0} in video 2 buffer", i));
		}
		frame_read_pos = std::min(video_frame_num_2+vm_option_cv::frame_buffer_size, vm_option_cv::frame_count_2);
		frame_buffer_back = frame_read_pos - 1;
	}

	bool _get_frame_2(cv::Mat& frame_2, const fnum& frame_num){
		if(!vm_option_cv::frame_buffer_size)
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
				vm_option_cv::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, frame_num);
				frame_read_pos = frame_num+1;
				return _read_frame(frame_2);
			}
		}
		else{
			frame_2 = frame_buffer_map[frame_num];

			if(vm_option_cv::debug && frame_2.empty())
				vm_log::error(std::format("vm_match::_get_frame_2: The frame is empty: frame num {0} in video 2 buffer", frame_num));

			return true;
		}
	}

	double compare_ssim(const cv::Mat& frame_1, const cv::Mat& frame_2){
		cv::Mat frame_1_gray, frame_2_gray;
		cv::cvtColor(frame_1, frame_1_gray, cv::COLOR_BGR2GRAY);
		cv::cvtColor(frame_2, frame_2_gray, cv::COLOR_BGR2GRAY);

		cv::Mat validImage1, validImage2;
		frame_1_gray.convertTo(validImage1, CV_32F); //数据类型转换为 float, 防止后续计算出现错误
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

		if(vm_option_cv::debug)
			vm_log::info(std::format("{0} SSIM: {1}", video_frame_num_1, cv::mean(ssim)[0]));
		return cv::mean(ssim)[0];
	}

	bool frame_cmp(const cv::Mat& frame_1, const cv::Mat& frame_2){
		return compare_ssim(frame_1, frame_2) >= vm_option_cv::ssim_threshold;
	}

	void do_match(){
		match_frame_list = new fnum[vm_option_cv::frame_count_1];
		video_frame_num_1 = video_frame_num_2 = 0;
		cv::Mat frame_1, frame_2;

		// 初始化buffer
		if(vm_option_cv::frame_buffer_size)
			_get_new_buffer();

		// 读取并对比
		vm_option_cv::video_cap_1.set(cv::CAP_PROP_POS_FRAMES, 0);
		for(; video_frame_num_1 < vm_option_cv::frame_count_1; ++video_frame_num_1){
			bool isread = _read_frame(vm_option_cv::video_cap_1, frame_1);
			if(!isread)
				vm_log::error(std::format(R"(vm_match::do_match: Can not read a frame: frame num {0} in video 1)", video_frame_num_1));

			if(!vm_option_cv::frame_buffer_size)
				vm_option_cv::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, video_frame_num_2);
			bool is_not_finded = true;
			for(fnum i = video_frame_num_2;
				i <= video_frame_num_2+vm_option_cv::frame_forward && i < vm_option_cv::frame_count_2;
				++i){
				bool isread = _get_frame_2(frame_2, i);
				if(!isread)
					vm_log::error(std::format(R"(vm_match::do_match: Can not read a frame: frame num {0} in video 2)", i));

				if(vm_option_cv::debug){
					if(frame_1.empty())
						vm_log::error(std::format(R"(vm_match::do_match: The frame is empty: frame num {0} in video {1})", video_frame_num_1, 1));
					if(frame_2.empty())
						vm_log::error(std::format(R"(vm_match::do_match: The frame is empty: frame num {0} in video {1})", i, 2));
					if(!frame_1.empty()&&!frame_2.empty()&&
					   frame_cmp(frame_1, frame_2)){
						match_frame_list[video_frame_num_1] = i;
						video_frame_num_2 = i+1;
						is_not_finded = false;
						break;
					}
				}
				else
					if(frame_cmp(frame_1, frame_2)){
						match_frame_list[video_frame_num_1] = i;
						video_frame_num_2 = i+1;
						is_not_finded = false;
						break;
					}
			}
			if(is_not_finded)
				match_frame_list[video_frame_num_1] = -1,
				++video_frame_num_2;
			vm_log::change_title(std::format(R"({0} / {1})", video_frame_num_1, vm_option_cv::frame_count_1-1));
		}

		vm_option_cv::video_cap_1.release();
		vm_option_cv::video_cap_2.release();
	}
}