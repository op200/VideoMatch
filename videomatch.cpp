#include "vm_log.h"
#include "vm_option_cv.h"
#include "vm_option_ff.h"
#include "vm_match_cv.h"
#include "vm_match_ff.h"
#include "vm_type.h"
#include "vm_output.h"

#include <functional>
#include <chrono>

int main(int argc, char* argv[]){
	// get options
	std::vector<std::string> args(argv, argv + argc);
	codec_type codec = codec_type::ffmpeg;
	for(int i = 0; i < args.size(); ++i){
		if(args[i]=="-c" || args[i]=="-codec"){
			if(args[i+1]=="ff" || args[i+1]=="ffmpeg"){
				codec = codec_type::ffmpeg;
				break;
			}
			if(args[i+1]=="cv" || args[i+1]=="opencv"){
				codec = codec_type::opencv;
				break;
			}
		}
	}
	std::function<void(std::vector<std::string>&)> get_option;
	std::function<void()> do_match;
	bool* benchmark = nullptr;
	switch(codec){
		case codec_type::ffmpeg:
			get_option = vm_option_ff::get_option;
			benchmark = &vm_option_ff::benchmark;
			do_match = vm_match_ff::do_match;
			break;
		case codec_type::opencv:
			get_option = vm_option_cv::get_option;
			benchmark = &vm_option_cv::benchmark;
			do_match = vm_match_cv::do_match;
			break;
	}
	get_option(args);
	

	// benchmark start
	std::chrono::steady_clock::time_point start_time;
	if(*benchmark)
		start_time = std::chrono::high_resolution_clock::now();

	// match
	do_match();

	// output
	vm_output::vm_output(codec);

	// benchmark end
	if(*benchmark)
		vm_log::info(std::format("benchmark {0}", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_time)));
}