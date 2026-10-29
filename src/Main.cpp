#include "AvirA.hpp"
#include "Scanner.hpp"
#include "Ilspy.hpp"
#include "PartWriter.hpp"
#include "Util.hpp"
#include <Windows.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

using namespace AvirA;

namespace
{
	struct S_Options
	{
		std::string M_Given;
		std::string M_GameFilter;
		std::string M_OutCustom;
		std::string M_IlspyHint;
		std::vector<std::string> M_Roots;
		std::vector<std::string> M_Asm;
		u32 M_PartKb = AVIRA_EXTRACTOR_PART_KB;
		bool M_OutSet = false;
		bool M_Fresh = false;
	};

	void Usage()
	{
		AvirA::C_Log::Line("Usage:");
		AvirA::C_Log::Line("  AviraMonoExtractor.exe <Assembly-CSharp.dll or Managed/Data folder> [options]");
		AvirA::C_Log::Line("  AviraMonoExtractor.exe [options]          scan Steam libraries and pick a game");
		AvirA::C_Log::Line("Options:");
		AvirA::C_Log::Line("  -o,  --out <dir>      output root, default out\\");
		AvirA::C_Log::Line("  -g,  --game <name>    filter scanned games by name");
		AvirA::C_Log::Line("  -r,  --root <dir>     extra scan root, repeatable");
		AvirA::C_Log::Line("  -a,  --asm <file>     extra Managed\\*.dll to decompile, repeatable");
		AvirA::C_Log::Line("  -i,  --ilspy <path>   path to ilspycmd.exe");
		AvirA::C_Log::Line("  -p,  --partsize <kb>  part file limit in KB, default " + std::to_string(AVIRA_EXTRACTOR_PART_KB));
		AvirA::C_Log::Line("  -f,  --fresh          ignore cache, decompile everything again");
	}

	bool FilterOkay(const std::string& Name, const std::string& Filter)
	{
		if (Filter.empty())
			return true;
		return Name.find(Filter) != std::string::npos;
	}

	bool ParseArgs(int argc, char** argv, S_Options& Options)
	{
		for (int i = 1; i < argc; i++)
		{
			std::string arg = argv[i];
			if ((arg == "-o" || arg == "--out") && i + 1 < argc)
			{
				Options.M_OutCustom = argv[++i];
				Options.M_OutSet = true;
			}
			else if ((arg == "-g" || arg == "--game") && i + 1 < argc)
				Options.M_GameFilter = argv[++i];
			else if ((arg == "-r" || arg == "--root") && i + 1 < argc)
				Options.M_Roots.push_back(argv[++i]);
			else if ((arg == "-a" || arg == "--asm") && i + 1 < argc)
				Options.M_Asm.push_back(argv[++i]);
			else if ((arg == "-i" || arg == "--ilspy") && i + 1 < argc)
				Options.M_IlspyHint = argv[++i];
			else if ((arg == "-p" || arg == "--partsize") && i + 1 < argc)
				Options.M_PartKb = (u32)std::atol(argv[++i]);
			else if (arg == "-f" || arg == "--fresh")
				Options.M_Fresh = true;
			else if (arg == "-h" || arg == "--help")
			{
				Usage();
				return false;
			}
			else if (Options.M_Given.empty())
				Options.M_Given = arg;
			else
			{
				AvirA::C_Log::Fail("Unexpected argument: " + arg);
				Usage();
				return false;
			}
		}
		if (Options.M_PartKb == 0 || Options.M_PartKb > 64 * 1024)
			Options.M_PartKb = AVIRA_EXTRACTOR_PART_KB;
		return true;
	}

