#pragma once

#pragma comment(lib, "winmm.lib")

#include <Windows.h>
#include <cstdint>
#include "../System/NonCopyable.h"

// =========================================================
// Application クラス
// Windows APIをラップし、ウィンドウの生成・破棄・メッセージループを管理する。
// コピーを禁止（NonCopyable）し、OSリソースの二重解放を防ぐ。
// =========================================================
class Application : NonCopyable {
public:
    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    Application(uint32_t width, uint32_t height);
    ~Application();

    // ---------------------------------------------------------
    // フロー制御 (Flow)
    // ---------------------------------------------------------
    void Run();

    // ---------------------------------------------------------
    // ゲッター (Getters)
    // ---------------------------------------------------------
    static uint32_t  GetWidth() { return m_Width; }
    static uint32_t  GetHeight() { return m_Height; }
    static HWND      GetWindow() { return m_hWnd; }
    static HINSTANCE GetHInstance() { return m_hInst; }

private:
    static bool InitApp();
    static void TermApp();
    static bool InitWnd();
    static void TermWnd();
    static void MainLoop();

    // ウィンドウプロシージャ（OSからのメッセージ処理）
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    static HINSTANCE m_hInst;
    static HWND      m_hWnd;
    static uint32_t  m_Width;
    static uint32_t  m_Height;
};