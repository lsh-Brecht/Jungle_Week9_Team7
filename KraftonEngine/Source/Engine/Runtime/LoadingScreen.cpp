#include "Engine/Runtime/LoadingScreen.h"

#include <algorithm>

void FLoadingScreen::Begin(HWND InHWnd)
{
    HWnd = InHWnd;
    Render(L"초기화 중...", 0.0f);
}

void FLoadingScreen::Update(const wchar_t* StatusText, float Progress)
{
    Render(StatusText, Progress);
    PumpMessages();
}

void FLoadingScreen::End()
{
    HWnd = nullptr;
}

void FLoadingScreen::PumpMessages()
{
    MSG Msg;
    while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
}

void FLoadingScreen::Render(const wchar_t* StatusText, float Progress)
{
    if (!HWnd)
    {
        return;
    }

    RECT ClientRect;
    GetClientRect(HWnd, &ClientRect);
    const int W = ClientRect.right;
    const int H = ClientRect.bottom;

    RECT WindowRect;
    GetWindowRect(HWnd, &WindowRect);
    HMONITOR Monitor = MonitorFromWindow(HWnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO MonInfo = { sizeof(MONITORINFO) };
    GetMonitorInfo(Monitor, &MonInfo);
    const RECT& Work = MonInfo.rcWork;

    const int VisLeft   = (std::max)(0, static_cast<int>(Work.left - WindowRect.left));
    const int VisTop    = (std::max)(0, static_cast<int>(Work.top - WindowRect.top));
    const int VisRight  = (std::min)(W, static_cast<int>(Work.right - WindowRect.left));
    const int VisBottom = (std::min)(H, static_cast<int>(Work.bottom - WindowRect.top));

    const int CX = (VisLeft + VisRight) / 2;
    const int CY = (VisTop + VisBottom) / 2;

    HDC Hdc = GetDC(HWnd);
    HDC MemDC = CreateCompatibleDC(Hdc);
    HBITMAP MemBitmap = CreateCompatibleBitmap(Hdc, W, H);
    HBITMAP OldBitmap = static_cast<HBITMAP>(SelectObject(MemDC, MemBitmap));

    // Background
    HBRUSH BgBrush = CreateSolidBrush(RGB(20, 20, 28));
    FillRect(MemDC, &ClientRect, BgBrush);
    DeleteObject(BgBrush);

    SetBkMode(MemDC, TRANSPARENT);

    // Title
    HFONT TitleFont = CreateFontW(
        52, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    SelectObject(MemDC, TitleFont);
    SetTextColor(MemDC, RGB(240, 240, 250));
    RECT TitleRect = { 0, CY - 100, W, CY - 40 };
    DrawTextW(MemDC, L"Game Tech Lab", -1, &TitleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(TitleFont);

    // Progress bar track
    const int BarW = (VisRight - VisLeft) / 3;
    const int BarH = 6;
    const int BarX = CX - BarW / 2;
    const int BarY = CY + 10;

    RECT TrackRect = { BarX, BarY, BarX + BarW, BarY + BarH };
    HBRUSH TrackBrush = CreateSolidBrush(RGB(45, 45, 60));
    FillRect(MemDC, &TrackRect, TrackBrush);
    DeleteObject(TrackBrush);

    // Progress bar fill
    const int FillW = static_cast<int>(BarW * Progress);
    if (FillW > 0)
    {
        RECT FillRect = { BarX, BarY, BarX + FillW, BarY + BarH };
        HBRUSH FillBrush = CreateSolidBrush(RGB(79, 193, 233));
        ::FillRect(MemDC, &FillRect, FillBrush);
        DeleteObject(FillBrush);
    }

    // Status text
    HFONT StatusFont = CreateFontW(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    SelectObject(MemDC, StatusFont);
    SetTextColor(MemDC, RGB(130, 140, 165));
    RECT StatusRect = { 0, BarY + BarH + 14, W, BarY + BarH + 46 };
    DrawTextW(MemDC, StatusText, -1, &StatusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(StatusFont);

    BitBlt(Hdc, 0, 0, W, H, MemDC, 0, 0, SRCCOPY);

    SelectObject(MemDC, OldBitmap);
    DeleteObject(MemBitmap);
    DeleteDC(MemDC);
    ReleaseDC(HWnd, Hdc);
}
