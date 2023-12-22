#include "GCC-Linux.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include "../FileUtility.h"
#include "../KlemmBuild.h"
#include <filesystem>

std::atomic<size_t> GCC_Linux::BuiltFiles;
std::atomic<size_t> GCC_Linux::AllFiles;

#if _WIN32
#define popen(...) _popen(__VA_ARGS__)
#define pclose(...) _pclose(__VA_ARGS__)
#endif

std::atomic<bool> BuildFailed = false;

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
				CompileFiles.push_back(Source);
				break;
			}
			if (std::filesystem::last_write_time(Path) > std::filesystem::last_write_time(ObjectFile))
			{
				CompileFiles.push_back(Source);
				break;
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
		if (Build->OutputPath[Build->OutputPath.size() - 1] != '/')
		{
			Build->OutputPath.append("/");
		}
	}

	CompileAll(Build);
	std::string SourcesString;

	for (auto& i : Sources)
	{
		SourcesString.append(" " + i + " ");
	}

	std::string OutputFile = Build->OutputPath;
	switch (Build->TargetType)
	{
	case BuildInfo::BuildType::DynamicLibrary:
		OutputFile.append("lib" + Build->TargetName + ".so");
		break;
	case BuildInfo::BuildType::Executable:
		OutputFile.append(Build->TargetName);
		break;
	case BuildInfo::BuildType::StaticLibrary:
		OutputFile.append("lib" + Build->TargetName + ".a");
		break;
	default:
		break;
	}

	bool RequiresReLink = false;

	std::string LibStrings;
	std::filesystem::file_time_type ExecWriteTime;
	if (!std::filesystem::exists(OutputFile))
	{
		RequiresReLink = true;
	}
	else
	{
		ExecWriteTime = std::filesystem::last_write_time(OutputFile);
	}
	for (auto& i : Build->Libraries)
	{
		if (FileUtility::GetFilenameFromPath(i).substr(0, 3) == "lib")
		{
			i = ":" + i;
		}

		LibStrings.append(" -L"
			+ std::filesystem::absolute(FileUtility::RemoveFilename(i)).string() 
			+ " -l" + FileUtility::GetFilenameFromPath(i));

		std::string SoName = FileUtility::RemoveFilename(i) + "/lib" + FileUtility::GetFilenameFromPath(i) + ".so";
		std::string ArchiveName = FileUtility::RemoveFilename(i) + "/lib" + FileUtility::GetFilenameFromPath(i) + ".a";

		if (std::filesystem::exists(SoName) && !std::filesystem::is_directory(SoName))
		{
			std::filesystem::copy(SoName,
				Build->OutputPath + "lib" + FileUtility::GetFilenameFromPath(i) + ".so",
				std::filesystem::copy_options::overwrite_existing);
			if (std::filesystem::last_write_time(SoName) > ExecWriteTime)
			{
				RequiresReLink = true;
			}
		}

		if (std::filesystem::exists(ArchiveName) && !std::filesystem::is_directory(ArchiveName))
		{
			if (std::filesystem::last_write_time(ArchiveName) > ExecWriteTime)
			{
				RequiresReLink = true;
			}
		}
	}
	std::string Command;

	switch (Build->TargetType)
	{
	case BuildInfo::BuildType::DynamicLibrary:
		Command = ("c++ -shared" + SourcesString + LibStrings + " -o " + OutputFile);
		break;
	case BuildInfo::BuildType::Executable:
		Command = ("c++ " + SourcesString + LibStrings + " -Wl,-rpath,'$ORIGIN' -o " + OutputFile);
		break;
	case BuildInfo::BuildType::StaticLibrary:
		Command = ("ar rcs " + OutputFile + SourcesString);
		break;
	default:
		break;
	}
	for (auto& Path : Sources)
	{
		if (!std::filesystem::exists(OutputFile))
		{
			RequiresReLink = true;
			break;
		}
		if (!std::filesystem::exists(Path))
		{
			RequiresReLink = true;
			continue;
		}
		if (std::filesystem::last_write_time(Path) > ExecWriteTime)
		{
			RequiresReLink = true;
		}
	}

	if (Sources.empty())
	{
		RequiresReLink = true;
	}
	if (RequiresReLink || (!CompileFiles.empty()))
	{
		std::cout << "- [100%] Linking..." << std::endl;
		int ret = system(Command.c_str());
		if (ret)
		{
			std::cout << "Build failed" << std::endl;
			return false;
		}

		std::cout << "Project '" << Build->TargetName << "' -> " << OutputFile << std::endl;
	}
	else
	{
		std::cout << "Project '" << Build->TargetName << "' is up to date - skipping" << std::endl;
	}
	return true;
}

std::string GCC_Linux::PreprocessFile(std::string Source, std::vector<std::string> Definitions)
{
	if (!std::filesystem::exists("Build"))
	{
		try
		{
			std::filesystem::create_directories("Build");
		}
		catch (std::exception&)
		{

		}
	}
	std::string DefString;
	for (auto& i : Definitions)
	{
		DefString.append(" -D " + i + " ");
	}
	std::string Command = "c++ -x c -E " + Source + DefString + " > Build/cpp.i";
	std::cout << Command << std::endl;
	int ret = system(Command.c_str());

	if (ret)
	{
		std::cout << "Preprocessing failed: " + Source << std::endl;
		return "";
	}

	if (!std::filesystem::exists("Build/cpp.i"))
	{
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
	BuiltFiles = 0;
	std::vector<std::vector<std::string>> BuildThreadFiles;
	BuildThreadFiles.resize(KlemmBuild::BuildThreads);
	BuildThreads.resize(KlemmBuild::BuildThreads);
	AllFiles = CompileFiles.size();
	for (size_t i = 0; i < CompileFiles.size(); i++)
	{
		BuildThreadFiles[i % KlemmBuild::BuildThreads].push_back(CompileFiles[i]);
	}

	for (uint8_t i = 0; i < KlemmBuild::BuildThreads; i++)
	{
		BuildThreads[i] = new std::thread(BuildThread, BuildThreadFiles[i], Build);
	}

	for (uint8_t i = 0; i < KlemmBuild::BuildThreads; i++)
	{
		BuildThreads[i]->join();
		delete BuildThreads[i];
	}
	BuildThreads.clear();
}

void GCC_Linux::BuildThread(std::vector<std::string> Files, BuildInfo* Build)
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
		Flags.append("-O0 ");
		break;
	case BuildInfo::OptimizationType::Smallest:
		Flags.append("-Os ");
		break;
	case BuildInfo::OptimizationType::Fastest:
		Flags.append("-O3 ");
		break;
	default:
		break;
	}

	for (auto& i : Build->PreProcessorDefinitions)
	{
		Flags.append(" -D " + i + " ");
	}

	for (auto& i : Files)
	{
		std::filesystem::create_directories("Build/" + Build->TargetName + "/" + FileUtility::RemoveFilename(i));
		std::string ObjectFile = "Build/" + Build->TargetName + "/" + i + ".o";
		std::cout << "- [" << (size_t)(((float)BuiltFiles / (float)AllFiles) * 100.0f) << "%] Compiling " + i << std::endl;
		BuiltFiles++;
		std::string Command = "c++ -c -MMD -MP " + Flags + i + IncludeString + " -fno-char8_t -fPIC -std=c++2a -o " + ObjectFile;
		//std::cout << Command << std::endl;
		int ret = system(Command.c_str());
		if (BuildFailed)
		{
			break;
		}
		if (ret)
		{
			BuildFailed = true;
			break;
		}
	}
}
