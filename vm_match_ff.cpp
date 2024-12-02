#include "vm_match_ff.h"

extern "C"{
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/buffer.h>
#include <libavutil/imgutils.h>
}
#include <format>
#include <map>
#include <algorithm>
#include <thread>
//#include <chrono>

#include "vm_option_ff.h"
#include "vm_log.h"


namespace vm_match_ff
{
	class AV_map: public std::map<fnum, AVFrame*>{
	public:
		using std::map<fnum, AVFrame*>::map;

		void clear(){
			for(std::pair<const fnum, AVFrame*>& pair : *this)
				av_frame_free(&pair.second);
			std::map<fnum, AVFrame*>::clear();
		}

		void erase(std::map<fnum, AVFrame*>::iterator it){
			av_frame_free(&it->second);
			std::map<fnum, AVFrame*>::erase(it);
		}

		void erase(fnum key) {
			auto it = this->find(key);
			if(it != this->end())
				erase(it);
		}
	};

	fnum* match_frame_list;
	AV_map frame_buffer_map;
	fnum frame_buffer_back, buffer_read_pos;
	fnum video_frame_num_1;
	AVFilterGraph* ssim_graph = avfilter_graph_alloc();
	bool can_not_flush_buffer;

	// 自动转换pix_fmt并缩放
	AVFrame* _auto_pix_fmt_process(AVFrame*& frame){

		AVFrame* new_frame = av_frame_alloc();

		new_frame->width = vm_option_ff::new_width;
		new_frame->height = vm_option_ff::new_height;
		new_frame->format = AV_PIX_FMT_GRAY8;

		// 为输出帧分配缓冲区
		if(av_image_alloc(new_frame->data, new_frame->linesize, vm_option_ff::new_width, vm_option_ff::new_height, AVPixelFormat::AV_PIX_FMT_GRAY8, 32) < 0){
			av_frame_free(&new_frame);
			vm_log::errore("vm_match_ff::_auto_pix_fmt_process: av_image_alloc: error");
		}

		SwsContext* sws_ctx = sws_getContext(
			frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
			vm_option_ff::new_width, vm_option_ff::new_height, AVPixelFormat::AV_PIX_FMT_GRAY8,
			SWS_POINT, NULL, NULL, NULL
		);
		if(!sws_ctx){
			av_frame_free(&new_frame);
			vm_log::errore("vm_match_ff::_auto_pix_fmt_process: sws_getContext: error");
		}

		// 执行转换
		sws_scale(sws_ctx,
				  frame->data, frame->linesize,
				  0, frame->height,
				  new_frame->data, new_frame->linesize);

		sws_freeContext(sws_ctx);
		return new_frame;
	}
	
	// 读取 video 2 的下一帧
	int8_t _read_frame_2(fnum frame_num){
		// 读取
		frame_buffer_map[frame_num] = av_frame_alloc();
		bool isread = false;
		AVPacket packet;
		while(av_read_frame(vm_option_ff::formatContext_2, &packet) >= 0){
			if(packet.stream_index == vm_option_ff::video_stream_index_2)
				if(avcodec_send_packet(vm_option_ff::codecContext_2, &packet) == 0)
					while(avcodec_receive_frame(vm_option_ff::codecContext_2, frame_buffer_map[frame_num]) == 0){
						isread = true;
						break;
					}
			av_packet_unref(&packet);
			if(isread)
				break;
		}

		// 包读完了读缓存，防止缓存剩帧没读
		if(!isread)
			if(avcodec_send_packet(vm_option_ff::codecContext_2, nullptr) == 0){
				// 强制写入所有帧到buffer
				while(avcodec_receive_frame(vm_option_ff::codecContext_2, frame_buffer_map[frame_num]) >= 0){
					isread = true;
					if(++frame_num < vm_option_ff::frame_count_2)
						frame_buffer_map[frame_num] = av_frame_alloc();
					else{
						can_not_flush_buffer = true;
						return -1;
					}
				}
			}

		if(!isread){
			frame_buffer_map.erase(frame_num);
			vm_log::error("vm_match_ff::_read_frame_2: Failed to read frame in video 2");
			return 1;
		}

		return 0;
	}

