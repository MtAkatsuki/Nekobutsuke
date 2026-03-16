#include "main.h"
#include "Application.h"
#include <objbase.h> // COM初期化用

// =========================================================
// エントリーポイント
// アプリケーションの初期設定（メモリリーク検知、コンソール制御、COM初期化）を行い、
// メインループへ処理を委譲する。
// =========================================================
int main(void) {

#if defined(DEBUG) || defined(_DEBUG)
    // デバッグ時はメモリリーク検出を有効化
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
    // リリースビルド時はコンソールウィンドウを非表示(SW_HIDE)にする
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) {
        ShowWindow(consoleWindow, SW_HIDE);
    }
#endif

    // COM(Component Object Model)の初期化 (マルチスレッドモデル)
    // ※ DirectXの各種コンポーネント（WIC等）を利用するために必須
    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to initialize COM!\n");
        return -1; // 防御的プログラミング：COM初期化失敗時は起動しない
    }

    // アプリケーション実行
    Application app(SCREEN_WIDTH, SCREEN_HEIGHT);
    app.Run();

    CoUninitialize();

    return 0;
}