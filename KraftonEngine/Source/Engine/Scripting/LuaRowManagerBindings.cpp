#include "LuaBindings.h"
#include "SolInclude.h"

#include "Runtime/RowManager.h"

void RegisterRowManagerBinding(sol::state& Lua)
{
    Lua.set_function("SetRowSize",
        [](int32 SlotCount, float SlotSize, float RowDepth)
        {
            FRowManager::Get().SetRowSize(SlotCount, SlotSize, RowDepth);
        });

    Lua.set_function("SetRowBufferCounts",
        [](int32 KeepRowsBehind, int32 KeepRowsAhead)
        {
            FRowManager::Get().SetRowBufferCounts(KeepRowsBehind, KeepRowsAhead);
        });

    Lua.set_function("SetRowBiome",
        [](int32 RowIndex, int32 BiomeType)
        {
            FRowManager::Get().SetRowBiome(RowIndex, BiomeType);
        });

    Lua.set_function("SpawnStaticObstacle",
        [](int32 RowIndex, int32 SlotIndex, const FString& PrefabPath)
        {
            FRowManager::Get().SpawnStaticObstacle(RowIndex, SlotIndex, PrefabPath);
        });

    Lua.set_function("SpawnDynamicVehicle",
        [](int32 RowIndex, const FString& PrefabPath, float Speed, int32 DirectionX)
        {
            FRowManager::Get().SpawnDynamicVehicle(RowIndex, PrefabPath, Speed, DirectionX);
        });

	Lua.set_function("MoveForward",
		[](int32 NewCurrentRowIndex)
		{
			FRowManager::Get().MoveForward(NewCurrentRowIndex);
		});

	Lua.set_function("ResetMap",
		[]()
		{
			FRowManager::Get().Shutdown();
		});
}
