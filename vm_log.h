#pragma once

#include <string>

namespace vm_log
{
	void output(const std::string& info);

	void change_title(const std::string& info);

	void error(const std::string& info);

	void errore(const std::string& info);
}