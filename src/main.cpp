/**
 * Win32 テキストエディタ
 * C++03 / Win32 API
 *
 * 機能:
 *   - ファイルの新規作成・開く・上書き保存・名前を付けて保存
 *   - 元に戻す・切り取り・コピー・貼り付け・削除・すべて選択
 *   - 右端折り返し切り替え
 *   - フォント選択
 *   - UTF-8 (BOM付き) での保存 / UTF-8・ANSI ファイルの読み込み
 *   - ステータスバー (行・列 表示)
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// 最低限の Windows バージョン (XP 以降)
#ifndef WINVER
#define WINVER       0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef _WIN32_IE
#define _WIN32_IE    0x0501
#endif

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <vector>
#include <string>

#include "resource.h"

// ============================================================
// 定数
// ============================================================

static const WCHAR* const WINDOW_CLASS_NAME  = L"Aoi32_C_AI_v1";
static const WCHAR* const APP_NAME           = L"Aoi32_C_AI";
static const WCHAR* const DEFAULT_FONT_FACE  = L"Consolas";
static const int          DEFAULT_FONT_PT    = 12;

// UTF-8 BOM
static const BYTE UTF8_BOM[3] = { 0xEF, 0xBB, 0xBF };

// ============================================================
// グローバル変数
// ============================================================

static HINSTANCE g_hInst        = NULL;
static HWND      g_hMainWnd     = NULL;
static HWND      g_hEdit        = NULL;
static HWND      g_hStatus      = NULL;
static HFONT     g_hFont        = NULL;
static WCHAR     g_szFilePath[MAX_PATH] = { 0 };
static bool      g_bModified    = false;
static bool      g_bWordWrap    = false;
static bool      g_bIgnoreChange = false;   // SetWindowText 時の誤検知抑制

// ============================================================
// 前方宣言
// ============================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool RegisterWindowClass(HINSTANCE hInst);
static HWND CreateMainWindow(HINSTANCE hInst, int nCmdShow);
static HMENU CreateMainMenu();
static void  CreateEditControl(HWND hParent);
static void  CreateStatusBar(HWND hParent);
static HFONT CreateDefaultFont();
static void  ResizeControls(HWND hParent);
static void  UpdateTitle(HWND hWnd);
static void  UpdateStatusBar();
static bool  ConfirmSave(HWND hWnd);
static const WCHAR* GetFileName(const WCHAR* szPath);

// ファイル操作
static void NewFile(HWND hWnd);
static bool OpenFile(HWND hWnd);
static bool SaveFile(HWND hWnd);
static bool SaveFileAs(HWND hWnd);
static bool DoLoadFile(const WCHAR* szPath);
static bool DoSaveFile(const WCHAR* szPath);

// 書式
static void ToggleWordWrap(HWND hWnd);
static void ChooseEditorFont(HWND hWnd);

// ヘルプ
static void ShowAboutDialog(HWND hWnd);

// ============================================================
// WinMain
// ============================================================

int WINAPI WinMain(
    HINSTANCE hInst,
    HINSTANCE /*hPrevInst*/,
    LPSTR     /*lpCmdLine*/,
    int       nCmdShow)
{
    g_hInst = hInst;

    // コモンコントロール初期化 (ステータスバー用)
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    if (!RegisterWindowClass(hInst)) {
        MessageBoxW(NULL, L"ウィンドウクラスの登録に失敗しました。", APP_NAME, MB_ICONERROR);
        return 1;
    }

    HWND hWnd = CreateMainWindow(hInst, nCmdShow);
    if (!hWnd) {
        MessageBoxW(NULL, L"ウィンドウの作成に失敗しました。", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // キーボードアクセラレータ (Ctrl+N/O/S)
    ACCEL accelTable[3];
    accelTable[0].fVirt = FVIRTKEY | FCONTROL; accelTable[0].key = 'N'; accelTable[0].cmd = IDM_FILE_NEW;
    accelTable[1].fVirt = FVIRTKEY | FCONTROL; accelTable[1].key = 'O'; accelTable[1].cmd = IDM_FILE_OPEN;
    accelTable[2].fVirt = FVIRTKEY | FCONTROL; accelTable[2].key = 'S'; accelTable[2].cmd = IDM_FILE_SAVE;
    HACCEL hAccel = CreateAcceleratorTableW(accelTable, 3);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAcceleratorW(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hAccel) DestroyAcceleratorTable(hAccel);
    if (g_hFont) DeleteObject(g_hFont);

    return (int)msg.wParam;
}

// ============================================================
// ウィンドウクラス登録
// ============================================================

static bool RegisterWindowClass(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);
    return RegisterClassExW(&wc) != 0;
}

// ============================================================
// メインウィンドウ作成
// ============================================================

static HWND CreateMainWindow(HINSTANCE hInst, int nCmdShow)
{
    HMENU hMenu = CreateMainMenu();

    HWND hWnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        APP_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, hMenu, hInst, NULL);

    if (!hWnd) {
        DestroyMenu(hMenu);
        return NULL;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return hWnd;
}

// ============================================================
// メインメニュー作成 (プログラムから生成)
// ============================================================

static HMENU CreateMainMenu()
{
    HMENU hMenu = CreateMenu();

    // ファイルメニュー
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_NEW,    L"新規作成(&N)\tCtrl+N");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_OPEN,   L"開く(&O)...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_SAVE,   L"上書き保存(&S)\tCtrl+S");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_SAVEAS, L"名前を付けて保存(&A)...");
    AppendMenuW(hFile, MF_SEPARATOR, 0,               NULL);
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_EXIT,   L"終了(&X)");
    AppendMenuW(hMenu, MF_POPUP,     (UINT_PTR)hFile, L"ファイル(&F)");

    // 編集メニュー
    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_UNDO,      L"元に戻す(&U)\tCtrl+Z");
    AppendMenuW(hEdit, MF_SEPARATOR, 0,                  NULL);
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_CUT,       L"切り取り(&T)\tCtrl+X");
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_COPY,      L"コピー(&C)\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_PASTE,     L"貼り付け(&P)\tCtrl+V");
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_DELETE,    L"削除(&D)\tDel");
    AppendMenuW(hEdit, MF_SEPARATOR, 0,                  NULL);
    AppendMenuW(hEdit, MF_STRING,    IDM_EDIT_SELECTALL, L"すべて選択(&A)\tCtrl+A");
    AppendMenuW(hMenu, MF_POPUP,     (UINT_PTR)hEdit,    L"編集(&E)");

    // 書式メニュー
    HMENU hFormat = CreatePopupMenu();
    AppendMenuW(hFormat, MF_STRING, IDM_FORMAT_WORDWRAP, L"右端で折り返す(&W)");
    AppendMenuW(hFormat, MF_STRING, IDM_FORMAT_FONT,     L"フォント(&F)...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFormat, L"書式(&O)");

    // ヘルプメニュー
    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"バージョン情報(&A)...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"ヘルプ(&H)");

    return hMenu;
}

