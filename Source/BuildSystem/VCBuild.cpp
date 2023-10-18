#if _WIN32
#include "VCBuild.h"
#include "../Command.h"
#include <filesystem>
#include <iostream>
#include "../FileUtility.h"
#include "../KlemmBuild.h"
#include <thread>

#define BUILD_NUM_THREADS 4
std::vector<int> VCBuild::BuildOutput;

struct CompileTimeFile
{
	std::string Path;
	std::string ObjPath;
	std::vector<std::string> ObjDeps;
};

bool VCBuild::RequiresRebuild(std::string File)
{
	std::string ObjectFile = "Build/" + File + ".obj";
	if (!std::filesystem::exists(ObjectFile))
	{
		return true;
	}

	if (std::filesystem::last_write_time(ObjectFile) < std::filesystem::last_write_time(File))
	{
		return true;
	}

	std::ifstream DepsFile ("Build/" + File + ".obj.kdep");
	while (!DepsFile.eof())
	{
		char Path[4096];
		DepsFile.getline(Path, 4096);
		if (!std::filesystem::exists(Path))
		{
			continue;
		}
		if (std::filesystem::last_write_time(Path) > std::filesystem::last_write_time(ObjectFile))
		{
			return true;
		}
	}
	return false;
}

void VCBuild::CompileThread(int Index)
{
	FILE* BuildProcess = _popen(("cmd.exe /c Build\\cmd\\build_t" + std::to_string(Index) + ".cmd").c_str(), "r");
	std::vector<std::string> FileDependencies;

	std::string CurrentLine;

	std::vector<CompileTimeFile> Files;
	CompileTimeFile CurrentCompiledFile;

	std::vector<std::string> ErrorStrings;

	while (true)
	{
		char NewChar;
		fread(&NewChar, sizeof(char), 1, BuildProcess);
		if (NewChar == '\n')
		{
			if (CurrentLine.substr(0, 22) == "Note: including file: ")
			{
				FileDependencies.push_back(CurrentLine.substr(22));
				CurrentCompiledFile.ObjDeps.push_back(CurrentLine.substr(22));
			}
			else if (CurrentLine.substr(0, 15) == "BUILD: COMPILE ")
			{
				if (!CurrentCompiledFile.Path.empty())
				{
					Files.push_back(CurrentCompiledFile);
				}
				CurrentCompiledFile.Path = CurrentLine.substr(15);
				CurrentCompiledFile.ObjPath = "Build/" + CurrentLine.substr(15) + ".obj";
				CurrentCompiledFile.ObjDeps.clear();
				std::cout << "- Compiling " + CurrentLine.substr(15) << std::endl;
			}
			else if (CurrentLine.find("error") != std::string::npos)
			{
				ErrorStrings.push_back(CurrentLine);
			}
			else if (CurrentLine == "BUILD: DONE")
			{
				break;
			}
			CurrentLine.clear();
			continue;
		}
		CurrentLine.append({ NewChar });
	}
	Files.push_back(CurrentCompiledFile);
	for (const auto& i : Files)
	{
		if (i.ObjPath.empty())
		{
			continue;
		}
		std::filesystem::create_directories(FileUtility::RemoveFilename(i.ObjPath));
		std::ofstream DepsFile = std::ofstream(i.ObjPath + ".kdep");
		for (const auto& j : i.ObjDeps)
		{
			DepsFile << "" << j.substr(j.find_first_not_of(" ")) << std::endl;
		}
		DepsFile.close();
	}

	int Return = _pclose(BuildProcess);
	BuildOutput[Index] = Return;
	if (Return)
	{
		std::cout << "Build failed: " << std::endl;
		for (auto& i : ErrorStrings)
		{
			std::cout << i << std::endl;
		}
	}
}

