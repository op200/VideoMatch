#include "vm_log.h"

#include <iostream>
#include <cstdlib>

namespace vm_log
{
	void output(const std::string& info){
		std::cout << info << '\n';
	}

	void change_title(const std::string& info){
		//std::cout<<"\033]0;"+info+"\007"<<std::flush;
		std::cout << "\033]0;" << info << "\007";
	}

	void error(const std::string& info){
		std::cerr << "\033[31m[ERROR]\033[0m" << info << '\n';
	}

	void errore(const std::string& info){
		error(info);
		std::exit(EXIT_SUCCESS);
	}
}