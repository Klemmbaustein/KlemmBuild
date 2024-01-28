#include "BuildSystem/VCBuild.h"
#include "BuildSystem/GCC-Linux.h"
#include "Makefile.h"
#include <iostream>
#include <filesystem>
#include "FileUtility.h"
#include "KlemmBuild.h"

namespace KlemmBuild
{
	std::mutex PrintMutex;
	uint8_t BuildThreads = 4;
}

static std::vector<std::string> GlobalDefines;

std::string KlemmBuild::BuildMakefile(std::string Makefile);

bool BuildMakefileTarget(Target* Build, Makefile& Make)
{
	BuildSystem* System = nullptr;
	{
#if _WIN32
		System = new VCBuild();
#else
		System = new GCC_Linux();
#endif
	}

	for (const auto& dep : Build->GetParameter("dependencies")->GetValues())
	{
		bool Found = false;
		for (Target* p : Make.Targets)
		{
			if (p->Name == dep)
			{
				std::cout << "Building dependency for target '" << Build->Name << "': '" << p->Name << "'" << std::endl;
				BuildMakefileTarget(p, Make);
				Found = true;
				break;
			}
		}
		if (!Found)
		{
			std::cout << "Could not find target '" << dep << "' referenced in '" << Build->Name << "'" << std::endl;
			KlemmBuild::Exit();
		}
	}
	Build->Invoke(System);

	delete System;
	return true;
}

std::string KlemmBuild::BuildMakefile(std::string Makefile)
{
	std::cout << " -- Building makefile " << Makefile << " -- " << std::endl;
	auto MakefilePath = std::filesystem::absolute(Makefile);
	auto PrevPath = std::filesystem::absolute(std::filesystem::current_path());
	std::filesystem::current_path(FileUtility::RemoveFilename(Makefile));

	auto LoadedMakefile = Makefile::ReadMakefile(MakefilePath.string(), GlobalDefines);
	if (LoadedMakefile.DefaultTarget == SIZE_MAX)
	{
		std::cout << "No default target specified - Building all" << std::endl;
		for (Target* i : LoadedMakefile.Targets)
		{
			if (!BuildMakefileTarget(i, LoadedMakefile))
			{
				KlemmBuild::Exit();
			}
		}
	}
	else
	{
		std::cout << "Starting with the default project - '" 
			<< LoadedMakefile.Targets[LoadedMakefile.DefaultTarget]->Name
			<< "'" 
			<< std::endl;

		if (!BuildMakefileTarget(LoadedMakefile.Targets[LoadedMakefile.DefaultTarget], LoadedMakefile))
		{
			KlemmBuild::Exit();
		}
	}

	std::cout << " -- Finished building makefile " << Makefile << " -- " << std::endl;
	std::filesystem::current_path(PrevPath);
	return "";
}

int main(int argc, char** argv)
{
	std::vector<std::string> Makefiles;
	for (int i = 1; i < argc; i++)
	{
		/**
		* @todo Implement a real argument parser
		*/
		std::string ArgStr = argv[i];
		if (ArgStr.substr(0, 2) == "-D")
		{
			GlobalDefines.push_back(ArgStr.substr(2));
		}
		else if (std::filesystem::exists(ArgStr))
		{
			Makefiles.push_back(ArgStr);
		}
		else if (ArgStr == "-h" || ArgStr == "--help")
		{
			std::cout << "Usage: " << argv[0] << " [Argument, Definition, Makefile]" << std::endl << std::endl;
			std::cout << "Arguments:" << std::endl;
			std::cout << "\t-h --help:    Displays this message" << std::endl;
			std::cout << "\t-v --version: Shows version information" << std::endl << std::endl;
			std::cout << "Definitions:" << std::endl;
			std::cout << "\tAny argument starting with -D" << std::endl;
			std::cout << "\tExample: The argument '-DDebug' defines the 'Debug' preprocessor definition for the given makefiles." << std::endl << std::endl;
			std::cout << "Makefile:" << std::endl;
			std::cout << "\tCan be the path to any file." << std::endl;
			return 0;
		}
		else if (ArgStr == "-v" || ArgStr == "--version")
		{
			std::cout << "KlemmBuild C++ build system v" << KlemmBuild::Version << " for ";
#if _WIN32
			std::cout << "Windows" << std::endl;
#else
			std::cout << "Linux" << std::endl;
#endif
			std::cout << "GitHub: https://github.com/Klemmbaustein/KlemmBuild" << std::endl;
			return 0;
		}
		else
		{
			std::cout << "Unknown argument: " << ArgStr << std::endl;
		}
	}
	if (Makefiles.empty())
	{
		if (std::filesystem::exists("makefile.kbld"))
		{
			Makefiles = { "makefile.kbld" };
		}
		else
		{
			std::cout << "No makefile found" << std::endl;
		}
	}
	for (auto& i : Makefiles)
	{
		KlemmBuild::BuildMakefile(i);
	}
}

void KlemmBuild::Exit()
{
	std::cout << "Build failed - exiting" << std::endl;
	exit(1);
}
