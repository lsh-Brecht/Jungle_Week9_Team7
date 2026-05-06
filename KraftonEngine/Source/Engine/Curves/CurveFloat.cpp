#include "Curves/CurveFloat.h"

#include "Core/Log.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

IMPLEMENT_CLASS(UCurveFloat, UObject)

TMap<FString, UCurveFloat*> UCurveFloat::CurveCache;

namespace
{
	namespace CurveKeys
	{
		constexpr const char* CurveName = "CurveName";
		constexpr const char* Keys = "Keys";
		constexpr const char* Time = "Time";
		constexpr const char* Value = "Value";
		constexpr const char* ArriveTangent = "ArriveTangent";
		constexpr const char* LeaveTangent = "LeaveTangent";
		constexpr const char* InterpMode = "InterpMode";
	}

	std::filesystem::path ResolveCurvePath(const FString& FilePath)
	{
		std::wstring ResolvedPath;
		FString Error;
		if (FPaths::TryResolvePackagePath(FilePath, ResolvedPath, &Error))
		{
			return std::filesystem::path(ResolvedPath);
		}

		return std::filesystem::path(FPaths::ToWide(FilePath));
	}

	ERichCurveInterpMode ClampInterpMode(int32 Value)
	{
		switch (Value)
		{
		case 0: return ERichCurveInterpMode::Constant;
		case 1: return ERichCurveInterpMode::Linear;
		case 2: return ERichCurveInterpMode::Cubic;
		default: return ERichCurveInterpMode::Linear;
		}
	}

	float EvalCubicHermite(const FRichCurveKey& Key0, const FRichCurveKey& Key1, float Alpha)
	{
		const float T = FMath::Clamp01(Alpha);
		const float T2 = T * T;
		const float T3 = T2 * T;
		const float DT = Key1.Time - Key0.Time;

		const float H00 = 2.0f * T3 - 3.0f * T2 + 1.0f;
		const float H10 = T3 - 2.0f * T2 + T;
		const float H01 = -2.0f * T3 + 3.0f * T2;
		const float H11 = T3 - T2;

		const float M0 = Key0.LeaveTangent * DT;
		const float M1 = Key1.ArriveTangent * DT;
		return H00 * Key0.Value + H10 * M0 + H01 * Key1.Value + H11 * M1;
	}

	json::JSON WriteKeyJson(const FRichCurveKey& Key)
	{
		json::JSON KeyJson = json::Object();
		KeyJson[CurveKeys::Time] = static_cast<double>(Key.Time);
		KeyJson[CurveKeys::Value] = static_cast<double>(Key.Value);
		KeyJson[CurveKeys::ArriveTangent] = static_cast<double>(Key.ArriveTangent);
		KeyJson[CurveKeys::LeaveTangent] = static_cast<double>(Key.LeaveTangent);
		KeyJson[CurveKeys::InterpMode] = static_cast<int>(Key.InterpMode);
		return KeyJson;
	}
}

UCurveFloat::~UCurveFloat()
{
	for (auto It = CurveCache.begin(); It != CurveCache.end();)
	{
		if (It->second == this)
		{
			It = CurveCache.erase(It);
		}
		else
		{
			++It;
		}
	}
}

float UCurveFloat::Evaluate(float InTime) const
{
	if (Keys.empty())
	{
		return 0.0f;
	}

	if (Keys.size() == 1 || InTime <= Keys.front().Time)
	{
		return Keys.front().Value;
	}

	if (InTime >= Keys.back().Time)
	{
		return Keys.back().Value;
	}

	auto Upper = std::upper_bound(Keys.begin(), Keys.end(), InTime,
		[](float Time, const FRichCurveKey& Key)
		{
			return Time < Key.Time;
		});

	if (Upper == Keys.begin())
	{
		return Keys.front().Value;
	}

	if (Upper == Keys.end())
	{
		return Keys.back().Value;
	}

	const FRichCurveKey& Key1 = *Upper;
	const FRichCurveKey& Key0 = *(Upper - 1);
	const float DeltaTime = Key1.Time - Key0.Time;
	if (FMath::Abs(DeltaTime) <= FMath::Epsilon)
	{
		return Key1.Value;
	}

	const float Alpha = FMath::Clamp01((InTime - Key0.Time) / DeltaTime);
	switch (Key0.InterpMode)
	{
	case ERichCurveInterpMode::Constant:
		return Key0.Value;
	case ERichCurveInterpMode::Cubic:
		return EvalCubicHermite(Key0, Key1, Alpha);
	case ERichCurveInterpMode::Linear:
	default:
		return FMath::Lerp(Key0.Value, Key1.Value, Alpha);
	}
}

int32 UCurveFloat::AddKey(const FRichCurveKey& Key)
{
	const int32 ExistingIndex = FindKeyIndex(Key.Time);
	if (ExistingIndex >= 0)
	{
		Keys[ExistingIndex] = Key;
		SortKeys();
		return FindKeyIndex(Key.Time);
	}

	Keys.push_back(Key);
	SortKeys();
	return FindKeyIndex(Key.Time);
}

int32 UCurveFloat::AddKey(float Time, float Value, ERichCurveInterpMode InterpMode, float ArriveTangent, float LeaveTangent)
{
	return AddKey(FRichCurveKey(Time, Value, InterpMode, ArriveTangent, LeaveTangent));
}

bool UCurveFloat::SetKey(int32 Index, const FRichCurveKey& Key)
{
	if (Index < 0 || Index >= GetNumKeys())
	{
		return false;
	}

	Keys[Index] = Key;
	SortKeys();
	return true;
}

