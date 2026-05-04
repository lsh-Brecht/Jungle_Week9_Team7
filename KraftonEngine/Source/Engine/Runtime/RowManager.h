#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "GameFramework/AActor.h"

#include <deque>

enum class ERowBiome : uint8
{
    Grass = 0,
    Road = 1,
    Railway = 2
};

struct FRowRuntimeConfig
{
    int32 SlotCount = 9;
    float SlotSize = 100.0f;
    float RowDepth = 100.0f;

    int32 KeepRowsBehind = 6;
    int32 KeepRowsAhead = 12;
};

struct FStaticObstacleData
{
    int32 SlotIndex = 0;
    FString PrefabPath;
    AActor* SpawnedActor = nullptr;
};

struct FRowData
{
	int32 RowIndex = 0;
	ERowBiome Biome = ERowBiome::Grass;
	TArray<FStaticObstacleData> StaticObstacles;
	TArray<AActor*> DynamicActors;

	void ClearActors();
};

class FRowManager : public TSingleton<FRowManager>
{
    friend class TSingleton<FRowManager>;

private:
    std::deque<FRowData> ActiveRows;
    FRowRuntimeConfig Config;

    FRowManager() = default;

public:
    void Initialize();
    void Shutdown();

    FRowData* GetRowData(int32 RowIndex);
    FRowData& PushEmptyRow(int32 RowIndex);

    void SetRowSize(int32 SlotCount, float SlotSize, float RowDepth);
    void SetRowBufferCounts(int32 KeepRowsBehind, int32 KeepRowsAhead);
    void SetRowBiome(int32 RowIndex, int32 BiomeType);
    void SpawnStaticObstacle(int32 RowIndex, int32 SlotIndex, const FString& PrefabPath);
    void SpawnDynamicVehicle(int32 RowIndex, const FString& PrefabPath, float Speed, int32 DirectionX);

    void MoveForward(int32 NewCurrentRowIndex);
    void PopOldRows(int32 ThresholdRowIndex);
};
