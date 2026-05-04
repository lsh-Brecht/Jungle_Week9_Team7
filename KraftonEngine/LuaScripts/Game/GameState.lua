local MODULE_NAME = "Game.GameState"
local Vec = FVector.new

local State = package.loaded[MODULE_NAME]
if type(State) ~= "table" then
    State = {}
    package.loaded[MODULE_NAME] = State
end

local DEFAULT_CONFIG = {
    PlayerName = "Player",
    HUDTextName = "HUD_Text",
    CreditsTextName = "Credits_Text",
    StartButtonName = "StartButton",
    RestartButtonName = "RestartButton",
    DefeatY = -1000.0,
    StartLocation = nil,
    AutoStart = false,
    Creators = {
        "KraftonEngine Team 7"
    }
}

State.Config = State.Config or {}
State.Mode = State.Mode or "Boot"
State.Score = State.Score or 0
State.Elapsed = State.Elapsed or 0.0
State.bInitialized = State.bInitialized or false
State.bCreditsPrinted = State.bCreditsPrinted or false
State.CachedPlayer = State.CachedPlayer or nil
State.CachedHUDText = State.CachedHUDText or nil
State.CachedCreditsText = State.CachedCreditsText or nil

local function copy_defaults(target, defaults)
    for key, value in pairs(defaults) do
        if target[key] == nil then
            if type(value) == "table" then
                local copied = {}
                for i, item in ipairs(value) do
                    copied[i] = item
                end
                target[key] = copied
            else
                target[key] = value
            end
        end
    end
end

local function is_valid(handle)
    return handle ~= nil and handle.IsValid ~= nil and handle:IsValid()
end

local function find_actor(name)
    if name == nil or name == "" or World == nil or World.FindActorByName == nil then
        return nil
    end
    return World.FindActorByName(name)
end

local function get_possessed_player_actor()
    if World == nil or World.GetPlayerController == nil then
        return nil
    end

    local controller = World.GetPlayerController(0)
    if not is_valid(controller) or controller.GetPossessedActor == nil then
        return nil
    end

    return controller:GetPossessedActor()
end

local function get_actor_name(actor)
    if is_valid(actor) and actor.Name ~= nil then
        return actor.Name
    end
    return ""
end

local function same_actor(a, b)
    if not is_valid(a) or not is_valid(b) then
        return false
    end
    if a.UUID ~= nil and b.UUID ~= nil then
        return a.UUID == b.UUID
    end
    return get_actor_name(a) ~= "" and get_actor_name(a) == get_actor_name(b)
end

local function set_visible(actor, visible)
    if is_valid(actor) then
        actor.Visible = visible
    end
end

local function get_or_create_actor(name, location)
    local actor = find_actor(name)
    if is_valid(actor) then
        return actor
    end

    actor = SpawnActor("AActor", location or Vec(0.0, 0.0, 0.0))
    if is_valid(actor) then
        actor.Name = name
    end
    return actor
end

local function get_text_component(actor)
    if not is_valid(actor) then
        return nil
    end
    local component = actor:GetOrAddComponent("TextRender")
    if component ~= nil and component.IsValid ~= nil and component:IsValid() then
        return component
    end
    return nil
end

local function set_text(actor, text, visible)
    local component = get_text_component(actor)
    if component == nil then
        return false
    end

    component:SetProperty("Text", text)
    component:SetProperty("Visible", visible ~= false)
    return true
end

local function format_time(seconds)
    seconds = math.max(0.0, seconds or 0.0)
    local whole = math.floor(seconds)
    local minutes = math.floor(whole / 60)
    local remain = whole - minutes * 60
    return string.format("%02d:%02d", minutes, remain)
end

