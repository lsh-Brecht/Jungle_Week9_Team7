#pragma once

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUAJIT 1

#include "SolInclude.h"

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Platform/DirectoryWatcher.h"

class AActor;
struct FLuaGameObjectHandle;

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
	bool ExecuteFile(const FString& Path);

	bool BindActor(AActor* Actor, const FString& ScriptPath);
	void UnbindActor(const AActor* Actor);
	void CallActorBeginPlay(AActor* Actor);
	void CallActorTick(AActor* Actor, float DeltaTime);
	void CallActorEndPlay(AActor* Actor);
	void CallActorOverlap(AActor* Actor, AActor* OtherActor);

private:
	FLuaScriptSubsystem() = default;

	void RegisterScriptDirectoryWatcher(const FString& ScriptSubDirectory);
	void OnScriptsChanged(const TSet<FString>& ChangedFiles);
	void ConfigureLuaState();
	void ConfigureLuaState(sol::state& TargetLua);
	void RegisterRuntimeFunctions(sol::state& TargetLua);

	bool ExecuteFileInternal(const FString& Path, bool bTrackAsEntry);
	bool ExecuteEntryScript(sol::state_view LuaView, const FString& NormalizedPath, TMap<FString, TSet<FString>>& OutScriptIncludes, TMap<FString, FString>& OutModulePaths);
	bool ExecuteScriptFile(sol::state_view LuaView, const FString& NormalizedPath);
	bool CompileFile(sol::state_view LuaView, const FString& NormalizedPath);
	bool ReloadScriptsAtomically(const TSet<FString>& ReloadTargets);

	FString ResolveScriptPath(const FString& Path) const;
	FString ResolveModulePath(const FString& ModuleName) const;
	FString MakeAbsoluteScriptPath(const FString& NormalizedPath) const;
	void RecordIncludeDependency(const FString& IncludePath);
	void RebuildIncludeDependents();
	TMap<FString, FString>& GetActiveModulePaths();

	sol::object IncludeFile(const FString& Path, sol::this_state State);
	sol::object RequireModule(const FString& ModuleName, sol::this_state State);

	struct FLuaDependencyContext
	{
		FString EntryScript;
		TSet<FString>* Includes = nullptr;
		TMap<FString, FString>* ModulePaths = nullptr;
	};

	struct FLuaActorBinding
	{
		uint32 ActorUUID = 0;
		FString ScriptPath;
		sol::environment Environment;
		sol::function BeginPlay;
		sol::function Tick;
		sol::function EndPlay;
		sol::function OnOverlap;
	};

	FLuaActorBinding* FindActorBinding(uint32 ActorUUID);
	const FLuaActorBinding* FindActorBinding(uint32 ActorUUID) const;
	void StartCoroutine(const char* FunctionName, const sol::function& Function, uint32 OwnerUUID);
	void StartCoroutine(const char* FunctionName, const sol::function& Function, uint32 OwnerUUID, float DeltaTime);
	void StartCoroutine(const char* FunctionName, const sol::function& Function, uint32 OwnerUUID, const FLuaGameObjectHandle& OtherActor);
	sol::thread CreateCoroutineThread(const sol::function& Function);
	void ResumeCoroutine(sol::thread Thread, uint32 OwnerUUID, const FString& FunctionName, int ArgCount);
	void HandleCoroutineResult(int Status, sol::thread Thread, lua_State* ThreadState, uint32 OwnerUUID, const FString& FunctionName);
	void ScheduleCoroutineResume(float Delay, sol::thread Thread, uint32 OwnerUUID, const FString& FunctionName);
	float ExtractYieldDelay(lua_State* ThreadState) const;

private:
	sol::state Lua;
	bool bInitialized = false;
	TArray<FSubscriptionID> WatchSubs;
	TSet<FString> LoadedScripts;
	TArray<FString> LoadedScriptOrder;
	TMap<FString, TSet<FString>> ScriptIncludes;
	TMap<FString, TSet<FString>> IncludeDependents;
	TMap<FString, FString> ModulePaths;
	TArray<FLuaDependencyContext> DependencyContextStack;
	TMap<uint32, FLuaActorBinding> ActorBindings;
};
