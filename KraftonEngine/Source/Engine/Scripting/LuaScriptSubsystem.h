#pragma once

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUAJIT 1

#include <sol/sol.hpp>

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class FLuaScriptSubsystem : public TSingleton<FLuaScriptSubsystem>
{
	friend class TSingleton<FLuaScriptSubsystem>;

public:
	void Initialize();
	void Shutdown();

	sol::state& GetState()
	{
		return Lua;
	}

	bool ExecuteString(const FString& Code);

private:
	FLuaScriptSubsystem() = default;

private:
	sol::state Lua;
	bool bInitialized = false;
};