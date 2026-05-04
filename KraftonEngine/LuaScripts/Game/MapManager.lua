-- MapManager.lua
-- 투명한 맵 관리자 액터에 부착되는 컴포넌트 스크립트

-- 모듈 스크립트를 로컬로 불러와 전역 오염 방지
local RowGenerator = require("Game/RowGenerator")

local MapManager = {
    playerPawn = nil,

    RowDepth = RowGenerator.MapConfig.RowDepth,        -- FRowRuntimeConfig의 RowDepth와 동일 (전진 방향 한 칸의 크기)
    PreloadRows = 40,        -- 플레이어 앞에 미리 생성해 둘 칸의 수
    CurrentPlayerRow = 0,    -- 플레이어가 현재 밟고 있는 칸의 인덱스
    HighestGeneratedRow = -1,-- 지금까지 생성된 가장 먼 칸의 인덱스

    ActiveSpawners = {},     -- 관리할 스패너 배열
    ActiveVehicles = {}      -- 수명을 관리할 생성된 차량 배열
}

_G.AddDynamicSpawner = function(rowIndex, prefab, speed, interval, dirY)
    table.insert(MapManager.ActiveSpawners, {
        RowIndex = rowIndex,
        Prefab = prefab,
        Speed = speed,
        Interval = interval,
        Timer = interval,
        DirY = dirY
    })
end

function BeginPlay()
    -- 1. RowGenerator 초기 설정
    if RowGenerator.ConfigureRows ~= nil then
         RowGenerator.ConfigureRows()
         print("[MapManager] RowGenerator 초기 설정 성공")
    end

    -- 2. 플레이어 폰 찾기
    if World ~= nil and World.GetPlayerController ~= nil then
        local controller = World.GetPlayerController(0)
        if controller then
            local pawn = controller:GetPossessedPawn()
            if pawn then
                MapManager.playerPawn = pawn
                print("[MapManager] Pawn 연결 성공")
            end
        end
    end

    -- 3. 게임 시작 시 초기 맵 미리 깔아두기
    for i = 0, MapManager.PreloadRows do
        RowGenerator.GenerateRow(i)
        MapManager.HighestGeneratedRow = i
        print("[MapManager] 초기 맵 생성 성공")
    end

    print("[MapManager] 초기 맵 생성 완료. 총" .. tostring(MapManager.PreloadRows + 1) .. "칸")

    MapManager_Reset = function()
        MapManager.CurrentPlayerRow = 0
        MapManager.HighestGeneratedRow = -1

        ResetMap()

        RowGenerator.ConfigureRows()
        for i = 0, MapManager.PreloadRows do
            RowGenerator.GenerateRow(i)
            MapManager.HighestGeneratedRow = i
        end

        print("[MapManager] 맵 리셋 완료")
    end
end

function Tick(deltaTime)
    -- 플레이어 폰이 아직 할당되지 않았다면 다시 찾기 (안전 장치)
    if MapManager.playerPawn == nil then
        if World ~= nil and World.GetPlayerController ~= nil then
            local controller = World.GetPlayerController(0)
            if controller then
                MapManager.playerPawn = controller:GetPossessedPawn()
            end
        end
        return
    end

    -- 1. 언리얼 엔진 좌표계: 전진 방향은 X축!
    local playerForwardAxis = MapManager.playerPawn.Location.X

    -- 2. 플레이어가 현재 위치한 칸(Row) 인덱스 계산
    local playerRowIndex = math.floor(playerForwardAxis / MapManager.RowDepth)

    -- 3. 플레이어가 새로운 칸으로 넘어갔는지 감지
    if playerRowIndex > MapManager.CurrentPlayerRow then
        local step = playerRowIndex - MapManager.CurrentPlayerRow
        MapManager.CurrentPlayerRow = playerRowIndex

        -- 4. 전진한 칸 수만큼 새로운 맵을 앞쪽에 생성
        for i = 1, step do
            MapManager.HighestGeneratedRow = MapManager.HighestGeneratedRow + 1
            RowGenerator.GenerateRow(MapManager.HighestGeneratedRow)
        end

        -- 5. C++ 측 기능 호출: LuaBindings.cpp에 등록된 글로벌 함수를 통해
        -- 임계값(NewCurrentRowIndex - KeepRowsBehind) 미만의 후방 청크를 제거
        if MoveForward ~= nil then
            MoveForward(MapManager.CurrentPlayerRow)
        end

        -- 임계값 미만(KeepRowsBehind칸 전) 스패너 지우기
        local threshold = MapManager.CurrentPlayerRow - RowGenerator.MapConfig.KeepRowsBehind
        for i = #MapManager.ActiveSpawners, 1, -1 do
            if MapManager.ActiveSpawners[i].RowIndex < threshold then
                table.remove(MapManager.ActiveSpawners, i)
            end
        end
    end

    -- 6. 루아 스패너 타이머 업데이트
    for _, spawner in ipairs(MapManager.ActiveSpawners) do
        spawner.Timer = spawner.Timer - deltaTime
        if spawner.Timer <= 0 then
            spawner.Timer = spawner.Interval
            if SpawnDynamicVehicle then
                local vehicle = SpawnDynamicVehicle(spawner.RowIndex, spawner.Prefab, spawner.Speed, spawner.DirY)
                if vehicle then
                    -- 차량의 수명을 화면 너비 / 속도로 계산해서 배열에 담기
                    local lifeTime = 100.0 / spawner.Speed
                    table.insert(MapManager.ActiveVehicles, {
                        Vehicle = vehicle,
                        Timer = lifeTime
                    })
                end
            end
        end
    end

    -- 7. 화면 밖을 넘어선(수명이 다한) 차량 제거 처리 (오브젝트 풀 반환)
    for i = #MapManager.ActiveVehicles, 1, -1 do
        local vInfo = MapManager.ActiveVehicles[i]
        vInfo.Timer = vInfo.Timer - deltaTime
        if vInfo.Timer <= 0 then
            if ReleaseActor then
                ReleaseActor(vInfo.Vehicle)
            end
            table.remove(MapManager.ActiveVehicles, i)
        end
    end
end