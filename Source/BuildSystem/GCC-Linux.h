#pragma once
#include "BuildSystem.h"
#include <thread>

class GCC_Linux : public BuildSystem
{
public:
	virtual std::string Compile(std::string Source, BuildInfo* Build) override;
	virtual bool Link(std::vector<std::string> Sources, BuildInfo* Build) override;
	virtual std::string PreprocessFile(std::string Source, std::vector<std::string> Definitions) override;
	virtual bool RequiresRebuild(std::string File, BuildInfo* Info) override;

	void CompileAll(BuildInfo* Build);

	std::vector<std::string> CompileFiles;
	std::vector<std::thread*> BuildThreads;

	static void BuildThread(std::vector<std::string> Files, BuildInfo* Build);
	static std::atomic<size_t> BuiltFiles;
	static std::atomic<size_t> AllFiles;
};
