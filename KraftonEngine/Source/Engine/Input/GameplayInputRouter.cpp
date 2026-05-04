#include "Engine/Input/GameplayInputRouter.h"

#include "Engine/Input/InputFrame.h"
#include "GameFramework/World.h"
#include "Scripting/LuaInputLibrary.h"
#include "Scripting/LuaScriptSubsystem.h"
#include "Viewport/GameViewportClient.h"

void FGameplayInputRouter::ApplyGuiCapture(FInputFrame& InputFrame)
{
    InputFrame.ApplyGuiCapture("GameplayGuiCapture");
}

bool FGameplayInputRouter::Route(FInputFrame& InputFrame, const FGameplayInputRouteContext& Context)
{
    ApplyGuiCapture(InputFrame);

    FLuaInputLibrary::SetCurrentFrame(&InputFrame);

    bool bViewportHandledInput = false;
    if (Context.bAllowScriptInput && Context.World)
    {
        FLuaScriptSubsystem::Get().CallInput(Context.World, Context.DeltaTime);
    }

    if (Context.bAllowViewportInput && Context.ViewportClient)
    {
        bViewportHandledInput = Context.ViewportClient->Tick(Context.DeltaTime, InputFrame);
    }

    FLuaInputLibrary::ClearCurrentFrame();
    return bViewportHandledInput;
}
