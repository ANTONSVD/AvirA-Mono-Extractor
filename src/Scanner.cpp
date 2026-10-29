#include "Scanner.hpp"
#include "Util.hpp"
#include <Windows.h>
#include <cstring>

namespace AvirA
{
	namespace
	{
		void AddTargetForData(std::vector<S_GameTarget>& Out, const std::string& GameDir, const std::string& DataDir)
		{
			std::string managed = DataDir + "\\Managed";
			std::string assembly = managed + "\\Assembly-CSharp.dll";
			if (!FileExists(assembly))
				return;
			S_GameTarget target;
			target.M_Name = GetFolderName(GameDir);
			target.M_GameDir = GameDir;
			target.M_DataDir = DataDir;
			target.M_ManagedDir = managed;
			target.M_MainAssembly = assembly;
			Out.push_back(target);
		}
	}

	std::vector<std::string> SteamLibraries()
	{
		std::vector<std::string> out;
		char program_files[MAX_PATH] = {};
		if (GetEnvironmentVariableA("ProgramFiles(x86)", program_files, MAX_PATH) == 0)
			return out;
		std::string steam = std::string(program_files) + "\\Steam";
		out.push_back(steam);
		FILE* file = nullptr;
		std::string vdf = steam + "\\steamapps\\libraryfolders.vdf";
		if (fopen_s(&file, vdf.c_str(), "rb") != 0 || !file)
			return out;
		std::fseek(file, 0, SEEK_END);
		long size = std::ftell(file);
		std::fseek(file, 0, SEEK_SET);
		std::string text;
		if (size > 0 && size < 1000000)
		{
			text.resize((size_t)size);
			size_t got = std::fread(&text[0], 1, text.size(), file);
			text.resize(got);
		}
		std::fclose(file);
		size_t at = 0;
		while ((at = text.find("\"path\"", at)) != std::string::npos)
		{
			at += 6;
			size_t first = text.find('"', at);
			if (first == std::string::npos)
				break;
			size_t second = text.find('"', first + 1);
			if (second == std::string::npos)
				break;
			std::string lib = text.substr(first + 1, second - first - 1);
			std::string unescaped;
			for (size_t i = 0; i < lib.size(); i++)
			{
				if (lib[i] == '\\' && i + 1 < lib.size() && lib[i + 1] == '\\')
				{
					unescaped.push_back('\\');
					i++;
				}
				else
					unescaped.push_back(lib[i]);
			}
			bool known = false;
			for (size_t i = 0; i < out.size(); i++)
			{
				if (_stricmp(out[i].c_str(), unescaped.c_str()) == 0)
				{
					known = true;
					break;
				}
			}
			if (!unescaped.empty() && !known)
				out.push_back(unescaped);
			at = second + 1;
		}
		return out;
	}

	void PushTargetsFromRoot(std::vector<S_GameTarget>& Out, const std::string& Common)
	{
		WIN32_FIND_DATAA found = {};
		HANDLE handle = FindFirstFileA((Common + "\\*").c_str(), &found);
		if (handle == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			std::string dir = found.cFileName;
			if (dir == "." || dir == "..")
				continue;
			PushTargetsAsGame(Out, Common + "\\" + dir);
		} while (FindNextFileA(handle, &found));
		FindClose(handle);
	}

	void PushTargetsAsGame(std::vector<S_GameTarget>& Out, const std::string& GameDir)
	{
		WIN32_FIND_DATAA found = {};
		HANDLE handle = FindFirstFileA((GameDir + "\\*").c_str(), &found);
		if (handle == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			std::string dir = found.cFileName;
			if (dir.size() < 5)
				continue;
			std::string tail = dir.substr(dir.size() - 5);
			if (_stricmp(tail.c_str(), "_Data") != 0)
				continue;
			AddTargetForData(Out, GameDir, GameDir + "\\" + dir);
		} while (FindNextFileA(handle, &found));
		FindClose(handle);
	}

	std::vector<S_GameTarget> FindGames(const std::vector<std::string>& ExtraRoots)
	{
		std::vector<S_GameTarget> out;
		std::vector<std::string> libraries = SteamLibraries();
		for (size_t l = 0; l < libraries.size(); l++)
			PushTargetsFromRoot(out, libraries[l] + "\\steamapps\\common");
		for (size_t r = 0; r < ExtraRoots.size(); r++)
		{
			size_t before = out.size();
			PushTargetsFromRoot(out, ExtraRoots[r]);
			if (out.size() == before)
				PushTargetsAsGame(out, ExtraRoots[r]);
		}
		DropDuplicates(out);
		return out;
	}

	void DropDuplicates(std::vector<S_GameTarget>& Out)
	{
		std::vector<S_GameTarget> out;
		for (size_t i = 0; i < Out.size(); i++)
		{
			bool known = false;
			for (size_t j = 0; j < out.size(); j++)
			{
				if (_stricmp(out[j].M_MainAssembly.c_str(), Out[i].M_MainAssembly.c_str()) == 0)
				{
					known = true;
					break;
				}
			}
			if (!known)
				out.push_back(Out[i]);
		}
		Out = out;
	}

	bool ResolveTargetPath(const std::string& Given, S_GameTarget& Out)
	{
		if (FileExists(Given))
		{
			if (Given.size() > 4 && _stricmp(Given.substr(Given.size() - 4).c_str(), ".dll") == 0)
			{
				std::string folder = Given.substr(0, Given.find_last_of("\\/"));
				std::string parent = folder;
				size_t slash = folder.find_last_of("\\/");
				if (slash != std::string::npos)
					parent = folder.substr(0, slash);
				Out = S_GameTarget();
				Out.M_GameDir = parent;
				Out.M_DataDir = folder;
				Out.M_ManagedDir = folder;
				Out.M_MainAssembly = Given;
				Out.M_Name = GetFolderName(parent);
				return true;
			}
			return false;
		}
		if (!DirExists(Given))
			return false;
		std::string managed = Given;
		if (FileExists(Given + "\\Assembly-CSharp.dll"))
			managed = Given;
		else if (DirExists(Given + "\\Managed") && FileExists(Given + "\\Managed\\Assembly-CSharp.dll"))
			managed = Given + "\\Managed";
		else
			return false;
		std::string dataDir = managed;
		size_t slash = managed.find_last_of("\\/");
		if (slash != std::string::npos)
			dataDir = managed.substr(0, slash);
		std::string gameDir = dataDir;
		size_t slash2 = dataDir.find_last_of("\\/");
		if (slash2 != std::string::npos)
			gameDir = dataDir.substr(0, slash2);
		Out = S_GameTarget();
		Out.M_GameDir = gameDir;
		Out.M_DataDir = dataDir;
		Out.M_ManagedDir = managed;
		Out.M_MainAssembly = managed + "\\Assembly-CSharp.dll";
		Out.M_Name = GetFolderName(gameDir);
		return FileExists(Out.M_MainAssembly);
	}
}