	bool PickIlspy(const S_Options& Options, std::string& IlspyExe)
	{
		if (CheckIlspy(Options.M_IlspyHint, IlspyExe))
			return true;
		std::string dotnet;
		if (!DotnetOnPath(dotnet))
		{
			AvirA::C_Log::Fail("ilspycmd not found and dotnet.exe is not on PATH.");
			AvirA::C_Log::Line("    Install it yourself: dotnet tool install -g ilspycmd");
			AvirA::C_Log::Line("    Or pass the exe:     --ilspy <path to ilspycmd.exe>");
			return false;
		}
		AvirA::C_Log::Info("ilspycmd not found.");
		std::printf("Install it now via \"dotnet tool install -g ilspycmd\"? [default Y / n]: ");
		std::fflush(stdout);
		char answer[16] = {};
		if (std::fgets(answer, sizeof(answer), stdin))
		{
			std::string reply = answer;
			if (!reply.empty() && (reply[0] == 'n' || reply[0] == 'N'))
			{
				AvirA::C_Log::Fail("No decompiler. Install ilspycmd or pass --ilspy.");
				return false;
			}
		}
		AvirA::C_Log::Info("Installing ilspycmd, this needs the internet...");
		if (!InstallIlspy(IlspyExe))
		{
			AvirA::C_Log::Fail("Install failed.");
			return false;
		}
		AvirA::C_Log::Ok("ilspycmd installed: " + IlspyExe);
		return true;
	}

	bool BuildAssemblyList(const S_GameTarget& Target, const S_Options& Options, std::vector<std::string>& Out)
	{
		Out.push_back(Target.M_MainAssembly);
		for (size_t i = 0; i < Options.M_Asm.size(); i++)
		{
			std::string raw = Options.M_Asm[i];
			std::string path = raw;
			if (FileExists(raw))
				path = raw;
			else if (raw.find('\\') == std::string::npos && raw.find('/') == std::string::npos)
				path = Target.M_ManagedDir + "\\" + raw;
			if (!FileExists(path))
			{
				AvirA::C_Log::Fail("Assembly not found: " + raw);
				return false;
			}
			Out.push_back(path);
		}
		return true;
	}

	std::string ProjectDirOf(const std::string& ExtractDir, const std::string& Dll)
	{
		std::string assemblyName = GetFileName(Dll);
		return ExtractDir + "\\Ilspy\\" + assemblyName.substr(0, assemblyName.size() - 4);
	}

	bool StampLine(const std::string& Path, std::string& Line)
	{
		WIN32_FIND_DATAA found = {};
		HANDLE handle = FindFirstFileA(Path.c_str(), &found);
		if (handle == INVALID_HANDLE_VALUE)
			return false;
		FindClose(handle);
		ULARGE_INTEGER size;
		size.HighPart = found.nFileSizeHigh;
		size.LowPart = found.nFileSizeLow;
		ULARGE_INTEGER stamp;
		stamp.HighPart = found.ftLastWriteTime.dwHighDateTime;
		stamp.LowPart = found.ftLastWriteTime.dwLowDateTime;
		Line = "Asm=" + GetFileName(Path) + "|"
			+ std::to_string((unsigned long long)size.QuadPart) + "|"
			+ std::to_string((unsigned long long)stamp.QuadPart);
		return true;
	}

	std::vector<std::string> ReadManifest(const std::string& Path)
	{
		std::vector<std::string> out;
		std::string bytes;
		if (!ReadFile(Path, bytes))
			return out;
		std::string piece;
		for (size_t i = 0; i <= bytes.size(); i++)
		{
			char c = i < bytes.size() ? bytes[i] : '\n';
			if (c == '\n' || c == '\r')
			{
				if (!piece.empty())
					out.push_back(piece);
				piece.clear();
				continue;
			}
			piece.push_back(c);
		}
		return out;
	}

	bool HasLine(const std::vector<std::string>& Lines, const std::string& Line)
	{
		for (size_t i = 0; i < Lines.size(); i++)
		{
			if (_stricmp(Lines[i].c_str(), Line.c_str()) == 0)
				return true;
		}
		return false;
	}

	void WriteManifest(const std::string& Path, const std::string& Text)
	{
		FILE* file = nullptr;
		if (fopen_s(&file, Path.c_str(), "wb") != 0 || !file)
			return;
		std::fwrite(Text.data(), 1, Text.size(), file);
		std::fclose(file);
	}

