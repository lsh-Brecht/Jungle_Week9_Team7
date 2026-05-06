#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"

class FArchive;

enum class ERichCurveInterpMode : uint8
{
	Constant = 0,
	Linear = 1,
	Cubic = 2,
};

struct FRichCurveKey
{
	float Time = 0.0f;
	float Value = 0.0f;
	float ArriveTangent = 0.0f;
	float LeaveTangent = 0.0f;
	ERichCurveInterpMode InterpMode = ERichCurveInterpMode::Linear;

	FRichCurveKey() = default;
	FRichCurveKey(float InTime, float InValue, ERichCurveInterpMode InInterpMode = ERichCurveInterpMode::Linear,
		float InArriveTangent = 0.0f, float InLeaveTangent = 0.0f)
		: Time(InTime)
		, Value(InValue)
		, ArriveTangent(InArriveTangent)
		, LeaveTangent(InLeaveTangent)
		, InterpMode(InInterpMode)
	{
	}
};

class UCurveFloat : public UObject
{
public:
	DECLARE_CLASS(UCurveFloat, UObject)

	~UCurveFloat() override;

	float Evaluate(float InTime) const;

	int32 AddKey(const FRichCurveKey& Key);
	int32 AddKey(float Time, float Value, ERichCurveInterpMode InterpMode = ERichCurveInterpMode::Linear,
		float ArriveTangent = 0.0f, float LeaveTangent = 0.0f);
	bool SetKey(int32 Index, const FRichCurveKey& Key);
	bool RemoveKey(int32 Index);
	bool RemoveKeyAtTime(float Time, float Tolerance = 1.0e-4f);
	int32 FindKeyIndex(float Time, float Tolerance = 1.0e-4f) const;

	void ClearKeys();
	void SetKeys(const TArray<FRichCurveKey>& InKeys);
	void SortKeys();

	const TArray<FRichCurveKey>& GetKeys() const { return Keys; }
	int32 GetNumKeys() const { return static_cast<int32>(Keys.size()); }
	bool IsEmpty() const { return Keys.empty(); }

	void SetCurveName(const FString& InCurveName) { CurveName = InCurveName; }
	const FString& GetCurveName() const { return CurveName; }
	const FString& GetSourceFilePath() const { return SourceFilePath; }

	bool SaveToFile(const FString& FilePath) const;
	static UCurveFloat* LoadFromFile(const FString& FilePath);
	static UCurveFloat* LoadFromCached(const FString& FilePath);
	static void ClearCache();

	virtual void Serialize(FArchive& Ar) override;

private:
	bool LoadInternal(const FString& FilePath);

	FString CurveName;
	FString SourceFilePath;
	TArray<FRichCurveKey> Keys;

	static TMap<FString, UCurveFloat*> CurveCache;
};
