#pragma once
#include "AvirA.hpp"
#include <string>
#include <vector>

namespace AvirA
{
	std::vector<std::string> SteamLibraries();
	void PushTargetsFromRoot(std::vector<S_GameTarget>& Out, const std::string& Common);
	void PushTargetsAsGame(std::vector<S_GameTarget>& Out, const std::string& GameDir);
	std::vector<S_GameTarget> FindGames(const std::vector<std::string>& ExtraRoots);
	void DropDuplicates(std::vector<S_GameTarget>& Out);
	bool ResolveTargetPath(const std::string& Given, S_GameTarget& Out);
}
