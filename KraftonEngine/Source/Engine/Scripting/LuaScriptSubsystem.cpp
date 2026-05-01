#include "LuaScriptSubsystem.h"

#include "LuaBindings.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Platform/Paths.h"
#include "Platform/DirectoryWatcher.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>

namespace
{
	FString ToGenericUtf8(const std::filesystem::path& Path)
	{
		return FPaths::ToUtf8(Path.generic_wstring());
	}

	FString NormalizeScriptPath(const std::filesystem::path& InPath)
	{
		std::filesystem::path Path = InPath;
		if (!Path.is_absolute())
		{
			Path = std::filesystem::path(FPaths::RootDir()) / Path;
		}

		Path = Path.lexically_normal();
		return ToGenericUtf8(Path);
	}

	bool IsLuaFile(const FString& Path)
	{
		std::filesystem::path FilePath(FPaths::ToWide(Path));
		std::wstring Extension = FilePath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(),
			[](wchar_t Ch) { return static_cast<wchar_t>(std::towlower(Ch)); });
		return Extension == L".lua";
	}
}

void FLuaScriptSubsystem::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	ConfigureLuaState();

	RegisterScriptDirectoryWatcher("Config/");
	RegisterScriptDirectoryWatcher("Game/");

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

	for (FSubscriptionID WatchSub : WatchSubs)
	{
		if (WatchSub != 0)
		{
			FDirectoryWatcher::Get().Unsubscribe(WatchSub);
		}
	}
	WatchSubs.clear();

	Lua = sol::state();
	LoadedScripts.clear();
	LoadedScriptOrder.clear();
	ScriptIncludes.clear();
	IncludeDependents.clear();
	ModulePaths.clear();
	DependencyContextStack.clear();
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

bool FLuaScriptSubsystem::ExecuteFile(const FString& Path)
{
	return ExecuteFileInternal(Path, true);
}

void FLuaScriptSubsystem::RegisterScriptDirectoryWatcher(const FString& ScriptSubDirectory)
{
	const std::wstring WatchDirectory = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptSubDirectory));
	FWatchID WatchID = FDirectoryWatcher::Get().Watch(WatchDirectory, ScriptSubDirectory);
	if (WatchID != 0)
	{
		WatchSubs.push_back(FDirectoryWatcher::Get().Subscribe(WatchID,
			[this](const TSet<FString>& Files) { OnScriptsChanged(Files); }));
	}
}

void FLuaScriptSubsystem::OnScriptsChanged(const TSet<FString>& ChangedFiles)
{
	TSet<FString> ReloadTargets;

	for (const FString& File : ChangedFiles)
	{
		if (!IsLuaFile(File))
		{
			continue;
		}

		bool bFoundTrackedTarget = false;
		const FString NormalizedPath = ResolveScriptPath(File);

		if (LoadedScripts.find(NormalizedPath) != LoadedScripts.end())
		{
			ReloadTargets.insert(NormalizedPath);
			bFoundTrackedTarget = true;
		}

		auto DependentIt = IncludeDependents.find(NormalizedPath);
		if (DependentIt != IncludeDependents.end())
		{
			for (const FString& DependentScript : DependentIt->second)
			{
				ReloadTargets.insert(DependentScript);
			}
			bFoundTrackedTarget = true;
		}

		if (!bFoundTrackedTarget)
		{
			ReloadTargets.insert(NormalizedPath);
		}
	}

	if (ReloadTargets.empty())
	{
		return;
	}

	UE_LOG("[LuaHotReload] Reloading %zu Lua script(s)...", ReloadTargets.size());
	if (ReloadScriptsAtomically(ReloadTargets))
	{
		for (const FString& Target : ReloadTargets)
		{
			UE_LOG("[LuaHotReload] OK: %s", Target.c_str());
			FNotificationManager::Get().AddNotification("Lua Reloaded: " + Target, ENotificationType::Success, 3.0f);
		}
	}
	else
	{
		UE_LOG("[LuaHotReload] FAILED (keeping previous Lua state)");
		FNotificationManager::Get().AddNotification("Lua Reload Failed: keeping previous state", ENotificationType::Error, 5.0f);
	}
}

