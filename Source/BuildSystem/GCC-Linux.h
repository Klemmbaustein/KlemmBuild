#pragma once
#include "BuildSystem.h"
#include <thread>

class GCC_Linux : public BuildSystem
{
public:
	virtual std::string Compile(std::string Source, Target* Build) override;
	virtual bool Link(std::vector<std::string> Sources, Target* Build) override;
	virtual std::string PreprocessFile(std::string Source, std::vector<std::string> Definitions) override;
	virtual bool RequiresRebuild(std::string File, Target* Info) override;

	void CompileAll(Target* Build);

	std::vector<std::string> CompileFiles;
	std::vector<std::thread*> BuildThreads;

	static void BuildThread(std::vector<std::string> Files, Target* Build);
	static std::atomic<size_t> BuiltFiles;
	static std::atomic<size_t> AllFiles;
};
