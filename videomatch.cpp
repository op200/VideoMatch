#include "vm_log.h"
#include "vm_option.h"
#include "vm_match.h"

#include <fstream>

int main(int argc, char* argv[]){
	std::vector<std::string> args(argv + 1, argv + argc);
	vm_option::get_option(args);
	vm_match::do_match();

	if(vm_option::output_type == vm_option::output_type_enum::framenum)
		for(uint32_t i = 0; i < vm_option::frame_count_1; ++i)
			vm_log::output(std::format(R"({0}->{1})", i, vm_match::match_frame_list[i]));

	if(!vm_option::log_path.empty()){
		std::ofstream log_file(vm_option::log_path, std::ios::out);
		if(log_file.is_open()){
			for(uint32_t i = 0; i < vm_option::frame_count_1; ++i)
				log_file << i << "->" << vm_match::match_frame_list[i] << '\n';
			log_file.close();
		}
		else vm_log::error("unable to open log file");
	}
}