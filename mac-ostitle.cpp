// ==WindhawkMod==
// @id           center-title-macos-buttons
// @name         Center Title & macOS Buttons
// @description  Centers window title and replaces standard caption buttons with macOS-like traffic lights for specified executables
// @version      1.0
// @author       Torba_Snigy
// @github       https://github.com/meshoksnega
// @include      myprogram.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Center Title & macOS Buttons
Centers the title text in the title bar and draws macOS-style traffic light buttons on the left.
Target processes are defined by @include entries. The mod can be enabled/disabled in Windhawk.
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

static std::atomic<bool> g_running(true);
static std::mutex g_mutex;
struct WNDHOOK { HWND hwnd; WNDPROC orig; };
static std::vector<WNDHOOK> g_hooks;

static BOOL IsTopLevelWindowOfThisProcess(HWND h)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId() && GetWindow(h, GW_OWNER) == NULL && IsWindowVisible(h);
}

static void InvalidateNonClient(HWND hwnd)
{
    RECT r;
    if (GetWindowRect(hwnd, &r))
        InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CALLBACK SubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_NCDESTROY)
    {
        std::lock_guard<std::mutex> lg(g_mutex);
        for (auto it = g_hooks.begin(); it != g_hooks.end(); ++it)
        {
            if (it->hwnd == hwnd)
            {
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)it->orig);
                g_hooks.erase(it);
                break;
            }
        }
    }

    if (uMsg == WM_NCHITTEST)
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int titleH = 30;
        if (pt.y >= 0 && pt.y <= titleH)
        {
            int leftPadding = 8;
            int buttonRadius = 7;
            int buttonsWidth = 3 * (buttonRadius * 2 + 6);
            if (pt.x >= leftPadding && pt.x <= leftPadding + buttonsWidth)
                return HTCLIENT;
            return HTCAPTION;
        }
    }

    if (uMsg == WM_NCLBUTTONDOWN)
    {
        POINTS ps = MAKEPOINTS(lParam);
        POINT pt = { ps.x, ps.y };
        ScreenToClient(hwnd, &pt);
        int leftPadding = 8;
        int buttonRadius = 7;
        for (int i = 0; i < 3; ++i)
        {
            RECT btn = { leftPadding + i * (buttonRadius * 2 + 6), 6, leftPadding + i * (buttonRadius * 2 + 6) + buttonRadius * 2, 6 + buttonRadius * 2 };
            if (PtInRect(&btn, pt))
            {
                if (i == 0) PostMessage(hwnd, WM_CLOSE, 0, 0);
                if (i == 1) ShowWindow(hwnd, SW_MINIMIZE);
                if (i == 2) SendMessage(hwnd, WM_SYSCOMMAND, (WPARAM)(IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE), 0);
                return 0;
            }
        }
    }

    if (uMsg == WM_NCPAINT || uMsg == WM_PAINT)
    {
        HRGN hrgn = NULL;
        HDC hdc = GetWindowDC(hwnd);
        if (hdc)
        {
            RECT wr; GetWindowRect(hwnd, &wr);
            int w = wr.right - wr.left; int h = wr.bottom - wr.top;
            RECT titleArea = { 0, 0, w, 30 };
            HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_ACTIVECAPTION));
            RECT drawRect = titleArea;
            FillRect(hdc, &drawRect, bg);
            DeleteObject(bg);

            HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT of = (HFONT)SelectObject(hdc, hf);
            WCHAR buf[512]; GetWindowTextW(hwnd, buf, 512);
            RECT txtRect = titleArea;
            txtRect.left += 60;
            txtRect.right -= 60;
            DrawTextW(hdc, buf, -1, &txtRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
            SelectObject(hdc, of);

            int leftPadding = 8;
            int r = 7;
            for (int i = 0; i < 3; ++i)
            {
                int cx = leftPadding + r + i * (r * 2 + 6);
                int cy = 6 + r;
                HBRUSH br;
                if (i == 0) br = CreateSolidBrush(RGB(255, 95, 86));
                else if (i == 1) br = CreateSolidBrush(RGB(255, 189, 46));
                else br = CreateSolidBrush(RGB(39, 201, 63));
                Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
                DeleteObject(br);
            }

            ReleaseDC(hwnd, hdc);
        }
        return 0;
    }

    return CallWindowProcW((WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC), hwnd, uMsg, wParam, lParam);
}

static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam)
{
    if (!IsTopLevelWindowOfThisProcess(hwnd)) return TRUE;
    std::lock_guard<std::mutex> lg(g_mutex);
    for (auto &h : g_hooks) if (h.hwnd == hwnd) return TRUE;
    WNDPROC orig = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)SubProc);
    if (orig)
    {
        g_hooks.push_back({ hwnd, orig });
        InvalidateNonClient(hwnd);
    }
    return TRUE;
}

extern "C" BOOL Wh_ModInit()
{
    std::thread([]
    {
        while (g_running)
        {
            EnumWindows(EnumProc, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }).detach();
    return TRUE;
}

extern "C" void Wh_ModBeforeUninit()
{
    g_running = false;
    std::lock_guard<std::mutex> lg(g_mutex);
    for (auto &h : g_hooks)
    {
        SetWindowLongPtrW(h.hwnd, GWLP_WNDPROC, (LONG_PTR)h.orig);
        InvalidateNonClient(h.hwnd);
    }
    g_hooks.clear();
}