	void _flush_buffer(){
		if(can_not_flush_buffer)
			return;
		// 读取新一段buffer
		if(video_frame_num_1+vm_option_ff::frame_forward >= buffer_read_pos){
			for(fnum i = buffer_read_pos;
				i < buffer_read_pos + vm_option_ff::frame_forward && i < vm_option_ff::frame_count_2;
				++i){
				switch(_read_frame_2(i))
				{
					case 1:
						vm_log::error(std::format("vm_match_ff::_flush_buffer: Get frame_2 error in frame {0}", i));
						break;
					case -1:
						buffer_read_pos = vm_option_ff::frame_count_2;
						return;
				}
			}

			buffer_read_pos += vm_option_ff::frame_forward;
		}

		// 移除超出的旧帧
		for(auto it = frame_buffer_map.begin(); it != frame_buffer_map.end(); ++it){
			if(it->first < std::min(vm_option_ff::frame_count_2, video_frame_num_1-static_cast<fnum>(vm_option_ff::frame_forward)))
				frame_buffer_map.erase(it);
			else break;
		}
	}

	// 初始化滤镜图并配置SSIM滤镜
	void init_ssim_filter_graph(){
		if(!ssim_graph)
			vm_log::errore("Failed to create filter graph");

		// 创建buffer源（主画面）
		AVFilterContext* buffersrc_ctx_main = nullptr;
		char args_main[512];
		snprintf(args_main, sizeof(args_main), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
				 vm_option_ff::new_width, vm_option_ff::new_height, AV_PIX_FMT_GRAY8, 1, 24);
		const AVFilter* buffersrc = avfilter_get_by_name("buffer");
		if(avfilter_graph_create_filter(&buffersrc_ctx_main, buffersrc, "src_main", args_main, nullptr, ssim_graph) < 0)
			vm_log::errore("Failed to create buffer source for main");

		// 创建buffer源（参考画面）
		AVFilterContext* buffersrc_ctx_ref = nullptr;
		char args_ref[512];
		snprintf(args_ref, sizeof(args_ref), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
				 vm_option_ff::new_width, vm_option_ff::new_height, AV_PIX_FMT_GRAY8, 1, 24);
		if(avfilter_graph_create_filter(&buffersrc_ctx_ref, buffersrc, "src_ref", args_ref, nullptr, ssim_graph) < 0)
			vm_log::errore("Failed to create buffer source for reference");

		// 创建ssim滤镜
		const AVFilter* ssim = avfilter_get_by_name("ssim");
		AVFilterContext* ssim_ctx = nullptr;
		if(avfilter_graph_create_filter(&ssim_ctx, ssim, "ssim", nullptr, nullptr, ssim_graph) < 0)
			vm_log::errore("Failed to create ssim filter");

		// 创建buffer汇
		const AVFilter* buffersink = avfilter_get_by_name("buffersink");
		AVFilterContext* buffersink_ctx = nullptr;
		if(avfilter_graph_create_filter(&buffersink_ctx, buffersink, "sink", nullptr, nullptr, ssim_graph) < 0)
			vm_log::error("Failed to create buffer sink");

		// 链接滤镜
		if(avfilter_link(buffersrc_ctx_main, 0, ssim_ctx, 0) < 0 ||
		   avfilter_link(buffersrc_ctx_ref, 0, ssim_ctx, 1) < 0 ||
		   avfilter_link(ssim_ctx, 0, buffersink_ctx, 0) < 0)
			vm_log::errore("Failed to link filters");

		// 配置滤镜图
		if(avfilter_graph_config(ssim_graph, nullptr) < 0)
			vm_log::errore("Failed to configure filter graph");
	}

	double compare_ssim(AVFrame* frame_1, AVFrame* frame_2){
		if(!frame_1){
			vm_log::error("vm_match_ff::compare_ssim: frame_1 is nullptr");
			return -1;
		}
		if(!frame_2){
			vm_log::error("vm_match_ff::compare_ssim: frame_2 is nullptr");
			return -1;
		}

		// 获取滤镜图的输入和输出上下文
		AVFilterContext* buffersrc_ctx_main = avfilter_graph_get_filter(ssim_graph, "src_main");
		AVFilterContext* buffersrc_ctx_ref = avfilter_graph_get_filter(ssim_graph, "src_ref");
		AVFilterContext* buffersink_ctx = avfilter_graph_get_filter(ssim_graph, "sink");

		// 将帧发送到滤镜图的输入端
		av_buffersrc_add_frame_flags(buffersrc_ctx_main, frame_1, AV_BUFFERSRC_FLAG_PUSH);
		av_buffersrc_add_frame_flags(buffersrc_ctx_ref, frame_2, AV_BUFFERSRC_FLAG_PUSH);

		// 从滤镜图的输出端获取 SSIM 值
		AVFrame* frame_out = av_frame_alloc();

		if(av_buffersink_get_frame(buffersink_ctx, frame_out) < 0){
			vm_log::error("vm_match_ff::compare_ssim: av_buffersink_get_frame: error");
			av_frame_free(&frame_out);
			return -1;
		}

		double ssim_value = 0;
		if(frame_out->metadata) {
			AVDictionaryEntry *tag = av_dict_get(frame_out->metadata, "lavfi.ssim.Y", NULL, 0);
			if(tag)
				ssim_value = atof(tag->value);
			else vm_log::error("vm_match_ff::compare_ssim: av_dict_get(frame_out->metadata ...) is NULL");
		}
		else vm_log::error("vm_match_ff::compare_ssim: frame_out->metadata is NULL");

		av_frame_free(&frame_out);

		av_freep(&frame_2->data[0]);
		av_frame_free(&frame_2);

		if(vm_option_ff::debug)
			vm_log::info(std::format("{0} SSIM: {1}", video_frame_num_1, ssim_value));

		return ssim_value;
	}

