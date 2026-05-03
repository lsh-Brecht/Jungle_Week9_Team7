-- Player.lua
-- Pawn 액터에 LuaScriptComponent로 붙이는 기준입니다.
-- 목적:
-- 1. Lua BeginPlay / Tick이 실제로 도는지 확인
-- 2. Lua Input.GetKey / GetMouseDelta가 실제로 들어오는지 확인
-- 3. Lua가 Pawn.Location을 직접 바꾸는지 확인
-- 4. C++ ControllerInputComponent 쪽 입력이 남아 있는지 간접 확인
-- 5. 입력이 없는데 Pawn 위치가 바뀌면 외부 이동으로 표시

local Vec = FVector.new
local Rot = FRotator.new

local DEBUG = true

-- 너무 많이 찍히면 콘솔이 느려지므로 주기 제한.
local DEBUG_TICK_EVERY_FRAME = 60
local DEBUG_IDLE_EVERY_FRAME = 120
local DEBUG_MOVING_EVERY_FRAME = 10
local DEBUG_LOOK_EVERY_FRAME = 10
local DEBUG_NATIVE_INPUT_EVERY_FRAME = 60

-- true면 매 프레임 입력 상태를 더 자주 봅니다. 로그가 많아집니다.
local DEBUG_VERBOSE_INPUT = false

local Player = {
    initialized = false,

    ownerObject = nil,
    pawn = nil,
    controller = nil,
    camera = nil,

    yaw = 0.0,
    pitch = -12.0,

    moveSpeed = 10.0,
    sprintMultiplier = 2.5,
    lookSensitivity = 0.08,

    minPitch = -70.0,
    maxPitch = 70.0,

    frame = 0,

    printedInitFail = false,
    printedFirstMove = false,
    printedFirstInput = false,
    printedFirstLook = false,
    printedInputNil = false,

    lastKeySignature = "",
    lastLocation = nil,
    lastLuaMovedFrame = -999999,
    lastNativeInputDebugFrame = -999999
}

local function Debug(msg)
    if DEBUG then
        print("[Player.lua] " .. msg)
    end
end

local function DebugEvery(tag, everyFrame, msg)
    if not DEBUG then
        return
    end

    if everyFrame <= 0 then
        print("[Player.lua][" .. tag .. "] " .. msg)
        return
    end

    if Player.frame % everyFrame == 0 then
        print("[Player.lua][" .. tag .. "] " .. msg)
    end
end

local function Num(v, fallback)
    if v == nil then
        return fallback or 0.0
    end
    return v
end

local function Field(obj, upperName, lowerName, fallback)
    if obj == nil then
        return fallback or 0.0
    end

    local a = obj[upperName]
    if a ~= nil then
        return a
    end

    local b = obj[lowerName]
    if b ~= nil then
        return b
    end

    return fallback or 0.0
end

local function VecX(v)
    return Field(v, "X", "x", 0.0)
end

local function VecY(v)
    return Field(v, "Y", "y", 0.0)
end

local function VecZ(v)
    return Field(v, "Z", "z", 0.0)
end

local function RotPitch(r)
    return Field(r, "Pitch", "pitch", 0.0)
end

local function RotYaw(r)
    return Field(r, "Yaw", "yaw", 0.0)
end

local function RotRoll(r)
    return Field(r, "Roll", "roll", 0.0)
end

local function VecStr(v)
    if v == nil then
        return "nil"
    end

    return string.format("(%.3f, %.3f, %.3f)", VecX(v), VecY(v), VecZ(v))
end

local function RotStr(r)
    if r == nil then
        return "nil"
    end

    return string.format("(Pitch=%.3f, Yaw=%.3f, Roll=%.3f)", RotPitch(r), RotYaw(r), RotRoll(r))
end

local function BoolStr(v)
    if v then
        return "true"
    end
    return "false"
end

local function Clamp(v, minValue, maxValue)
    if v < minValue then return minValue end
    if v > maxValue then return maxValue end
    return v
end

local function SafeDeltaTime(dt)
    if dt == nil or dt <= 0.0 or dt > 0.1 then
        return 1.0 / 60.0
    end
    return dt
end

