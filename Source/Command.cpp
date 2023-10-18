#include "Command.h"
#include <iostream>

static bool EchoCommands = true;

void SetEchoCommands(bool NewEchoCommands)
{
	EchoCommands = NewEchoCommands;
}

int ExecuteCommand(std::string Command)
{
	return system(Command.c_str());
}
