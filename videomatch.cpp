#include "vm_log.h"
#include "vm_option.h"
#include "vm_match.h"
#include "vm_type.h"

#include <fstream>

#include <chrono>

int main(int argc, char* argv[]){
	std::vector<std::string> args(argv + 1, argv + argc);
	vm_option::get_option(args);
	
	std::chrono::steady_clock::time_point start_time;
	if(vm_option::benchmark)
		start_time = std::chrono::high_resolution_clock::now();

	vm_match::do_match();

	if(vm_option::output_type == vm_option::output_type_enum::framenum)
		for(fnum i = 0; i < vm_option::frame_count_1; ++i)
			vm_log::output(std::format(R"({0}->{1})", i, vm_match::match_frame_list[i]));

	if(!vm_option::log_path.empty()){
		std::ofstream log_file(vm_option::log_path, std::ios::out);
		if(log_file.is_open()){
			for(fnum i = 0; i < vm_option::frame_count_1; ++i)
				log_file << i << "->" << vm_match::match_frame_list[i] << '\n';
			log_file.close();
		}
		else vm_log::error("unable to open log file");
	}

	if(vm_option::benchmark)
		vm_log::info(std::format("benchmark {0}", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_time)));
}