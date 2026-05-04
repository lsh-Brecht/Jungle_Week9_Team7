#pragma once

#include "Engine/Input/InputSystem.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

struct FInputFrame
{
    explicit FInputFrame(const FInputSystemSnapshot& InSnapshot)
        : Snapshot(InSnapshot)
    {
    }

    const FInputSystemSnapshot& GetSnapshot() const { return Snapshot; }

    bool IsKeyConsumed(int VK) const
    {
        if (!IsValidKey(VK))
        {
            return true;
        }

        if (bKeyboardConsumed)
        {
            return true;
        }

        if (bMouseConsumed && IsMouseButtonKey(VK))
        {
            return true;
        }

        return KeyConsumed[VK];
    }

    void ConsumeKey(int VK)
    {
        if (IsValidKey(VK))
        {
            KeyConsumed[VK] = true;
        }
    }

    void ConsumeKeyboard()
    {
        bKeyboardConsumed = true;
        for (int VK = 0; VK < 256; ++VK)
        {
            if (!IsMouseButtonKey(VK))
            {
                KeyConsumed[VK] = true;
            }
        }
    }

    void ConsumeMouse()
    {
        bMouseConsumed = true;
        bMouseDeltaConsumed = true;
        bScrollConsumed = true;
        ConsumeKey(VK_LBUTTON);
        ConsumeKey(VK_RBUTTON);
        ConsumeKey(VK_MBUTTON);
        ConsumeKey(VK_XBUTTON1);
        ConsumeKey(VK_XBUTTON2);
    }

    void ConsumeMouseDelta()
    {
        bMouseDeltaConsumed = true;
    }

    void ConsumeScroll()
    {
        bScrollConsumed = true;
    }

    void ConsumeMovement()
    {
        bMovementConsumed = true;
        ConsumeKey('W');
        ConsumeKey('A');
        ConsumeKey('S');
        ConsumeKey('D');
        ConsumeKey('Q');
        ConsumeKey('E');
        ConsumeKey(VK_SPACE);
        ConsumeKey(VK_SHIFT);
        ConsumeKey(VK_LSHIFT);
        ConsumeKey(VK_RSHIFT);
        ConsumeKey(VK_CONTROL);
        ConsumeKey(VK_LCONTROL);
        ConsumeKey(VK_RCONTROL);
    }

    void ConsumeLook()
    {
        bLookConsumed = true;
        ConsumeMouseDelta();
    }

    void ConsumeAll()
    {
        ConsumeKeyboard();
        ConsumeMouse();
        bMovementConsumed = true;
        bLookConsumed = true;
    }

    bool IsMovementConsumed() const { return bMovementConsumed; }
    bool IsLookConsumed() const { return bLookConsumed; }
    bool IsKeyboardConsumed() const { return bKeyboardConsumed; }
    bool IsMouseConsumed() const { return bMouseConsumed; }

    bool IsDown(int VK) const
    {
        return IsValidKey(VK) && !IsKeyConsumed(VK) && Snapshot.KeyDown[VK];
    }

    bool WasPressed(int VK) const
    {
        return IsValidKey(VK) && !IsKeyConsumed(VK) && Snapshot.KeyPressed[VK];
    }

    bool WasReleased(int VK) const
    {
        return IsValidKey(VK) && !IsKeyConsumed(VK) && Snapshot.KeyReleased[VK];
    }

    int GetMouseDeltaX() const
    {
        return (bMouseConsumed || bMouseDeltaConsumed || bLookConsumed) ? 0 : Snapshot.MouseDeltaX;
    }

    int GetMouseDeltaY() const
    {
        return (bMouseConsumed || bMouseDeltaConsumed || bLookConsumed) ? 0 : Snapshot.MouseDeltaY;
    }

    int GetScrollDelta() const
    {
        return (bMouseConsumed || bScrollConsumed) ? 0 : Snapshot.ScrollDelta;
    }

    POINT GetMousePosition() const
    {
        return Snapshot.MousePos;
    }

    bool IsGuiUsingMouse() const { return Snapshot.bGuiUsingMouse; }
    bool IsGuiUsingKeyboard() const { return Snapshot.bGuiUsingKeyboard; }
    bool IsGuiUsingTextInput() const { return Snapshot.bGuiUsingTextInput; }
    bool IsWindowFocused() const { return Snapshot.bWindowFocused; }
    bool IsMouseCaptured() const { return Snapshot.bUsingRawMouse; }

private:
    static bool IsValidKey(int VK)
    {
        return VK >= 0 && VK < 256;
    }

    static bool IsMouseButtonKey(int VK)
    {
        return VK == VK_LBUTTON
            || VK == VK_RBUTTON
            || VK == VK_MBUTTON
            || VK == VK_XBUTTON1
            || VK == VK_XBUTTON2;
    }

private:
    FInputSystemSnapshot Snapshot;
    bool KeyConsumed[256] = {};
    bool bKeyboardConsumed = false;
    bool bMouseConsumed = false;
    bool bMouseDeltaConsumed = false;
    bool bScrollConsumed = false;
    bool bMovementConsumed = false;
    bool bLookConsumed = false;
};
