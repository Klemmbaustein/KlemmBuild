#pragma once
#include <vector>
#include <string>

struct BuildInfo
{
public:
	enum class OptimisationLevel
	{
		None,
		Fastest,
		Smallest
	};

	enum class BuildType
	{
		Executable,
		DynamicLibrary,
		StaticLibrary
	};

	std::vector<std::string> CompiledFiles;
	std::vector<std::string> IncludePaths;


	BuildType TargetType = BuildType::Executable;
	std::string TargetName;
};