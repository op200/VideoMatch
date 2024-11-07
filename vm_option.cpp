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
    int16_t frame_buffer_size = -1;
    bool benchmark;

    cv::VideoCapture video_cap_1, video_cap_2;
    uint32_t frame_count_1, frame_count_2, new_width, new_height;

	void get_option(std::vector<std::string>& args){
		for(std::string arg : args){
            if(arg == "-v" || arg == "-version"){
                vm_log::output(std::format("{0}\nVersion: {1}\n{2}", PROGRAM_NAME, VERSION, HOME_LINK));
                std::exit(EXIT_SUCCESS);
            }
                if(arg == "-h" || arg == "-help"){
                    vm_log::output(std::format(
R"({0} v{1} help:
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
        if it is empty, no output file
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

Performance options:
    -benchmark
        output running time (ms)

    -buffer <int 1..32767>
        manually set frame_buffer size
        do not set the -buffer too large, or your memory will bomb
        case -1: auto: -buffer = -backward>32766 ? 32767 : -backward+1
        case 0: close frame_buffer
        default: {7}
)", PROGRAM_NAME, VERSION,
std::invoke(
    []()->std::string{switch(output_type){
        case vm_option::output_type_enum::nooutput:return "nooutput";
        case vm_option::output_type_enum::framenum: return "framenum";
                    }}),
log_path, ssim_threshold, frame_scale, frame_backward, frame_buffer_size));
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
            if(args[i]=="-buffer")
                frame_buffer_size = std::stoi(args[i+1]);
            if(args[i]=="-benchmark")
                benchmark = true;
        }

        // 视频校验
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

        // 参数校验
        if(ssim_threshold<=0 || ssim_threshold>1)
            vm_log::errore(std::format("-scale {0} out of range", ssim_threshold));

        if(frame_scale==0)
            vm_log::errore(std::format("-scale {0} out of range", frame_scale));

        if(frame_backward==0)
            vm_log::errore(std::format("-backward {0} out of range", frame_backward));

        if(frame_buffer_size<-1)
            vm_log::errore(std::format("-buffer {0} out of range", frame_buffer_size));

        // 新值
        frame_count_1 = (fnum)video_cap_1.get(cv::CAP_PROP_FRAME_COUNT);
        frame_count_2 = (fnum)video_cap_2.get(cv::CAP_PROP_FRAME_COUNT);
        if(frame_count_1 != frame_count_2)
            vm_log::warning(std::format("the two videos have different frames: {0}f {1}f", frame_count_1, frame_count_2));
        video_cap_1.set(cv::CAP_PROP_POS_FRAMES, frame_count_1-1);
        video_cap_2.set(cv::CAP_PROP_POS_FRAMES, frame_count_2-1);
        if(!video_cap_1.grab())
            vm_log::errore(std::format("-input1 video packaging error, can not read frame {0}", frame_count_1-1));
        if(!video_cap_2.grab())
            vm_log::errore(std::format("-input2 video packaging error, can not read frame {0}", frame_count_2-1));

        double fps1 = video_cap_1.get(cv::CAP_PROP_FPS), fps2 = video_cap_2.get(cv::CAP_PROP_FPS);
        if(fps1 != fps2)
            vm_log::warning(std::format("the two videos have different fps: {0}fps {1}fps", fps1, fps2));

        new_width = (fnum)video_cap_1.get(cv::CAP_PROP_FRAME_WIDTH) / frame_scale;
        new_height = (fnum)video_cap_1.get(cv::CAP_PROP_FRAME_HEIGHT) / frame_scale;
        frame_buffer_size = frame_backward>32766 ? 32767 : frame_backward+1;
	}
}