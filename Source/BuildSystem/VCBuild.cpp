#if _WIN32
#include "VCBuild.h"
#include <filesystem>
#include <iostream>
#include "../FileUtility.h"
#include "../KlemmBuild.h"
#include "../Format.h"
#include <thread>

std::vector<int> VCBuild::BuildOutput;
std::atomic<size_t> VCBuild::BuiltFiles;
std::atomic<size_t> VCBuild::AllFiles;

struct CompileTimeFile
{
	std::string Path;
	std::string ObjPath;
	std::vector<std::string> ObjDeps;
};

bool VCBuild::RequiresRebuild(std::string File, Target* Info)
{
	std::string ObjectFile = "Build/" + Info->Name + "/" + File + ".obj";
	if (!std::filesystem::exists(ObjectFile))
	{
		return true;
	}

	if (std::filesystem::last_write_time(ObjectFile) < std::filesystem::last_write_time(File))
	{
		return true;
	}
	std::string DepsFilePath = "Build/" + Info->Name + "/" + File + ".obj.kdep";
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
	out << cmd << std::endl;
	out << "exit /k %errorlevel%";
	out.close();
	int ret = system("Build\\cmd\\cmd.cmd");
	std::filesystem::remove("Build\\cmd\\cmd.cmd");
	return ret;
}

