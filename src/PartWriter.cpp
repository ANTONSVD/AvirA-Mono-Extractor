#include "PartWriter.hpp"
#include <Windows.h>
#include <string>

namespace AvirA
{
	bool C_PartWriter::Initialize(const std::string& OutDir, const std::string& BaseName, u32 LimitKb)
	{
		if (LimitKb == 0)
			LimitKb = AVIRA_EXTRACTOR_PART_KB;
		M_OutDir = OutDir;
		M_BaseName = BaseName;
		M_Limit = (u64)LimitKb * 1024ull;
		return NextPart();
	}

	void C_PartWriter::WriteBytes(const std::string& Bytes)
	{
		if (!M_File || Bytes.empty())
			return;
		size_t written = std::fwrite(Bytes.data(), 1, Bytes.size(), M_File);
		M_Size += written;
		M_Total += written;
	}

	void C_PartWriter::WriteHeader(const std::string& SourcePath)
	{
		std::string rule(40, '=');
		std::string lead = M_Size == 0 ? std::string() : "\n";
		WriteBytes(lead + rule + "\n\tFILE: " + SourcePath + "\n" + rule + "\n\n");
		M_Sources++;
	}

	bool C_PartWriter::NextPart()
	{
		if (M_File)
		{
			std::fclose(M_File);
			M_File = nullptr;
		}
		M_Index++;
		char index[32] = {};
		std::snprintf(index, sizeof(index), "%llu", (unsigned long long)M_Index);
		M_CurrentPath = M_OutDir + "\\" + M_BaseName + "_Part_" + index + ".cs";
		if (fopen_s(&M_File, M_CurrentPath.c_str(), "wb") != 0 || !M_File)
			return false;
		M_Size = 0;
		return true;
	}

	bool C_PartWriter::Add(const std::string& SourcePath, const std::string& Bytes)
	{
		if (!M_File)
			return false;
		WriteHeader(SourcePath);
		size_t at = 0;
		while (at < Bytes.size())
		{
			if (M_Size >= M_Limit)
			{
				if (!NextPart())
					return false;
			}
			u64 free_space = M_Limit - M_Size;
			size_t left = Bytes.size() - at;
			if (left <= free_space)
			{
				WriteBytes(Bytes.substr(at, left));
				at = Bytes.size();
				continue;
			}
			if (free_space < 8192)
			{
				if (!NextPart())
					return false;
				continue;
			}
			WriteBytes(Bytes.substr(at, (size_t)free_space));
			at += (size_t)free_space;
		}
		WriteBytes("\n");
		return true;
	}

	void C_PartWriter::Finish()
	{
		if (M_File)
		{
			std::fclose(M_File);
			M_File = nullptr;
		}
		if (M_Total == 0 && !M_CurrentPath.empty())
			DeleteFileA(M_CurrentPath.c_str());
	}

	u64 C_PartWriter::GetParts() const
	{
		return M_Index;
	}

	u64 C_PartWriter::GetSources() const
	{
		return M_Sources;
	}

	u64 C_PartWriter::GetBytes() const
	{
		return M_Total;
	}
}
