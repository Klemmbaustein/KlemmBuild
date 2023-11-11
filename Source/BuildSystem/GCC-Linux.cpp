#include "GCC-Linux.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include "../FileUtility.h"
#include <filesystem>

#if _WIN32
#define popen(...) _popen(__VA_ARGS__)
#define pclose(...) _pclose(__VA_ARGS__)
#endif

std::string GCC_Linux::Compile(std::string Source, BuildInfo* Build)
{
	std::string ObjectFile = "Build/" + Build->TargetName + "/" + Source + ".o";
	std::string DepsPath = "Build/" + Build->TargetName + "/" + Source + ".d";

	if (std::filesystem::exists(DepsPath) && std::filesystem::exists(ObjectFile))
	{
		std::ifstream DepsFile = std::ifstream(DepsPath);
		std::string File;
		std::vector<std::string> Dependencies = { Source };

		bool ReadingFirstLine = true;
		while (!DepsFile.eof())
		{
			char Line[4096];
			DepsFile.getline(Line, 4096);
			std::string LineStr = Line;
			if (LineStr.empty())
			{
				continue;
			}
			if (LineStr[LineStr.size() - 1] != '\\' && ReadingFirstLine)
			{
				ReadingFirstLine = false;
				continue;
			}
			if (!ReadingFirstLine)
			{
				if (LineStr[LineStr.size() - 1] == ':')
				{
					LineStr.pop_back();
				}
				Dependencies.push_back(LineStr);
			}
		}

		for (auto& Path : Dependencies)
		{
			if (!std::filesystem::exists(Path))
			{
				continue;
			}
			if (std::filesystem::last_write_time(Path) > std::filesystem::last_write_time(ObjectFile))
			{
				CompileFiles.push_back(Source);
			}
		}
		DepsFile.close();
	}
	else
	{
		CompileFiles.push_back(Source);
	}
	return "Build/" + Build->TargetName + "/" + Source + ".o";
}

bool GCC_Linux::Link(std::vector<std::string> Sources, BuildInfo* Build)
{
	if (!Build->OutputPath.empty())
	{
		std::filesystem::create_directories(Build->OutputPath);
		Build->OutputPath.append("/");
	}

	CompileAll(Build);
	std::string SourcesString;

	for (auto& i : Sources)
	{
		SourcesString.append(" " + i + " ");
	}

	std::string LibStrings;
	for (auto& i : Build->Libraries)
	{
		LibStrings.append(" -L"
			+ std::filesystem::absolute(FileUtility::RemoveFilename(i)).string() 
			+ " -l" + FileUtility::GetFilenameFromPath(i));

		std::string SoName = FileUtility::RemoveFilename(i) + "/lib" + FileUtility::GetFilenameFromPath(i) + ".so";

		if (std::filesystem::exists(SoName) && !std::filesystem::is_directory(SoName))
		{
			std::filesystem::copy(SoName,
				Build->OutputPath + "/lib" + FileUtility::GetFilenameFromPath(i) + ".so",
				std::filesystem::copy_options::overwrite_existing);
		}
	}
	int ret = 1;
	std::string Command;
	std::string OutFile;
	switch (Build->TargetType)
	{
	case BuildInfo::BuildType::DynamicLibrary:
		Command = ("c++ -shared" + SourcesString + LibStrings + " -o " + Build->OutputPath + "lib" + Build->TargetName + ".so");
		OutFile = Build->OutputPath + "lib" + Build->TargetName + ".so";
		break;
	case BuildInfo::BuildType::Executable:
		Command = ("c++ " + SourcesString + LibStrings + " -Wl,-rpath,'$ORIGIN' -o " + Build->OutputPath + Build->TargetName);
		OutFile = Build->OutputPath + Build->TargetName;
		break;
	case BuildInfo::BuildType::StaticLibrary:
		Command = ("ar rcs " + Build->OutputPath + "lib" + Build->TargetName + ".a" + SourcesString);
		OutFile = Build->OutputPath + "lib" + Build->TargetName + ".a";
		break;
	default:
		break;
	}

	bool RequiresReLink = false;
	for (auto& Path : Sources)
	{
		if (!std::filesystem::exists(Path))
		{
			continue;
		}
		if (std::filesystem::last_write_time(Path) > std::filesystem::last_write_time(OutFile))
		{
			RequiresReLink = true;
		}
	}

	if (RequiresReLink)
	{
		std::cout << "- Linking..." << std::endl;
		ret = system(Command.c_str());

		std::cout << "Project '" << Build->TargetName << "' -> " << OutFile << std::endl;
	}
	else
	{
		std::cout << "Project '" << Build->TargetName << "' is up to date - skipping" << std::endl;
		ret = 0;
	}
	return !ret;
}

std::string GCC_Linux::PreprocessFile(std::string Source, std::vector<std::string> Definitions)
{
	std::filesystem::create_directories("Build/");
	std::string DefString;
	for (auto& i : Definitions)
	{
		DefString.append(" -D " + i + " ");
	}
	int ret = system(("cpp " + Source + DefString + " -o Build/cpp.i; echo $END").c_str());

	if (ret)
	{
		std::cout << "Preprocessing failed: " + Source << std::endl;
		return "";
	}

	std::ifstream in = std::ifstream("Build/cpp.i");
	std::string File;

	while (!in.eof())
	{
		char Line[4096];
		in.getline(Line, 4096);
		if (Line[0] != '#')
		{
			File.append(Line);
			File.append("\n");
		}
	}
	in.close();

	return File;
}

bool GCC_Linux::RequiresRebuild(std::string File, BuildInfo* Info)
{
	return true;
}


void GCC_Linux::CompileAll(BuildInfo* Build)
{
	std::string IncludeString;

	for (auto& i : Build->IncludePaths)
	{
		IncludeString.append(" -I" + i + " ");
	}

	std::string Flags;

	switch (Build->TargetOpt)
	{
	case BuildInfo::OptimizationType::None:
		Flags.append("-O0");
		break;
	case BuildInfo::OptimizationType::Smallest:
		Flags.append("-Os");
		break;
	case BuildInfo::OptimizationType::Fastest:
		Flags.append("-O3");
		break;
	default:
		break;
	}
	for (auto& i : CompileFiles)
	{
		std::filesystem::create_directories("Build/" + Build->TargetName + "/" + FileUtility::RemoveFilename(i));
		std::string ObjectFile = "Build/" + Build->TargetName + "/" + i + ".o";
		std::cout << "- Compiling " + i << std::endl;
		system(("c++ -c -MMD -MP " + i + IncludeString + " " + Flags + " -fPIC -std=c++2a -o " + ObjectFile).c_str());
	}
}
