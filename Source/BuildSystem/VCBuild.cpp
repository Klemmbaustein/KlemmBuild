#if _WIN32
#include "VCBuild.h"
#include "../Command.h"
#include <filesystem>
#include <iostream>
#include "../FileUtility.h"
#include "../KlemmBuild.h"
#include <thread>

std::vector<int> VCBuild::BuildOutput;

struct CompileTimeFile
{
	std::string Path;
	std::string ObjPath;
	std::vector<std::string> ObjDeps;
};

bool VCBuild::RequiresRebuild(std::string File, BuildInfo* Info)
{
	std::string ObjectFile = "Build/" + Info->TargetName + "/" + File + ".obj";
	if (!std::filesystem::exists(ObjectFile))
	{
		return true;
	}

	if (std::filesystem::last_write_time(ObjectFile) < std::filesystem::last_write_time(File))
	{
		return true;
	}
	std::string DepsFilePath = "Build/" + Info->TargetName + "/" + File + ".obj.kdep";
	if (!std::filesystem::exists(DepsFilePath))
	{
		return true;
	}
	std::ifstream DepsFile = std::ifstream(DepsFilePath);
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

int VCBuild::ShellExecute(std::string cmd)
{
	std::ofstream out = std::ofstream("Build\\cmd\\cmd.cmd");
	out << "@echo off" << std::endl;
	out << "set VSCMD_ARG_HOST_ARCH=x64" << std::endl;
	out << "set VSCMD_ARG_TGT_ARCH=x64" << std::endl;
	out << "set VSCMD_ARG_APP_PLAT=Desktop" << std::endl;
	out << "set VSINSTALLDIR=C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\" << std::endl;
	out << "call \"%VSINSTALLDIR%\\Common7\\Tools\\vsdevcmd\\ext\\vcvars.bat\"" << std::endl;
	out << "call \"%VSINSTALLDIR%\\Common7\\Tools\\vsdevcmd\\core\\winsdk.bat\"" << std::endl;
	out << "if not defined INCLUDE set INCLUDE=%__VSCMD_VCVARS_INCLUDE%%__VSCMD_WINSDK_INCLUDE%%__VSCMD_NETFX_INCLUDE%%INCLUDE%" << std::endl;
	out << cmd;
	out << "exit /k %errorlevel%";
	out.close();
	int ret = system("Build\\cmd\\cmd.cmd");
	std::filesystem::remove("Build\\cmd\\cmd.cmd");
	return ret;
}

void VCBuild::CompileThread(int Index, BuildInfo* Info)
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
				CurrentCompiledFile.ObjPath = "Build/" + Info->TargetName + "/" + CurrentLine.substr(15) + ".obj";
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
	std::string ToolsLocation = VSLocation + "\\VC\\Tools\\MSVC\\";

	for (auto& i : std::filesystem::directory_iterator(ToolsLocation))
	{
		ToolsLocation.append(i.path().filename().string() + "\\bin\\Hostx64\\x64\\");
		break;
	}

	ClLocation = "\"" + ToolsLocation + "cl.exe\"";
	std::filesystem::create_directories("Build/cmd");
	if (UsedForBuild)
	{
		BuildScripts.resize(KlemmBuild::BuildThreads + 1);
		for (size_t i = 0; i <= KlemmBuild::BuildThreads; i++)
		{
			if (i < KlemmBuild::BuildThreads)
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
			BuildScripts[i] << "set VSINSTALLDIR=" + VSLocation + "\\" << std::endl;
			BuildScripts[i] << "call \"%VSINSTALLDIR%\\Common7\\Tools\\vsdevcmd\\ext\\vcvars.bat\"" << std::endl;
			BuildScripts[i] << "call \"%VSINSTALLDIR%\\Common7\\Tools\\vsdevcmd\\core\\winsdk.bat\"" << std::endl;
			BuildScripts[i] << "if not defined INCLUDE set INCLUDE=%__VSCMD_VCVARS_INCLUDE%%__VSCMD_WINSDK_INCLUDE%%__VSCMD_NETFX_INCLUDE%%INCLUDE%" << std::endl;
		}
		BuildOutput.resize(KlemmBuild::BuildThreads);
	}
}

