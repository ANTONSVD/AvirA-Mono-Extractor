#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define AVIRA_EXTRACTOR_NAME "AvirA-Mono-Extractor"
#define AVIRA_EXTRACTOR_VERSION "1.0.0"
#define AVIRA_EXTRACTOR_PART_KB 560u

namespace AvirA
{
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;

	class C_Log
	{
	public:
		static void Ok(const std::string& Text)
		{
			std::printf("[+] %s\n", Text.c_str());
		}

		static void Info(const std::string& Text)
		{
			std::printf("[*] %s\n", Text.c_str());
		}

		static void Fail(const std::string& Text)
		{
			std::printf("[x] %s\n", Text.c_str());
		}

		static void Line(const std::string& Text)
		{
			std::printf("%s\n", Text.c_str());
		}
	};

	struct S_GameTarget
	{
		std::string M_Name;
		std::string M_GameDir;
		std::string M_DataDir;
		std::string M_ManagedDir;
		std::string M_MainAssembly;
	};
}
