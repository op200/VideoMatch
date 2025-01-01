#include "vm_option_ff.h"

#include <format>
#include <cstdlib>

#include "vm_version.h"
#include "vm_log.h"

namespace vm_option_ff
{
    std::string input_video_path_1, input_video_path_2, log_path;
    output_type_enum output_type = output_type_enum::framenum;
    uint16_t frame_scale = 1;
    double ssim_threshold = 0.992;
    int16_t frame_forward = 24;
    bool benchmark = false, debug = false;
    std::string hwaccel("");

    int8_t video_stream_index_1 = -1, video_stream_index_2 = -1;
    AVFormatContext *formatContext_1 = nullptr, *formatContext_2 = nullptr;
    AVCodecContext *codecContext_1, *codecContext_2;

    fnum frame_count_1, frame_count_2;
    uint32_t new_width, new_height;

    std::string _get_output_type_string(){
        switch(output_type){
            case output_type_enum::nooutput: return "nooutput";
            case output_type_enum::framenum: return "framenum";
        }
    }
    
    void get_option(std::vector<std::string>& args){
        std::string version_info = std::format("{0}\nVersion: {1}\n{2}\nFFmpeg: {3}", PROGRAM_NAME, VERSION, HOME_LINK, av_version_info());
        for(std::string arg : args){
            if(arg == "-v" || arg == "-version"){
                vm_log::output(version_info);
                std::exit(EXIT_SUCCESS);
            }
            if(arg == "-h" || arg == "-help"){
                vm_log::output(std::format(
R"({0}

Switch API:
    -c / -codec <string>
        ff / ffmpeg: FFmpeg API
        cv / opencv: OpenCV API
        Default: ffmpeg

Program info:
    -h / -help
        Print help

    -v / -version
        Print version

Input options:
    -i1 / -input1 <string>
        Input the path of the first video

    -i2 / -input2 <string>
        Input the path of the second video

Output options:
    -t / -type <string>
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
    -th / -threshold <float 0..1.0>
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

    -hw / -hwaccel <string>
        Select the hardware acceleration

Debug options:
    -debug
        Output debug messages on the command line
        Will not be terminated when certain errors occurs
)",version_info,
_get_output_type_string(),
log_path, ssim_threshold, frame_scale, frame_forward));
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
            if(args[i]=="-benchmark")
                benchmark = true;
            if(args[i]=="-hw" || args[i]=="-hwaccel")
                hwaccel = args[i+1];
            if(args[i]=="-debug")
                debug = true;
        }

        // 视频校验
        if(input_video_path_1.empty())
            vm_log::errore("Need input video 1 (-i1)");
        if(input_video_path_2.empty())
            vm_log::errore("Need input video 2 (-i2)");

        // 输入
        if(avformat_open_input(&formatContext_1, input_video_path_1.c_str(), nullptr, nullptr) != 0)
            vm_log::errore("The video 1 \""+input_video_path_1+"\" can not be opened");
        if(avformat_open_input(&formatContext_2, input_video_path_2.c_str(), nullptr, nullptr) != 0)
            vm_log::errore("The video 2 \""+input_video_path_2+"\" can not be opened");

        if(avformat_find_stream_info(formatContext_1, nullptr) < 0){
            avformat_close_input(&formatContext_1);
            vm_log::errore("Can not find stream information in video 1 \""+input_video_path_1+"\"");
        }
        if(avformat_find_stream_info(formatContext_2, nullptr) < 0){
            avformat_close_input(&formatContext_2);
            vm_log::errore("Can not find stream information in video 2 \""+input_video_path_2+"\"");
        }

        // 搜索第一个视频流
        for(unsigned i = 0; i < formatContext_1->nb_streams; ++i){
            if(formatContext_1->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO){
                video_stream_index_1 = i;
                break;
            }
        }
        if(video_stream_index_1 == -1){
            avformat_close_input(&formatContext_1);
            vm_log::errore("Can not find a video stream in video 1 \""+input_video_path_1+"\"");
        }
        AVStream* videoStream_1 = formatContext_1->streams[video_stream_index_1];

        for(unsigned i = 0; i < formatContext_2->nb_streams; ++i){
            if(formatContext_2->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO){
                video_stream_index_2 = i;
                break;
            }
        }
        if(video_stream_index_2 == -1){
            avformat_close_input(&formatContext_2);
            vm_log::errore("Can not find a video stream in video 2 \""+input_video_path_2+"\"");
        }
        AVStream* videoStream_2 = formatContext_2->streams[video_stream_index_2];

        // 校验宽高
        if(videoStream_1->codecpar->width != videoStream_2->codecpar->width ||
           videoStream_1->codecpar->height != videoStream_2->codecpar->height)
            vm_log::errore(std::format("The two videos have different widths or heights: {0}x{1} {2}x{3}",
                                       videoStream_1->codecpar->width, videoStream_1->codecpar->height,
                                       videoStream_2->codecpar->width, videoStream_2->codecpar->height));

        // 获取视频流的解码器上下文
        AVCodecParameters* codecParams_1 = videoStream_1->codecpar;
        const AVCodec* codec_1 = avcodec_find_decoder(codecParams_1->codec_id);
        if(!codec_1){
            avformat_close_input(&formatContext_1);
            vm_log::errore("Failed to find codec in video 1 \""+input_video_path_1+"\"");
        }
        codecContext_1 = avcodec_alloc_context3(codec_1);
        if(!codecContext_1){
            avformat_close_input(&formatContext_1);
            vm_log::errore("Failed to allocate video codec context in video 1 \""+input_video_path_1+"\"");
        }
        if(avcodec_parameters_to_context(codecContext_1, codecParams_1) < 0){
            avcodec_free_context(&codecContext_1);
            avformat_close_input(&formatContext_1);
            vm_log::errore("Failed to copy codec parameters to decoder context in video 1 \""+input_video_path_1+"\"");
        }

        AVCodecParameters* codecParams_2 = videoStream_2->codecpar;
        const AVCodec* codec_2 = avcodec_find_decoder(codecParams_2->codec_id);
        if(!codec_2){
            avformat_close_input(&formatContext_2);
            vm_log::errore("Failed to find codec in video 2 \""+input_video_path_2+"\"");
        }
        codecContext_2 = avcodec_alloc_context3(codec_2);
        if(!codecContext_2){
            avformat_close_input(&formatContext_2);
            vm_log::errore("Failed to allocate video codec context in video 2 \""+input_video_path_2+"\"");
        }
        if(avcodec_parameters_to_context(codecContext_2, codecParams_2) < 0){
            avcodec_free_context(&codecContext_2);
            avformat_close_input(&formatContext_2);
            vm_log::errore("Failed to copy codec parameters to decoder context in video 2 \""+input_video_path_2+"\"");
        }

        // 设置多线程解码
        if(codec_1->capabilities & AV_CODEC_CAP_FRAME_THREADS || codec_1->capabilities & AV_CODEC_CAP_SLICE_THREADS){
            codecContext_1->thread_type = FF_THREAD_FRAME;
            codecContext_1->thread_count = 0;
        }
        if(codec_2->capabilities & AV_CODEC_CAP_FRAME_THREADS || codec_2->capabilities & AV_CODEC_CAP_SLICE_THREADS){
            codecContext_2->thread_type = FF_THREAD_FRAME;
            codecContext_2->thread_count = 0;
        }

        // 设置硬件加速
        if(hwaccel!=""){
            AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(hwaccel.c_str());
            if(hw_type == AV_HWDEVICE_TYPE_NONE)
                vm_log::error(std::format("Not find the hwaccel type: {0}", hwaccel));

            AVBufferRef *hw_device_ctx = nullptr;
            if((av_hwdevice_ctx_create(&hw_device_ctx, hw_type, NULL, NULL, 0)) != 0)
                vm_log::error("Failed to create hardware device context");


            codecContext_1->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            if(!codecContext_1->hw_device_ctx)
                vm_log::error("Failed to create reference to hardware context in video 1");


            codecContext_2->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            if(!codecContext_2->hw_device_ctx)
                vm_log::error("Failed to create reference to hardware context in video 2");
        }

        // 打开1
        if(avcodec_open2(codecContext_1, codec_1, nullptr) < 0){
            avcodec_free_context(&codecContext_1);
            avformat_close_input(&formatContext_1);
            vm_log::errore("Failed to open codec in video 1 \""+input_video_path_1+"\"");
        }

        // 打开2
        if(avcodec_open2(codecContext_2, codec_2, nullptr) < 0){
            avcodec_free_context(&codecContext_2);
            avformat_close_input(&formatContext_2);
            vm_log::errore("Failed to open codec in video 2 \""+input_video_path_2+"\"");
        }


        // 参数校验
        if(ssim_threshold<0 || ssim_threshold>1)
            vm_log::errore(std::format("-scale {0} out of range", ssim_threshold));

        if(frame_scale==0)
            vm_log::errore(std::format("-scale {0} out of range", frame_scale));

        if(frame_forward<=0 || frame_forward==32767)
            vm_log::errore(std::format("-forward {0} out of range", frame_forward));


        // 新值
        // 猜测帧数
        bool have_frame_count = true;
        AVRational r_frame_rate = av_guess_frame_rate(formatContext_1, videoStream_1, nullptr);
        if(av_cmp_q(r_frame_rate, videoStream_1->avg_frame_rate)){
            vm_log::warning("The video 1 is VFR");
            have_frame_count = false;
        }
        else
            frame_count_1 = static_cast<fnum>(round(formatContext_1->duration / AV_TIME_BASE * av_q2d(videoStream_1->avg_frame_rate)));

        r_frame_rate = av_guess_frame_rate(formatContext_2, videoStream_2, nullptr);
        if(av_cmp_q(r_frame_rate, videoStream_2->avg_frame_rate)){
            vm_log::warning("The video 2 is VFR");
            have_frame_count = false;
        }
        else
            frame_count_2 = static_cast<fnum>(round(formatContext_2->duration / AV_TIME_BASE * av_q2d(videoStream_2->avg_frame_rate)));

        if(!have_frame_count)
            vm_log::warning("VFR video exists, frame rate guesses may not be accurate (Incorrect muxing may cause a program to mistake CFR video for VFR)");

        if(debug)
            vm_log::info(std::format("The two videos frame counts: Metadata: {0} F & {1} F; Guess: {2} F & {3} F", videoStream_1->nb_frames, videoStream_2->nb_frames, frame_count_1, frame_count_2));

        if(videoStream_1->nb_frames && videoStream_2->nb_frames &&
           (frame_count_1!=videoStream_1->nb_frames || frame_count_2!=videoStream_2->nb_frames))
            vm_log::warning(std::format("The two videos have different frame counts between metadata and guess: Metadata: {0} F & {1} F; Guess: {2} F & {3} F", videoStream_1->nb_frames, videoStream_2->nb_frames, frame_count_1, frame_count_2));

        if(frame_count_1 != frame_count_2)
            vm_log::warning(std::format("The two videos have different frame counts: {0} F & {1} F", frame_count_1, frame_count_2));

        if(debug)
            vm_log::info(std::format("The two videos FPS: {0}/{1} FPS & {2}/{3} FPS", videoStream_1->avg_frame_rate.num, videoStream_1->avg_frame_rate.den, videoStream_2->avg_frame_rate.num, videoStream_2->avg_frame_rate.den));

        if(av_cmp_q(videoStream_1->avg_frame_rate, videoStream_2->avg_frame_rate))
            vm_log::warning(std::format("The two videos have different FPS: {0}/{1} FPS & {2}/{3} FPS", videoStream_1->avg_frame_rate.num, videoStream_1->avg_frame_rate.den, videoStream_2->avg_frame_rate.num, videoStream_2->avg_frame_rate.den));

        new_width = static_cast<uint32_t>(videoStream_1->codecpar->width) / frame_scale;
        new_height = static_cast<uint32_t>(videoStream_1->codecpar->height) / frame_scale;


        if(debug)
            vm_log::info(std::format(R"("{0}" -i1 "{1}" -i2 "{2}" -t {3} -log {4} -th {5} -scale {6} -forward {7} {8}-hw {9} {10} -c ff)",
                                     args[0], input_video_path_1, input_video_path_2,
                                     _get_output_type_string(),
                                     log_path, ssim_threshold, frame_scale, frame_forward,
                                     benchmark ? "-benchmark " : "",
                                     hwaccel,
                                     debug ? "-debug" : ""));
    }
}