local function GetKey(name)
    if Input == nil then
        print("[Player.lua][INPUT_FATAL] Input table is nil")
        return false
    end

    if Input.GetKey == nil then
        print("[Player.lua][INPUT_FATAL] Input.GetKey is nil")
        return false
    end

    return Input.GetKey(name)
end

local function GetMouseDelta()
    if Input == nil or Input.GetMouseDelta == nil then
        return { x = 0.0, y = 0.0 }
    end

    local d = Input.GetMouseDelta()
    if d == nil then
        return { x = 0.0, y = 0.0 }
    end

    return d
end

local DebugInputFrame = 0
local LastInputSignature = ""

local function B(v)
    if v then return "true" end
    return "false"
end

local function SafeCallBool(fn, fallback)
    if fn == nil then
        return fallback
    end

    local ok, result = pcall(fn)
    if not ok then
        return fallback
    end

    return result
end

local function SafeMousePos()
    if Input == nil or Input.GetMousePosition == nil then
        return "nil"
    end

    local ok, pos = pcall(Input.GetMousePosition)
    if not ok or pos == nil then
        return "nil"
    end

    local x = pos.x or pos.X or 0
    local y = pos.y or pos.Y or 0

    return string.format("(%.1f, %.1f)", x, y)
end

local function SafeMouseDelta()
    if Input == nil or Input.GetMouseDelta == nil then
        return "nil"
    end

    local ok, d = pcall(Input.GetMouseDelta)
    if not ok or d == nil then
        return "nil"
    end

    local x = d.x or d.X or 0
    local y = d.y or d.Y or 0

    return string.format("(%.1f, %.1f)", x, y)
end

local function ProbeRawInput()
    DebugInputFrame = DebugInputFrame + 1

    if Input == nil then
        if DebugInputFrame % 60 == 0 then
            print("[Player.lua][INPUT_PROBE] Input=nil")
        end
        return
    end

    local w      = Input.GetKey and Input.GetKey("W") or false
    local a      = Input.GetKey and Input.GetKey("A") or false
    local s      = Input.GetKey and Input.GetKey("S") or false
    local d      = Input.GetKey and Input.GetKey("D") or false

    local lowerW = Input.GetKey and Input.GetKey("w") or false
    local lowerA = Input.GetKey and Input.GetKey("a") or false
    local lowerS = Input.GetKey and Input.GetKey("s") or false
    local lowerD = Input.GetKey and Input.GetKey("d") or false

    local space  = Input.GetKey and Input.GetKey("SPACE") or false
    local shift  = Input.GetKey and Input.GetKey("SHIFT") or false
    local ctrl   = Input.GetKey and Input.GetKey("CTRL") or false
    local esc    = Input.GetKey and Input.GetKey("ESCAPE") or false

    local mouse1 = Input.GetKey and Input.GetKey("MOUSE1") or false
    local mouse2 = Input.GetKey and Input.GetKey("MOUSE2") or false

    local windowFocused = SafeCallBool(Input.IsWindowFocused, false)
    local guiMouse = SafeCallBool(Input.IsGuiUsingMouse, false)
    local guiKeyboard = SafeCallBool(Input.IsGuiUsingKeyboard, false)
    local captured = SafeCallBool(Input.IsMouseCaptured, false)

    local signature =
        "focus=" .. B(windowFocused) ..
        " guiMouse=" .. B(guiMouse) ..
        " guiKeyboard=" .. B(guiKeyboard) ..
        " captured=" .. B(captured) ..
        " W=" .. B(w) ..
        " A=" .. B(a) ..
        " S=" .. B(s) ..
        " D=" .. B(d) ..
        " w=" .. B(lowerW) ..
        " a=" .. B(lowerA) ..
        " s=" .. B(lowerS) ..
        " d=" .. B(lowerD) ..
        " SPACE=" .. B(space) ..
        " SHIFT=" .. B(shift) ..
        " CTRL=" .. B(ctrl) ..
        " ESC=" .. B(esc) ..
        " M1=" .. B(mouse1) ..
        " M2=" .. B(mouse2) ..
        " mousePos=" .. SafeMousePos() ..
        " mouseDelta=" .. SafeMouseDelta()

    -- 상태가 바뀌면 즉시 출력
    if signature ~= LastInputSignature then
        print("[Player.lua][INPUT_PROBE_CHANGED] " .. signature)
        LastInputSignature = signature
    end

    -- 상태가 안 바뀌어도 60프레임마다 출력
    if DebugInputFrame % 60 == 0 then
        print("[Player.lua][INPUT_PROBE] " .. signature)
    end
