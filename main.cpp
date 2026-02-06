#include    "main.h"
#include    "Application.h"
#include    <objbase.h>//COM初期化用

// エントリーポイント
int main(void)
{

#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
    HWND consoleWindow = GetConsoleWindow(); // コンソールウィンドウのハンドルを取得
    ShowWindow(consoleWindow, SW_HIDE);     // コンソールウィンドウを非表示にする
#endif//defined(DEBUG) || defined(_DEBUG)

    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr)) 
    {
       OutputDebugString("Failed to initialize COM!\n");
    }
    // アプリケーション実行
    Application app(SCREEN_WIDTH,SCREEN_HEIGHT);
    app.Run();

    CoUninitialize();

    return 0;
}