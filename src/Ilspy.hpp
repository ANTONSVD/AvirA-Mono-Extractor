#pragma once
#include "AvirA.hpp"
#include <string>

namespace AvirA
{
	bool CheckIlspy(const std::string& Hint, std::string& Resolved);
	bool DotnetOnPath(std::string& Resolved);
	bool InstallIlspy(std::string& Resolved);
	bool DecompileAssembly(const std::string& IlspyExe, const std::string& Dll, const std::string& OutDir);
}