local function build_credit_text(reason)
    local lines = {}
    lines[#lines + 1] = "GAME OVER"
    if reason ~= nil and reason ~= "" then
        lines[#lines + 1] = "Reason: " .. tostring(reason)
    end
    lines[#lines + 1] = "Score: " .. tostring(State.Score or 0)
    lines[#lines + 1] = "Time: " .. format_time(State.Elapsed or 0.0)
    lines[#lines + 1] = ""
    lines[#lines + 1] = "Credits"

    local creators = State.Config.Creators or DEFAULT_CONFIG.Creators
    for _, name in ipairs(creators) do
        lines[#lines + 1] = "- " .. tostring(name)
    end

    lines[#lines + 1] = ""
    lines[#lines + 1] = "Press R or Enter to restart"
    return table.concat(lines, "\n")
end

function State.Configure(config)
    State.Config = State.Config or {}
    copy_defaults(State.Config, DEFAULT_CONFIG)

    if type(config) == "table" then
        for key, value in pairs(config) do
            State.Config[key] = value
        end
    end

    copy_defaults(State.Config, DEFAULT_CONFIG)
end

function State.RefreshReferences()
    copy_defaults(State.Config, DEFAULT_CONFIG)

    State.CachedPlayer = find_actor(State.Config.PlayerName)
    if not is_valid(State.CachedPlayer) then
        State.CachedPlayer = get_possessed_player_actor()
    end
    State.CachedHUDText = get_or_create_actor(State.Config.HUDTextName, Vec(0.0, 0.0, 180.0))
    State.CachedCreditsText = get_or_create_actor(State.Config.CreditsTextName, Vec(0.0, 0.0, 230.0))

    if is_valid(State.CachedCreditsText) then
        set_visible(State.CachedCreditsText, false)
    end
end

function State.GetPlayer()
    if not is_valid(State.CachedPlayer) then
        State.CachedPlayer = find_actor(State.Config.PlayerName)
    end
    if not is_valid(State.CachedPlayer) then
        State.CachedPlayer = get_possessed_player_actor()
    end
    return State.CachedPlayer
end

function State.IsPlayer(actor)
    if not is_valid(actor) then
        return false
    end

    local player = State.GetPlayer()
    if same_actor(actor, player) then
        return true
    end

    return get_actor_name(actor) == State.Config.PlayerName
end

function State.SetPlayerMovementEnabled(enabled)
    local player = State.GetPlayer()
    if not is_valid(player) then
        return
    end

    local hop = player.HopMovement
    if hop ~= nil and hop.IsValid ~= nil and hop:IsValid() then
        hop.Simulating = enabled
        if not enabled then
            hop:ClearMovementInput()
            hop:StopMovementImmediately()
        end
        return
    end

    local movement = player:GetComponent("Movement")
    if movement ~= nil and movement.IsValid ~= nil and movement:IsValid() then
        movement.Active = enabled
        movement.TickEnabled = enabled
    end
end

function State.SetMenuObjectsVisible(visible)
    set_visible(find_actor(State.Config.StartButtonName), visible)
    set_visible(find_actor(State.Config.RestartButtonName), visible)
end

function State.UpdateHUD()
    local text = "State: " .. tostring(State.Mode) ..
        "\nScore: " .. tostring(State.Score or 0) ..
        "\nTime: " .. format_time(State.Elapsed or 0.0)

    if State.Mode == "Ready" then
        text = text .. "\nPress Enter or touch StartButton"
    elseif State.Mode == "GameOver" then
        text = text .. "\nPress R or touch RestartButton"
    end

    if not set_text(State.CachedHUDText, text, true) then
        print(text)
    end
end

function State.BeginPlay()
    State.Configure(State.Config)
    State.RefreshReferences()

    if State.Config.StartLocation == nil then
        local player = State.GetPlayer()
        if is_valid(player) then
            State.Config.StartLocation = player.Location
        end
    end

    State.Score = 0
    State.Elapsed = 0.0
    State.bCreditsPrinted = false

    if State.Config.AutoStart then
        State.StartGame()
    else
        State.Mode = "Ready"
        State.SetPlayerMovementEnabled(false)
        State.SetMenuObjectsVisible(true)
        set_visible(State.CachedCreditsText, false)
        State.UpdateHUD()
    end

    State.bInitialized = true
end

function State.StartGame()
    if State.Mode == "Playing" then
        return
    end

    State.Mode = "Playing"
    State.Score = 0
    State.Elapsed = 0.0
    State.bCreditsPrinted = false

    local player = State.GetPlayer()
    if is_valid(player) and State.Config.StartLocation ~= nil then
        player.Location = State.Config.StartLocation
    end

    State.SetPlayerMovementEnabled(true)
    State.SetMenuObjectsVisible(false)
    set_visible(State.CachedCreditsText, false)
    State.UpdateHUD()
    print("[Game] Start")
end

function State.ReturnToStartScreen(reason)
    if State.Mode ~= "Playing" then
        return
    end

    State.Mode = "Ready"
    State.Score = 0
    State.Elapsed = 0.0
    State.bCreditsPrinted = false

    local player = State.GetPlayer()
    if is_valid(player) and State.Config.StartLocation ~= nil then
        player.Location = State.Config.StartLocation
    end

    State.SetPlayerMovementEnabled(false)
    State.SetMenuObjectsVisible(true)
    set_visible(State.CachedCreditsText, false)
    State.UpdateHUD()

    if reason ~= nil and reason ~= "" then
        print("[Game] ReturnToStartScreen: " .. tostring(reason))
    end
end

function State.RestartRun()
    State.StartGame()
end

function State.RestartLevel()
    if Game ~= nil and Game.Restart ~= nil then
        Game.Restart()
        return
    end
    State.RestartRun()
end

function State.GameOver(reason)
    if State.Mode == "GameOver" then
        return
    end

    State.Mode = "GameOver"
    State.SetPlayerMovementEnabled(false)
    State.SetMenuObjectsVisible(true)

    local credits = build_credit_text(reason or "Defeat")
    if not set_text(State.CachedCreditsText, credits, true) then
        print(credits)
    end

    State.UpdateHUD()

    if not State.bCreditsPrinted then
        print(credits)
        State.bCreditsPrinted = true
    end
end

function State.AddScore(amount)
    State.Score = (State.Score or 0) + (amount or 1)
    State.UpdateHUD()
end

function State.Tick(deltaTime)
    if not State.bInitialized then
        State.BeginPlay()
    end

    if State.Mode == "Ready" then
        if Input.GetKeyDown("ENTER") or Input.GetKeyDown("SPACE") then
            State.StartGame()
        end
        return
    end

    if State.Mode == "GameOver" then
        if Input.GetKeyDown("R") or Input.GetKeyDown("ENTER") then
            State.RestartRun()
        end
        return
    end

    if State.Mode ~= "Playing" then
        return
    end

    State.Elapsed = (State.Elapsed or 0.0) + (deltaTime or 0.0)

    local player = State.GetPlayer()
    if is_valid(player) then
        local location = player.Location
        if location ~= nil and location.Z ~= nil and location.Z < State.Config.DefeatY then
            State.GameOver("Fell out of stage")
            return
        end
    end

    State.UpdateHUD()
end

return State
