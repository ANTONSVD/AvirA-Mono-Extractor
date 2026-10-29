#pragma once
#include "AvirA.hpp"
#include <cstdio>
#include <string>

namespace AvirA
{
	class C_PartWriter
	{
	public:
		bool Initialize(const std::string& OutDir, const std::string& BaseName, u32 LimitKb);
		bool Add(const std::string& SourcePath, const std::string& Bytes);
		void Finish();
		u64 GetParts() const;
		u64 GetSources() const;
		u64 GetBytes() const;

	private:
		FILE* M_File = nullptr;
		std::string M_OutDir;
		std::string M_BaseName;
		std::string M_CurrentPath;
		u64 M_Limit = 0;
		u64 M_Index = 0;
		u64 M_Size = 0;
		u64 M_Sources = 0;
		u64 M_Total = 0;

		bool NextPart();
		void WriteBytes(const std::string& Bytes);
		void WriteHeader(const std::string& SourcePath);
	};
}
