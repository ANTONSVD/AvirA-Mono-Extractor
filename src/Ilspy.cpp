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

		bool RunQuiet(const std::string& CommandLine)
		{
			std::vector<char> buffer(CommandLine.begin(), CommandLine.end());
			buffer.push_back(0);
			SECURITY_ATTRIBUTES inherit = {};
			inherit.nLength = sizeof(inherit);
			inherit.bInheritHandle = TRUE;
			HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
			PROCESS_INFORMATION info = {};
			STARTUPINFOA setup = {};
			setup.cb = sizeof(setup);
			setup.dwFlags = STARTF_USESTDHANDLES;
			setup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			setup.hStdOutput = nul;
			setup.hStdError = nul;
			BOOL made = CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &setup, &info);
			DWORD code = 1;
			if (made)
			{
				WaitForSingleObject(info.hProcess, INFINITE);
				if (GetExitCodeProcess(info.hProcess, &code))
					made = code == 0;
				else
					made = FALSE;
				CloseHandle(info.hThread);
				CloseHandle(info.hProcess);
			}
			if (nul && nul != INVALID_HANDLE_VALUE)
				CloseHandle(nul);
			return made != FALSE;
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
		static const char* const Ladder[] =
		{
			nullptr,
			"10.1.1.8388",
			"9.1.0.7988",
			"9.0.0.7889",
			"8.2.0.7535"
		};
		for (int i = 0; i < 5; i++)
		{
			std::string command = "\"" + dotnet + "\" tool install -g ilspycmd -v minimal";
			if (Ladder[i])
				command += " --version " + std::string(Ladder[i]);
			if (RunQuiet(command) && CheckIlspy(std::string(), Resolved))
				return true;
			RunQuiet("\"" + dotnet + "\" tool uninstall -g ilspycmd -v minimal");
		}
		return false;
	}

	bool DecompileAssembly(const std::string& IlspyExe, const std::string& Dll, const std::string& OutDir)
	{
		std::string command = "\"" + IlspyExe + "\" -p -o \"" + OutDir + "\" \"" + Dll + "\"";
		CreateDirs(OutDir);
		return RunInherit(command);
	}
}