	bool PartsExist(const std::string& ExtractDir, const std::string& Name)
	{
		WIN32_FIND_DATAA found = {};
		HANDLE handle = FindFirstFileA((ExtractDir + "\\" + Name + "_Part_*.cs").c_str(), &found);
		bool ok = handle != INVALID_HANDLE_VALUE;
		if (ok)
			FindClose(handle);
		return ok;
	}
}

namespace
{
	bool RunGame(const S_GameTarget& TargetOriginal, const S_Options& Options)
	{
		const S_GameTarget Target = TargetOriginal;
		std::string Name = CleanName(Target.M_Name);
		AvirA::C_Log::Ok("Game: " + Target.M_Name + " | " + Target.M_ManagedDir);

		std::string OutRoot;
		if (!Options.M_OutSet)
			OutRoot = ExeDir();
		else if (IsAbsolute(Options.M_OutCustom))
			OutRoot = Options.M_OutCustom;
		else
		{
			char cwd[MAX_PATH] = {};
			GetCurrentDirectoryA(MAX_PATH, cwd);
			OutRoot = std::string(cwd) + "\\" + Options.M_OutCustom;
		}
		std::string ExtractDir = OutRoot + "\\Extract_" + Name;
		CreateDirs(ExtractDir);

		std::vector<std::string> assemblies;
		if (!BuildAssemblyList(Target, Options, assemblies))
			return false;

		std::string ManifestPath = ExtractDir + "\\Manifest.txt";
		std::vector<std::string> knownLines;
		if (!Options.M_Fresh)
			knownLines = ReadManifest(ManifestPath);
		std::string rootLine = "Root=" + Target.M_ManagedDir;
		if (!HasLine(knownLines, rootLine))
			knownLines.clear();
		std::string partsLine = "Parts=" + std::to_string((unsigned long long)Options.M_PartKb);
		bool partsKnown = HasLine(knownLines, partsLine);

		std::vector<std::string> projectDirs;
		std::vector<std::string> newManifestLines;
		newManifestLines.push_back(rootLine);
		newManifestLines.push_back(partsLine);
		bool decompiledAny = false;
		std::string IlspyExe;
		for (size_t i = 0; i < assemblies.size(); i++)
		{
			std::string line;
			if (!StampLine(assemblies[i], line))
			{
				AvirA::C_Log::Fail("Missing assembly: " + assemblies[i]);
				return false;
			}
			std::string projectDir = ProjectDirOf(ExtractDir, assemblies[i]);
			if (HasLine(knownLines, line) && DirExists(projectDir))
			{
				projectDirs.push_back(projectDir);
				newManifestLines.push_back(line);
				continue;
			}
			if (!decompiledAny && !PickIlspy(Options, IlspyExe))
				return false;
			decompiledAny = true;
			std::string assemblyName = GetFileName(assemblies[i]);
			AvirA::C_Log::Info("Decompiling " + assemblyName + " ...");
			if (!DecompileAssembly(IlspyExe, assemblies[i], projectDir))
			{
				AvirA::C_Log::Fail("ilspycmd returned an error on " + assemblyName);
				return false;
			}
			AvirA::C_Log::Ok("Decompiled: " + projectDir);
			projectDirs.push_back(projectDir);
			newManifestLines.push_back(line);
		}

		if (!decompiledAny && partsKnown && PartsExist(ExtractDir, Name))
		{
			AvirA::C_Log::Ok("Nothing changed in " + Target.M_Name + ", closing.");
			AvirA::C_Log::Ok("Output folder: " + ExtractDir);
			return true;
		}

		std::vector<std::string> sources;
		for (size_t i = 0; i < projectDirs.size(); i++)
			ListFiles(projectDirs[i], ".cs", true, sources);
		SortFiles(sources);
		if (sources.empty())
		{
			AvirA::C_Log::Fail("No .cs files appeared in the project output.");
			return false;
		}
		AvirA::C_Log::Info("Source files: " + std::to_string(sources.size()));

		C_PartWriter writer;
		if (!writer.Initialize(ExtractDir, Name, Options.M_PartKb))
		{
			AvirA::C_Log::Fail("Cannot create part file in " + ExtractDir);
			return false;
		}
		for (size_t i = 0; i < sources.size(); i++)
		{
			std::string bytes;
			if (!ReadFile(sources[i], bytes))
				continue;
			if (!writer.Add(sources[i], bytes))
			{
				AvirA::C_Log::Fail("Write error, parts are incomplete.");
				writer.Finish();
				return false;
			}
		}
		writer.Finish();

		std::string manifestText;
		for (size_t i = 0; i < newManifestLines.size(); i++)
			manifestText += newManifestLines[i] + "\n";
		WriteManifest(ManifestPath, manifestText);
		AvirA::C_Log::Ok("Manifest updated.");

		AvirA::C_Log::Ok("Parts: " + std::to_string(writer.GetParts()));
		AvirA::C_Log::Ok("Sources glued: " + std::to_string(writer.GetSources()));
		AvirA::C_Log::Ok("Glued bytes: " + std::to_string((unsigned long long)writer.GetBytes()));
		AvirA::C_Log::Ok("Output folder: " + ExtractDir);
		return true;
	}
	bool OwnsConsoleWindow()
	{
		HWND window = GetConsoleWindow();
		if (!window)
			return false;
		DWORD ids[8] = {};
		DWORD count = GetConsoleProcessList(ids, 8);
		return count <= 1;
	}