end

local function Length3(v)
    if v == nil then
        return 0.0
    end

    local x = VecX(v)
    local y = VecY(v)
    local z = VecZ(v)

    return math.sqrt(x * x + y * y + z * z)
end

local function Distance(a, b)
    if a == nil or b == nil then
        return 0.0
    end

    local dx = VecX(a) - VecX(b)
    local dy = VecY(a) - VecY(b)
    local dz = VecZ(a) - VecZ(b)

    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

local function BuildInputState()
    local state = {
        W = GetKey("W"),
        A = GetKey("A"),
        S = GetKey("S"),
        D = GetKey("D"),

        SPACE = GetKey("SPACE"),

        CTRL = GetKey("CTRL") or GetKey("LCTRL"),
        SHIFT = GetKey("SHIFT") or GetKey("LSHIFT")
    }

    state.any =
        state.W or state.A or state.S or state.D or
        state.SPACE or state.CTRL or state.SHIFT

    state.signature =
        "W=" .. BoolStr(state.W) ..
        " A=" .. BoolStr(state.A) ..
        " S=" .. BoolStr(state.S) ..
        " D=" .. BoolStr(state.D) ..
        " SPACE=" .. BoolStr(state.SPACE) ..
        " CTRL=" .. BoolStr(state.CTRL) ..
        " SHIFT=" .. BoolStr(state.SHIFT)

    return state
end

local function DebugInputState(state)
    if not DEBUG then
        return
    end

    if state.signature ~= Player.lastKeySignature then
        Debug("[INPUT] frame=" .. tostring(Player.frame) .. " " .. state.signature)
        Player.lastKeySignature = state.signature
    elseif DEBUG_VERBOSE_INPUT then
        DebugEvery("INPUT", DEBUG_IDLE_EVERY_FRAME, state.signature)
    end

    if state.any and not Player.printedFirstInput then
        Debug("[INPUT_FIRST] Lua에서 키 입력 감지됨: " .. state.signature)
        Player.printedFirstInput = true
    end
end

local function GetControllerInputObject(controller)
    if controller == nil then
        return nil
    end

    local input = nil

    if controller.GetControllerInput ~= nil then
        input = controller:GetControllerInput()
    end

    if input == nil and controller.Input ~= nil then
        input = controller.Input
    end

    return input
end

local function DisableNativeControllerInput(controller)
    if controller == nil then
        return false
    end

    local input = GetControllerInputObject(controller)

    if input ~= nil then
        if DEBUG and Player.frame - Player.lastNativeInputDebugFrame >= DEBUG_NATIVE_INPUT_EVERY_FRAME then
            Player.lastNativeInputDebugFrame = Player.frame

            Debug(
                "[NATIVE_INPUT_DISABLED] ControllerInput exists. " ..
                "MoveSpeed=0, LookSensitivity=0, SprintMultiplier=1 적용"
            )
        end

        return true
    end

    if DEBUG and Player.frame - Player.lastNativeInputDebugFrame >= DEBUG_NATIVE_INPUT_EVERY_FRAME then
        Player.lastNativeInputDebugFrame = Player.frame
        Debug("[NATIVE_INPUT_CHECK] ControllerInput object를 찾지 못했습니다. C++ 입력 컴포넌트가 없거나 Lua에 노출되지 않았을 수 있습니다.")
    end

    return false
end

