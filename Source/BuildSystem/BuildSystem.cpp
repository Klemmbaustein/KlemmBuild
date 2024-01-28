#include "BuildSystem.h"
#include "../Format.h"

std::string BuildSystem::GetCompileCommand(Target* Build, std::string Compiler, std::string Args, std::string File)
{
	return Format::FormatString(Build->GetParameter("compileFormat")->Value,
		{
			Format::FormatArg("compiler", Compiler),
			Format::FormatArg("args", Args),
			Format::FormatArg("file", File),
		});
}

std::string BuildSystem::GetLinkCommand(Target* Build, std::string Linker, std::string Args, std::vector<std::string> Files)
{
	std::string FilesString;

	for (auto& i : Files)
	{
		FilesString.append(" " + i + " ");
	}

	return Format::FormatString(Build->GetParameter("linkFormat")->Value,
		{
			Format::FormatArg("link", Linker),
			Format::FormatArg("args", Args),
			Format::FormatArg("files", FilesString),
		});
}
