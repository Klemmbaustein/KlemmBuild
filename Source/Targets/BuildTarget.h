#pragma once
#include "Target.h"
#include "../BuildSystem/BuildSystem.h"

/**
* @brief
* Target responsible for building C++ sources to an executable or library.
*/
class BuildTarget : public Target
{
public:
	BuildTarget();

	virtual void Invoke(BuildSystem* System) override;
};