// ============================================================
// エディットコントロール作成
// ============================================================

static void CreateEditControl(HWND hParent)
{
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                    ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
    if (!g_bWordWrap) {
        dwStyle |= WS_HSCROLL | ES_AUTOHSCROLL;
    }

    g_hEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        NULL,
        dwStyle,
        0, 0, 0, 0,
        hParent,
        (HMENU)(UINT_PTR)IDC_EDIT,
        g_hInst,
        NULL);

    if (!g_hEdit) return;

    // 文字数制限を解除
    SendMessageW(g_hEdit, EM_SETLIMITTEXT, 0, 0);

    // フォントを設定
    if (!g_hFont) {
        g_hFont = CreateDefaultFont();
    }
    if (g_hFont) {
        SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    }
}

// ============================================================
// ステータスバー作成
// ============================================================

static void CreateStatusBar(HWND hParent)
{
    g_hStatus = CreateWindowExW(
        0,
        STATUSCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hParent,
        (HMENU)(UINT_PTR)IDC_STATUS,
        g_hInst,
        NULL);

    if (!g_hStatus) return;

    // パーツ幅を初期設定
    int parts[2] = { 200, -1 };
    SendMessageW(g_hStatus, SB_SETPARTS, 2, (LPARAM)parts);
}

// ============================================================
// デフォルトフォント作成
// ============================================================

static HFONT CreateDefaultFont()
{
    HDC hDC = GetDC(NULL);
    int nHeight = -MulDiv(DEFAULT_FONT_PT, GetDeviceCaps(hDC, LOGPIXELSY), 72);
    ReleaseDC(NULL, hDC);

    return CreateFontW(
        nHeight, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        DEFAULT_FONT_FACE);
}

