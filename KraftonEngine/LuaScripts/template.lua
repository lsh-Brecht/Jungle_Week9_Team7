function BeginPlay()
    -- 프리팹 풀 웜업 (테스트용 큐브 10개)
    WarmUpPrefabPool("Asset/Prefab/AStaticMeshActor_1.Prefab", 10)

    -- 테스트 소환
    local loc = obj.Location
    local testActor = AcquirePrefab("Asset/Prefab/AStaticMeshActor_1.Prefab", loc)
    if testActor then
        print("[Prefab Test] Successfully spawned TestCube!")
    else
        print("[Prefab Test] Failed to spawn TestCube!")
    end
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function Tick(dt)
end

