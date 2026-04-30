#include "LuaScriptSubsystem.h"

#include "LuaBindings.h"
#include "Core/Log.h"

void FLuaScriptSubsystem::Initialize()
{
	if (bInitialized)
	{
		return;
	}
	Lua.open_libraries(sol::lib::base);
	RegisterLuaBindings(Lua);
	bInitialized = true;
	UE_LOG("[Lua] Lua Scripting Initialized.");
}

// Lua 상태를 새 빈 상태로 바꿔서 기존 Lua 환경을 정리
void FLuaScriptSubsystem::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}
	Lua = sol::state();
	bInitialized = false;
	UE_LOG("[Lua] Lua Scripting Shutdown.");
}

// 문자열로 들어온 Lua 코드를 실행
bool FLuaScriptSubsystem::ExecuteString(const FString& Code)
{
	if (!bInitialized)
	{
		UE_LOG("[Lua] Lua Scripting Not Initialized.");
		return false;
	}
	
	sol::protected_function_result Result = Lua.safe_script(Code.c_str(), sol::script_pass_on_error);
	
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[Lua] Lua Scripting Error: %s", Error.what());
		return false;
	}
	
	return true;
}

