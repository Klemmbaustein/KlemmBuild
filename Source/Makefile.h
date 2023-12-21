#pragma once
#include "Build.h"


struct Makefile
{
	std::vector<BuildInfo*> Projects;
	size_t DefaultProject = SIZE_MAX;
	static Makefile ReadMakefile(std::string File, std::vector<std::string> Definitions);
};
