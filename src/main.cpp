#include "MainWnd.h"
#include "resource.h"

#include <commctrl.h>

// ============================================================
// WinMain - エントリポイント
// ============================================================

int WINAPI WinMain(
    HINSTANCE hInst,
    HINSTANCE /*hPrevInst*/,
    LPSTR     /*lpCmdLine*/,
    int       nCmdShow)
{
    // コモンコントロール初期化 (ステータスバー用)
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // ウィンドウ初期化
    HWND hWnd = NULL;
    if (!InitMainWindow(hInst, nCmdShow, &hWnd)) {
        MessageBoxW(NULL,
            L"\u30a6\u30a3\u30f3\u30c9\u30a6\u306e\u521d\u671f\u5316\u306b\u5931\u6557\u3057\u307e\u3057\u305f\u3002",
            L"Aoi32_C_AI",
            MB_ICONERROR);
        return 1;
    }

    // キーボードアクセラレータ (Ctrl+N/O/S)
    ACCEL accelTable[3];
    accelTable[0].fVirt = FVIRTKEY | FCONTROL; accelTable[0].key = 'N'; accelTable[0].cmd = IDM_FILE_NEW;
    accelTable[1].fVirt = FVIRTKEY | FCONTROL; accelTable[1].key = 'O'; accelTable[1].cmd = IDM_FILE_OPEN;
    accelTable[2].fVirt = FVIRTKEY | FCONTROL; accelTable[2].key = 'S'; accelTable[2].cmd = IDM_FILE_SAVE;
    HACCEL hAccel = CreateAcceleratorTableW(accelTable, 3);

    // メッセージループ
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAcceleratorW(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // リソース解放
    if (hAccel) DestroyAcceleratorTable(hAccel);
    CleanupMainWindow();

    return (int)msg.wParam;
}