local function SetupCamera()
    if Player.pawn == nil or Player.controller == nil then
        Debug("[CAMERA_FAIL] pawn 또는 controller가 nil입니다.")
        return false
    end

    if Player.pawn.GetOrAddCamera == nil then
        Debug("[CAMERA_FAIL] pawn:GetOrAddCamera() 함수가 없습니다. Pawn Lua 바인딩 확인 필요.")
        return false
    end

    Player.camera = Player.pawn:GetOrAddCamera()

    if Player.camera == nil then
        Debug("[CAMERA_FAIL] pawn:GetOrAddCamera()가 nil을 반환했습니다.")
        return false
    end

    Player.camera.ViewMode = "ThirdPerson"
    Player.camera.UseOwnerAsTarget = true
    Player.camera.TargetOffset = Vec(0.0, 0.0, 1.0)
    Player.camera.BackDistance = 6.0
    Player.camera.Height = 2.5
    Player.camera.SideOffset = 0.0
    Player.camera.FOVDegrees = 70.0

    if Player.camera.SetTargetActor ~= nil then
        Player.camera:SetTargetActor(Player.ownerObject)
    else
        Debug("[CAMERA_WARN] camera:SetTargetActor() 함수가 없습니다.")
    end

    if Player.camera.SetAsActiveCamera ~= nil then
        Player.camera:SetAsActiveCamera()
    else
        Debug("[CAMERA_WARN] camera:SetAsActiveCamera() 함수가 없습니다.")
    end

    if Player.controller.SetActiveCamera ~= nil then
        Player.controller:SetActiveCamera(Player.camera)
    else
        Debug("[CAMERA_WARN] controller:SetActiveCamera() 함수가 없습니다.")
    end

    if World ~= nil and World.SetActiveCamera ~= nil then
        World.SetActiveCamera(Player.camera)
    else
        Debug("[CAMERA_WARN] World.SetActiveCamera() 함수가 없습니다.")
    end

    Debug("[CAMERA_OK] Camera 동적 생성/활성화 완료. ViewMode=ThirdPerson, Target=owner Pawn")

    return true
end

local function SetupController()
    if World == nil then
        Debug("[CONTROLLER_FAIL] World table이 nil입니다.")
        return false
    end

    if World.GetOrCreatePlayerController ~= nil then
        Player.controller = World.GetOrCreatePlayerController()
        Debug("[CONTROLLER] World.GetOrCreatePlayerController() 호출")
    else
        Debug("[CONTROLLER_WARN] World.GetOrCreatePlayerController() 함수가 없습니다.")
    end

    if Player.controller == nil and World.SpawnPlayerController ~= nil then
        Player.controller = World.SpawnPlayerController(Vec(0.0, 0.0, 0.0))
        Debug("[CONTROLLER] World.SpawnPlayerController() fallback 호출")
    end

    if Player.controller == nil then
        Debug("[CONTROLLER_FAIL] PlayerController 생성/획득 실패")
        return false
    end

    DisableNativeControllerInput(Player.controller)

    if Player.controller.Possess ~= nil then
        Player.controller:Possess(Player.pawn)
        Debug("[CONTROLLER_OK] controller:Possess(owner Pawn) 호출")
    else
        Debug("[CONTROLLER_FAIL] controller:Possess() 함수가 없습니다.")
        return false
    end

    if Player.controller.SetControlRotation ~= nil then
        Player.controller:SetControlRotation(Rot(Player.pitch, Player.yaw, 0.0))
        Debug("[CONTROLLER_OK] ControlRotation 초기화: " .. string.format("pitch=%.3f yaw=%.3f", Player.pitch, Player.yaw))
    else
        Debug("[CONTROLLER_WARN] controller:SetControlRotation() 함수가 없습니다.")
    end

    return true
end

