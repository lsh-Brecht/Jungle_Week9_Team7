#include "Runtime/RowManager.h"

#include "Runtime/ObjectPoolSystem.h"
#include "Scripting/LuaWorldLibrary.h"
#include "Component/Movement/ProjectileMovementComponent.h"

#include <algorithm>

void FRowData::ClearActors()
{
    for (FStaticObstacleData& Obstacle : StaticObstacles)
    {
        if (Obstacle.SpawnedActor)
        {
            FObjectPoolSystem::Get().ReleaseActor(Obstacle.SpawnedActor);
            Obstacle.SpawnedActor = nullptr;
        }
    }
    StaticObstacles.clear();

	for (AActor* DynActor : DynamicActors)
	{
		if (DynActor)
		{
			FObjectPoolSystem::Get().ReleaseActor(DynActor);
		}
	}
	DynamicActors.clear();
}

void FRowManager::Initialize()
{
    ActiveRows.clear();
}

void FRowManager::Shutdown()
{
	for (FRowData& Row : ActiveRows)
	{
		Row.ClearActors();
	}
	ActiveRows.clear();
}

FRowData* FRowManager::GetRowData(int32 RowIndex)
{
    for (FRowData& Row : ActiveRows)
    {
        if (Row.RowIndex == RowIndex)
        {
            return &Row;
        }
    }
    return nullptr;
}

FRowData& FRowManager::PushEmptyRow(int32 RowIndex)
{
    if (FRowData* Existing = GetRowData(RowIndex))
    {
        return *Existing;
    }

    FRowData NewRow;
    NewRow.RowIndex = RowIndex;

    auto InsertIt = std::find_if(ActiveRows.begin(), ActiveRows.end(),
        [RowIndex](const FRowData& Row)
        {
            return Row.RowIndex > RowIndex;
        });

    return *ActiveRows.insert(InsertIt, NewRow);
}

void FRowManager::SetRowSize(int32 SlotCount, float SlotSize, float RowDepth)
{
    Config.SlotCount = SlotCount;
    Config.SlotSize = SlotSize;
    Config.RowDepth = RowDepth;
}

void FRowManager::SetRowBufferCounts(int32 KeepRowsBehind, int32 KeepRowsAhead)
{
    Config.KeepRowsBehind = KeepRowsBehind;
    Config.KeepRowsAhead = KeepRowsAhead;
}

void FRowManager::SetRowBiome(int32 RowIndex, int32 BiomeType)
{
    FRowData& Row = PushEmptyRow(RowIndex);
    Row.Biome = static_cast<ERowBiome>(BiomeType);
}

void FRowManager::SpawnStaticObstacle(int32 RowIndex, int32 SlotIndex, const FString& PrefabPath)
{
    FRowData& Row = PushEmptyRow(RowIndex);

    FStaticObstacleData Obstacle;
    Obstacle.SlotIndex = SlotIndex;
    Obstacle.PrefabPath = PrefabPath;

	const float OffsetY = (static_cast<float>(Config.SlotCount) - 1.0f) * 0.5f;
	const float WorldY = (static_cast<float>(SlotIndex) - OffsetY) * Config.SlotSize;
	const float WorldX = static_cast<float>(RowIndex) * Config.RowDepth;

	const FVector SpawnLocation(WorldX, WorldY, 0.0f);
    const FRotator SpawnRotation = FRotator();

    UWorld* World = FLuaWorldLibrary::GetActiveWorld();
    if (World)
    {
        Obstacle.SpawnedActor = FObjectPoolSystem::Get().AcquirePrefab(World, PrefabPath, SpawnLocation, SpawnRotation);
    }

    Row.StaticObstacles.push_back(Obstacle);
}

void FRowManager::SpawnDynamicVehicle(int32 RowIndex, const FString& PrefabPath, float Speed, int32 DirectionX)
{
	FRowData& Row = PushEmptyRow(RowIndex);

	UWorld* World = FLuaWorldLibrary::GetActiveWorld();
	if (!World) return;

	const float OffsetY = (static_cast<float>(Config.SlotCount) - 1.0f) * 0.5f;
	const float ExtentY = OffsetY * Config.SlotSize;

	const float SpawnY = (DirectionX > 0) ? -ExtentY - Config.SlotSize : ExtentY + Config.SlotSize;
	const float WorldX = static_cast<float>(Row.RowIndex) * Config.RowDepth;

	const FVector SpawnLocation(WorldX, SpawnY, 0.0f);

	FRotator SpawnRotation = FRotator();
	if (DirectionX < 0)
	{
		SpawnRotation.Yaw = 180.0f; 
	}

	AActor* SpawnedActor = FObjectPoolSystem::Get().AcquirePrefab(World, PrefabPath, SpawnLocation, SpawnRotation);

	if (SpawnedActor)
	{
		Row.DynamicActors.push_back(SpawnedActor);
		for (UActorComponent* Comp : SpawnedActor->GetComponents())
		{
			if (UProjectileMovementComponent* ProjComp = Cast<UProjectileMovementComponent>(Comp))
			{
				ProjComp->SetVelocity(FVector(0.0f, static_cast<float>(DirectionX), 0.0f) * Speed);
				break;
			}
		}
	}
}

void FRowManager::MoveForward(int32 NewCurrentRowIndex)
{
    const int32 Threshold = NewCurrentRowIndex - Config.KeepRowsBehind;
    PopOldRows(Threshold);
}

void FRowManager::PopOldRows(int32 ThresholdRowIndex)
{
    while (!ActiveRows.empty() && ActiveRows.front().RowIndex < ThresholdRowIndex)
    {
        ActiveRows.front().ClearActors();
        ActiveRows.pop_front();
    }
}
