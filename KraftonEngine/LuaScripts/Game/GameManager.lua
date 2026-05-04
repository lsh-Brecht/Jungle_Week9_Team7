local State = require("Game.GameState")

local Config = {
    PlayerName = "Player",
    HUDTextName = "HUD_Text",
    CreditsTextName = "Credits_Text",
    StartButtonName = "StartButton",
    RestartButtonName = "RestartButton",
    DefeatY = -1000.0,
    AutoStart = false,
    Creators = {
        "KraftonEngine Team 7",
        "Programmer: replace this name",
        "Designer: replace this name"
    }
}

function BeginPlay()
    State.Configure(Config)
    State.BeginPlay()
end

function Tick(deltaTime)
    State.Tick(deltaTime)
end

function EndPlay()
    print("[GameManager] EndPlay")
end

function OnUIEvent(eventName)
    if eventName == "start" then
        State.Mode = "Ready"
        State.StartGame()

        if type(MapManager_Reset) == "function" then
            MapManager_Reset()
        end
    end
end