local function Bootstrap()
    if Player.initialized then
        return true
    end

    Debug("[BOOT] Bootstrap 시작")

    Player.ownerObject = owner or obj

    Debug("[BOOT] owner exists=" .. BoolStr(Player.ownerObject ~= nil))

    if Player.ownerObject == nil then
        if not Player.printedInitFail then
            Debug("[BOOT_FAIL] owner가 nil입니다. 이 Lua는 Pawn 액터의 LuaScriptComponent에 붙어 있어야 합니다.")
            Player.printedInitFail = true
        end
        return false
    end

    if Player.ownerObject.AsPawn == nil then
        if not Player.printedInitFail then
            Debug("[BOOT_FAIL] owner는 있지만 owner:AsPawn() 함수가 없습니다. owner가 GameObject wrapper인지 확인 필요.")
            Player.printedInitFail = true
        end
        return false
    end

    Player.pawn = Player.ownerObject:AsPawn()

    Debug("[BOOT] owner:AsPawn() result=" .. BoolStr(Player.pawn ~= nil))

    if Player.pawn == nil then
        if not Player.printedInitFail then
            Debug("[BOOT_FAIL] owner는 존재하지만 Pawn이 아닙니다. 이 스크립트를 Pawn 액터에 붙여야 합니다.")
            Player.printedInitFail = true
        end
        return false
    end

    Debug("[PAWN] Initial Location=" .. VecStr(Player.pawn.Location))
    Debug("[PAWN] Initial Rotation=" .. RotStr(Player.pawn.Rotation))

    local currentRot = Player.pawn.Rotation
    if currentRot ~= nil then
        Player.yaw = RotYaw(currentRot)
    end

    local okController = SetupController()
    if not okController then
        return false
    end

    local okCamera = SetupCamera()
    if not okCamera then
        return false
    end

    Player.pawn.Rotation = Rot(0.0, Player.yaw, 0.0)

    if Player.controller ~= nil and Player.controller.SetControlRotation ~= nil then
        Player.controller:SetControlRotation(Rot(Player.pitch, Player.yaw, 0.0))
    end

    if Input ~= nil and Input.SetMouseCaptured ~= nil then
        Input.SetMouseCaptured(true)
        Debug("[INPUT] Input.SetMouseCaptured(true) 호출")
    else
        Debug("[INPUT_WARN] Input.SetMouseCaptured() 함수가 없습니다.")
    end

    Player.lastLocation = Player.pawn.Location
    Player.initialized = true

    Debug("[BOOT_OK] 초기화 완료: owner Pawn 사용, Controller 연결, Camera 동적 생성/활성화")
    Debug("[BOOT_OK] Lua가 이 Pawn을 제어합니다. 이동은 Player.pawn.Location 직접 대입으로 처리됩니다.")

    return true
end

local function UpdateLook(dt)
    local mouse = GetMouseDelta()

    local dx = Field(mouse, "X", "x", 0.0)
    local dy = Field(mouse, "Y", "y", 0.0)

    if dx == 0.0 and dy == 0.0 then
        return false
    end

    if not Player.printedFirstLook then
        Debug("[LOOK_FIRST] Lua에서 MouseDelta 감지됨: dx=" .. tostring(dx) .. " dy=" .. tostring(dy))
        Player.printedFirstLook = true
    end

    Player.yaw = Player.yaw + dx * Player.lookSensitivity
    Player.pitch = Clamp(
        Player.pitch + dy * Player.lookSensitivity,
        Player.minPitch,
        Player.maxPitch
    )

    if Player.controller ~= nil and Player.controller.SetControlRotation ~= nil then
        Player.controller:SetControlRotation(Rot(Player.pitch, Player.yaw, 0.0))
    end

    Player.pawn.Rotation = Rot(0.0, Player.yaw, 0.0)

    DebugEvery(
        "LOOK",
        DEBUG_LOOK_EVERY_FRAME,
        string.format(
            "frame=%d mouse=(%.3f, %.3f) yaw=%.3f pitch=%.3f pawnRot=%s",
            Player.frame,
            dx,
            dy,
            Player.yaw,
            Player.pitch,
            RotStr(Player.pawn.Rotation)
        )
    )

    return true
end

