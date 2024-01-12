#include "Makefile.h"
#include <nlohmann/json.hpp>
#include "BuildSystem/VCBuild.h"
#include "BuildSystem/GCC-Linux.h"
#include <iostream>
#include <optional>
#include "Targets/BuildTarget.h"
#include "Targets/MakefileTarget.h"
#include "Targets/CommandTarget.h"
#include "KlemmBuild.h"
using namespace nlohmann;

static std::optional<json> SafeGet(const json& from, std::string Field)
{
	if (!from.contains(Field))
	{
		return std::optional<json>();
	}

	return from.at(Field);
}

static json Get(const json& from, std::string Field)
{
	if (!from.contains(Field))
	{
		std::cout << "Could not find expected paramter " << Field << std::endl;
		KlemmBuild::Exit();
	}

	return from.at(Field);
}

static void VerifyPath(std::string Path, std::string Name, bool CreateIfMissing)
{
	if (!std::filesystem::exists(Path))
	{
		if (CreateIfMissing)
		{
			std::filesystem::create_directories(Path);
			return;
		}
		std::cout << "The given path for " << Name << " does not exist: '" << Path << "'" << std::endl;
		KlemmBuild::Exit();
	}
}

static void AssignValue(json Value, Target::Parameter* Param)
{
	switch (Param->ParameterType)
	{	
	case Target::Parameter::Type::String:
		Param->Value = Value;
		break;
	case Target::Parameter::Type::Path:
		Param->Value = Value;
		VerifyPath(Param->Value, Param->Name, true);
		break;
	case Target::Parameter::Type::StringList:
		for (const auto& i : Value)
		{
			Param->GetValues().push_back(i);
		}
		break;
	case Target::Parameter::Type::Boolean:
		Param->Value = Value.get<bool>() ? "1" : "0";
		break;
	case Target::Parameter::Type::Enum:
		for (const auto& i : Param->ValidValues)
		{
			if (i == Value.get<std::string>())
			{
				Param->Value = i;
				return;
			}
		}
		std::cout << "Invalid option for " << Param->Name << ": " << Value << std::endl;
		KlemmBuild::Exit();
		break;
	case Target::Parameter::Type::Number:
		Param->Value = std::to_string(Value.get<int>());
		break;
	case Target::Parameter::Type::Paths:
		for (const auto& i : Value)
		{
			std::string FileString = i;
			size_t LastAsterisk = FileString.find_last_of("*");
			if (LastAsterisk == std::string::npos)
			{
				VerifyPath(FileString, Param->Name, false);
				Param->GetValues().push_back(FileString);
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

						std::replace(EntryString.begin(), EntryString.end(), '\\', '/');

						if (TargetExtension.empty())
						{
							Param->GetValues().push_back(EntryString);
						}
						else if (DotIndex != std::string::npos && EntryString.substr(DotIndex) == TargetExtension)
						{
							Param->GetValues().push_back(EntryString);
						}
					}
				}
				else
				{
					std::cout << FileString.substr(0, LastAsterisk) << " is not a directory" << std::endl;
					KlemmBuild::Exit();
				}
			}
		}

		break;
	default:
		break;
	}
}

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

		std::vector<Target*> Targets;

		for (const auto& t : Get(MakefileJson, "targets"))
		{
			Target* NewProject = nullptr;
			
			std::string Type = SafeGet(t, "type").value_or("build");

			if (Type == "build")
			{
				NewProject = new BuildTarget();
			}
			if (Type == "makefile")
			{
				NewProject = new MakefileTarget();
			}
			if (Type == "command")
			{
				NewProject = new CommandTarget();
			}

			if (NewProject == nullptr)
			{
				std::cout << "Unknown target type: " + Type << std::endl;
				KlemmBuild::Exit();
				return {};
			}
			
			for (auto& i : t.items())
			{
				if (i.key() == "name")
				{
					NewProject->Name = i.value();
					continue;
				}
				if (i.key() == "type")
				{
					continue;
				}
				auto* Param = NewProject->GetParameter(i.key());
				if (Param == nullptr)
				{
					std::cout << "Unknown argument: " + i.key() << std::endl;
					KlemmBuild::Exit();
				}
				AssignValue(i.value(), Param);
			}


			Targets.push_back(NewProject);
		}

		Makefile m;
		m.Targets = Targets;
		if (MakefileJson.contains("defaultTarget"))
		{
			for (size_t i = 0; i < m.Targets.size(); i++)
			{
				if (Targets[i]->Name == MakefileJson.at("defaultTarget").get<std::string>())
				{
					m.DefaultTarget = i;
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