bool UCurveFloat::RemoveKey(int32 Index)
{
	if (Index < 0 || Index >= GetNumKeys())
	{
		return false;
	}

	Keys.erase(Keys.begin() + Index);
	return true;
}

bool UCurveFloat::RemoveKeyAtTime(float Time, float Tolerance)
{
	const int32 Index = FindKeyIndex(Time, Tolerance);
	return RemoveKey(Index);
}

int32 UCurveFloat::FindKeyIndex(float Time, float Tolerance) const
{
	auto It = std::lower_bound(Keys.begin(), Keys.end(), Time,
		[](const FRichCurveKey& Key, float InTime)
		{
			return Key.Time < InTime;
		});

	if (It != Keys.end() && FMath::Abs(It->Time - Time) <= Tolerance)
	{
		return static_cast<int32>(It - Keys.begin());
	}

	if (It != Keys.begin())
	{
		auto Previous = It - 1;
		if (FMath::Abs(Previous->Time - Time) <= Tolerance)
		{
			return static_cast<int32>(Previous - Keys.begin());
		}
	}

	return -1;
}

void UCurveFloat::ClearKeys()
{
	Keys.clear();
}

void UCurveFloat::SetKeys(const TArray<FRichCurveKey>& InKeys)
{
	Keys = InKeys;
	SortKeys();
}

void UCurveFloat::SortKeys()
{
	std::stable_sort(Keys.begin(), Keys.end(),
		[](const FRichCurveKey& A, const FRichCurveKey& B)
		{
			return A.Time < B.Time;
		});
}

bool UCurveFloat::SaveToFile(const FString& FilePath) const
{
	using namespace json;

	if (FilePath.empty())
	{
		return false;
	}

	JSON Root = Object();
	Root[CurveKeys::CurveName] = CurveName;

	JSON KeysJson = Array();
	for (const FRichCurveKey& Key : Keys)
	{
		KeysJson.append(WriteKeyJson(Key));
	}
	Root[CurveKeys::Keys] = KeysJson;

	const std::filesystem::path DiskPath = ResolveCurvePath(FilePath);
	if (DiskPath.has_parent_path())
	{
		std::filesystem::create_directories(DiskPath.parent_path());
	}

	std::ofstream File(DiskPath);
	if (!File.is_open())
	{
		UE_LOG("Failed to save curve: %s", FilePath.c_str());
		return false;
	}

	File << Root;
	return true;
}

UCurveFloat* UCurveFloat::LoadFromFile(const FString& FilePath)
{
	if (FilePath.empty())
	{
		return nullptr;
	}

	auto It = CurveCache.find(FilePath);
	if (It != CurveCache.end())
	{
		return It->second;
	}

	UCurveFloat* Curve = UObjectManager::Get().CreateObject<UCurveFloat>();
	if (!Curve->LoadInternal(FilePath))
	{
		UObjectManager::Get().DestroyObject(Curve);
		return nullptr;
	}

	CurveCache[FilePath] = Curve;
	return Curve;
}

UCurveFloat* UCurveFloat::LoadFromCached(const FString& FilePath)
{
	auto It = CurveCache.find(FilePath);
	return It != CurveCache.end() ? It->second : nullptr;
}

void UCurveFloat::ClearCache()
{
	CurveCache.clear();
}

void UCurveFloat::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CurveName;
	Ar << SourceFilePath;
	Ar << Keys;
	if (Ar.IsLoading())
	{
		SortKeys();
	}
}

bool UCurveFloat::LoadInternal(const FString& FilePath)
{
	using namespace json;

	const std::filesystem::path DiskPath = ResolveCurvePath(FilePath);
	std::ifstream File(DiskPath);
	if (!File.is_open())
	{
		UE_LOG("Failed to load curve: %s", FilePath.c_str());
		return false;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);
	if (Root.IsNull())
	{
		UE_LOG("Failed to parse curve JSON: %s", FilePath.c_str());
		return false;
	}

	CurveName = Root.hasKey(CurveKeys::CurveName)
		? Root[CurveKeys::CurveName].ToString()
		: FPaths::ToUtf8(DiskPath.stem().wstring());
	SourceFilePath = FilePath;
	Keys.clear();

	if (Root.hasKey(CurveKeys::Keys))
	{
		JSON KeysJson = Root[CurveKeys::Keys];
		for (int32 Index = 0; Index < static_cast<int32>(KeysJson.length()); ++Index)
		{
			JSON KeyJson = KeysJson[Index];
			FRichCurveKey Key;
			if (KeyJson.hasKey(CurveKeys::Time)) Key.Time = static_cast<float>(KeyJson[CurveKeys::Time].ToFloat());
			if (KeyJson.hasKey(CurveKeys::Value)) Key.Value = static_cast<float>(KeyJson[CurveKeys::Value].ToFloat());
			if (KeyJson.hasKey(CurveKeys::ArriveTangent)) Key.ArriveTangent = static_cast<float>(KeyJson[CurveKeys::ArriveTangent].ToFloat());
			if (KeyJson.hasKey(CurveKeys::LeaveTangent)) Key.LeaveTangent = static_cast<float>(KeyJson[CurveKeys::LeaveTangent].ToFloat());
			if (KeyJson.hasKey(CurveKeys::InterpMode)) Key.InterpMode = ClampInterpMode(KeyJson[CurveKeys::InterpMode].ToInt());
			Keys.push_back(Key);
		}
	}

	SortKeys();
	return true;
}
