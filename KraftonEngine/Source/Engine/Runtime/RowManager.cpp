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

void FRowManager::Tick(float DeltaTime)
{
	UWorld* World = FLuaWorldLibrary::GetActiveWorld();
	if (!World) return;

	for (FRowData& Row : ActiveRows)
	{
		if (Row.Spawner.bIsActive)
		{
			// 1. 타이머 감소
			Row.Spawner.SpawnTimer -= DeltaTime;

			if (Row.Spawner.SpawnTimer <= 0.0f)
			{
				// 2. 타이머 초기화
				Row.Spawner.SpawnTimer = Row.Spawner.SpawnInterval;

				// 3. 스폰 위치 계산 (화면 밖에서 등장하도록 설정)
				const float OffsetX = (static_cast<float>(Config.SlotCount) - 1.0f) * 0.5f;
				// 가장자리 슬롯의 X 좌표
				const float ExtentX = OffsetX * Config.SlotSize;

				// DirectionX가 1(오른쪽)이면 왼쪽 끝 화면 밖에서, -1(왼쪽)이면 오른쪽 끝 화면 밖에서 스폰
				const float SpawnX = (Row.Spawner.DirectionX > 0) ? -ExtentX - Config.SlotSize : ExtentX + Config.SlotSize;
				const float WorldZ = static_cast<float>(Row.RowIndex) * Config.RowDepth;

				const FVector SpawnLocation(SpawnX, 0.0f, WorldZ);

				// 4. 이동 방향에 맞춰서 차량이 앞을 보도록 회전 (엔진의 기본 Forward 방향에 따라 Yaw 값은 조정이 필요할 수 있어)
				FRotator SpawnRotation = FRotator();
				if (Row.Spawner.DirectionX < 0)
				{
					// 왼쪽으로 달리는 경우 180도 회전
					// SpawnRotation.Yaw = 180.0f; 
				}

				// 5. 오브젝트 풀에서 액터 스폰 (fire-and-forget)
				AActor* SpawnedActor = FObjectPoolSystem::Get().AcquirePrefab(World, Row.Spawner.PrefabPath, SpawnLocation, SpawnRotation);

				if (SpawnedActor)
				{
					// 6. ProjectileMovementComponent를 찾아서 속도 및 방향 설정
					for (UActorComponent* Comp : SpawnedActor->GetComponents())
					{
						if (UProjectileMovementComponent* ProjComp = Cast<UProjectileMovementComponent>(Comp))
						{
							// 직접 접근 대신 SetVelocity() 함수를 사용하도록 수정
							ProjComp->SetVelocity(FVector(static_cast<float>(Row.Spawner.DirectionX), 0.0f, 0.0f) * Row.Spawner.Speed);
							break;
						}
					}
				}
			}
		}
	}
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

    const float OffsetX = (static_cast<float>(Config.SlotCount) - 1.0f) * 0.5f;
    const float WorldX = (static_cast<float>(SlotIndex) - OffsetX) * Config.SlotSize;
    const float WorldZ = static_cast<float>(RowIndex) * Config.RowDepth;

    const FVector SpawnLocation(WorldX, 0.0f, WorldZ);
    const FRotator SpawnRotation = FRotator();

    UWorld* World = FLuaWorldLibrary::GetActiveWorld();
    if (World)
    {
        Obstacle.SpawnedActor = FObjectPoolSystem::Get().AcquirePrefab(World, PrefabPath, SpawnLocation, SpawnRotation);
    }

    Row.StaticObstacles.push_back(Obstacle);
}

void FRowManager::SetDynamicSpawner(int32 RowIndex, const FString& PrefabPath, float Speed, float Interval, int32 DirectionX)
{
    FRowData& Row = PushEmptyRow(RowIndex);
    Row.Spawner.bIsActive = true;
    Row.Spawner.PrefabPath = PrefabPath;
    Row.Spawner.Speed = Speed;
    Row.Spawner.SpawnInterval = Interval;
    Row.Spawner.SpawnTimer = Interval;
    Row.Spawner.DirectionX = DirectionX;
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
