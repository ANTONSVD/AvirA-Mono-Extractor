#include "Ilspy.hpp"
#include "Util.hpp"
#include <Windows.h>
#include <vector>
#include <cstring>

namespace AvirA
{
	namespace
	{
		bool IsExePath(const std::string& Path)
		{
			if (Path.size() > 4 && _stricmp(Path.substr(Path.size() - 4).c_str(), ".exe") == 0)
				return true;
			if (Path.size() > 4 && _stricmp(Path.substr(Path.size() - 4).c_str(), ".cmd") == 0)
				return true;
			if (Path.size() > 4 && _stricmp(Path.substr(Path.size() - 4).c_str(), ".bat") == 0)
				return true;
			return false;
		}

		bool RunInherit(const std::string& CommandLine)
		{
			std::vector<char> buffer(CommandLine.begin(), CommandLine.end());
			buffer.push_back(0);
			PROCESS_INFORMATION info = {};
			STARTUPINFOA setup = {};
			setup.cb = sizeof(setup);
			BOOL made = CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &setup, &info);
			if (!made)
				return false;
			WaitForSingleObject(info.hProcess, INFINITE);
			DWORD code = 1;
			GetExitCodeProcess(info.hProcess, &code);
			CloseHandle(info.hThread);
			CloseHandle(info.hProcess);
			return code == 0;
		}
	}

	bool CheckIlspy(const std::string& Hint, std::string& Resolved)
	{
		std::string candidates[4];
		candidates[0] = Hint;
		candidates[1] = ExeDir() + "\\Tools\\ilspycmd.exe";
		candidates[2] = EnvVar("USERPROFILE") + "\\.dotnet\\tools\\ilspycmd.exe";
		candidates[3] = EnvVar("LOCALAPPDATA") + "\\Microsoft\\dotnet\\tools\\ilspycmd.exe";
		for (int i = 0; i < 4; i++)
		{
			if (!IsExePath(candidates[i]))
				continue;
			if (!FileExists(candidates[i]))
				continue;
			Resolved = candidates[i];
			return true;
		}
		if (ToolOnPath("ilspycmd.exe", Resolved))
			return true;
		return false;
	}

	bool DotnetOnPath(std::string& Resolved)
	{
		if (ToolOnPath("dotnet.exe", Resolved))
			return true;
		std::string program_files = EnvVar("ProgramFiles");
		std::string candidate = program_files + "\\dotnet\\dotnet.exe";
		if (FileExists(candidate))
		{
			Resolved = candidate;
			return true;
		}
		return false;
	}

	bool InstallIlspy(std::string& Resolved)
	{
		std::string dotnet;
		if (!DotnetOnPath(dotnet))
			return false;
		if (!RunInherit("\"" + dotnet + "\" tool install -g ilspycmd -v minimal"))
			return false;
		return CheckIlspy(std::string(), Resolved);
	}

	bool DecompileAssembly(const std::string& IlspyExe, const std::string& Dll, const std::string& OutDir)
	{
		std::string command = "\"" + IlspyExe + "\" -p -o \"" + OutDir + "\" \"" + Dll + "\"";
		CreateDirs(OutDir);
		return RunInherit(command);
	}
}
