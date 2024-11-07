#include "vm_log.h"

#include <iostream>
#include <cstdlib>
#include <format>
#include <Windows.h>

#include "vm_version.h"

namespace vm_log
{
	void output(const std::string& info){
		std::cout << info << '\n';
	}

	void change_title(const std::string& info){
		std::string output = std::format("{0} {1} v{2}", info, PROGRAM_NAME, VERSION);
		//std::cout << "\033]0;" << output << "\007";
		SetConsoleTitle(output.c_str());
	}

	void error(const std::string& info){
		std::cerr << "\033[31m[" << PROGRAM_NAME << " ERROR]\033[0m " << info << '\n';
	}

	void errore(const std::string& info){
		error(info);
		std::exit(EXIT_SUCCESS);
	}

	void warning(const std::string& info){
		std::cerr << "\033[33m[" << PROGRAM_NAME << " WARNING]\033[0m " << info << '\n';
	}

	void info(const std::string& info){
		std::cerr << "\033[35m[" << PROGRAM_NAME << " INFO]\033[0m " << info << '\n';
	}
}