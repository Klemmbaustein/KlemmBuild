#pragma once
#include <vector>
#include <string>

class BuildSystem;

/**
* @brief
* A type of target for the build system.
* 
* The target type of the target declared in a makefile is defined by the "type" property. The default type is BuildTarget
*/
class Target
{
public:

	std::string Name;

	struct Parameter
	{
		enum class Type
		{
			String,
			Path,
			Paths,
			Number,
			Boolean,
			Enum,
			StringList
		};

		std::string Name;

		Type ParameterType = Type::String;

		std::string Value;

		std::vector<std::string> ValidValues;
		std::vector<std::string> Aliases;

		Parameter(Type ParamType, std::string Name, std::string DefaultValue, std::vector<std::string> Aliases, std::vector<std::string> ValidValues = {});

		std::vector<std::string>& GetValues()
		{
			if (ParameterType != Type::Paths && ParameterType != Type::StringList)
			{
				exit(1);
			}
			return ValidValues;
		}
	};

	std::vector<Parameter> Parameters;

	Target();

	/**
	* @brief
	* Invokes the target to build.
	*/
	virtual void Invoke(BuildSystem* System) = 0;

	/**
	* @brief
	* Returns a parameter from the given name.
	* 
	* @param Name
	* The name of the parameter
	* 
	* @return
	* A parameter with a matching name or alias.
	*/
	Parameter* GetParameter(std::string Name);
};