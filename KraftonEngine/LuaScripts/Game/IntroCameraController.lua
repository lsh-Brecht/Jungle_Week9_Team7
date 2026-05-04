-- IntroCameraController.lua

-- UI callback / LuaJIT native crash 방어용.
-- JIT가 없는 빌드에서는 조용히 무시됩니다.
if jit ~= nil and jit.off ~= nil then
    pcall(jit.off, true, true)
end

-- =========================================================
-- Intro camera settings
-- =========================================================

-- 화면 중앙으로 보여줄 지점이 X축으로 왕복하는 범위입니다.
-- sweepX는 카메라 위치가 아니라 "화면 중앙 타겟 X"입니다.
local SWEEP_RANGE  = 20.0
local SWEEP_SPEED  = 2.0

local TARGET_Y     = 0.0
local TARGET_Z     = 0.0

local CAM_Z        = 30.0
local ORTHO_WIDTH  = 40.0

-- 45도 우하단 시점.
-- 방향이 반대면 VIEW_YAW만 135.0, 45.0, -135.0 중 하나로 바꾸면 됩니다.
local VIEW_PITCH   = 45.0
local VIEW_YAW     = -45.0

-- 게임 시작 시 게임 카메라로 넘어가는 블렌드 시간입니다.
local START_BLEND_TIME = 1.2

-- =========================================================
-- Runtime state
-- =========================================================

local pc           = nil
local introActor   = nil
local introCam     = nil
local gameCam      = nil

local sweepX       = 0.0
local direction    = 1.0

local bIntroActive = false
local bStarted     = false
local pendingStart = false

-- =========================================================
-- Helpers
-- =========================================================

local function V(x, y, z)
    return FVector.new(x, y, z)
end

local function R(pitch, yaw, roll)
    return FRotator.new(pitch, yaw, roll)
end

local function is_valid(h)
    return h ~= nil and h.IsValid ~= nil and h:IsValid()
end

local function safe_call(label, fn)
    local ok, err = pcall(fn)

    if not ok then
        print("[IntroCam][ERROR] " .. tostring(label) .. ": " .. tostring(err))
        return false
    end

    return true
end

local function clear_ui_handler()
    if UI ~= nil and UI.ClearEventHandler ~= nil then
        safe_call("UI.ClearEventHandler", function()
            UI.ClearEventHandler()
        end)
    end
end

local function deg_to_rad(deg)
    return deg * 3.1415926535 / 180.0
end

-- VIEW_PITCH / VIEW_YAW 방향으로 target을 바라보도록 카메라 위치를 역산합니다.
-- 즉 targetX, targetY, targetZ가 화면 중앙에 오도록 WorldLocation을 구합니다.
local function get_camera_location_for_target(targetX, targetY, targetZ)
    local pitchRad = deg_to_rad(VIEW_PITCH)
    local yawRad   = deg_to_rad(VIEW_YAW)

    local cp = math.cos(pitchRad)

    local fx = cp * math.cos(yawRad)
    local fy = cp * math.sin(yawRad)
    local fz = -math.sin(pitchRad)

    local distance = (CAM_Z - targetZ) / (-fz)

    return V(
        targetX - fx * distance,
        targetY - fy * distance,
        CAM_Z
    )
end

local function apply_intro_camera_transform(targetX)
    if not is_valid(introCam) then return end

    introCam.WorldLocation = get_camera_location_for_target(targetX, TARGET_Y, TARGET_Z)
    introCam.WorldRotation = R(VIEW_PITCH, VIEW_YAW, 0.0)
end

local function set_camera_property(cam, name, value)
    if not is_valid(cam) then return false end
    if cam.SetProperty == nil then return false end

    return safe_call("SetProperty " .. tostring(name), function()
        cam:SetProperty(name, value)
    end)
end

local function configure_intro_camera(cam)
    if not is_valid(cam) then return end

    cam.ViewMode      = "Static"
    cam.Orthographic  = true
    cam.OrthoWidth    = ORTHO_WIDTH

    -- 인트로 카메라는 직접 Tick에서 움직입니다.
    set_camera_property(cam, "Enable Smoothing", false)

    apply_intro_camera_transform(0.0)
end

local function configure_game_camera_transition(cam)
    if not is_valid(cam) then return end

    -- PlayerCameraManager는 전환 대상 카메라의 TransitionSettings를 참고합니다.
    set_camera_property(cam, "Blend Time", START_BLEND_TIME)

    -- 0 = Linear, 1 = EaseIn, 2 = EaseOut, 3 = EaseInOut
    set_camera_property(cam, "Blend Function", 3)

    -- 0 = SwitchAtStart, 1 = SwitchAtHalf, 2 = SwitchAtEnd
    -- Ortho -> Perspective 전환이 튀면 2로 바꿔보세요.
    set_camera_property(cam, "Projection Switch Mode", 2)

    set_camera_property(cam, "Blend Location", true)
    set_camera_property(cam, "Blend Rotation", true)
    set_camera_property(cam, "Blend FOV", true)
    set_camera_property(cam, "Blend Ortho Width", true)

    -- 게임 카메라 자체 추적도 부드럽게 유지합니다.
    set_camera_property(cam, "Enable Smoothing", true)
    set_camera_property(cam, "Location Lag Speed", 8.0)
    set_camera_property(cam, "Rotation Lag Speed", 8.0)
    set_camera_property(cam, "FOV Lag Speed", 8.0)
    set_camera_property(cam, "Ortho Width Lag Speed", 8.0)
