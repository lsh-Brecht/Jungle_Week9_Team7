local State = require("Game.GameState")

function BeginPlay()
    State.Configure()
end

function OnOverlap(otherActor)
    if State.CanStartFromWorldButton ~= nil
        and State.CanStartFromWorldButton()
        and State.IsPlayer(otherActor) then
        State.StartGame("WorldStartButton")
    end
end