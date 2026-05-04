#include "Engine/Input/GameplayInputRouter.h"

#include "Engine/Input/InputFrame.h"
#include "GameFramework/World.h"
#include "Scripting/LuaInputLibrary.h"
#include "Scripting/LuaScriptSubsystem.h"
#include "Viewport/GameViewportClient.h"

void FGameplayInputRouter::ApplyGuiCapture(FInputFrame& InputFrame)
{
    if (InputFrame.IsGuiUsingKeyboard() || InputFrame.IsGuiUsingTextInput())
    {
        InputFrame.ConsumeKeyboard();
    }

    if (InputFrame.IsGuiUsingMouse())
    {
        InputFrame.ConsumeMouse();
    }
}

bool FGameplayInputRouter::Route(FInputFrame& InputFrame, const FGameplayInputRouteContext& Context)
{
    ApplyGuiCapture(InputFrame);

    FLuaInputLibrary::SetCurrentFrame(&InputFrame);

    if (Context.bAllowScriptInput && Context.World)
    {
        FLuaScriptSubsystem::Get().CallInput(Context.World, Context.DeltaTime);
    }

    bool bViewportHandledInput = false;
    if (Context.bAllowViewportInput && Context.ViewportClient)
    {
        bViewportHandledInput = Context.ViewportClient->Tick(Context.DeltaTime, InputFrame);
    }

    FLuaInputLibrary::ClearCurrentFrame();
    return bViewportHandledInput;
}
