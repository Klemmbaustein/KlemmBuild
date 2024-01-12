#pragma once

namespace KlemmBuild
{
	constexpr const char* Version = "0.1.0";

	extern uint8_t BuildThreads;
	void Exit();
	std::string BuildMakefile(std::string Makefile);
}

namespace Log
{
	void Info();
	void Error();
}