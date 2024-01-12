#pragma once
#include "Target.h"

class CommandTarget : public Target
{
public:
	CommandTarget();

	virtual void Invoke(BuildSystem* System) override;
};