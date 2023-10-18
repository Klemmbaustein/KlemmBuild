#pragma once
#include <vector>
#include <string>

struct BuildInfo
{
public:
	enum class OptimizationType
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

	bool IsMakefile = false;
	std::string MakefilePath;
	std::string OutputPath;

	std::vector<std::string> CompiledFiles;
	std::vector<std::string> IncludePaths;
	std::vector<std::string> Libraries;
	std::vector<std::string> Dependencies;


	OptimizationType TargetOpt = OptimizationType::None;
	BuildType TargetType = BuildType::Executable;
	std::string TargetName;
};