void FLuaScriptSubsystem::ConfigureLuaState()
{
	ConfigureLuaState(Lua);
}

void FLuaScriptSubsystem::ConfigureLuaState(sol::state& TargetLua)
{
	TargetLua = sol::state();
	TargetLua.open_libraries(
		sol::lib::base,
		sol::lib::package,
		sol::lib::coroutine,
		sol::lib::math,
		sol::lib::table,
		sol::lib::string);

	RegisterLuaBindings(TargetLua);
	RegisterRuntimeFunctions(TargetLua);

	sol::table Package = TargetLua["package"];
	FString ExistingPath = Package["path"].get_or(FString());

	const FString ScriptDir = ToGenericUtf8(std::filesystem::path(FPaths::ScriptDir()).lexically_normal());
	const FString ScriptPathPrefix = ScriptDir.empty() || ScriptDir.back() == '/'
		? ScriptDir
		: ScriptDir + "/";

	Package["path"] = ScriptPathPrefix + "?.lua;" + ScriptPathPrefix + "?/init.lua;" + ExistingPath;
}

void FLuaScriptSubsystem::RegisterRuntimeFunctions(sol::state& TargetLua)
{
	TargetLua.set_function("Include",
		[this](const FString& Path, sol::this_state State) -> sol::object
		{
			return IncludeFile(Path, State);
		});

	TargetLua.set_function("LuaInclude",
		[this](const FString& Path, sol::this_state State) -> sol::object
		{
			return IncludeFile(Path, State);
		});

	TargetLua.set_function("dofile",
		[this](const FString& Path, sol::this_state State) -> sol::object
		{
			return IncludeFile(Path, State);
		});

	TargetLua.set_function("require",
		[this](const FString& ModuleName, sol::this_state State) -> sol::object
		{
			return RequireModule(ModuleName, State);
		});
}

bool FLuaScriptSubsystem::ExecuteFileInternal(const FString& Path, bool bTrackAsEntry)
{
	if (!bInitialized)
	{
		UE_LOG("[Lua] Lua Scripting Not Initialized.");
		return false;
	}

	const FString NormalizedPath = ResolveScriptPath(Path);

	if (!bTrackAsEntry)
	{
		if (!CompileFile(Lua, NormalizedPath))
		{
			return false;
		}
		return ExecuteScriptFile(Lua, NormalizedPath);
	}

	TSet<FString> ReloadTargets;
	ReloadTargets.insert(NormalizedPath);
	return ReloadScriptsAtomically(ReloadTargets);
}

bool FLuaScriptSubsystem::ExecuteEntryScript(sol::state_view LuaView, const FString& NormalizedPath, TMap<FString, TSet<FString>>& OutScriptIncludes, TMap<FString, FString>& OutModulePaths)
{
	if (!CompileFile(LuaView, NormalizedPath))
	{
		return false;
	}

	TSet<FString> NewIncludes;
	DependencyContextStack.push_back({ NormalizedPath, &NewIncludes, &OutModulePaths });
	const bool bSuccess = ExecuteScriptFile(LuaView, NormalizedPath);
	DependencyContextStack.pop_back();

	if (!bSuccess)
	{
		return false;
	}

	OutScriptIncludes[NormalizedPath] = std::move(NewIncludes);

	return true;
}

