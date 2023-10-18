#pragma once
#include <string>

namespace FileUtility
{
	std::string GetFilenameFromPath(const std::string& Path);
	std::string RemoveFilename(const std::string& Path);
	std::string GetFilenameWithoutExtensionFromPath(const std::string& Path);
}