static std::string GetSystemCommandReturnValue(const std::string& command)
{
	std::system((command + " > temp.txt").c_str());

	std::ifstream ifs("temp.txt");
	std::string ret{ std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>() };
	ifs.close(); // must close the inout stream so the file can be cleaned up
	if (std::remove("temp.txt") != 0)
	{
		perror("Error deleting temporary file");
	}
	ret.erase(std::remove(ret.begin(), ret.end(), '\n'), ret.cend()); //remove newline from the end of the file
	return ret;
}

std::string VCBuild::GetVSLocation()
{
	auto test = GetSystemCommandReturnValue("cmd /C \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\"\
 -latest -property installationPath");
	return test;
}

VCBuild::VCBuild(bool UsedForBuild)
{
	std::string VSLocation = GetVSLocation();
	std::string ToolsLocation = VSLocation + "\\VC\\Tools\\MSVC\\14.37.32822\\bin\\Hostx64\\x64\\";
	VCVarsLocation = "\"" + VSLocation + "\\VC\\Auxiliary\\Build\\vcvarsall.bat\"";
	ClLocation = "\"" + ToolsLocation + "cl.exe\"";
	LinkExeLocation = "\"" + ToolsLocation + "link.exe\"";
	std::filesystem::create_directories("Build/cmd");
	if (UsedForBuild)
	{
		BuildScripts.resize(BUILD_NUM_THREADS + 1);
		for (size_t i = 0; i <= BUILD_NUM_THREADS; i++)
		{
			if (i < BUILD_NUM_THREADS)
			{
				BuildScripts[i] = std::ofstream("Build/cmd/build_t" + std::to_string(i) + ".cmd");
			}
			else
			{
				BuildScripts[i] = std::ofstream("Build/cmd/link.cmd");
			}
			BuildScripts[i] << "@echo off" << std::endl;
			BuildScripts[i] << "set VSCMD_ARG_HOST_ARCH=x64" << std::endl;
			BuildScripts[i] << "set VSCMD_ARG_TGT_ARCH=x64" << std::endl;
			BuildScripts[i] << "set VSCMD_ARG_APP_PLAT=Desktop" << std::endl;
			BuildScripts[i] << "set VSINSTALLDIR=C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\" << std::endl;
			BuildScripts[i] << "call \"%VSINSTALLDIR%\\Common7\\Tools\\vsdevcmd\\ext\\vcvars.bat\"" << std::endl;
			BuildScripts[i] << "call \"%VSINSTALLDIR%\\Common7\\Tools\\vsdevcmd\\core\\winsdk.bat\"" << std::endl;
			BuildScripts[i] << "if not defined INCLUDE set INCLUDE=%__VSCMD_VCVARS_INCLUDE%%__VSCMD_WINSDK_INCLUDE%%__VSCMD_NETFX_INCLUDE%%INCLUDE%" << std::endl;
		}
		BuildOutput.resize(BUILD_NUM_THREADS);
	}
}

VCBuild::~VCBuild()
{
	if (BuildScripts.empty())
	{
		return;
	}
	BuildScripts[BUILD_NUM_THREADS].close();
	for (int i = 0; i < BUILD_NUM_THREADS; i++)
	{
		BuildScripts[i].close();
	}
}

