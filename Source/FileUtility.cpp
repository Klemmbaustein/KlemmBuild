#include "FileUtility.h"

std::string FileUtility::GetFilenameFromPath(const std::string& Path)
{
	return Path.substr(Path.find_last_of("/\\") + 1);
}

std::string FileUtility::RemoveFilename(const std::string& Path)
{
	size_t Index = Path.find_last_of("/\\");
	if (Index == std::string::npos)
	{
		return "";
	}
	return Path.substr(0, Index);
}
