#include "BuildSystem/VCBuild.h"
#include "Makefile.h"
#include <iostream>
#include <filesystem>
#include "FileUtility.h"

std::string BuildMakefile(std::string Makefile);

bool BuildProject(BuildInfo* Build, Makefile& Make)
{
	BuildSystem* System = nullptr;
	{
		System = new VCBuild();
	}

	for (const auto& dep : Build->Dependencies)
	{
		for (BuildInfo* p : Make.Projects)
		{
			if (p->TargetName == dep)
			{
				std::cout << "Building dependency for project '" << Build->TargetName << "': '" << p->TargetName << "'" << std::endl;
				BuildProject(p, Make);
			}
		}
	}

	if (Build->IsMakefile)
	{
		BuildMakefile(Build->MakefilePath);
		delete System;
		return "";
	}

	std::vector<std::string> ObjectFiles;

	for (const auto& i : Build->CompiledFiles)
	{
		ObjectFiles.push_back(System->Compile(i, Build));
	}

	bool OutFile = System->Link(ObjectFiles, Build);
	delete System;
	return OutFile;
}

std::string BuildMakefile(std::string Makefile)
{
	auto MakefilePath = std::filesystem::absolute(Makefile);
	auto PrevPath = std::filesystem::absolute(std::filesystem::current_path());
	std::filesystem::current_path(FileUtility::RemoveFilename(Makefile));

	auto LoadedMakefile = Makefile::ReadMakefile(MakefilePath.string());
	if (LoadedMakefile.DefaultProject == SIZE_MAX)
	{
		for (BuildInfo* i : LoadedMakefile.Projects)
		{
			if (!BuildProject(i, LoadedMakefile))
			{
				std::cout << "Build failed - exiting" << std::endl;
				exit(1);
			}
		}
	}
	else
	{
		if (!BuildProject(LoadedMakefile.Projects[LoadedMakefile.DefaultProject], LoadedMakefile))
		{
			std::cout << "Build failed - exiting" << std::endl;
			exit(1);
		}
	}

	std::filesystem::current_path(PrevPath);
	return "";
}

int main()
{
	BuildMakefile("makefile.json");
}