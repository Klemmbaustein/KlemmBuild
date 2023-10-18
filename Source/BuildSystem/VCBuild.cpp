#if _WIN32
#include "VCBuild.h"
#include "../Command.h"
#include <filesystem>
#include <iostream>
#include "../FileUtility.h"
#include "../KlemmBuild.h"

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

VCBuild::VCBuild()
{
	std::string VSLocation = GetVSLocation();
	std::string ToolsLocation = VSLocation + "\\VC\\Tools\\MSVC\\14.37.32822\\bin\\Hostx64\\x64\\";
	VCVarsLocation = "\"" + VSLocation + "\\VC\\Auxiliary\\Build\\vcvarsall.bat\"";
	ClLocation = "\"" + ToolsLocation + "cl.exe\"";
	LinkExeLocation = "\"" + ToolsLocation + "link.exe\"";
	BuildScript = std::ofstream("build.cmd");
	BuildScript << "call " << VCVarsLocation << " x64" << std::endl;
}

VCBuild::~VCBuild()
{
	BuildScript.close();
	std::filesystem::remove("build.cmd");
}

std::string VCBuild::Compile(std::string Source, BuildInfo* Build)
{
	BuildScript << "echo BUILD: COMPILE " << Source << std::endl;

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
	BuildScript << ClLocation
		+ " /nologo /showIncludes /c /std:c++20 /EHsc "
		+ OptString
		+ " "
		+ Source
		+ Includes 
		+ " /Fo:\"Build/"
		+ Source
		+ ".obj\"" << " || goto :error" << std::endl;
	return "Build/" + Source + ".obj";
}
std::string VCBuild::Link(std::vector<std::string> Sources, BuildInfo* Build)
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
			std::filesystem::copy(i + ".dll", Build->OutputPath + FileUtility::GetFilenameFromPath(i) + ".dll");
		}
	}

	BuildScript << "echo BUILD: LINK" << std::endl;

	std::string OutputPath = "./";
	if (!Build->OutputPath.empty())
	{
		OutputPath = Build->OutputPath;
		std::filesystem::create_directories(OutputPath);
	}

	switch (Build->TargetType)
	{
	case BuildInfo::BuildType::Executable:
		BuildScript << LinkExeLocation << AllObjs << " /nologo /std:c++20 /OUT:" << OutputPath << Build->TargetName << ".exe || goto :error" << std::endl;
		break;
	case BuildInfo::BuildType::DynamicLibrary:
		BuildScript << LinkExeLocation << " /nologo /DLL /OUT:" << OutputPath << Build->TargetName << ".dll" << AllObjs << " || goto :error" << std::endl;
		break;

	default:
		break;
	}
	BuildScript << "echo BUILD: DONE" << std::endl;
	BuildScript << "exit" << std::endl;
	BuildScript << ":error" << std::endl;
	BuildScript << "echo BUILD: COMPILED WITH ERROS" << std::endl;
	BuildScript << "echo BUILD: DONE" << std::endl;
	BuildScript << "exit 1" << std::endl;
	BuildScript.close();
	FILE* BuildProcess = _popen("cmd.exe /c build.cmd", "r");

	std::string CurrentLine;
	std::vector<std::string> FileDependencies;

	struct CompileTimeFile
	{
		std::string Path;
		std::string ObjPath;
		std::vector<std::string> ObjDeps;
	};

	std::vector<CompileTimeFile> Files;
	CompileTimeFile CurrentCompiledFile;

	std::ofstream OutLog = std::ofstream("Build/kbuild.log");
	OutLog << "KlemmBuild version " << KlemmBuild::Version << std::endl;
	OutLog << "Build log generated at " << std::chrono::system_clock::now() << std::endl;
	OutLog << "---------------------------------------------------------";

	std::vector<std::string> ErrorStrings;

	while (true)
	{
		char NewChar;
		fread(&NewChar, sizeof(char), 1, BuildProcess);
		OutLog << NewChar;
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
	Files.push_back(CurrentCompiledFile);

	
	for (const auto& i : Files)
	{
		std::filesystem::create_directories(FileUtility::RemoveFilename(i.ObjPath));
		std::ofstream DepsFile = std::ofstream(i.ObjPath + ".kdep");
		DepsFile << "Compile: " << FileUtility::GetFilenameFromPath(i.Path) + " : " + FileUtility::GetFilenameFromPath(i.ObjPath) << std::endl;
		for (const auto& j : i.ObjDeps)
		{
			DepsFile << "	Includes: " << j.substr(j.find_first_not_of(" ")) << std::endl;
		}
	}
	
	int Return = _pclose(BuildProcess);

	if (Return)
	{
		std::cout << "Build failed: " << std::endl;
		for (auto& i : ErrorStrings)
		{
			std::cout << i << std::endl;
		}

		return "";
	}

	std::filesystem::remove("build.cmd");
	BuildScript = std::ofstream("build.cmd");
	BuildScript << "call " << VCVarsLocation << " x64" << std::endl;

	switch (Build->TargetType)
	{
	case BuildInfo::BuildType::Executable:
		std::cout << "Project '" << Build->TargetName << "' -> " << Build->TargetName << ".exe" << std::endl;
		return Build->TargetName + ".exe";
		break;
	case BuildInfo::BuildType::DynamicLibrary:
		std::cout << "Project '" << Build->TargetName << "' -> " << Build->TargetName << ".dll" << std::endl;
		return Build->TargetName + ".lib";
	default:
		break;
	}
	return "";
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