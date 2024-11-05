#include "vm_match.h"

#include <opencv2/opencv.hpp>
#include <format>

#include "vm_option.h"
#include "vm_log.h"

namespace vm_match
{
	int32_t* match_frame_list;

	void do_match(){
		match_frame_list = new int32_t[vm_option::frame_count_1];
		uint32_t video_frame_num_1 = 0, video_frame_num_2 = 0;
		cv::Mat frame_1, frame_2;

		for(; video_frame_num_1 < vm_option::frame_count_1; ++video_frame_num_1){
			vm_option::video_cap_1.read(frame_1);
			vm_option::video_cap_2.set(cv::CAP_PROP_POS_FRAMES, video_frame_num_2);
			bool is_not_finded = true;
			for(uint32_t i = video_frame_num_2;
				i <= video_frame_num_2+vm_option::frame_backward && i < vm_option::frame_count_2;
				++i){
				bool isread = vm_option::video_cap_2.read(frame_2);
				if(!isread)vm_log::error(video_frame_num_1+" "+video_frame_num_2);
				if(frame_2.empty())vm_log::error(video_frame_num_1+" "+video_frame_num_2);
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

	bool frame_cmp(const cv::Mat& frame_1, const cv::Mat& frame_2){
		return compare_ssim(frame_1, frame_2) >= vm_option::ssim_threshold;
	}

    double compare_ssim(const cv::Mat& frame_1, const cv::Mat& frame_2){
		cv::Mat frame_1_gray, frame_2_gray;
		cv::cvtColor(frame_1, frame_1_gray, cv::COLOR_BGR2GRAY);
		cv::cvtColor(frame_2, frame_2_gray, cv::COLOR_BGR2GRAY);

		cv::Mat frame_1_gray_resize, frame_2_gray_resize;
		if(vm_option::frame_scale == 1)
			frame_1_gray_resize = frame_1_gray,
			frame_2_gray_resize = frame_2_gray;
		else
			cv::resize(frame_1_gray, frame_1_gray_resize,
					   cv::Size(vm_option::new_width, vm_option::new_height),
					   0, 0, cv::INTER_NEAREST),
			cv::resize(frame_2_gray, frame_2_gray_resize,
					   cv::Size(vm_option::new_width, vm_option::new_height),
					   0, 0, cv::INTER_NEAREST);

		cv::Mat validImage1, validImage2;
		frame_1_gray_resize.convertTo(validImage1, CV_32F); //数据类型转换为 float,防止后续计算出现错误
		frame_2_gray_resize.convertTo(validImage2, CV_32F);
		
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
}