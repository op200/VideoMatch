#include "vm_option.h"

#include <format>
#include <cstdlib>

#include "vm_version.h"
#include "vm_log.h"

namespace vm_option
{
    std::string input_video_path_1, input_video_path_2, log_path;
    output_type_enum output_type = output_type_enum::framenum;
    uint16_t frame_scale = 1, frame_backward = 24;
    double ssim_threshold = 0.995;

    cv::VideoCapture video_cap_1, video_cap_2;
    uint32_t frame_count_1, frame_count_2, new_width, new_height;

	void get_option(std::vector<std::string>& args){
		for(std::string arg : args){
            if(arg == "-v" || arg == "-version"){
                vm_log::output(std::format("{0}\nVersion: {1}\n{2}", PROGRAM_NAME, VERSION, HOME_LINK));
                std::exit(EXIT_SUCCESS);
            }
                if(arg == "-h" || arg == "-help"){
                    vm_log::output(std::format(R"({0} v{1} help:
Program info:
    -h/-help
        print help

    -v/-version
        print version

Input options:
    -i1/-input1 <string>
        input the path of the first video

    -i2/-input2 <string>
        input the path of the second video

Output options:
    -t/-type <string>
        set the output type
        nooutput: no output
        framenum: output the number of matching frames
        default: "{2}"

    -log <string>
        set the path of log file
        required that -type is not nooutput
        if it is None, no output file
        default: "{3}"

Filter options:
    -th/-threshold <float 0..1.0>
        set the ssim_threshold value
        default: {4}

Accuracy options:
    -scale <int 1..65535>
        scaling images for comparison
        e.g. -scale 2 == 0.5x
        default: {5}

    -backward <int 1..65535>
        maximum additional frames to compare if no matching frames can be found
        default: {6}
)", PROGRAM_NAME, VERSION, (int)output_type, log_path, ssim_threshold, frame_scale, frame_backward));
                    std::exit(EXIT_SUCCESS);
                }
		}

        args.push_back("");
        for(int i = 0; i < args.size(); ++i){
            if(args[i]=="-i1" || args[i]=="-input1")
                input_video_path_1 = args[i+1];
            if(args[i]=="-i2" || args[i]=="-input2")
                input_video_path_2 = args[i+1];
            if(args[i]=="-t" || args[i]=="-type"){
                if(args[i+1]=="nooutput")
                    output_type = output_type_enum::nooutput;
                if(args[i+1]=="framenum")
                    output_type = output_type_enum::framenum;
            }
            if(args[i]=="-log")
                log_path = args[i+1];
            if(args[i]=="-th" || args[i]=="-threshold")
                ssim_threshold = std::stod(args[i+1]);
            if(args[i]=="-scale")
                frame_scale = std::stoi(args[i+1]);
            if(args[i]=="-backward")
                frame_backward = std::stoi(args[i+1]);
        }

        // 校验
        if(input_video_path_1.empty())
            vm_log::errore("no input -i1");
        if(input_video_path_2.empty())
            vm_log::errore("no input -i2");

        video_cap_1.open(input_video_path_1, cv::CAP_FFMPEG);
        if(!video_cap_1.isOpened())
            vm_log::errore("the video \""+input_video_path_1+"\" can not be opened");
        video_cap_2.open(input_video_path_2, cv::CAP_FFMPEG);
        if(!video_cap_2.isOpened())
            vm_log::errore("the video \""+input_video_path_2+"\" can not be opened");

        if(video_cap_1.get(cv::CAP_PROP_FRAME_WIDTH)!=video_cap_2.get(cv::CAP_PROP_FRAME_WIDTH) ||
           video_cap_1.get(cv::CAP_PROP_FRAME_HEIGHT)!=video_cap_2.get(cv::CAP_PROP_FRAME_HEIGHT))
            vm_log::errore("the two videos have different widths or heights");

        if(ssim_threshold<=0 || ssim_threshold>1)
            vm_log::errore(std::format("-scale {0} out of range", ssim_threshold));

        if(frame_scale==0)
            vm_log::errore(std::format("-scale {0} out of range", frame_scale));

        if(frame_backward==0)
            vm_log::errore(std::format("-backward {0} out of range", frame_backward));

        // 新值
        frame_count_1 = (uint32_t)video_cap_1.get(cv::CAP_PROP_FRAME_COUNT);
        frame_count_2 = (uint32_t)video_cap_2.get(cv::CAP_PROP_FRAME_COUNT);
        new_width = (uint32_t)video_cap_1.get(cv::CAP_PROP_FRAME_WIDTH) / frame_scale;
        new_height = (uint32_t)video_cap_1.get(cv::CAP_PROP_FRAME_HEIGHT) / frame_scale;
	}
}