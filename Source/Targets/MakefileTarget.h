#pragma once
#include "Target.h"

class MakefileTarget : public Target
{
public:
	MakefileTarget();

	virtual void Invoke(BuildSystem* System) override;
};