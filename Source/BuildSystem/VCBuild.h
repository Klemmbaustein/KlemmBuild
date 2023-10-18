#if _WIN32
#pragma once
#include "BuildSystem.h"
#include <fstream>

#define WITH_VCBUILD 1


class VCBuild : public BuildSystem
{
public:
	VCBuild(bool UsedForBuild = true);
	~VCBuild();

	virtual std::string Compile(std::string Source, BuildInfo* Build) override;
	virtual bool Link(std::vector<std::string> Sources, BuildInfo* Build) override;
	virtual std::string PreprocessFile(std::string Source, std::vector<std::string> Definitions) override;
	virtual bool RequiresRebuild(std::string File) override;
	static std::string GetVSLocation();
protected:
	static void CompileThread(int Index);
	int CompileIndex = 0;
	std::string ClLocation;
	std::string LinkExeLocation;
	std::string VCVarsLocation;
	std::vector<std::ofstream> BuildScripts;
	static std::vector<int> BuildOutput;
};
#endif