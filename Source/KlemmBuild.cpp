#include "BuildSystem/VCBuild.h"
#include "BuildSystem/GCC-Linux.h"
#include "Makefile.h"
#include <iostream>
#include <filesystem>
#include "FileUtility.h"

namespace KlemmBuild
{
	uint8_t BuildThreads = 4;
}

static std::vector<std::string> GlobalDefines;

std::string BuildMakefile(std::string Makefile);

bool BuildProject(BuildInfo* Build, Makefile& Make)
{
	BuildSystem* System = nullptr;
	{
#if _WIN32
		System = new VCBuild();
#else
		System = new GCC_Linux();
#endif
	}

	for (const auto& dep : Build->Dependencies)
	{
		bool Found = false;
		for (BuildInfo* p : Make.Projects)
		{
			if (p->TargetName == dep)
			{
				std::cout << "Building dependency for project '" << Build->TargetName << "': '" << p->TargetName << "'" << std::endl;
				BuildProject(p, Make);
				Found = true;
				break;
			}
		}
		if (!Found)
		{
			std::cout << "Could not find project '" << dep << "' referenced in '" << Build->TargetName << "'" << std::endl;
			std::cout << "Build failed - exiting" << std::endl;
			exit(1);
		}
	}

	if (!Build->PreBuildCommand.empty())
	{
		System->ShellExecute(Build->PreBuildCommand);
	}
	if (!Build->BuildCommand.empty())
	{
		System->ShellExecute(Build->BuildCommand);
		delete System;
		return "";
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
	std::cout << " -- Building makefile " << Makefile << " -- " << std::endl;
	auto MakefilePath = std::filesystem::absolute(Makefile);
	auto PrevPath = std::filesystem::absolute(std::filesystem::current_path());
	std::filesystem::current_path(FileUtility::RemoveFilename(Makefile));

	auto LoadedMakefile = Makefile::ReadMakefile(MakefilePath.string(), GlobalDefines);
	if (LoadedMakefile.DefaultProject == SIZE_MAX)
	{
		std::cout << "No default project specified - Building all" << std::endl;
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
		std::cout << "Starting with the default project - '" 
			<< LoadedMakefile.Projects[LoadedMakefile.DefaultProject]->TargetName
			<< "'" 
			<< std::endl;

		if (!BuildProject(LoadedMakefile.Projects[LoadedMakefile.DefaultProject], LoadedMakefile))
		{
			std::cout << "Build failed - exiting" << std::endl;
			exit(1);
		}
	}

	std::cout << " -- Finished building makefile " << Makefile << " -- " << std::endl;
	std::filesystem::current_path(PrevPath);
	return "";
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		GlobalDefines.push_back(argv[i]);
	}
	BuildMakefile("makefile.json");
}