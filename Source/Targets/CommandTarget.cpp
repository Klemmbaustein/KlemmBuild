#include "CommandTarget.h"
#include "../BuildSystem/BuildSystem.h"
#include <iostream>
#include "../KlemmBuild.h"

CommandTarget::CommandTarget()
{
	Parameters.push_back(Parameter(Parameter::Type::String, "command", "", { "cmd" }));
}

void CommandTarget::Invoke(BuildSystem* System)
{
	Parameter* Command = GetParameter("command");
	if (Command == nullptr)
	{
		std::cout << "Error: Target type is 'command' but the 'command' parameter has not been set." << std::endl;
		KlemmBuild::Exit();
		return;
	}

	System->ShellExecute(Command->Value);
}