	void WaitForClose()
	{
		if (!OwnsConsoleWindow())
			return;
		Sleep(2000);
	}
}

int main(int argc, char** argv)
{
	std::printf("%s\n", AVIRA_EXTRACTOR_NAME);

	S_Options Options;
	if (!ParseArgs(argc, argv, Options))
		return 1;

	int Code = 1;
	if (!Options.M_Given.empty())
	{
		S_GameTarget target;
		if (!ResolveTargetPath(Options.M_Given, target))
		{
			AvirA::C_Log::Fail("That path does not look like a Mono game: " + Options.M_Given);
			AvirA::C_Log::Line("    Need Assembly-CSharp.dll, or \\*_Data\\Managed folder.");
			WaitForClose();
			return 1;
		}
		Code = RunGame(target, Options) ? 0 : 1;
		WaitForClose();
		return Code;
	}

	std::vector<S_GameTarget> games = FindGames(Options.M_Roots);
	if (!Options.M_GameFilter.empty())
	{
		std::vector<S_GameTarget> kept;
		for (size_t i = 0; i < games.size(); i++)
		{
			if (FilterOkay(games[i].M_Name, Options.M_GameFilter))
				kept.push_back(games[i]);
		}
		games = kept;
	}
	if (games.empty())
	{
		AvirA::C_Log::Fail("No Mono games found. Pass Assembly-CSharp.dll or a Managed folder as argument.");
		WaitForClose();
		return 1;
	}

	size_t pick = 0;
	if (games.size() == 1)
	{
		AvirA::C_Log::Info("Found game: " + games[0].M_Name);
	}
	else
	{
		AvirA::C_Log::Line("Found Mono games:");
		for (size_t i = 0; i < games.size(); i++)
			AvirA::C_Log::Line("  [" + std::to_string(i + 1) + "] " + games[i].M_Name + " (" + GetFolderName(games[i].M_DataDir) + ")");
		while (true)
		{
			std::printf("Select game [1-%llu] (default 1): ", (unsigned long long)games.size());
			std::fflush(stdout);
			char answer[32] = {};
			if (!std::fgets(answer, sizeof(answer), stdin))
				break;
			int number = std::atoi(answer);
			if (number >= 1 && (size_t)number <= games.size())
			{
				pick = (size_t)number - 1;
				break;
			}
			if (answer[0] == '\n' || answer[0] == '\r')
				break;
			if (answer[0] == 0)
				break;
			AvirA::C_Log::Fail("Bad number, try again.");
		}
	}
	Code = RunGame(games[pick], Options) ? 0 : 1;
	WaitForClose();
	return Code;
}
