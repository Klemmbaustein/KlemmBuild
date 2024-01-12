#if _WIN32
#pragma once
#include "BuildSystem.h"
#include <fstream>
#include <atomic>

#define WITH_VCBUILD 1


class VCBuild : public BuildSystem
{
public:
	VCBuild(bool UsedForBuild = true);
	~VCBuild();

	virtual std::string Compile(std::string Source, Target* Build) override;
	virtual bool Link(std::vector<std::string> Sources, Target* Build) override;
	virtual std::string PreprocessFile(std::string Source, std::vector<std::string> Definitions) override;
	virtual bool RequiresRebuild(std::string File, Target* Info) override;
	virtual int ShellExecute(std::string cmd) override;
	static std::string GetVSLocation();
protected:
	static void CompileThread(int Index, Target* Info);
	int CompileIndex = 0;
	std::string ClLocation;
	std::vector<std::ofstream> BuildScripts;
	static std::vector<int> BuildOutput;
	static std::atomic<size_t> BuiltFiles;
	static std::atomic<size_t> AllFiles;
};
#endif