VCBuild::~VCBuild()
{
	if (BuildScripts.empty())
	{
		return;
	}
	BuildScripts[KlemmBuild::BuildThreads].close();
	for (int i = 0; i < KlemmBuild::BuildThreads; i++)
	{
		BuildScripts[i].close();
	}
}

std::string VCBuild::Compile(std::string Source, BuildInfo* Build)
{
	if (RequiresRebuild(Source, Build))
	{
		int CompileThreadIndex = CompileIndex++ % KlemmBuild::BuildThreads;
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

		std::string PreprocessorString;
		for (auto& i : Build->PreProcessorDefinitions)
		{
			PreprocessorString.append(" /D " + i + " ");
		}

		std::filesystem::create_directories(FileUtility::RemoveFilename("Build/" + Build->TargetName + "/" + Source));
		BuildScripts[CompileThreadIndex] << "cl /nologo /showIncludes /MD /c /permissive- /Zc:preprocessor /std:c++20 /EHsc "
			+ OptString
			+ " "
			+ PreprocessorString
			+ Source
			+ Includes
			+ " /Fo:\"Build/"
			+ Build->TargetName 
			+ "/"
			+ Source
			+ ".obj\"" << " || goto :error" << std::endl;
	}
	return "Build/" + Build->TargetName + "/" + Source + ".obj";
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
				Build->OutputPath + "/" + FileUtility::GetFilenameFromPath(i) + ".dll",
				std::filesystem::copy_options::overwrite_existing);
		}
	}

	for (int i = 0; i < KlemmBuild::BuildThreads; i++)
	{
		BuildScripts[i] << "echo BUILD: DONE" << std::endl;
		BuildScripts[i] << "exit" << std::endl;
		BuildScripts[i] << ":error" << std::endl;
		BuildScripts[i] << "echo BUILD: DONE" << std::endl;
		BuildScripts[i] << "exit 1" << std::endl;
		BuildScripts[i].close();
	}

	std::vector<std::thread*> BuildThreads;
	for (int i = 0; i < KlemmBuild::BuildThreads; i++)
	{
		BuildThreads.push_back(new std::thread(CompileThread, i, Build));
	}

	bool Failed = false;
	for (int i = 0; i < KlemmBuild::BuildThreads; i++)
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

	BuildScripts[KlemmBuild::BuildThreads] << "echo BUILD: LINK" << std::endl;

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
		BuildScripts[KlemmBuild::BuildThreads]
			<< "link"
			<< AllObjs << " /nologo /MD /OUT:"
			<< OutputPath
			<< Build->TargetName
			<< ".exe || goto :error" << std::endl;
		OutputFile.append(".exe");
		break;
	case BuildInfo::BuildType::DynamicLibrary:
		BuildScripts[KlemmBuild::BuildThreads]
			<< "link"
			<< " /nologo /DLL /MD /OUT:"
			<< OutputPath
			<< Build->TargetName
			<< ".dll"
			<< AllObjs
			<< " || goto :error" << std::endl;
		OutputFile.append(".dll");
		break;
	case BuildInfo::BuildType::StaticLibrary:
		BuildScripts[KlemmBuild::BuildThreads]
			<< "lib"
			<< " /nologo /OUT:"
			<< OutputPath
			<< Build->TargetName
			<< ".lib"
			<< AllObjs
			<< " || goto :error" << std::endl;
		OutputFile.append(".lib");
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
			if (!std::filesystem::exists(i + ".lib"))
			{
				continue;
			}
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

	BuildScripts[KlemmBuild::BuildThreads] << "echo BUILD: DONE" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads] << "exit" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads] << ":error" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads] << "echo BUILD: COMPILED WITH ERROS" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads] << "echo BUILD: DONE" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads] << "exit 1" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads].close();

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
				CurrentCompiledFile.ObjPath = "Build/" + Build->TargetName + "/" + CurrentLine.substr(15) + ".obj";
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
	std::string DefinitionString;
	for (auto& i : Definitions)
	{
		DefinitionString.append(" /D " + i + " ");
	}
	std::cout << "Reading ";

	FILE* Preprocessor = _popen((ClLocation + " /c /EP /nologo /D MSVC_WINDOWS " + DefinitionString + Source + " && echo $END || echo $END").c_str(), "r");

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