// ============================================================
// コントロールリサイズ
// ============================================================

static void ResizeControls(HWND hParent)
{
    RECT rcClient;
    GetClientRect(hParent, &rcClient);

    int nStatusHeight = 0;
    if (g_hStatus) {
        // ステータスバーを自動リサイズ
        SendMessageW(g_hStatus, WM_SIZE, 0, 0);
        RECT rcStatus;
        GetWindowRect(g_hStatus, &rcStatus);
        nStatusHeight = rcStatus.bottom - rcStatus.top;

        // パーツ幅を更新 (右から250pxを行列表示に)
        int parts[2];
        parts[0] = rcClient.right - 250;
        if (parts[0] < 50) parts[0] = 50;
        parts[1] = -1;
        SendMessageW(g_hStatus, SB_SETPARTS, 2, (LPARAM)parts);
    }

    if (g_hEdit) {
        SetWindowPos(
            g_hEdit, NULL,
            0, 0,
            rcClient.right,
            rcClient.bottom - nStatusHeight,
            SWP_NOZORDER);
    }
}

// ============================================================
// タイトルバー更新
// ============================================================

static void UpdateTitle(HWND hWnd)
{
    WCHAR szTitle[MAX_PATH + 64];
    const WCHAR* szName = (g_szFilePath[0] != L'\0')
        ? GetFileName(g_szFilePath)
        : L"無題";

    wsprintfW(szTitle, L"%s%s - %s",
        g_bModified ? L"*" : L"",
        szName,
        APP_NAME);

    SetWindowTextW(hWnd, szTitle);
}

// ============================================================
// ステータスバー更新 (行・列)
// ============================================================

static void UpdateStatusBar()
{
    if (!g_hStatus || !g_hEdit) return;

    // 左パート: ファイルパス
    const WCHAR* szName = (g_szFilePath[0] != L'\0')
        ? g_szFilePath
        : L"無題";
    SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)szName);

    // 右パート: 行・列
    DWORD dwStart = 0, dwEnd = 0;
    SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    int nLine = (int)SendMessageW(g_hEdit, EM_LINEFROMCHAR, (WPARAM)dwStart, 0) + 1;
    int nLineIndex = (int)SendMessageW(g_hEdit, EM_LINEINDEX, (WPARAM)(nLine - 1), 0);
    int nCol  = (int)dwStart - nLineIndex + 1;

    WCHAR szPos[64];
    wsprintfW(szPos, L"  %d行  %d列", nLine, nCol);
    SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)szPos);
}

// ============================================================
// 保存確認ダイアログ
// ============================================================

static bool ConfirmSave(HWND hWnd)
{
    if (!g_bModified) return true;

    const WCHAR* szName = (g_szFilePath[0] != L'\0')
        ? GetFileName(g_szFilePath)
        : L"無題";

    WCHAR szMsg[MAX_PATH + 64];
    wsprintfW(szMsg, L"\u300c%s\u300dへの変更を保存しますか\uff1f", szName);

    int nRet = MessageBoxW(hWnd, szMsg, APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (nRet == IDYES)    return SaveFile(hWnd);
    if (nRet == IDNO)     return true;
    return false;  // IDCANCEL
}

// ============================================================
// ファイル名だけを取り出すヘルパー
// ============================================================

static const WCHAR* GetFileName(const WCHAR* szPath)
{
    const WCHAR* pLast = szPath;
    for (const WCHAR* p = szPath; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            pLast = p + 1;
        }
    }
    return pLast;
}

// ============================================================
// 新規作成
// ============================================================

static void NewFile(HWND hWnd)
{
    if (!ConfirmSave(hWnd)) return;

    g_bIgnoreChange = true;
    SetWindowTextW(g_hEdit, L"");
    g_bIgnoreChange = false;

    g_szFilePath[0] = L'\0';
    g_bModified     = false;
    UpdateTitle(hWnd);
    UpdateStatusBar();
}

// ============================================================
// ファイルを開く
// ============================================================

