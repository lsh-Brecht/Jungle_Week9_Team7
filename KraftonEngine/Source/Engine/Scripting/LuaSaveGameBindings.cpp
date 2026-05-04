#include "LuaBindings.h"
#include "SolInclude.h"

#include "Core/CoreTypes.h"
#include "Core/Log.h"
#include "Engine/Platform/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
	std::wstring GetScoreSavePath()
	{
		FPaths::CreateDir(FPaths::SaveDir());
		return FPaths::Combine(FPaths::SaveDir(), L"Score.sav");
	}

	int32 ParseBestScoreText(const std::string& Text)
	{
		// Supported formats:
		//   BestScore=123
		//   { "BestScore": 123 }
		//   123
		std::size_t Start = Text.find("BestScore");
		if (Start == std::string::npos)
		{
			Start = 0;
		}

		while (Start < Text.size() && !std::isdigit(static_cast<unsigned char>(Text[Start])) && Text[Start] != '-')
		{
			++Start;
		}

		if (Start >= Text.size())
		{
			return 0;
		}

		std::size_t End = Start;
		if (Text[End] == '-')
		{
			++End;
		}
		while (End < Text.size() && std::isdigit(static_cast<unsigned char>(Text[End])))
		{
			++End;
		}

		try
		{
			return (std::max)(0, std::stoi(Text.substr(Start, End - Start)));
		}
		catch (...)
		{
			return 0;
		}
	}
}

void RegisterSaveGameBinding(sol::state& Lua)
{
	sol::table SaveGame = Lua.get_or("SaveGame", Lua.create_table());
	Lua["SaveGame"] = SaveGame;

	SaveGame.set_function("GetScoreSavePath", []() -> std::string
	{
		return FPaths::ToUtf8(GetScoreSavePath());
	});

	SaveGame.set_function("LoadBestScore", []() -> int32
	{
		const std::wstring Path = GetScoreSavePath();
		std::ifstream File(Path, std::ios::binary);
		if (!File)
		{
			return 0;
		}

		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		const int32 BestScore = ParseBestScoreText(Buffer.str());
		UE_LOG("[SaveGame] LoadBestScore: %d (%s)", BestScore, FPaths::ToUtf8(Path).c_str());
		return BestScore;
	});

	SaveGame.set_function("SaveBestScore", [](int32 BestScore) -> bool
	{
		BestScore = (std::max)(0, BestScore);
		const std::wstring Path = GetScoreSavePath();

		std::ofstream File(Path, std::ios::binary | std::ios::trunc);
		if (!File)
		{
			UE_LOG("[SaveGame] SaveBestScore failed: %s", FPaths::ToUtf8(Path).c_str());
			return false;
		}

		File << "BestScore=" << BestScore << "\n";
		const bool bOk = File.good();
		UE_LOG("[SaveGame] SaveBestScore: %d (%s)", BestScore, FPaths::ToUtf8(Path).c_str());
		return bOk;
	});
}
