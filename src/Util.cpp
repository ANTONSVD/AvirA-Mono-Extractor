#include "Util.hpp"
#include <Windows.h>
#include <algorithm>
#include <cstring>

namespace AvirA
{
	std::string ExeDir()
	{
		char path[MAX_PATH] = {};
		DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
		std::string dir(path, path + length);
		size_t slash = dir.find_last_of("\\/");
		if (slash != std::string::npos)
			dir = dir.substr(0, slash);
		if (dir.empty())
			dir = ".";
		return dir;
	}

	std::string EnvVar(const char* Name)
	{
		char buffer[MAX_PATH] = {};
		DWORD got = GetEnvironmentVariableA(Name, buffer, MAX_PATH);
		if (!got)
			return std::string();
		return std::string(buffer, got);
	}

	bool IsAbsolute(const std::string& Path)
	{
		if (Path.size() >= 2 && Path[1] == ':')
			return true;
		if (Path.size() >= 2 && (Path[0] == '\\' || Path[0] == '/') && (Path[1] == '\\' || Path[1] == '/'))
			return true;
		if (!Path.empty() && (Path[0] == '\\' || Path[0] == '/'))
			return true;
		return false;
	}

	bool FileExists(const std::string& Path)
	{
		DWORD attrs = GetFileAttributesA(Path.c_str());
		return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool DirExists(const std::string& Path)
	{
		DWORD attrs = GetFileAttributesA(Path.c_str());
		return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
	}

	void CreateDirs(const std::string& Path)
	{
		std::string part;
		for (size_t i = 0; i <= Path.size(); i++)
		{
			if (i == Path.size() || Path[i] == '\\' || Path[i] == '/')
			{
				if (!part.empty() && part.back() != ':')
					CreateDirectoryA(part.c_str(), nullptr);
			}
			if (i < Path.size())
				part.push_back(Path[i]);
		}
	}

	std::string CleanName(const std::string& Name)
	{
		std::string out;
		for (size_t i = 0; i < Name.size(); i++)
		{
			char c = Name[i];
			bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
				|| c == ' ' || c == '.' || c == '-' || c == '_' || c == '(' || c == ')';
			out.push_back(ok ? c : '_');
		}
		while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
			out.pop_back();
		if (out.empty())
			out = "Game";
		return out;
	}

	std::string GetFileName(const std::string& Path)
	{
		size_t slash = Path.find_last_of("\\/");
		if (slash == std::string::npos)
			return Path;
		return Path.substr(slash + 1);
	}

	std::string GetFolderName(const std::string& Path)
	{
		std::string clean = Path;
		while (!clean.empty() && (clean.back() == '\\' || clean.back() == '/'))
			clean.pop_back();
		size_t slash = clean.find_last_of("\\/");
		if (slash == std::string::npos)
			return clean;
		return clean.substr(slash + 1);
	}

	bool ReadFile(const std::string& Path, std::string& Bytes)
	{
		FILE* file = nullptr;
		if (fopen_s(&file, Path.c_str(), "rb") != 0 || !file)
			return false;
		std::fseek(file, 0, SEEK_END);
		long size = std::ftell(file);
		std::fseek(file, 0, SEEK_SET);
		if (size <= 0)
		{
			std::fclose(file);
			return false;
		}
		Bytes.resize((size_t)size);
		size_t got = std::fread(&Bytes[0], 1, Bytes.size(), file);
		std::fclose(file);
		Bytes.resize(got);
		return !Bytes.empty();
	}

	void ListFiles(const std::string& Dir, const std::string& Ext, bool Recurs, std::vector<std::string>& Out)
	{
		WIN32_FIND_DATAA found = {};
		HANDLE handle = FindFirstFileA((Dir + "\\*").c_str(), &found);
		if (handle == INVALID_HANDLE_VALUE)
			return;
		do
		{
			std::string name = found.cFileName;
			if (name == "." || name == "..")
				continue;
			std::string full = Dir + "\\" + name;
			if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (Recurs)
					ListFiles(full, Ext, Recurs, Out);
				continue;
			}
			if (Ext.empty() || name.size() < Ext.size())
				continue;
			std::string tail = name.substr(name.size() - Ext.size());
			if (_stricmp(tail.c_str(), Ext.c_str()) != 0)
				continue;
			Out.push_back(full);
		} while (FindNextFileA(handle, &found));
		FindClose(handle);
	}

	void SortFiles(std::vector<std::string>& Out)
	{
		std::sort(Out.begin(), Out.end(), [](const std::string& A, const std::string& B)
		{
			return _stricmp(A.c_str(), B.c_str()) < 0;
		});
	}

	bool ToolOnPath(const std::string& ExeName, std::string& Resolved)
	{
		std::string path = EnvVar("PATH");
		size_t at = 0;
		while (at <= path.size())
		{
			size_t next = path.find(';', at);
			std::string piece = path.substr(at, next == std::string::npos ? std::string::npos : next - at);
			if (!piece.empty())
			{
				while (!piece.empty() && (piece.back() == '"' || piece.back() == ' '))
					piece.pop_back();
				if (!piece.empty() && (piece.front() != '"' || piece.size() > 1))
				{
					if (piece.front() == '"')
						piece = piece.substr(1);
				}
				std::string candidate = piece + "\\" + ExeName;
				if (FileExists(candidate))
				{
					Resolved = candidate;
					return true;
				}
			}
			if (next == std::string::npos)
				break;
			at = next + 1;
		}
		return false;
	}
}
