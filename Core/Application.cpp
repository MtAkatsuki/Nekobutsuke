#include "Application.h"
#include "../System/imgui/imgui_impl_win32.h"
#include "Game.h"
#include <stdexcept>
#include "DebugLog.h"

namespace {
    // --- 設定・定数 (Magic Numbers) ---
    constexpr auto WINDOW_CLASS_NAME = TEXT("NekoButsuke");
    constexpr auto WINDOW_TITLE = TEXT("NekoButsuke");
    constexpr UINT TIMER_RESOLUTION_MS = 1; // Windowsの高精度タイマーの分解能(ms)
}

// --- 静的メンバ変数の実体 ---
HINSTANCE Application::m_hInst = nullptr;
HWND      Application::m_hWnd = nullptr;
uint32_t  Application::m_Width = 0;
uint32_t  Application::m_Height = 0;

// ImGuiのWin32プロシージャハンドラ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Application::Application(uint32_t width, uint32_t height) {
    m_Width = width;
    m_Height = height;

    // Sleep等の精度を上げるため、OSのタイマー分解能を最小値(1ms)に設定
    timeBeginPeriod(TIMER_RESOLUTION_MS);
}

Application::~Application() {
    timeEndPeriod(TIMER_RESOLUTION_MS);
}

void Application::Run() {
    if (InitApp()) {
        MainLoop();
    }
    TermApp();
}

bool Application::InitApp() {
    return InitWnd();
}

void Application::TermApp() {
    TermWnd();
}

bool Application::InitWnd(){
    // インスタンスハンドルを取得.
    HINSTANCE hInst = GetModuleHandle(nullptr);
    if (!hInst) return false;

    // ウィンドウの設定.
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wc.hCursor = LoadCursor(hInst, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);

    // ウィンドウの登録.
    if (!RegisterClassEx(&wc)) return false;

    // インスタンスハンドル設定.
    m_hInst = hInst;

    // クライアント領域（描画エリア）が指定解像度になるようウィンドウサイズを逆算
    RECT rc = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rc, style, FALSE);

    // ウィンドウを生成.
    m_hWnd = CreateWindowEx(
        0, WINDOW_CLASS_NAME, WINDOW_TITLE, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, m_hInst, nullptr
    );

    if (!m_hWnd) return false;

    // ウィンドウを表示.
    ShowWindow(m_hWnd, SW_SHOWNORMAL);

    // ウィンドウを更新.
    UpdateWindow(m_hWnd);

    // ウィンドウにフォーカスを設定.
    SetFocus(m_hWnd);

    // 正常終了.
    return true;
}

void Application::TermWnd()
{
    // ウィンドウの登録を解除.
    if (m_hInst)
    {
        UnregisterClass(WINDOW_CLASS_NAME, m_hInst);
    }

    m_hInst = nullptr;
    m_hWnd = nullptr;
}

void Application::MainLoop()
{
    MSG msg = {};

    try
    {
        DBG_ERROR("Calling gameinit()...");
        gameinit();
        DBG_ERROR("gameinit() finished successfully.");
    }
    catch (const std::exception& e)
    {
        DBG_ERROR("!!! FATAL ERROR !!!");
        DBG_ERROR("Exception caught: " << e.what());
        system("PAUSE"); 
        return; 
    }
    catch (...)
    {
        DBG_ERROR("!!! UNKNOWN ERROR !!!");
        system("PAUSE");
        return;
    }

    // Windowsメッセージループ
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // OSからのメッセージ処理がない空き時間にゲームループを回す
            gameloop();
        }
    }

    gamedispose();

}

LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // ImGuiにイベントを渡す
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp))
        return true;

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return DefWindowProc(hWnd, msg, wp, lp);
}