static bool OpenFile(HWND hWnd)
{
    if (!ConfirmSave(hWnd)) return false;

    WCHAR szPath[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hWnd;
    ofn.lpstrFilter =
        L"テキストファイル (*.txt)\0*.txt\0"
        L"すべてのファイル (*.*)\0*.*\0";
    ofn.lpstrFile   = szPath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if (!GetOpenFileNameW(&ofn)) return false;

    if (!DoLoadFile(szPath)) {
        WCHAR szMsg[MAX_PATH + 64];
        wsprintfW(szMsg, L"ファイルを開けませんでした:\n%s", szPath);
        MessageBoxW(hWnd, szMsg, APP_NAME, MB_ICONERROR);
        return false;
    }

    lstrcpyW(g_szFilePath, szPath);
    g_bModified = false;
    UpdateTitle(hWnd);
    UpdateStatusBar();
    return true;
}

// ============================================================
// 上書き保存
// ============================================================

static bool SaveFile(HWND hWnd)
{
    if (g_szFilePath[0] == L'\0') {
        return SaveFileAs(hWnd);
    }
    if (!DoSaveFile(g_szFilePath)) {
        WCHAR szMsg[MAX_PATH + 64];
        wsprintfW(szMsg, L"ファイルを保存できませんでした:\n%s", g_szFilePath);
        MessageBoxW(hWnd, szMsg, APP_NAME, MB_ICONERROR);
        return false;
    }
    g_bModified = false;
    UpdateTitle(hWnd);
    return true;
}

// ============================================================
// 名前を付けて保存
// ============================================================

static bool SaveFileAs(HWND hWnd)
{
    WCHAR szPath[MAX_PATH] = { 0 };
    if (g_szFilePath[0] != L'\0') {
        lstrcpyW(szPath, g_szFilePath);
    }

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hWnd;
    ofn.lpstrFilter =
        L"テキストファイル (*.txt)\0*.txt\0"
        L"すべてのファイル (*.*)\0*.*\0";
    ofn.lpstrFile   = szPath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if (!GetSaveFileNameW(&ofn)) return false;

    if (!DoSaveFile(szPath)) {
        WCHAR szMsg[MAX_PATH + 64];
        wsprintfW(szMsg, L"ファイルを保存できませんでした:\n%s", szPath);
        MessageBoxW(hWnd, szMsg, APP_NAME, MB_ICONERROR);
        return false;
    }

    lstrcpyW(g_szFilePath, szPath);
    g_bModified = false;
    UpdateTitle(hWnd);
    UpdateStatusBar();
    return true;
}

// ============================================================
// ファイル読み込み (UTF-8 BOM 付き / UTF-8 / ANSI 対応)
// ============================================================

static bool DoLoadFile(const WCHAR* szPath)
{
    HANDLE hFile = CreateFileW(
        szPath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<BYTE> buf(dwFileSize + 1, 0);
    DWORD dwRead = 0;
    BOOL bOK = ReadFile(hFile, &buf[0], dwFileSize, &dwRead, NULL);
    CloseHandle(hFile);

    if (!bOK || dwRead != dwFileSize) return false;

    // BOM検出
    const BYTE* pData     = &buf[0];
    DWORD       dwDataLen = dwRead;
    UINT        nCodePage = CP_ACP;

    if (dwRead >= 3 &&
        pData[0] == UTF8_BOM[0] &&
        pData[1] == UTF8_BOM[1] &&
        pData[2] == UTF8_BOM[2])
    {
        pData     += 3;
        dwDataLen -= 3;
        nCodePage  = CP_UTF8;
    } else {
        // BOM なし: UTF-8 として試みる
        nCodePage = CP_UTF8;
    }

    // バイト列 → UTF-16 変換
    int nWideLen = MultiByteToWideChar(
        nCodePage, 0,
        (LPCCH)pData, (int)dwDataLen,
        NULL, 0);

    if (nWideLen <= 0 && nCodePage == CP_UTF8) {
        // UTF-8 変換失敗 → ANSI にフォールバック
        nCodePage = CP_ACP;
        nWideLen  = MultiByteToWideChar(
            nCodePage, 0,
            (LPCCH)pData, (int)dwDataLen,
            NULL, 0);
    }
    if (nWideLen <= 0) return false;

    std::vector<WCHAR> wbuf(nWideLen + 1, L'\0');
    MultiByteToWideChar(
        nCodePage, 0,
        (LPCCH)pData, (int)dwDataLen,
        &wbuf[0], nWideLen);

    // EN_CHANGE による g_bModified 更新を抑制してからテキストをセット
    g_bIgnoreChange = true;
    SetWindowTextW(g_hEdit, &wbuf[0]);
    g_bIgnoreChange = false;

    return true;
}

// ============================================================
// ファイル保存 (UTF-8 BOM 付き)
// ============================================================

static bool DoSaveFile(const WCHAR* szPath)
{
    int nLen = GetWindowTextLengthW(g_hEdit);
    std::vector<WCHAR> wbuf(nLen + 1, L'\0');
    GetWindowTextW(g_hEdit, &wbuf[0], nLen + 1);

    // UTF-16 → UTF-8 変換
    int nUtf8Len = WideCharToMultiByte(
        CP_UTF8, 0,
        &wbuf[0], nLen,
        NULL, 0, NULL, NULL);
    if (nUtf8Len < 0) return false;

    // BOM (3バイト) + 本文
    std::vector<BYTE> outBuf(3 + nUtf8Len);
    outBuf[0] = UTF8_BOM[0];
    outBuf[1] = UTF8_BOM[1];
    outBuf[2] = UTF8_BOM[2];

    WideCharToMultiByte(
        CP_UTF8, 0,
        &wbuf[0], nLen,
        (LPSTR)&outBuf[3], nUtf8Len,
        NULL, NULL);

    HANDLE hFile = CreateFileW(
        szPath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD dwWritten = 0;
    BOOL bOK = WriteFile(hFile, &outBuf[0], (DWORD)outBuf.size(), &dwWritten, NULL);
    CloseHandle(hFile);

    return bOK && (dwWritten == (DWORD)outBuf.size());
}

// ============================================================
// 折り返し切り替え
// ============================================================

static void ToggleWordWrap(HWND hWnd)
{
    // テキストを退避
    int nLen = GetWindowTextLengthW(g_hEdit);
    std::vector<WCHAR> wbuf(nLen + 1, L'\0');
    GetWindowTextW(g_hEdit, &wbuf[0], nLen + 1);

    // カーソル位置を退避
    DWORD dwStart = 0, dwEnd = 0;
    SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    g_bWordWrap = !g_bWordWrap;

    // 書式メニューのチェックマーク更新
    HMENU hMenu   = GetMenu(hWnd);
    HMENU hFormat = GetSubMenu(hMenu, 2);
    CheckMenuItem(hFormat, IDM_FORMAT_WORDWRAP,
        MF_BYCOMMAND | (g_bWordWrap ? MF_CHECKED : MF_UNCHECKED));

    // エディットコントロールを再作成
    DestroyWindow(g_hEdit);
    g_hEdit = NULL;
    CreateEditControl(hWnd);

    // テキストを復元 (EN_CHANGE 抑制)
    g_bIgnoreChange = true;
    SetWindowTextW(g_hEdit, &wbuf[0]);
    g_bIgnoreChange = false;

    // カーソル位置を復元
    SendMessageW(g_hEdit, EM_SETSEL, (WPARAM)dwStart, (LPARAM)dwEnd);

    ResizeControls(hWnd);
    SetFocus(g_hEdit);
}

// ============================================================
// フォント選択
// ============================================================

static void ChooseEditorFont(HWND hWnd)
{
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    if (g_hFont) {
        GetObjectW(g_hFont, sizeof(lf), &lf);
    }

    CHOOSEFONTW cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = hWnd;
    cf.lpLogFont   = &lf;
    cf.Flags       = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOVERTFONTS;

    if (!ChooseFontW(&cf)) return;

    HFONT hNew = CreateFontIndirectW(&lf);
    if (!hNew) return;

    if (g_hFont) DeleteObject(g_hFont);
    g_hFont = hNew;

    if (g_hEdit) {
        SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    }
}

// ============================================================
// バージョン情報
// ============================================================

static void ShowAboutDialog(HWND hWnd)
{
    MessageBoxW(hWnd,
        L"Win32 \u30c6\u30ad\u30b9\u30c8\u30a8\u30c7\u30a3\u30bf\n"
        L"\u30d0\u30fc\u30b8\u30e7\u30f3 1.0\n\n"
        L"Win32 API / C++03\n"
        L"\u4fdd\u5b58\u5f62\u5f0f: UTF-8 (BOM\u4ed8\u304d)",
        L"\u30d0\u30fc\u30b8\u30e7\u30f3\u60c5\u5831",
        MB_OK | MB_ICONINFORMATION);
}

// ============================================================
// ウィンドウプロシージャ
// ============================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // -------------------------------------------------------
    case WM_CREATE:
        g_hMainWnd = hWnd;
        CreateEditControl(hWnd);
        CreateStatusBar(hWnd);
        UpdateTitle(hWnd);
        UpdateStatusBar();
        return 0;

    // -------------------------------------------------------
    case WM_SIZE:
        ResizeControls(hWnd);
        return 0;

    // -------------------------------------------------------
    case WM_SETFOCUS:
        if (g_hEdit) SetFocus(g_hEdit);
        return 0;

    // -------------------------------------------------------
    case WM_COMMAND:
        if (lParam != 0) {
            // コントロール通知
            if ((HWND)lParam == g_hEdit) {
                WORD wNotify = HIWORD(wParam);
                if (wNotify == EN_CHANGE) {
                    if (!g_bIgnoreChange && !g_bModified) {
                        g_bModified = true;
                        UpdateTitle(hWnd);
                    }
                    UpdateStatusBar();
                } else if (wNotify == EN_MAXTEXT) {
                    MessageBoxW(hWnd,
                        L"\u30c6\u30ad\u30b9\u30c8\u306e\u6700\u5927\u30b5\u30a4\u30ba\u306b\u9054\u3057\u307e\u3057\u305f\u3002",
                        APP_NAME, MB_ICONWARNING);
                }
            }
        } else {
            // メニューコマンド
            switch (LOWORD(wParam))
            {
            case IDM_FILE_NEW:    NewFile(hWnd);                                   break;
            case IDM_FILE_OPEN:   OpenFile(hWnd);                                  break;
            case IDM_FILE_SAVE:   SaveFile(hWnd);                                  break;
            case IDM_FILE_SAVEAS: SaveFileAs(hWnd);                                break;
            case IDM_FILE_EXIT:   SendMessageW(hWnd, WM_CLOSE, 0, 0);             break;

            case IDM_EDIT_UNDO:      SendMessageW(g_hEdit, WM_UNDO,  0, 0);       break;
            case IDM_EDIT_CUT:       SendMessageW(g_hEdit, WM_CUT,   0, 0);       break;
            case IDM_EDIT_COPY:      SendMessageW(g_hEdit, WM_COPY,  0, 0);       break;
            case IDM_EDIT_PASTE:     SendMessageW(g_hEdit, WM_PASTE, 0, 0);       break;
            case IDM_EDIT_DELETE:    SendMessageW(g_hEdit, WM_CLEAR, 0, 0);       break;
            case IDM_EDIT_SELECTALL: SendMessageW(g_hEdit, EM_SETSEL, 0, -1);     break;

            case IDM_FORMAT_WORDWRAP: ToggleWordWrap(hWnd);                        break;
            case IDM_FORMAT_FONT:     ChooseEditorFont(hWnd);                      break;

            case IDM_HELP_ABOUT:   ShowAboutDialog(hWnd);                          break;
            }
        }
        return 0;

    // -------------------------------------------------------
    case WM_INITMENUPOPUP:
    {
        // 編集メニューの項目有効/無効を動的更新
        HMENU hEditMenu = GetSubMenu(GetMenu(hWnd), 1);
        if ((HMENU)wParam == hEditMenu) {
            DWORD dwStart = 0, dwEnd = 0;
            SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);
            BOOL bHasSel  = (dwStart != dwEnd);
            BOOL bCanUndo = (BOOL)SendMessageW(g_hEdit, EM_CANUNDO, 0, 0);
            BOOL bCanPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) ||
                             IsClipboardFormatAvailable(CF_TEXT);

            EnableMenuItem(hEditMenu, IDM_EDIT_UNDO,   MF_BYCOMMAND | (bCanUndo  ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_CUT,    MF_BYCOMMAND | (bHasSel   ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_COPY,   MF_BYCOMMAND | (bHasSel   ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_PASTE,  MF_BYCOMMAND | (bCanPaste ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_DELETE, MF_BYCOMMAND | (bHasSel   ? MF_ENABLED : MF_GRAYED));
        }
        return 0;
    }

    // -------------------------------------------------------
    case WM_CLOSE:
        if (ConfirmSave(hWnd)) {
            DestroyWindow(hWnd);
        }
        return 0;

    // -------------------------------------------------------
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