std::string VCBuild::Compile(std::string Source, BuildInfo* Build)
{
	if (RequiresRebuild(Source))
	{
		int CompileThreadIndex = CompileIndex++ % BUILD_NUM_THREADS;
		BuildScripts[CompileThreadIndex] << "echo BUILD: COMPILE " << Source << std::endl;

		std::string Includes = "";
		for (const auto& i : Build->IncludePaths)
		{
			Includes.append(" /I\"" + i + "\" ");
		}

		std::string OptString;
		switch (Build->TargetOpt)
		{
		case BuildInfo::OptimizationType::None:
			OptString = "/Od";
			break;

		case BuildInfo::OptimizationType::Fastest:
			OptString = "/O2";
			break;

		case BuildInfo::OptimizationType::Smallest:
			OptString = "/O1";
			break;

		default:
			break;
		}
		std::filesystem::create_directories(FileUtility::RemoveFilename("Build/" + Source));
		BuildScripts[CompileThreadIndex] << ClLocation
			+ " /nologo /showIncludes /c /std:c++20 /EHsc "
			+ OptString
			+ " "
			+ Source
			+ Includes
			+ " /Fo:\"Build/"
			+ Source
			+ ".obj\"" << " || goto :error" << std::endl;
	}
	return "Build/" + Source + ".obj";
}
bool VCBuild::Link(std::vector<std::string> Sources, BuildInfo* Build)
{
	std::cout << "Building project '" << Build->TargetName << "' with build system MSVC" << std::endl;

	std::string AllObjs;
	for (const auto& i : Sources)
	{
		AllObjs.append(" " + i);
	}
	for (const auto& i : Build->Libraries)
	{
		AllObjs.append(" " + i + ".lib");
		if (std::filesystem::exists(i + ".dll") && !std::filesystem::is_directory(i + ".dll"))
		{
			std::filesystem::copy(i + ".dll",
				Build->OutputPath + FileUtility::GetFilenameFromPath(i) + ".dll",
				std::filesystem::copy_options::overwrite_existing);
		}
	}

	for (int i = 0; i < BUILD_NUM_THREADS; i++)
	{
		BuildScripts[i] << "echo BUILD: DONE" << std::endl;
		BuildScripts[i] << "exit" << std::endl;
		BuildScripts[i] << ":error" << std::endl;
		BuildScripts[i] << "echo BUILD: DONE" << std::endl;
		BuildScripts[i] << "exit 1" << std::endl;
		BuildScripts[i].close();
	}

	std::vector<std::thread*> BuildThreads;
	for (int i = 0; i < BUILD_NUM_THREADS; i++)
	{
		BuildThreads.push_back(new std::thread(CompileThread, i));
	}

	bool Failed = false;
	for (int i = 0; i < BUILD_NUM_THREADS; i++)
	{
		BuildThreads[i]->join();
		delete BuildThreads[i];
		if (BuildOutput[i])
		{
			Failed = true;
		}
	}

	if (Failed)
	{
		return false;
	}

	BuildScripts[BUILD_NUM_THREADS] << "echo BUILD: LINK" << std::endl;

	std::string OutputPath = "./";
	if (!Build->OutputPath.empty())
	{
		OutputPath = Build->OutputPath;
		if (OutputPath[OutputPath.size() - 1] != '/')
		{
			OutputPath.append("/");
		}
		std::filesystem::create_directories(OutputPath);
	}

	std::string OutputFile = OutputPath + Build->TargetName;

	switch (Build->TargetType)
	{
	case BuildInfo::BuildType::Executable:
		BuildScripts[BUILD_NUM_THREADS] 
			<< LinkExeLocation
			<< AllObjs << " /nologo /OUT:"
			<< OutputPath
			<< Build->TargetName
			<< ".exe || goto :error" << std::endl;
		OutputFile.append(".exe");
		break;
	case BuildInfo::BuildType::DynamicLibrary:
		BuildScripts[BUILD_NUM_THREADS] 
			<< LinkExeLocation
			<< " /nologo /DLL /OUT:"
			<< OutputPath
			<< Build->TargetName
			<< ".dll"
			<< AllObjs
			<< " || goto :error" << std::endl;
		OutputFile.append(".dll");
		break;

	default:
		break;
	}

	bool Relink = false;
	if (std::filesystem::exists(OutputFile))
	{
		for (const auto& i : Sources)
		{
			if (!std::filesystem::exists(i))
			{
				std::cout << "Warning: " << i << " does not exist" << std::endl;
				continue;
			}
			if (std::filesystem::last_write_time(i) > std::filesystem::last_write_time(OutputFile))
			{
				Relink = true;
			}
		}
		for (const auto& i : Build->Libraries)
		{
			if (std::filesystem::last_write_time(i + ".lib") > std::filesystem::last_write_time(OutputFile))
			{
				Relink = true;
			}
		}
	}
	else
	{
		Relink = true;
	}

	BuildScripts[BUILD_NUM_THREADS] << "echo BUILD: DONE" << std::endl;
	BuildScripts[BUILD_NUM_THREADS] << "exit" << std::endl;
	BuildScripts[BUILD_NUM_THREADS] << ":error" << std::endl;
	BuildScripts[BUILD_NUM_THREADS] << "echo BUILD: COMPILED WITH ERROS" << std::endl;
	BuildScripts[BUILD_NUM_THREADS] << "echo BUILD: DONE" << std::endl;
	BuildScripts[BUILD_NUM_THREADS] << "exit 1" << std::endl;
	BuildScripts[BUILD_NUM_THREADS].close();

	if (!Relink)
	{
		std::cout << "Project '" << Build->TargetName << "' is up to date - skipping" << std::endl;
		return  true;
	}


	FILE* BuildProcess = _popen("cmd.exe /c Build\\cmd\\link.cmd", "r");

	std::string CurrentLine;
	std::vector<std::string> FileDependencies;

	std::vector<CompileTimeFile> Files;
	CompileTimeFile CurrentCompiledFile;

	std::vector<std::string> ErrorStrings;

	while (true)
	{
		char NewChar;
		fread(&NewChar, sizeof(char), 1, BuildProcess);
		if (NewChar == '\n')
		{
			if (CurrentLine.substr(0, 22) == "Note: including file: ")
			{
				FileDependencies.push_back(CurrentLine.substr(22));
				CurrentCompiledFile.ObjDeps.push_back(CurrentLine.substr(22));
			}
			else if (CurrentLine.substr(0, 15) == "BUILD: COMPILE ")
			{
				if (!CurrentCompiledFile.Path.empty())
				{
					Files.push_back(CurrentCompiledFile);
				}
				CurrentCompiledFile.Path = CurrentLine.substr(15);
				CurrentCompiledFile.ObjPath = "Build/" + CurrentLine.substr(15) + ".obj";
				CurrentCompiledFile.ObjDeps.clear();
				std::cout << "- Compiling " + CurrentLine.substr(15) << std::endl;
			}
			else if (CurrentLine.find("error") != std::string::npos)
			{
				ErrorStrings.push_back(CurrentLine);
			}
			else if (CurrentLine == "BUILD: DONE")
			{
				break;
			}
			else if (CurrentLine == "BUILD: LINK")
			{
				std::cout << "- Linking..." << std::endl;
			}
			CurrentLine.clear();
			continue;
		}
		CurrentLine.append({NewChar});
	}
	
	int Return = _pclose(BuildProcess);

	if (Return)
	{
		std::cout << "Build failed: " << std::endl;
		for (auto& i : ErrorStrings)
		{
			std::cout << i << std::endl;
		}

		return false;
	}

	std::cout << "Project '" << Build->TargetName << "' -> " << OutputFile << std::endl;

	return true;
}
std::string VCBuild::PreprocessFile(std::string Source, std::vector<std::string> Definitions)
{
	std::cout << "Reading ";
	FILE* Preprocessor = _popen((ClLocation + "/c /EP /nologo /D MSVC_WINDOWS /D CONFIG=Release " + Source + " && echo $END").c_str(), "r");

	std::string File;
	while (true)
	{
		char NewChar;
		fread(&NewChar, sizeof(char), 1, Preprocessor);
		File.append({NewChar});
		if (File.size() >= 4 && File.substr(File.size() - 4) == "$END")
		{
			File = File.substr(0, File.size() - 4);
			break;
		}
	}


	int ret = _pclose(Preprocessor);
	
	if (ret)
	{
		std::cout << "Preprocessing failed: " + Source << std::endl;
		return "";
	}

	return File;
}
#endif