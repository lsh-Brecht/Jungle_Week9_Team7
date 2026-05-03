function BeginPlay()
    print("[BeginPlay] UUID = " .. obj.UUID)
    obj:PrintLocation()
    
    -- 프리팹 풀 웜업 (테스트용 큐브 10개)
    WarmUpPrefabPool("Data/Prefabs/TestCube.prefab", 10)

    -- 테스트 소환
    local loc = obj.Location
    loc.z = loc.z + 200 -- 현재 액터 위쪽에 스폰
    local testActor = AcquirePrefab("Data/Prefabs/TestCube.prefab", loc, FRotator(0, 0, 0))
    if testActor then
        print("[Prefab Test] Successfully spawned TestCube!")
    else
        print("[Prefab Test] Failed to spawn TestCube!")
    end
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation()
end
