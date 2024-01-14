#include "BuildTarget.h"
#include "../KlemmBuild.h"

BuildTarget::BuildTarget()
{
	Parameters.push_back(Parameter(Parameter::Type::Paths, "sources", "", { "files", "src"}, {}));
	Parameters.push_back(Parameter(Parameter::Type::Paths, "includes", "", { "inc" }));
	Parameters.push_back(Parameter(Parameter::Type::StringList, "libraries", "", { "libs", "lib"}));
	Parameters.push_back(Parameter(Parameter::Type::Path, "outputPath", "", { "out", "output" }));
	Parameters.push_back(Parameter(Parameter::Type::Enum, "configuration", "executable", { "config" }, {"executable", "staticLibrary", "sharedLibrary"}));
	Parameters.push_back(Parameter(Parameter::Type::Enum, "optimization", "none", { "opt", "optimisation" }, { "none", "smallest", "fastest" }));
	Parameters.push_back(Parameter(Parameter::Type::Boolean, "debug", "0", {}));
	Parameters.push_back(Parameter(Parameter::Type::StringList, "defines", "", { "defs", "definitions"}));
}

void BuildTarget::Invoke(BuildSystem* System)
{
	std::vector<std::string> ObjectFiles;

	for (const auto& i : GetParameter("sources")->GetValues())
	{
		ObjectFiles.push_back(System->Compile(i, this));
	}

	if (!System->Link(ObjectFiles, this))
	{
		KlemmBuild::Exit();
	}
}