local function UpdateMovement(dt, inputState)
    local x = 0.0
    local y = 0.0
    local z = 0.0

    if inputState.W then x = x + 1.0 end
    if inputState.S then x = x - 1.0 end
    if inputState.D then y = y + 1.0 end
    if inputState.A then y = y - 1.0 end

    if inputState.SPACE then z = z + 1.0 end
    if inputState.CTRL then z = z - 1.0 end

    if x == 0.0 and y == 0.0 and z == 0.0 then
        return false
    end

    local yawRad = math.rad(Player.yaw)

    local forward = Vec(math.cos(yawRad), math.sin(yawRad), 0.0)
    local right = Vec(-math.sin(yawRad), math.cos(yawRad), 0.0)
    local up = Vec(0.0, 0.0, 1.0)

    local move = forward * x + right * y + up * z

    if Length3(move) <= 0.0001 then
        return false
    end

    move = move:Normalized()

    local speed = Player.moveSpeed
    if inputState.SHIFT then
        speed = speed * Player.sprintMultiplier
    end

    local oldLocation = Player.pawn.Location
    local newLocation = oldLocation + move * speed * dt

    Player.pawn.Location = newLocation

    Player.lastLuaMovedFrame = Player.frame

    if not Player.printedFirstMove then
        Debug("[MOVE_FIRST] Lua가 Pawn.Location을 직접 변경했습니다.")
        Debug("[MOVE_FIRST] old=" .. VecStr(oldLocation) .. " new=" .. VecStr(newLocation))
        Player.printedFirstMove = true
    end

    DebugEvery(
        "MOVE",
        DEBUG_MOVING_EVERY_FRAME,
        string.format(
            "frame=%d dt=%.5f input=(x=%.1f y=%.1f z=%.1f) speed=%.3f old=%s new=%s delta=%.6f",
            Player.frame,
            dt,
            x,
            y,
            z,
            speed,
            VecStr(oldLocation),
            VecStr(newLocation),
            Distance(oldLocation, newLocation)
        )
    )

    return true
end

local function DetectExternalMovement(inputState)
    if Player.pawn == nil then
        return
    end

    local currentLocation = Player.pawn.Location

    if Player.lastLocation == nil then
        Player.lastLocation = currentLocation
        return
    end

    local movedDistance = Distance(Player.lastLocation, currentLocation)

    if movedDistance > 0.0001 then
        local luaMovedRecently = math.abs(Player.frame - Player.lastLuaMovedFrame) <= 1

        if not luaMovedRecently then
            Debug(
                "[EXTERNAL_MOVE?] 입력/Lua 이동 직후가 아닌데 Pawn 위치가 변경되었습니다. " ..
                "C++ MovementComponent, ControllerInputComponent, Physics, 다른 Lua/Component 가능성 확인 필요. " ..
                "last=" .. VecStr(Player.lastLocation) ..
                " current=" .. VecStr(currentLocation) ..
                " delta=" .. string.format("%.6f", movedDistance) ..
                " inputAny=" .. BoolStr(inputState ~= nil and inputState.any)
            )
        end
    end

    Player.lastLocation = currentLocation
end

function BeginPlay()
    Debug("[BEGIN] BeginPlay 호출됨")
    Bootstrap()
end

function Tick(deltaTime)
    Player.frame = Player.frame + 1

    local dt = SafeDeltaTime(deltaTime)

    if not Bootstrap() then
        DebugEvery("WAIT", DEBUG_IDLE_EVERY_FRAME, "Bootstrap 실패 상태. owner/pawn/controller/camera 준비 대기 중.")
        return
    end

    DisableNativeControllerInput(Player.controller)

    if Input ~= nil and Input.SetMouseCaptured ~= nil then
        Input.SetMouseCaptured(true)
    end

    local inputState = BuildInputState()
    DebugInputState(inputState)

    local locBeforeTick = Player.pawn.Location

    local looked = UpdateLook(dt)
    local moved = UpdateMovement(dt, inputState)

    if Player.camera ~= nil then
        if Player.controller ~= nil and Player.controller.SetActiveCamera ~= nil then
            Player.controller:SetActiveCamera(Player.camera)
        end

        if World ~= nil and World.SetActiveCamera ~= nil then
            World.SetActiveCamera(Player.camera)
        end
    end

    DetectExternalMovement(inputState)

    DebugEvery(
        "TICK",
        DEBUG_TICK_EVERY_FRAME,
        string.format(
            "frame=%d dt=%.5f luaAlive=true inputAny=%s movedByLua=%s lookedByLua=%s locBefore=%s locAfter=%s yaw=%.3f pitch=%.3f",
            Player.frame,
            dt,
            BoolStr(inputState.any),
            BoolStr(moved),
            BoolStr(looked),
            VecStr(locBeforeTick),
            VecStr(Player.pawn.Location),
            Player.yaw,
            Player.pitch
        )
    )
end

function EndPlay()
    if Input ~= nil and Input.SetMouseCaptured ~= nil then
        Input.SetMouseCaptured(false)
    end

    Debug("[END] EndPlay 호출됨")
end