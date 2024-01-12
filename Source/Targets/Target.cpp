#include "Target.h"

Target::Target()
{
	Parameters.push_back(Parameter(Parameter::Type::StringList, "dependencies", "", {"deps"}));
}

Target::Parameter* Target::GetParameter(std::string Name)
{
	for (Parameter& p : Parameters)
	{
		if (p.Name == Name)
		{
			return &p;
		}

		for (const auto& i : p.Aliases)
		{
			if (i == Name)
			{
				return &p;
			}
		}
	}

	return nullptr;
}

Target::Parameter::Parameter(Type ParamType, std::string Name, std::string DefaultValue, std::vector<std::string> Aliases, std::vector<std::string> ValidValues)
{
	this->Name = Name;
	this->ValidValues = ValidValues;
	this->Aliases = Aliases;
	ParameterType = ParamType;
	Value = DefaultValue;
}
