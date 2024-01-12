#include "MakefileTarget.h"
#include "../KlemmBuild.h"
#include <iostream>

MakefileTarget::MakefileTarget()
{
	Parameters.push_back(Parameter(Parameter::Type::Path, "makefile", "", { "file" }));
}

void MakefileTarget::Invoke(BuildSystem* System)
{
	Parameter* Makefile = GetParameter("makefile");
	if (Makefile == nullptr)
	{
		std::cout << "Error: Target type is 'makefile' but the 'makefile' parameter has not been set." << std::endl;
		KlemmBuild::Exit();
		return;
	}

	KlemmBuild::BuildMakefile(Makefile->Value);
}