	bool frame_cmp(AVFrame*& frame_1, AVFrame*& frame_2){
		bool is_pass = compare_ssim(frame_1, frame_2) >= vm_option_ff::ssim_threshold;
		if(is_pass){
			av_freep(&frame_1->data[0]);
			av_frame_free(&frame_1);
		}
		return is_pass;
	}

	void do_match(){
		match_frame_list = new fnum[vm_option_ff::frame_count_1];
		buffer_read_pos = video_frame_num_1 = 0;
		can_not_flush_buffer = false;
		AVFrame *frame_1 = av_frame_alloc(), *frame_2 = av_frame_alloc();
		AVFrame *frame_1_resize, *frame_2_resize;
		AVPacket packet_1;

		// 配置SSIM滤镜
		init_ssim_filter_graph();

		// 打印进度子线程
		std::thread([&](){
			fnum denominator = vm_option_ff::frame_count_1-1;
			while(true){
				vm_log::change_title(std::format(R"({0} / {1} {2:.1f}%)",
												 video_frame_num_1, denominator, 100.0*video_frame_num_1/denominator));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}).detach();

		// 读取并对比
		while(av_read_frame(vm_option_ff::formatContext_1, &packet_1) >= 0){
			if(packet_1.stream_index == vm_option_ff::video_stream_index_1){
				if(avcodec_send_packet(vm_option_ff::codecContext_1, &packet_1) == 0){
					while(avcodec_receive_frame(vm_option_ff::codecContext_1, frame_1) == 0){
						// 执行

						//vm_log::info(std::format("{0}",video_frame_num_1));

						frame_1_resize = _auto_pix_fmt_process(frame_1);
						_flush_buffer();
						bool is_not_finded = true;
						for(auto p : frame_buffer_map){
							frame_2 = p.second;
							frame_2_resize = _auto_pix_fmt_process(frame_2);

							if(frame_cmp(frame_1_resize, frame_2_resize)){
								frame_buffer_map.erase(p.first);
								match_frame_list[video_frame_num_1] = p.first;
								is_not_finded = false;
								break;
							}
						}
						if(is_not_finded)
							match_frame_list[video_frame_num_1] = -1;

						++video_frame_num_1;
					}
				}
			}
			av_packet_unref(&packet_1);
		}

		// 发空包，以防缓冲区中仍有帧
		if(avcodec_send_packet(vm_option_ff::codecContext_1, nullptr) == 0) {
			while(avcodec_receive_frame(vm_option_ff::codecContext_1, frame_1) >= 0) {
				// 执行
				//vm_log::info(std::format("{0}", video_frame_num_1));
				frame_1_resize = _auto_pix_fmt_process(frame_1);

				bool is_not_finded = true;
				for(auto p : frame_buffer_map){

					frame_2 = p.second;
					frame_2_resize = _auto_pix_fmt_process(frame_2);

					if(frame_cmp(frame_1_resize, frame_2_resize)){
						frame_buffer_map.erase(p.first);
						match_frame_list[video_frame_num_1] = p.first;
						is_not_finded = false;
						break;
					}
				}
				if(is_not_finded)
					match_frame_list[video_frame_num_1] = -1;

				++video_frame_num_1;
			}
		}

		// 清理
		av_frame_free(&frame_1);
		avformat_close_input(&vm_option_ff::formatContext_1);
		avformat_close_input(&vm_option_ff::formatContext_2);
		avcodec_free_context(&vm_option_ff::codecContext_1);
		avcodec_free_context(&vm_option_ff::codecContext_2);
	}
}
