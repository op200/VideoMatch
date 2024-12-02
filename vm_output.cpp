#include "vm_output.h"

#include "vm_log.h"
#include "vm_option_cv.h"
#include "vm_option_ff.h"
#include "vm_match_cv.h"
#include "vm_match_ff.h"

#include <fstream>

namespace vm_output
{
	void vm_output(codec_type codec){
		if(codec == codec_type::ffmpeg){
			if(vm_option_ff::output_type == vm_option_ff::output_type_enum::framenum)
				for(fnum i = 0; i < vm_option_ff::frame_count_1; ++i)
					vm_log::output(std::format(R"({0}->{1})", i, vm_match_ff::match_frame_list[i]==-1 ? static_cast<int64_t>(-1) : static_cast<int64_t>(vm_match_ff::match_frame_list[i])));

			if(!vm_option_ff::log_path.empty()){
				std::ofstream log_file(vm_option_ff::log_path, std::ios::out);
				if(log_file.is_open()){
					for(fnum i = 0; i < vm_option_ff::frame_count_1; ++i)
						log_file << i << "->" << (vm_match_ff::match_frame_list[i]==-1 ? static_cast<int64_t>(-1) : static_cast<int64_t>(vm_match_ff::match_frame_list[i])) << '\n';
					log_file.close();
				}
				else vm_log::error("unable to open log file");
			}
		}


		else if(codec == codec_type::opencv){
			if(vm_option_cv::output_type == vm_option_cv::output_type_enum::framenum)
				for(fnum i = 0; i < vm_option_cv::frame_count_1; ++i)
					vm_log::output(std::format(R"({0}->{1})", i, vm_match_cv::match_frame_list[i]==-1 ? static_cast<int64_t>(-1) : static_cast<int64_t>(vm_match_cv::match_frame_list[i])));

			if(!vm_option_cv::log_path.empty()){
				std::ofstream log_file(vm_option_cv::log_path, std::ios::out);
				if(log_file.is_open()){
					for(fnum i = 0; i < vm_option_cv::frame_count_1; ++i)
						log_file << i << "->" << (vm_match_cv::match_frame_list[i]==-1 ? static_cast<int64_t>(-1) : static_cast<int64_t>(vm_match_cv::match_frame_list[i])) << '\n';
					log_file.close();
				}
				else vm_log::error("unable to open log file");
			}
		}
	}
}