bool FLuaScriptSubsystem::ExecuteScriptFile(sol::state_view LuaView, const FString& NormalizedPath)
{
	const FString AbsolutePath = MakeAbsoluteScriptPath(NormalizedPath);

	std::ifstream File(FPaths::ToWide(AbsolutePath), std::ios::binary);
	if (!File)
	{
		UE_LOG("[Lua] Lua File Execution Error (%s): Cannot open file", NormalizedPath.c_str());
		return false;
	}
	const std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());

	sol::protected_function_result Result = LuaView.safe_script(Content, sol::script_pass_on_error, "@" + AbsolutePath);

	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[Lua] Lua File Execution Error (%s): %s", NormalizedPath.c_str(), Error.what());
		return false;
	}

	return true;
}

bool FLuaScriptSubsystem::CompileFile(sol::state_view LuaView, const FString& NormalizedPath)
{
	const FString AbsolutePath = MakeAbsoluteScriptPath(NormalizedPath);

	std::ifstream File(FPaths::ToWide(AbsolutePath), std::ios::binary);
	if (!File)
	{
		UE_LOG("[Lua] Lua File Compile Error (%s): Cannot open file", NormalizedPath.c_str());
		return false;
	}
	const std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());

	sol::load_result LoadResult = LuaView.load_buffer(Content.data(), Content.size(), "@" + AbsolutePath);
	if (!LoadResult.valid())
	{
		sol::error Error = LoadResult;
		UE_LOG("[Lua] Lua File Compile Error (%s): %s", NormalizedPath.c_str(), Error.what());
		return false;
	}
	return true;
}

bool FLuaScriptSubsystem::ReloadScriptsAtomically(const TSet<FString>& ReloadTargets)
{
	if (ReloadTargets.empty())
	{
		return true;
	}

	sol::state NewLua;
	ConfigureLuaState(NewLua);

	TSet<FString> NewLoadedScripts = LoadedScripts;
	TArray<FString> NewLoadedScriptOrder = LoadedScriptOrder;
	for (const FString& Target : ReloadTargets)
	{
		if (NewLoadedScripts.insert(Target).second)
		{
			NewLoadedScriptOrder.push_back(Target);
		}
	}

	TMap<FString, TSet<FString>> NewScriptIncludes;
	TMap<FString, FString> NewModulePaths;

	for (const FString& Script : NewLoadedScriptOrder)
	{
		if (!ExecuteEntryScript(NewLua, Script, NewScriptIncludes, NewModulePaths))
		{
			return false;
		}
	}

	Lua = std::move(NewLua);
	LoadedScripts = std::move(NewLoadedScripts);
	LoadedScriptOrder = std::move(NewLoadedScriptOrder);
	ScriptIncludes = std::move(NewScriptIncludes);
	ModulePaths = std::move(NewModulePaths);
	RebuildIncludeDependents();

	return true;
}

FString FLuaScriptSubsystem::ResolveScriptPath(const FString& Path) const
{
	std::filesystem::path InputPath(FPaths::ToWide(Path));
	if (InputPath.is_absolute())
	{
		return NormalizeScriptPath(InputPath);
	}

	const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
	const std::filesystem::path ScriptDir = std::filesystem::path(FPaths::ScriptDir());

	std::filesystem::path Candidate = (RootPath / InputPath).lexically_normal();
	if (std::filesystem::exists(Candidate))
	{
		return NormalizeScriptPath(Candidate);
	}

	Candidate = (ScriptDir / InputPath).lexically_normal();
	if (!Candidate.has_extension())
	{
		Candidate.replace_extension(L".lua");
	}
	if (std::filesystem::exists(Candidate))
	{
		return NormalizeScriptPath(Candidate);
	}

	return NormalizeScriptPath(Candidate);
}

