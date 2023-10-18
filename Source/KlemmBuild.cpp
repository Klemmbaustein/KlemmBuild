#include "BuildSystem/VCBuild.h"
#include "Makefile.h"
#include <iostream>
#include <filesystem>
#include "FileUtility.h"

std::string BuildMakefile(std::string Makefile);

std::string BuildProject(BuildInfo* Build, Makefile& Make, BuildSystem* System = nullptr)
{
	bool OwnsSystem = false;
	if (System == nullptr)
	{
		System = new VCBuild();
		OwnsSystem = true;
	}

	for (const auto& dep : Build->Dependencies)
	{
		for (BuildInfo* p : Make.Projects)
		{
			if (p->TargetName == dep)
			{
				std::cout << "Building dependency for project '" << Build->TargetName << "': '" << p->TargetName << "'" << std::endl;
				BuildProject(p, Make, System);
			}
		}
	}

	if (Build->IsMakefile)
	{
		BuildMakefile(Build->MakefilePath);
		if (OwnsSystem)
		{
			delete System;
		}
		return "";
	}

	std::vector<std::string> ObjectFiles;

	for (const auto& i : Build->CompiledFiles)
	{
		ObjectFiles.push_back(System->Compile(i, Build));
	}

	std::string OutFile = System->Link(ObjectFiles, Build);
	if (OwnsSystem)
	{
		delete System;
	}
	return OutFile;
}

std::string BuildMakefile(std::string Makefile)
{
	std::cout << "Building makefile " << Makefile << std::endl;
	auto MakefilePath = std::filesystem::absolute(Makefile);
	auto PrevPath = std::filesystem::absolute(std::filesystem::current_path());
	std::filesystem::current_path(FileUtility::RemoveFilename(Makefile));

	if (!std::filesystem::exists(MakefilePath))
	{
		throw 2;
		return "";
	}

	auto LoadedMakefile = Makefile::ReadMakefile(MakefilePath.string());
	if (LoadedMakefile.DefaultProject == SIZE_MAX)
	{
		for (BuildInfo* i : LoadedMakefile.Projects)
		{
			BuildProject(i, LoadedMakefile);
		}
	}
	else
	{
		BuildProject(LoadedMakefile.Projects[LoadedMakefile.DefaultProject], LoadedMakefile);
	}

	std::filesystem::current_path(PrevPath);
	return "";
}

int main()
{
	BuildMakefile("makefile.json");
}