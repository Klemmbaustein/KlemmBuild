#pragma once
#include "../Build.h"

class BuildSystem
{
public:
	virtual ~BuildSystem() {}
	/*
	* Compiles the file 'Source' with the BuildInfo 'Build'. Returns a path to the new object file.
	*/
	virtual std::string Compile(std::string Source, BuildInfo* Build) = 0;

	virtual std::string Link(std::vector<std::string> Sources, BuildInfo* Build) = 0;

	virtual std::string PreprocessFile(std::string Source, std::vector<std::string> Definitions) = 0;
};