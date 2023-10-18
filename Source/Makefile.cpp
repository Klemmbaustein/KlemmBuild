#include "Makefile.h"
#include <nlohmann/json.hpp>
#include "BuildSystem/VCBuild.h"
#include <iostream>
using namespace nlohmann;

std::vector<BuildInfo*> Makefile::ReadMakefile(std::string File)
{
#if WITH_VCBUILD
	VCBuild Build;
#endif
	try
	{
		json MakefileJson = json::parse(Build.PreprocessFile(File, {}));

		std::vector<BuildInfo*> Projects;

		for (const auto& i : MakefileJson.at("projects"))
		{
			BuildInfo* NewProject = new BuildInfo();
			NewProject->TargetName = i.at("name");
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
						for (const auto& entry : std::filesystem::recursive_directory_iterator(FileString.substr(0, LastAsterisk)))
						{
							std::string EntryString = entry.path().string();
							if (entry.is_directory())
							{
								continue;
							}
							size_t DotIndex = EntryString.find_last_of(".");

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

			Projects.push_back(NewProject);
		}
		return Projects;
	}
	catch (json::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
	return {};
}
