#include "BuildSystem/VCBuild.h"
#include "Makefile.h"

std::string BuildProject(BuildInfo* Build)
{
	BuildSystem* System = nullptr;

	System = new VCBuild();

	std::vector<std::string> ObjectFiles;

	for (const auto& i : Build->CompiledFiles)
	{
		ObjectFiles.push_back(System->Compile(i, Build));
	}

	std::string OutFile = System->Link(ObjectFiles, Build);
	delete System;
	return OutFile;
}

int main()
{
	auto Makefiles = Makefile::ReadMakefile("makefile.json");
	for (BuildInfo* i : Makefiles)
	{
		BuildProject(i);
	}
}