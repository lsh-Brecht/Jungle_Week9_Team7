local RowGenerator = {}

local BIOME = {
    GRASS = 0,
    ROAD = 1,
    RAILWAY = 2
}

local BIOME_NAME = {
    [0] = "GRASS",
    [1] = "ROAD",
    [2] = "RAILWAY"
}

local PREFABS = {
    TREE1 = "Asset/Prefab/TreeA.Prefab",
    TREE2 = "Asset/Prefab/TreeB.Prefab",
    ROCK = "Asset/Prefab/BasicCube.Prefab",

    RacingCar = "Asset/Prefab/RacingCar.Prefab",
    CARA = "Asset/Prefab/CarA.Prefab",
    CARB = "Asset/Prefab/CarB.Prefab",
    CARC = "Asset/Prefab/CarC.Prefab",
    CARD = "Asset/Prefab/CarD.Prefab",

    TRAIN = "Asset/Prefab/BasicCube.Prefab" -- Train 프리팹이 없다면 대체
}

RowGenerator.MapConfig = {
    SlotCount = 9,
    SlotSize = 2.0,
    RowDepth = 2.0
}
RowGenerator.MapConfig.MaxSlotIndex = RowGenerator.MapConfig.SlotCount - 1

local LastSafeSlot = math.floor(RowGenerator.MapConfig.SlotCount / 2)

-- 2. 가중치 테이블 설정
-- 지형의 가중치
local BiomeWeights = {
    { type = BIOME.GRASS, weight = 50 },   -- 50%
    { type = BIOME.ROAD, weight = 35 },    -- 35%
    { type = BIOME.RAILWAY, weight = 15 }  -- 15%
}

-- 장애물별 등장 가중치 설정 (합이 꼭 100일 필요는 없어)
local ObstacleWeights = {
    { prefab = PREFABS.TREE1, weight = 50 }, -- 50의 비중
    { prefab = PREFABS.TREE2, weight = 50 }, -- 50의 비중
    --{ prefab = PREFABS.SIGN, weight = 10 }  -- 10의 비중
}

-- 차량 등장 확률 (가중치)
local VehicleWeights = {
    { type = PREFABS.RacingCar, weight = 20 },
    { type = PREFABS.CARA, weight = 20 },
    { type = PREFABS.CARB, weight = 20 },
    { type = PREFABS.CARC, weight = 20 },
    { type = PREFABS.CARD, weight = 20 },
}

function RowGenerator.ConfigureRows()
    SetRowSize(RowGenerator.MapConfig.SlotCount, RowGenerator.MapConfig.SlotSize, RowGenerator.MapConfig.RowDepth)
    SetRowBufferCounts(15, 15)

    if World and World.WarmUpPrefabPool then
        World.WarmUpPrefabPool(PREFABS.TREE1, 100)
        World.WarmUpPrefabPool(PREFABS.TREE2, 100)
        -- World.WarmUpPrefabPool(PREFABS.CARA, 30)
        -- World.WarmUpPrefabPool(PREFABS.ROCK, 20)
    end
end

-- 가중치 기반 독립적 선택
local function ChooseWeighted(table)
    local totalWeight = 0
    for _, item in ipairs(table) do
        totalWeight = totalWeight + item.weight
    end

    local randWeight = math.random() * totalWeight
    local weightSum = 0

    for _, item in ipairs(table) do
        weightSum = weightSum + item.weight
        if randWeight <= weightSum then
            return item
        end
    end
    return table[1]
end

-- 진행도(RowIndex)에 따른 장애물 확률 증가
function RowGenerator.GetObstacleChance(rowIndex)
    -- 기본 0.1(10%)에서 시작, RowIndex가 오를수록 증가. 최대 0.7(70%)까지만.
    return math.min(0.7, 0.1 + (rowIndex * 0.001))
end

function RowGenerator.GenerateRow(rowIndex)
    -- 1. 지형 결정 (Markov Chain)
    local biome = ChooseWeighted(BiomeWeights)
    -- local biomeType = biome.type
    local biomeType = BIOME.GRASS
    SetRowBiome(rowIndex, biomeType)
    print("Biome : " .. (BIOME_NAME[biomeType] or tostring(biomeType)))

    -- 2. 안전한 경로 계산 (-1 ~ 1 슬롯 이동)
    local nextSafeSlot = LastSafeSlot + math.random(-1, 1)
    nextSafeSlot = math.max(0, math.min(RowGenerator.MapConfig.MaxSlotIndex, nextSafeSlot))
    LastSafeSlot = nextSafeSlot -- 다음 Row를 위해 갱신

    if biomeType == BIOME.GRASS then
        local obstacleChance = RowGenerator.GetObstacleChance(rowIndex)
        
        for slot = 0, RowGenerator.MapConfig.MaxSlotIndex do
            if slot ~= nextSafeSlot then -- 안전 구역이 아닌 곳만 장애물 스폰[cite: 9]
                if math.random() < obstacleChance then
                    -- 가중치에 따라 확률적으로 장애물 프리팹을 선택
                    local obstacle = ChooseWeighted(ObstacleWeights)
                    
                    SpawnStaticObstacle(rowIndex, slot, obstacle.prefab)
                end
            end
        end

-- (GenerateRow 함수 내부의 ROAD 처리 부분)
    elseif biomeType == BIOME.ROAD then
        -- UE 축에 맞춰 좌우 이동 방향을 Y축으로 명명 (1: 오른쪽, -1: 왼쪽)
        local dirY = (math.random(0, 1) == 0) and -1 or 1
        
        -- 1. 이 차선에 등장할 차량 종류를 확률로 결정
        local selectedVehicle = ChooseWeighted(VehicleWeights)
        
        local speed = 0
        local interval = 0

        -- 2. 뽑힌 차량의 종류에 따라 속도와 스폰 주기를 다르게 세팅
        if selectedVehicle.type == PREFABS.CAR then
            -- 승용차: 표준 속도, 표준 간격
            speed = 5.0 + (rowIndex * 0.1)
            interval = math.max(1.0, 3.0 - (rowIndex * 0.05))

        elseif selectedVehicle.type == PREFABS.TRUCK then
            -- 트럭: 느린 속도, 넓은 간격 (길막 주의)
            speed = 3.5 + (rowIndex * 0.08)
            interval = math.max(2.0, 5.0 - (rowIndex * 0.03))

        elseif selectedVehicle.type == PREFABS.SPORTS_CAR then
            -- 스포츠카: 매우 빠른 속도, 짧은 간격 (위협적)
            speed = 10.0 + (rowIndex * 0.15)
            interval = math.max(0.5, 2.0 - (rowIndex * 0.05))
        end

        -- 3. 결정된 데이터로 해당 Row에 스포너 등록
        SetDynamicSpawner(rowIndex, selectedVehicle.type, speed, interval, dirY)

    elseif biomeType == BIOME.RAILWAY then
        -- 기차는 매우 빠르고 간격이 긺
        local speed = 20.0 + (rowIndex * 0.2)
        local interval = math.max(3.0, 6.0 - (rowIndex * 0.02))
        local dirY = (math.random(0, 1) == 0) and -1 or 1

        SetDynamicSpawner(rowIndex, PREFABS.TRAIN, speed, interval, dirY)
    end
end

return RowGenerator