#if _WIN32
#pragma once
#include "BuildSystem.h"
#include <fstream>

#define WITH_VCBUILD 1

class VCBuild : public BuildSystem
{
public:
	VCBuild();

	virtual std::string Compile(std::string Source, BuildInfo* Build) override;
	virtual std::string Link(std::vector<std::string> Sources, BuildInfo* Build) override;
	virtual std::string PreprocessFile(std::string Source, std::vector<std::string> Definitions) override;
	static std::string GetVSLocation();
protected:
	std::string ClLocation;
	std::string LinkExeLocation;
	std::string VCVarsLocation;
	std::ofstream BuildScript;
};
#endif