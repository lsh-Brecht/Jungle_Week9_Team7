local BIOME = {
    GRASS = 0,
    ROAD = 1,
    RAILWAY = 2
}

local PREFABS = {
    TREE = "Data/Prefab/Tree.Prefab",
    ROCK = "Data/Prefab/Rock.Prefab",
    CAR = "Data/Prefab/Car.Prefab",
    TRAIN = "Data/Prefab/Train.Prefab"
}

local LastSafeSlot = 4
local CurrentBiomeState = BIOME.GRASS

-- Markov Chain 전이 확률표 (현재 지형에 따른 다음 지형의 가중치)
-- 형식: [현재지형] = { {다음지형, 가중치}, ... }
local BiomeWeights = {
    { type = BIOME.GRASS, weight = 50 },   -- 50%
    { type = BIOME.ROAD, weight = 35 },    -- 35%
    { type = BIOME.RAILWAY, weight = 15 }  -- 15%
}

-- 장애물별 등장 가중치 설정 (합이 꼭 100일 필요는 없어)
local ObstacleWeights = {
    { prefab = PREFABS.TREE, weight = 60 }, -- 60의 비중
    { prefab = PREFABS.ROCK, weight = 30 }, -- 30의 비중
    { prefab = PREFABS.SIGN, weight = 10 }  -- 10의 비중
}

_G.RowGenerator = {}

function RowGenerator.ConfigureRows()
    SetRowSize(9, 100.0, 100.0)
    SetRowBufferCounts(6, 12)
end

-- 가중치 기반 독립적 지형 선택
function RowGenerator.ChooseBiome()
    local totalWeight = 0
    for _, b in ipairs(BiomeWeights) do
        totalWeight = totalWeight + b.weight
    end

    local randWeight = math.random() * totalWeight
    local weightSum = 0

    for _, b in ipairs(BiomeWeights) do
        weightSum = weightSum + b.weight
        if randWeight <= weightSum then
            return b.type
        end
    end

    return BIOME.GRASS
end

-- 가중치 기반으로 장애물을 하나 뽑아주는 헬퍼 함수
function RowGenerator.ChooseObstaclePrefab()
    local totalWeight = 0
    for _, obs in ipairs(ObstacleWeights) do
        totalWeight = totalWeight + obs.weight
    end

    local randWeight = math.random() * totalWeight
    local weightSum = 0

    for _, obs in ipairs(ObstacleWeights) do
        weightSum = weightSum + obs.weight
        if randWeight <= weightSum then
            return obs.prefab
        end
    end
    return PREFABS.TREE -- 기본값
end

-- 진행도(RowIndex)에 따른 장애물 확률 증가
function RowGenerator.GetObstacleChance(rowIndex)
    -- 기본 0.1(10%)에서 시작, RowIndex가 오를수록 증가. 최대 0.7(70%)까지만.
    return math.min(0.7, 0.1 + (rowIndex * 0.005))
end

function RowGenerator.GenerateRow(rowIndex)
    -- 1. 지형 결정 (Markov Chain)
    local biomeType = RowGenerator.ChooseNextBiome()
    SetRowBiome(rowIndex, biomeType)

    -- 2. 안전한 경로 계산 (-1 ~ 1 슬롯 이동)
    local nextSafeSlot = LastSafeSlot + math.random(-1, 1)
    nextSafeSlot = math.max(0, math.min(8, nextSafeSlot))
    LastSafeSlot = nextSafeSlot -- 다음 Row를 위해 갱신

    if biomeType == BIOME.GRASS then
        local obstacleChance = RowGenerator.GetObstacleChance(rowIndex)
        
        for slot = 0, 8 do
            if slot ~= nextSafeSlot then -- 안전 구역이 아닌 곳만 장애물 스폰[cite: 9]
                if math.random() < obstacleChance then
                    -- 가중치에 따라 확률적으로 장애물 프리팹을 선택
                    local prefab = RowGenerator.ChooseObstaclePrefab()
                    
                    SpawnStaticObstacle(rowIndex, slot, prefab)
                end
            end
        end

    elseif biomeType == BIOME.ROAD then
        -- RowIndex에 비례해 속도는 빨라지고 생성 주기는 짧아짐
        local speed = 5.0 + (rowIndex * 0.1)
        local interval = math.max(0.8, 3.0 - (rowIndex * 0.05))
        local dirX = (math.random(0, 1) == 0) and -1 or 1

        SetDynamicSpawner(rowIndex, PREFABS.CAR, speed, interval, dirX) --[cite: 9]

    elseif biomeType == BIOME.RAILWAY then
        -- 기차는 매우 빠르고 간격이 긺
        local speed = 20.0 + (rowIndex * 0.2)
        local interval = math.max(3.0, 6.0 - (rowIndex * 0.02))
        local dirX = (math.random(0, 1) == 0) and -1 or 1

        SetDynamicSpawner(rowIndex, PREFABS.TRAIN, speed, interval, dirX)
    end
end