FString FLuaScriptSubsystem::ResolveModulePath(const FString& ModuleName) const
{
	FString RelativePath = ModuleName;
	std::replace(RelativePath.begin(), RelativePath.end(), '.', '/');
	std::replace(RelativePath.begin(), RelativePath.end(), '\\', '/');

	std::filesystem::path ModulePath(FPaths::ToWide(RelativePath));
	if (!ModulePath.has_extension())
	{
		ModulePath.replace_extension(L".lua");
	}

	std::filesystem::path Candidate = std::filesystem::path(FPaths::ScriptDir()) / ModulePath;
	if (std::filesystem::exists(Candidate))
	{
		return NormalizeScriptPath(Candidate);
	}

	if (ModulePath.extension() == L".lua")
	{
		std::filesystem::path ModuleDir = ModulePath;
		ModuleDir.replace_extension();
		std::filesystem::path InitCandidate = std::filesystem::path(FPaths::ScriptDir()) / ModuleDir / L"init.lua";
		if (std::filesystem::exists(InitCandidate))
		{
			return NormalizeScriptPath(InitCandidate);
		}
	}

	return ResolveScriptPath(RelativePath);
}

FString FLuaScriptSubsystem::MakeAbsoluteScriptPath(const FString& NormalizedPath) const
{
	std::filesystem::path Path(FPaths::ToWide(NormalizedPath));
	if (Path.is_absolute())
	{
		return ToGenericUtf8(Path.lexically_normal());
	}
	return ToGenericUtf8((std::filesystem::path(FPaths::RootDir()) / Path).lexically_normal());
}

void FLuaScriptSubsystem::RecordIncludeDependency(const FString& IncludePath)
{
	if (DependencyContextStack.empty())
	{
		return;
	}

	FLuaDependencyContext& Context = DependencyContextStack.back();
	if (!Context.Includes)
	{
		return;
	}

	const FString NormalizedInclude = ResolveScriptPath(IncludePath);
	if (NormalizedInclude != Context.EntryScript)
	{
		Context.Includes->insert(NormalizedInclude);
	}
}

void FLuaScriptSubsystem::RebuildIncludeDependents()
{
	IncludeDependents.clear();

	for (const auto& [Script, Includes] : ScriptIncludes)
	{
		for (const FString& Include : Includes)
		{
			IncludeDependents[Include].insert(Script);
		}
	}
}

TMap<FString, FString>& FLuaScriptSubsystem::GetActiveModulePaths()
{
	if (!DependencyContextStack.empty() && DependencyContextStack.back().ModulePaths)
	{
		return *DependencyContextStack.back().ModulePaths;
	}

	return ModulePaths;
}

sol::object FLuaScriptSubsystem::IncludeFile(const FString& Path, sol::this_state State)
{
	sol::state_view LuaView(State);

	const FString NormalizedPath = ResolveScriptPath(Path);
	RecordIncludeDependency(NormalizedPath);

	if (!CompileFile(LuaView, NormalizedPath) || !ExecuteScriptFile(LuaView, NormalizedPath))
	{
		return sol::make_object(LuaView, false);
	}

	return sol::make_object(LuaView, true);
}

sol::object FLuaScriptSubsystem::RequireModule(const FString& ModuleName, sol::this_state State)
{
	sol::state_view LuaView(State);
	sol::table Package = LuaView["package"];
	sol::table Loaded = Package["loaded"];

	sol::object CachedModule = Loaded[ModuleName];
	if (CachedModule.valid() && CachedModule.get_type() != sol::type::nil)
	{
		return CachedModule;
	}

	const FString NormalizedPath = ResolveModulePath(ModuleName);
	RecordIncludeDependency(NormalizedPath);
	TMap<FString, FString>& ActiveModulePaths = GetActiveModulePaths();
	ActiveModulePaths[ModuleName] = NormalizedPath;

	if (!CompileFile(LuaView, NormalizedPath) || !ExecuteScriptFile(LuaView, NormalizedPath))
	{
		return sol::nil;
	}

	sol::object LoadedModule = Loaded[ModuleName];
	if (LoadedModule.valid() && LoadedModule.get_type() != sol::type::nil)
	{
		return LoadedModule;
	}

	sol::object Result = sol::make_object(LuaView, true);
	Loaded[ModuleName] = Result;
	return Result;
}
