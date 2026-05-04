local State = require("Game.GameState")

function BeginPlay()
    State.Configure()
end

function OnOverlap(otherActor)
    if State.IsPlayer(otherActor) then
        State.StartGame()
    end
end
