#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

class FLoadingScreen
{
public:
    void Begin(HWND InHWnd);
    void Update(const wchar_t* StatusText, float Progress);
    void End();

private:
    void Render(const wchar_t* StatusText, float Progress);
    void PumpMessages();

    HWND HWnd = nullptr;
};