end

-- =========================================================
-- Camera setup
-- =========================================================

local function setup_intro_camera()
    introActor = World.FindActorByName("IntroCamera")

    if not is_valid(introActor) then
        print("[IntroCam] IntroCamera not found. Spawning runtime camera.")
        introActor = World.SpawnCamera(get_camera_location_for_target(0.0, TARGET_Y, TARGET_Z))
    end

    if not is_valid(introActor) then
        print("[IntroCam] IntroCamera actor invalid")
        return nil
    end

    local cam = introActor.Camera

    if not is_valid(cam) then
        cam = introActor:GetOrAddCamera()
    end

    if not is_valid(cam) then
        print("[IntroCam] Intro camera component invalid")
        return nil
    end

    return cam
end

local function init_intro_camera()
    print("[IntroCam] BeginPlay called")

    pc = World.GetPlayerController(0)
    if not is_valid(pc) then
        pc = World.GetOrCreatePlayerController()
    end

    if not is_valid(pc) then
        print("[IntroCam] PlayerController invalid")
        return
    end

    -- AutoWire가 이미 잡아둔 캐릭터/게임 카메라를 저장합니다.
    gameCam = pc:GetActiveCamera()

    if not is_valid(gameCam) then
        gameCam = World.GetActiveCamera()
    end

    if not is_valid(gameCam) then
        print("[IntroCam] Gameplay camera invalid")
        return
    end

    introCam = setup_intro_camera()
    if not is_valid(introCam) then
        return
    end

    sweepX = 0.0
    direction = 1.0
    bIntroActive = true
    bStarted = false
    pendingStart = false

    configure_intro_camera(introCam)
    configure_game_camera_transition(gameCam)

    -- 인트로 진입은 즉시 인트로 카메라로 고정합니다.
    pc:SetActiveCamera(introCam)
    World.SetActiveCamera(introCam)

    print("[IntroCam] Intro camera activated")
end

-- =========================================================
-- Start game
-- =========================================================

function StartGame()
    if bStarted then
        print("[IntroCam] StartGame ignored: already started")
        return
    end

    print("[IntroCam] StartGame entered from Tick")

    bStarted = true
    pendingStart = false
    bIntroActive = false

    if not is_valid(pc) then
        print("[IntroCam] StartGame failed: pc invalid")
        return
    end

    if not is_valid(gameCam) then
        print("[IntroCam] StartGame failed: gameCam invalid")
        return
    end

    configure_game_camera_transition(gameCam)

    -- 중요: 여기서 World.SetActiveCamera(gameCam)를 호출하지 않습니다.
    -- 같이 호출하면 PlayerController 블렌드를 건너뛰고 즉시 바뀔 수 있습니다.
    pc:SetActiveCameraWithBlend(gameCam)

    print("[IntroCam] StartGame: smooth blend requested")
end

-- =========================================================
-- UI
-- =========================================================

local function bind_ui()
    clear_ui_handler()

    if UI ~= nil and UI.SetEventHandler ~= nil then
        UI.SetEventHandler(function(eventName)
            print("[IntroCam] UI event = " .. tostring(eventName))

            if eventName == "start" then
                if bStarted or pendingStart then
                    print("[IntroCam] duplicate start ignored")
                    return
                end

                -- RmlUi click callback 안에서는 카메라/월드 상태를 바로 바꾸지 않습니다.
                -- 다음 Tick에서 StartGame을 실행합니다.
                pendingStart = true
            end
        end)

        print("[IntroCam] UI handler bound")
    else
        print("[IntroCam] UI binding not available")
    end
end

-- =========================================================
-- Engine entry points
-- =========================================================

function BeginPlay()
    clear_ui_handler()
    init_intro_camera()
    bind_ui()
end

function Tick(dt)
    if pendingStart then
        pendingStart = false
        StartGame()
    end

    if not bIntroActive then return end
    if not is_valid(introCam) then return end

    sweepX = sweepX + direction * SWEEP_SPEED * dt

    if sweepX > SWEEP_RANGE then
        sweepX = SWEEP_RANGE
        direction = -1.0
    elseif sweepX < -SWEEP_RANGE then
        sweepX = -SWEEP_RANGE
        direction = 1.0
    end

    apply_intro_camera_transform(sweepX)
end

function EndPlay()
    print("[IntroCam] EndPlay")

    clear_ui_handler()

    bIntroActive = false
    bStarted = false
    pendingStart = false

    introCam = nil
    gameCam = nil
    introActor = nil
    pc = nil
end
