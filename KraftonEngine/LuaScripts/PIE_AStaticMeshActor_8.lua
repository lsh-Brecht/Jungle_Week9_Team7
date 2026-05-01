function BeginPlay()
    print("[BeginPlay] UUID = " .. obj.UUID)
    obj:PrintLocation()
    
    local projectile = obj:GetOrAddProjectileMovement()
    projectile.InitialSpeed = 1.0
    projectile.MaxSpeed = 1.0
    
    Wait(10.0)
    print("After 10 seconds")
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
    obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation()
end
