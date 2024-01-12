#pragma once
#include "Targets/Target.h"


struct Makefile
{
	std::vector<Target*> Targets;
	size_t DefaultTarget = SIZE_MAX;
	static Makefile ReadMakefile(std::string File, std::vector<std::string> Definitions);
};
