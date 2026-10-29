#pragma once
#include "AvirA.hpp"
#include <string>
#include <vector>

namespace AvirA
{
	std::string ExeDir();
	std::string EnvVar(const char* Name);
	bool IsAbsolute(const std::string& Path);
	bool FileExists(const std::string& Path);
	bool DirExists(const std::string& Path);
	void CreateDirs(const std::string& Path);
	std::string CleanName(const std::string& Name);
	std::string GetFileName(const std::string& Path);
	std::string GetFolderName(const std::string& Path);
	bool ReadFile(const std::string& Path, std::string& Bytes);
	void ListFiles(const std::string& Dir, const std::string& Ext, bool Recurs, std::vector<std::string>& Out);
	void SortFiles(std::vector<std::string>& Out);
	bool ToolOnPath(const std::string& ExeName, std::string& Resolved);
}
