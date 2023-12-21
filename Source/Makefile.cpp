#include "Makefile.h"
#include <nlohmann/json.hpp>
#include "BuildSystem/VCBuild.h"
#include "BuildSystem/GCC-Linux.h"
#include <iostream>
using namespace nlohmann;
Makefile Makefile::ReadMakefile(std::string File, std::vector<std::string> Defines)
{
#if _WIN32
	VCBuild Build = VCBuild(false);
#else
	GCC_Linux Build = GCC_Linux();
#endif
	try
	{
		json MakefileJson = json::parse(Build.PreprocessFile(File, Defines));

		std::vector<BuildInfo*> Projects;

		for (const auto& i : MakefileJson.at("projects"))
		{
			BuildInfo* NewProject = new BuildInfo();
			NewProject->TargetName = i.at("name");


			if (i.contains("preBuildCommand"))
			{
				NewProject->PreBuildCommand = i.at("preBuildCommand");
			}

			if (i.contains("dependencies"))
			{
				for (const auto& dep : i.at("dependencies"))
				{
					NewProject->Dependencies.push_back(dep);
				}
			}

			if (i.contains("command"))
			{
				NewProject->BuildCommand = i.at("command");
				Projects.push_back(NewProject);
				continue;
			}

			if (i.contains("makefile"))
			{
				NewProject->IsMakefile = true;
				NewProject->MakefilePath = i.at("makefile");
				Projects.push_back(NewProject);
				continue;
			}

			for (const auto& f : i.at("sources"))
			{
				std::string FileString = f;
				size_t LastAsterisk = FileString.find_last_of("*");
				if (LastAsterisk == std::string::npos)
				{
					NewProject->CompiledFiles.push_back(FileString);
				}
				else
				{
					if (std::filesystem::is_directory(FileString.substr(0, LastAsterisk)))
					{
						std::string TargetExtension = FileString.substr(LastAsterisk);
						if (!TargetExtension.empty())
						{
							TargetExtension = TargetExtension.substr(1);
						}
						for (const auto& entry 
							: std::filesystem::recursive_directory_iterator(FileString.substr(0, LastAsterisk)))
						{
							std::string EntryString = entry.path().string();
							if (entry.is_directory())
							{
								continue;
							}
							size_t DotIndex = EntryString.find_last_of(".");

							std::replace(EntryString.begin(), EntryString.end(), '\\', '/'); // replace all 'x' to 'y'

							if (TargetExtension.empty())
							{
								NewProject->CompiledFiles.push_back(EntryString);
							}
							else if (DotIndex != std::string::npos && EntryString.substr(DotIndex) == TargetExtension)
							{
								NewProject->CompiledFiles.push_back(EntryString);
							}
						}
					}
					else
					{
						std::cout << FileString.substr(0, LastAsterisk) << " is not a directory" << std::endl;
						return {};
					}
				}
			}

			if (i.contains("includes"))
			{
				for (const auto& inc : i.at("includes"))
				{
					NewProject->IncludePaths.push_back(inc);
				}
			}
			if (i.contains("outputPath"))
			{
				NewProject->OutputPath = i.at("outputPath");;
			}
			if (i.contains("libs"))
			{
				for (const auto& lib : i.at("libs"))
				{
					NewProject->Libraries.push_back(lib);
				}
			}
			if (i.contains("defines"))
			{
				for (const auto& def : i.at("defines"))
				{
					NewProject->PreProcessorDefinitions.push_back(def);
				}
			}
			if (i.contains("type"))
			{
				if (i.at("type") == "executable")
				{
					NewProject->TargetType = BuildInfo::BuildType::Executable;
				}
				else if (i.at("type") == "dynamic_lib")
				{
					NewProject->TargetType = BuildInfo::BuildType::DynamicLibrary;
				}
				else if (i.at("type") == "static_lib")
				{
					NewProject->TargetType = BuildInfo::BuildType::StaticLibrary;
				}
			}

			if (i.contains("optimization"))
			{
				if (i.at("optimization") == "none")
				{
					NewProject->TargetOpt = BuildInfo::OptimizationType::None;
				}
				else if (i.at("optimization") == "fast")
				{
					NewProject->TargetOpt = BuildInfo::OptimizationType::Fastest;
				}
				else if (i.at("optimization") == "small")
				{
					NewProject->TargetOpt = BuildInfo::OptimizationType::Smallest;
				}
			}

			if (i.contains("debug"))
			{
				NewProject->GenerateDebugInfo = i.at("debug");
			}

			Projects.push_back(NewProject);
		}

		Makefile m;
		m.Projects = Projects;
		if (MakefileJson.contains("defaultProject"))
		{
			for (size_t i = 0; i < m.Projects.size(); i++)
			{
				if (Projects[i]->TargetName == MakefileJson.at("defaultProject").get<std::string>())
				{
					m.DefaultProject = i;
				}
			}
		}
		return m;
	}
	catch (json::exception& e)
	{
		std::cout << "Error: " << e.what() << std::endl;
		std::cout << Build.PreprocessFile(File, Defines) << std::endl;
	}
	return {};
}
