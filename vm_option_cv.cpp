#include "vm_option_cv.h"

#include <format>
#include <cstdlib>

#include "vm_version.h"
#include "vm_log.h"

namespace vm_option_cv
{
    std::string input_video_path_1, input_video_path_2, log_path;
    output_type_enum output_type = output_type_enum::framenum;
    uint16_t frame_scale = 1;
    double ssim_threshold = 0.995;
    int16_t frame_buffer_size = -1, frame_forward = 24;
    bool benchmark = false, debug = false;
    cv::VideoCaptureAPIs decoder = cv::CAP_FFMPEG;
    cv::VideoAccelerationType hwaccel = cv::VIDEO_ACCELERATION_NONE;

    cv::VideoCapture video_cap_1, video_cap_2;
    fnum frame_count_1, frame_count_2;
    uint32_t new_width, new_height;

    std::string _get_output_type_string(){
        switch(output_type){
            case output_type_enum::nooutput: return "nooutput";
            case output_type_enum::framenum: return "framenum";
        }
    }

	void get_option(std::vector<std::string>& args){
        std::string version_info = std::format("{0}\nVersion: {1}\n{2}\nOpenCV: {3}", PROGRAM_NAME, VERSION, HOME_LINK, CV_VERSION);
        for(std::string arg : args){
            if(arg=="-v" || arg=="-version"){
                vm_log::output(version_info);
                std::exit(EXIT_SUCCESS);
            }
                if(arg=="-h" || arg=="-help"){
                    vm_log::output(std::format(
R"({0}

Program info:
    -h/-help
        Print help

    -v/-version
        Print version

Input options:
    -i1/-input1 <string>
        Input the path of the first video

    -i2/-input2 <string>
        Input the path of the second video

Output options:
    -t/-type <string>
        Set the output type
        Nooutput: no output
        Framenum: output the number of matching frames
        Default: "{1}"

    -log <string>
        Set the path of log file
        Required that -type is not nooutput
        If it is empty, no output file
        Default: "{2}"

Filter options:
    -th/-threshold <float 0..1.0>
        Set the ssim_threshold value
        Default: {3}

Accuracy options:
    -scale <int 1..65535>
        Scaling images for comparison
        e.g. -scale 2 == 0.5x
        Default: {4}

    -forward <int 1..32766>
        Maximum additional frames to compare if no matching frames can be found
        Default: {5}

Performance options:
    -benchmark
        Output running time (ms)

    -buffer <int -1..32767>
        Manually set frame_buffer size
        Do not set the -buffer too large, or your memory will bomb
        case -1: auto: -buffer = -forward + 1
        case 0: close frame_buffer
        Default: {6}

    -de/-decoder <int>
        Select decoder (OpenCV API)
        May require additional dll
               0: CAP_ANY
             200: CAP_VFW
             200: CAP_V4L
             200: CAP_V4L2
             300: CAP_FIREWIRE
             300: CAP_FIREWARE
             300: CAP_IEEE1394
             300: CAP_DC1394
             300: CAP_CMU1394
             500: CAP_QT
             600: CAP_UNICAP
             700: CAP_DSHOW
             800: CAP_PVAPI
             900: CAP_OPENNI
             910: CAP_OPENNI_ASUS
            1000: CAP_ANDROID
            1100: CAP_XIAPI
            1200: CAP_AVFOUNDATION
            1300: CAP_GIGANETIX
            1400: CAP_MSMF
            1410: CAP_WINRT
            1500: CAP_INTELPERC
            1500: CAP_REALSENSE
            1600: CAP_OPENNI2
            1610: CAP_OPENNI2_ASUS
            1620: CAP_OPENNI2_ASTRA
            1700: CAP_GPHOTO2
            1800: CAP_GSTREAMER
            1900: CAP_FFMPEG
            2000: CAP_IMAGES
            2100: CAP_ARAVIS
            2200: CAP_OPENCV_MJPEG
            2300: CAP_INTEL_MFX
            2400: CAP_XINE
            2500: CAP_UEYE
            2600: CAP_OBSENSOR
        Default: 1900 (CAP_FFMPEG)

    -hw/-hwaccel <int>
        Select the hardware acceleration (OpenCV API: CAP_PROP_HW_ACCELERATION)
            0: VIDEO_ACCELERATION_NONE
            1: VIDEO_ACCELERATION_ANY
            2: VIDEO_ACCELERATION_D3D11
            3: VIDEO_ACCELERATION_VAAPI
            4: VIDEO_ACCELERATION_MFX
        Default: 0 (VIDEO_ACCELERATION_NONE)

Debug options:
    -debug
        Output debug messages on the command line
        Will not be terminated when certain errors occurs
)", version_info,
_get_output_type_string(),
log_path, ssim_threshold, frame_scale, frame_forward, frame_buffer_size));
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
            if(args[i]=="-forward")
                frame_forward = std::stoi(args[i+1]);
            if(args[i]=="-buffer")
                frame_buffer_size = std::stoi(args[i+1]);
            if(args[i]=="-benchmark")
                benchmark = true;
            if(args[i]=="-de" || args[i]=="-decoder")
                decoder = static_cast<cv::VideoCaptureAPIs>(std::stoi(args[i+1]));
            if(args[i]=="-hw" || args[i]=="-hwaccel")
                hwaccel = static_cast<cv::VideoAccelerationType>(std::stoi(args[i+1]));
            if(args[i]=="-debug")
                debug = true;
        }

        // 视频校验
        if(input_video_path_1.empty())
            vm_log::errore("Need input video 1 (-i1)");
        if(input_video_path_2.empty())
            vm_log::errore("Need input video 2 (-i2)");

        video_cap_1.open(input_video_path_1, decoder);
        if(!video_cap_1.isOpened())
            vm_log::errore("The video 1 \""+input_video_path_1+"\" can not be opened");
        video_cap_2.open(input_video_path_2, decoder);
        if(!video_cap_2.isOpened())
            vm_log::errore("The video 2 \""+input_video_path_2+"\" can not be opened");

        video_cap_1.set(cv::CAP_PROP_HW_ACCELERATION, hwaccel);
        video_cap_2.set(cv::CAP_PROP_HW_ACCELERATION, hwaccel);

        if(video_cap_1.get(cv::CAP_PROP_FRAME_WIDTH) != video_cap_2.get(cv::CAP_PROP_FRAME_WIDTH) ||
           video_cap_1.get(cv::CAP_PROP_FRAME_HEIGHT) != video_cap_2.get(cv::CAP_PROP_FRAME_HEIGHT))
            vm_log::errore(std::format("The two videos have different widths or heights: {0}x{1} {2}x{3}",
                                       video_cap_1.get(cv::CAP_PROP_FRAME_WIDTH), video_cap_1.get(cv::CAP_PROP_FRAME_HEIGHT),
                                       video_cap_2.get(cv::CAP_PROP_FRAME_WIDTH), video_cap_2.get(cv::CAP_PROP_FRAME_HEIGHT)));

        // 参数校验
        if(ssim_threshold<0 || ssim_threshold>1)
            vm_log::errore(std::format("-scale {0} out of range", ssim_threshold));

        if(frame_scale == 0)
            vm_log::errore(std::format("-scale {0} out of range", frame_scale));

        if(frame_forward == 0 || frame_forward == 32767)
            vm_log::errore(std::format("-forward {0} out of range", frame_forward));

        if(frame_buffer_size < -1)
            vm_log::errore(std::format("-buffer {0} out of range", frame_buffer_size));
        else if(frame_buffer_size == -1)
            frame_buffer_size = frame_forward+1;

        // 新值
        frame_count_1 = static_cast<fnum>(video_cap_1.get(cv::CAP_PROP_FRAME_COUNT));
        frame_count_2 = static_cast<fnum>(video_cap_2.get(cv::CAP_PROP_FRAME_COUNT));
        if(frame_count_1 != frame_count_2)
            vm_log::warning(std::format("The two videos have different frame counts: {0} F {1} F", frame_count_1, frame_count_2));
        video_cap_1.set(cv::CAP_PROP_POS_FRAMES, frame_count_1-1);
        video_cap_2.set(cv::CAP_PROP_POS_FRAMES, frame_count_2-1);
        if(!video_cap_1.grab())
            vm_log::errore(std::format("Video 1 container error, can not read frame {0}", frame_count_1-1));
        if(!video_cap_2.grab())
            vm_log::errore(std::format("Video 2 container error, can not read frame {0}", frame_count_2-1));

        double fps1 = video_cap_1.get(cv::CAP_PROP_FPS), fps2 = video_cap_2.get(cv::CAP_PROP_FPS);
        if(fps1 != fps2)
            vm_log::warning(std::format("The two videos have different FPS: {0} FPS & {1} FPS", fps1, fps2));

        new_width = static_cast<uint32_t>(video_cap_1.get(cv::CAP_PROP_FRAME_WIDTH)) / frame_scale;
        new_height = static_cast<uint32_t>(video_cap_1.get(cv::CAP_PROP_FRAME_HEIGHT)) / frame_scale;



        if(debug)
            vm_log::info(std::format(R"("{0}" -i1 "{1}" -i2 "{2}" -t {3} -log {4} -th {5} -scale {6} -forward {7} {8}-buffer {9} -de {10} -hw {11} {12} -c cv)",
                                     args[0], input_video_path_1, input_video_path_2,
                                     _get_output_type_string(),
                                     log_path, ssim_threshold, frame_scale, frame_forward,
                                     benchmark ? "-benchmark " : "",
                                     frame_buffer_size,
                                     static_cast<uint32_t>(decoder), static_cast<uint32_t>(hwaccel),
                                     debug ? "-debug" : ""));
	}
}