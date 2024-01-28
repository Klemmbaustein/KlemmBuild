#pragma once
#include <mutex>

namespace KlemmBuild
{
	constexpr const char* Version = "0.1.1";

	extern uint8_t BuildThreads;
	void Exit();
	std::string BuildMakefile(std::string Makefile);
	extern std::mutex PrintMutex;
}

namespace Log
{
	void Info();
	void Error();
}