#pragma once
#include "Build.h"

namespace Makefile
{
	std::vector<BuildInfo*> ReadMakefile(std::string File);
}