void VCBuild::CompileThread(int Index, Target* Info)
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
				CurrentCompiledFile.ObjPath = "Build/" + Info->Name + "/" + CurrentLine.substr(15) + ".obj";
				CurrentCompiledFile.ObjDeps.clear();
				KlemmBuild::PrintMutex.lock();
				std::cout << "- [" << (size_t)(((float)BuiltFiles / (float)AllFiles) * 100.0f) << "%] Compiling " + CurrentLine.substr(15) << std::endl;
				KlemmBuild::PrintMutex.unlock();
				BuiltFiles++;
			}
			else if (CurrentLine.find("error") != std::string::npos)
			{
				ErrorStrings.push_back(CurrentLine);
			}
			else if (CurrentLine == "BUILD: DONE")
			{
				break;
			}
			// cl.exe prints the file name of the compiled file. Ignore that.
			else if (FileUtility::GetExtension(CurrentLine) != "cpp")
			{
				std::cout << CurrentLine << std::endl;
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

std::string VCBuild::Compile(std::string Source, Target* Build)
{
	if (RequiresRebuild(Source, Build))
	{
		std::filesystem::create_directories(Build->GetParameter("outputPath")->Value);
		int CompileThreadIndex = CompileIndex++ % KlemmBuild::BuildThreads;
		BuildScripts[CompileThreadIndex] << "echo BUILD: COMPILE " << Source << std::endl;

		std::string Includes = "";
		for (const auto& i : Build->GetParameter("includes")->GetValues())
		{
			Includes.append(" /I\"" + i + "\" ");
		}

		std::string Args;
		std::string Optimization = Build->GetParameter("optimization")->Value;

		if (Optimization == "none")
			Args.append("/Od");
		else if (Optimization == "smallest")
			Args.append("/O2");
		else if (Optimization == "fastest")
			Args.append("/O1");

		std::string PreprocessorString;
		for (auto& i : Build->GetParameter("defines")->GetValues())
		{
			PreprocessorString.append(" /D " + i + " ");
		}

		if (Build->GetParameter("debug")->Value == "1")
		{
			Args.append(" /DEBUG /Zi /Fd:" + Build->GetParameter("outputPath")->Value + "/" + Build->Name + ".pdb ");
		}

		std::string LanguageVersion = Build->GetParameter("languageVersion")->Value;
		bool WithoutU8Char = Build->GetParameter("u8char")->Value == "0";

		if (LanguageVersion == "latest")
		{
			LanguageVersion = "c++latest";
			if (WithoutU8Char)
			{
				Args.append(" /Zc:char8_t- ");
			}
		}
		else if (WithoutU8Char && std::stoi(LanguageVersion.substr(LanguageVersion.size() - 2)) >= 20)
		{
			Args.append(" /Zc:char8_t- ");
		}
		LanguageVersion = " /std:" + LanguageVersion;


		std::string Command = GetCompileCommand(Build, "cl.exe", Args
			+ " "
			+ LanguageVersion
			+ PreprocessorString
			+ Includes
			+ " /FS /nologo /showIncludes /D KLEMMBUILD /MD /c /permissive- /Zc:preprocessor /EHsc "
			+ " /Fo:\"Build/"
			+ Build->Name
			+ "/"
			+ Source
			+ ".obj\"",
			Source);
		std::filesystem::create_directories(FileUtility::RemoveFilename("Build/" + Build->Name + "/" + Source));
		BuildScripts[CompileThreadIndex]
			<< Command
			<< " || goto :error" << std::endl;
		AllFiles++;
	}
	return "Build/" + Build->Name + "/" + Source + ".obj";
}
bool VCBuild::Link(std::vector<std::string> Sources, Target* Build)
{
	std::string OutPath = Build->GetParameter("outputPath")->Value;

	std::string Libs;

	for (const auto& i : Build->GetParameter("libraries")->GetValues())
	{
		Libs.append(" " + i + ".lib ");
		if (std::filesystem::exists(i + ".dll") && !std::filesystem::is_directory(i + ".dll"))
		{
			std::string ToLib = OutPath + "/" + FileUtility::GetFilenameFromPath(i) + ".dll";
			try
			{
				std::filesystem::copy(i + ".dll",
					ToLib,
					std::filesystem::copy_options::overwrite_existing);
			}
			catch (std::exception)
			{
			}
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
	if (!OutPath.empty())
	{
		OutputPath = OutPath;
		if (OutputPath[OutputPath.size() - 1] != '/')
		{
			OutputPath.append("/");
		}
		std::filesystem::create_directories(OutputPath);
	}

	std::string OutputFile = OutputPath + Build->Name;

	std::string VCCoreDeps =
		"\"kernel32.lib\" "
		"\"user32.lib\" "
		"\"gdi32.lib\" "
		"\"winspool.lib\" "
		"\"comdlg32.lib\" "
		"\"advapi32.lib\" "
		"\"shell32.lib\" "
		"\"ole32.lib\" "
		"\"oleaut32.lib\" "
		"\"uuid.lib\" "
		"\"odbc32.lib\" "
		"\"odbccp32.lib\" ";

	if (OutPath.empty())
	{
		OutPath = ".";
	}

	std::string Args;
	if (Build->GetParameter("debug")->Value == "1")
	{
		Args.append(" /DEBUG /Zi /FS /Fd:" + OutPath + "/" + Build->Name + ".pdb ");
	}

	std::string Config = Build->GetParameter("configuration")->Value;

	if (Config == "executable")
	{
		BuildScripts[KlemmBuild::BuildThreads]
			<< GetLinkCommand(Build, "link.exe", VCCoreDeps
				+ " /nologo /out:"
				+ OutputPath
				+ Build->Name
				+ ".exe "
				+ Libs
				+ VCCoreDeps
				+ Args, Sources)
			<< " || goto :error" << std::endl;
		OutputFile.append(".exe");
	}
	else if (Config == "sharedLibrary")
	{
		BuildScripts[KlemmBuild::BuildThreads]
			<< GetLinkCommand(Build, "link.exe",
				" /nologo /dll "
				+ Args
				+ Libs
				+ " /out:"
				+ OutputPath
				+ Build->Name
				+ ".dll",
				Sources)
			<< " || goto :error" << std::endl;
		OutputFile.append(".dll");
	}
	else if (Config == "staticLibrary")
	{
		BuildScripts[KlemmBuild::BuildThreads]
			<< GetLinkCommand(Build, "lib.exe",
				" /nologo /out:"
				+ OutputPath
				+ Build->Name
				+ ".lib",
				Sources)
			<< " || goto :error" << std::endl;
		OutputFile.append(".lib");
	}
	else
	{
		KlemmBuild::Exit();
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
		for (const auto& i : Build->GetParameter("libraries")->GetValues())
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
	BuildScripts[KlemmBuild::BuildThreads] << "echo BUILD: DONE" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads] << "exit 1" << std::endl;
	BuildScripts[KlemmBuild::BuildThreads].close();

	if (!Relink)
	{
		std::cout << "Target '" << Build->Name << "' is up to date - skipping" << std::endl;
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
			else if (CurrentLine.find("error") != std::string::npos)
			{
				ErrorStrings.push_back(CurrentLine);
			}
			else if (CurrentLine.find("warning") != std::string::npos)
			{
				ErrorStrings.push_back(CurrentLine);
			}
			else if (CurrentLine == "BUILD: DONE")
			{
				break;
			}
			else if (CurrentLine == "BUILD: LINK")
			{
				std::cout << "- [100%] Linking..." << std::endl;
			}
			else
			{
				std::cout << CurrentLine << std::endl;
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

	std::cout << "Target '" << Build->Name << "' -> " << OutputFile << std::endl;

	return true;
}

std::string VCBuild::PreprocessFile(std::string Source, std::vector<std::string> Definitions)
{
	std::string DefinitionString;
	for (auto& i : Definitions)
	{
		DefinitionString.append(" /D " + i + " ");
	}

	FILE* Preprocessor = _popen((ClLocation 
		+ " /c /EP /nologo /D MSVC_WINDOWS "
		+ DefinitionString
		+ Source
		+ " && echo $END || echo $END").c_str(), "r");

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