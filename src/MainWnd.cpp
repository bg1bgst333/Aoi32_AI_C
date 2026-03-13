#include "MainWnd.h"
#include "FileIO.h"
#include "resource.h"

#include <commdlg.h>
#include <commctrl.h>
#include <vector>

// ============================================================
// 定数
// ============================================================

static const WCHAR* const WINDOW_CLASS_NAME = L"Aoi32_AI_C_v1";
static const WCHAR* const APP_NAME          = L"Aoi32_AI_C";
static const WCHAR* const DEFAULT_FONT_FACE = L"Consolas";
static const int          DEFAULT_FONT_PT   = 12;

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
static bool      g_bIgnoreChange = false;
static TextEncoding g_enc        = ENC_UTF8_BOM;
static LineEnding   g_le         = LE_CRLF;

// ============================================================
// 前方宣言 (このファイル内のみ)
// ============================================================

static bool RegisterMainWindowClass(HINSTANCE hInst);
static HWND CreateMainWindowImpl(HINSTANCE hInst, int nCmdShow);
static HMENU CreateMainMenu();
static void  CreateEditControl(HWND hParent);
static void  CreateStatusBar(HWND hParent);
static HFONT CreateDefaultFont();
static void  ResizeControls(HWND hParent);
static void  UpdateTitle(HWND hWnd);
static void  UpdateStatusBar();
static bool  ConfirmSave(HWND hWnd);
static const WCHAR* GetFileName(const WCHAR* szPath);

static void NewFile(HWND hWnd);
static bool OpenFile(HWND hWnd);
static bool SaveFile(HWND hWnd);
static bool SaveFileAs(HWND hWnd);
static bool PerformSave(HWND hWnd, const WCHAR* szPath);

static void ToggleWordWrap(HWND hWnd);
static void ChooseEditorFont(HWND hWnd);
static void ShowAboutDialog(HWND hWnd);

// ============================================================
// 公開関数: InitMainWindow
// ============================================================

bool InitMainWindow(HINSTANCE hInst, int nCmdShow, HWND* phWnd)
{
    g_hInst = hInst;

    if (!RegisterMainWindowClass(hInst)) return false;

    HWND hWnd = CreateMainWindowImpl(hInst, nCmdShow);
    if (!hWnd) return false;

    if (phWnd) *phWnd = hWnd;
    return true;
}

// ============================================================
// 公開関数: CleanupMainWindow
// ============================================================

void CleanupMainWindow()
{
    if (g_hFont) {
        DeleteObject(g_hFont);
        g_hFont = NULL;
    }
}

// ============================================================
// ウィンドウクラス登録
// ============================================================

static bool RegisterMainWindowClass(HINSTANCE hInst)
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

static HWND CreateMainWindowImpl(HINSTANCE hInst, int nCmdShow)
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
// メインメニュー作成
// ============================================================

static HMENU CreateMainMenu()
{
    HMENU hMenu = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_NEW,    L"新規作成(&N)\tCtrl+N");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_OPEN,   L"開く(&O)...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_SAVE,   L"上書き保存(&S)\tCtrl+S");
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_SAVEAS, L"名前を付けて保存(&A)...");
    AppendMenuW(hFile, MF_SEPARATOR, 0,               NULL);
    AppendMenuW(hFile, MF_STRING,    IDM_FILE_EXIT,   L"終了(&X)");
    AppendMenuW(hMenu, MF_POPUP,     (UINT_PTR)hFile, L"ファイル(&F)");

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

    HMENU hFormat = CreatePopupMenu();
    AppendMenuW(hFormat, MF_STRING,    IDM_FORMAT_WORDWRAP, L"右端で折り返す(&W)");
    AppendMenuW(hFormat, MF_STRING,    IDM_FORMAT_FONT,     L"フォント(&F)...");
    AppendMenuW(hFormat, MF_SEPARATOR, 0,                   NULL);

    HMENU hEnc = CreatePopupMenu();
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_SJIS,         L"Shift-JIS");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_JIS,          L"JIS (ISO-2022-JP)");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_UTF8,         L"UTF-8");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_UTF8_BOM,     L"UTF-8 (BOM 付き)");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_UTF16_LE,     L"UTF-16 LE");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_UTF16_LE_BOM, L"UTF-16 LE (BOM 付き)");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_UTF16_BE,     L"UTF-16 BE");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_UTF16_BE_BOM, L"UTF-16 BE (BOM 付き)");
    AppendMenuW(hEnc, MF_STRING, IDM_ENC_EUCJP,        L"EUC-JP");
    AppendMenuW(hFormat, MF_POPUP, (UINT_PTR)hEnc, L"文字コード(&E)");

    HMENU hLe = CreatePopupMenu();
    AppendMenuW(hLe, MF_STRING, IDM_LE_CRLF, L"CRLF (Windows)");
    AppendMenuW(hLe, MF_STRING, IDM_LE_LF,   L"LF (Unix)");
    AppendMenuW(hLe, MF_STRING, IDM_LE_CR,   L"CR (Mac)");
    AppendMenuW(hFormat, MF_POPUP, (UINT_PTR)hLe, L"改行コード(&L)");

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFormat, L"書式(&O)");

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

    SendMessageW(g_hEdit, EM_SETLIMITTEXT, 0, 0);

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

    int parts[3] = { 200, 400, -1 };
    SendMessageW(g_hStatus, SB_SETPARTS, 3, (LPARAM)parts);
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
        SendMessageW(g_hStatus, WM_SIZE, 0, 0);
        RECT rcStatus;
        GetWindowRect(g_hStatus, &rcStatus);
        nStatusHeight = rcStatus.bottom - rcStatus.top;

        int parts[3];
        parts[0] = rcClient.right - 350;
        if (parts[0] < 50) parts[0] = 50;
        parts[1] = rcClient.right - 160;
        if (parts[1] < parts[0] + 50) parts[1] = parts[0] + 50;
        parts[2] = -1;
        SendMessageW(g_hStatus, SB_SETPARTS, 3, (LPARAM)parts);
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
// ステータスバー更新
// ============================================================

static const WCHAR* EncName(TextEncoding enc)
{
    switch (enc) {
    case ENC_SJIS:         return L"Shift-JIS";
    case ENC_JIS:          return L"JIS";
    case ENC_UTF8:         return L"UTF-8";
    case ENC_UTF8_BOM:     return L"UTF-8 BOM";
    case ENC_UTF16_LE:     return L"UTF-16 LE";
    case ENC_UTF16_LE_BOM: return L"UTF-16 LE BOM";
    case ENC_UTF16_BE:     return L"UTF-16 BE";
    case ENC_UTF16_BE_BOM: return L"UTF-16 BE BOM";
    case ENC_EUCJP:        return L"EUC-JP";
    default:               return L"UTF-8 BOM";
    }
}

static const WCHAR* LeName(LineEnding le)
{
    switch (le) {
    case LE_CRLF: return L"CRLF";
    case LE_LF:   return L"LF";
    case LE_CR:   return L"CR";
    default:      return L"CRLF";
    }
}

static void UpdateStatusBar()
{
    if (!g_hStatus || !g_hEdit) return;

    const WCHAR* szName = (g_szFilePath[0] != L'\0')
        ? g_szFilePath
        : L"無題";
    SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)szName);

    DWORD dwStart = 0, dwEnd = 0;
    SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    int nLine      = (int)SendMessageW(g_hEdit, EM_LINEFROMCHAR, (WPARAM)dwStart, 0) + 1;
    int nLineIndex = (int)SendMessageW(g_hEdit, EM_LINEINDEX, (WPARAM)(nLine - 1), 0);
    int nCol       = (int)dwStart - nLineIndex + 1;

    WCHAR szPos[64];
    wsprintfW(szPos, L"  %d行  %d列", nLine, nCol);
    SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)szPos);

    WCHAR szEncLe[64];
    wsprintfW(szEncLe, L"  %s  %s", EncName(g_enc), LeName(g_le));
    SendMessageW(g_hStatus, SB_SETTEXTW, 2, (LPARAM)szEncLe);
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
    wsprintfW(szMsg, L"\u300c%s\u300d\u3078\u306e\u5909\u66f4\u3092\u4fdd\u5b58\u3057\u307e\u3059\u304b\uff1f", szName);

    int nRet = MessageBoxW(hWnd, szMsg, APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (nRet == IDYES)  return SaveFile(hWnd);
    if (nRet == IDNO)   return true;
    return false;
}

// ============================================================
// ファイル名取得ヘルパー
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
// ファイル操作
// ============================================================

static void NewFile(HWND hWnd)
{
    if (!ConfirmSave(hWnd)) return;

    g_bIgnoreChange = true;
    SetWindowTextW(g_hEdit, L"");
    g_bIgnoreChange = false;

    g_szFilePath[0] = L'\0';
    g_bModified     = false;
    g_enc           = ENC_UTF8_BOM;
    g_le            = LE_CRLF;
    UpdateTitle(hWnd);
    UpdateStatusBar();
}

static bool OpenFile(HWND hWnd)
{
    if (!ConfirmSave(hWnd)) return false;

    WCHAR szPath[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hWnd;
    ofn.lpstrFilter =
        L"\u30c6\u30ad\u30b9\u30c8\u30d5\u30a1\u30a4\u30eb (*.txt)\0*.txt\0"
        L"\u3059\u3079\u3066\u306e\u30d5\u30a1\u30a4\u30eb (*.*)\0*.*\0";
    ofn.lpstrFile   = szPath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if (!GetOpenFileNameW(&ofn)) return false;

    std::vector<WCHAR> content;
    TextEncoding newEnc = ENC_UTF8_BOM;
    LineEnding   newLe  = LE_CRLF;
    if (!DoLoadFile(szPath, content, newEnc, newLe)) {
        WCHAR szMsg[MAX_PATH + 64];
        wsprintfW(szMsg,
            L"\u30d5\u30a1\u30a4\u30eb\u3092\u958b\u3051\u307e\u305b\u3093\u3067\u3057\u305f:\n%s",
            szPath);
        MessageBoxW(hWnd, szMsg, APP_NAME, MB_ICONERROR);
        return false;
    }
    g_enc = newEnc;
    g_le  = newLe;

    g_bIgnoreChange = true;
    SetWindowTextW(g_hEdit, &content[0]);
    g_bIgnoreChange = false;

    lstrcpyW(g_szFilePath, szPath);
    g_bModified = false;
    UpdateTitle(hWnd);
    UpdateStatusBar();
    return true;
}

static bool SaveFile(HWND hWnd)
{
    if (g_szFilePath[0] == L'\0') {
        return SaveFileAs(hWnd);
    }
    return PerformSave(hWnd, g_szFilePath);
}

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
        L"\u30c6\u30ad\u30b9\u30c8\u30d5\u30a1\u30a4\u30eb (*.txt)\0*.txt\0"
        L"\u3059\u3079\u3066\u306e\u30d5\u30a1\u30a4\u30eb (*.*)\0*.*\0";
    ofn.lpstrFile   = szPath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if (!GetSaveFileNameW(&ofn)) return false;

    if (!PerformSave(hWnd, szPath)) return false;

    lstrcpyW(g_szFilePath, szPath);
    UpdateStatusBar();
    return true;
}

// エディットコントロールからテキストを取得して DoSaveFile を呼ぶ
static bool PerformSave(HWND hWnd, const WCHAR* szPath)
{
    int nLen = GetWindowTextLengthW(g_hEdit);
    std::vector<WCHAR> wbuf(nLen + 1, L'\0');
    GetWindowTextW(g_hEdit, &wbuf[0], nLen + 1);

    if (!DoSaveFile(szPath, &wbuf[0], nLen, g_enc, g_le)) {
        WCHAR szMsg[MAX_PATH + 64];
        wsprintfW(szMsg,
            L"\u30d5\u30a1\u30a4\u30eb\u3092\u4fdd\u5b58\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f:\n%s",
            szPath);
        MessageBoxW(hWnd, szMsg, APP_NAME, MB_ICONERROR);
        return false;
    }

    g_bModified = false;
    UpdateTitle(hWnd);
    return true;
}

// ============================================================
// 折り返し切り替え
// ============================================================

static void ToggleWordWrap(HWND hWnd)
{
    int nLen = GetWindowTextLengthW(g_hEdit);
    std::vector<WCHAR> wbuf(nLen + 1, L'\0');
    GetWindowTextW(g_hEdit, &wbuf[0], nLen + 1);

    DWORD dwStart = 0, dwEnd = 0;
    SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

    g_bWordWrap = !g_bWordWrap;

    HMENU hMenu   = GetMenu(hWnd);
    HMENU hFormat = GetSubMenu(hMenu, 2);
    CheckMenuItem(hFormat, IDM_FORMAT_WORDWRAP,
        MF_BYCOMMAND | (g_bWordWrap ? MF_CHECKED : MF_UNCHECKED));

    DestroyWindow(g_hEdit);
    g_hEdit = NULL;
    CreateEditControl(hWnd);

    g_bIgnoreChange = true;
    SetWindowTextW(g_hEdit, &wbuf[0]);
    g_bIgnoreChange = false;

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
        L"Aoi32_AI_C\n"
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
    case WM_CREATE:
        g_hMainWnd = hWnd;
        CreateEditControl(hWnd);
        CreateStatusBar(hWnd);
        UpdateTitle(hWnd);
        UpdateStatusBar();
        return 0;

    case WM_SIZE:
        ResizeControls(hWnd);
        return 0;

    case WM_SETFOCUS:
        if (g_hEdit) SetFocus(g_hEdit);
        return 0;

    case WM_COMMAND:
        if (lParam != 0) {
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
            switch (LOWORD(wParam))
            {
            case IDM_FILE_NEW:    NewFile(hWnd);                               break;
            case IDM_FILE_OPEN:   OpenFile(hWnd);                              break;
            case IDM_FILE_SAVE:   SaveFile(hWnd);                              break;
            case IDM_FILE_SAVEAS: SaveFileAs(hWnd);                            break;
            case IDM_FILE_EXIT:   SendMessageW(hWnd, WM_CLOSE, 0, 0);         break;

            case IDM_EDIT_UNDO:      SendMessageW(g_hEdit, WM_UNDO,  0, 0);   break;
            case IDM_EDIT_CUT:       SendMessageW(g_hEdit, WM_CUT,   0, 0);   break;
            case IDM_EDIT_COPY:      SendMessageW(g_hEdit, WM_COPY,  0, 0);   break;
            case IDM_EDIT_PASTE:     SendMessageW(g_hEdit, WM_PASTE, 0, 0);   break;
            case IDM_EDIT_DELETE:    SendMessageW(g_hEdit, WM_CLEAR, 0, 0);   break;
            case IDM_EDIT_SELECTALL: SendMessageW(g_hEdit, EM_SETSEL, 0, -1); break;

            case IDM_FORMAT_WORDWRAP: ToggleWordWrap(hWnd);    break;
            case IDM_FORMAT_FONT:     ChooseEditorFont(hWnd);  break;

            case IDM_ENC_SJIS:         g_enc = ENC_SJIS;         UpdateStatusBar(); break;
            case IDM_ENC_JIS:          g_enc = ENC_JIS;          UpdateStatusBar(); break;
            case IDM_ENC_UTF8:         g_enc = ENC_UTF8;         UpdateStatusBar(); break;
            case IDM_ENC_UTF8_BOM:     g_enc = ENC_UTF8_BOM;     UpdateStatusBar(); break;
            case IDM_ENC_UTF16_LE:     g_enc = ENC_UTF16_LE;     UpdateStatusBar(); break;
            case IDM_ENC_UTF16_LE_BOM: g_enc = ENC_UTF16_LE_BOM; UpdateStatusBar(); break;
            case IDM_ENC_UTF16_BE:     g_enc = ENC_UTF16_BE;     UpdateStatusBar(); break;
            case IDM_ENC_UTF16_BE_BOM: g_enc = ENC_UTF16_BE_BOM; UpdateStatusBar(); break;
            case IDM_ENC_EUCJP:        g_enc = ENC_EUCJP;        UpdateStatusBar(); break;

            case IDM_LE_CRLF: g_le = LE_CRLF; UpdateStatusBar(); break;
            case IDM_LE_LF:   g_le = LE_LF;   UpdateStatusBar(); break;
            case IDM_LE_CR:   g_le = LE_CR;   UpdateStatusBar(); break;

            case IDM_HELP_ABOUT: ShowAboutDialog(hWnd); break;
            }
        }
        return 0;

    case WM_INITMENUPOPUP:
    {
        HMENU hMainMenu   = GetMenu(hWnd);
        HMENU hEditMenu   = GetSubMenu(hMainMenu, 1);
        HMENU hFormatMenu = GetSubMenu(hMainMenu, 2);
        // 書式メニュー: 0=折り返し, 1=フォント, 2=Sep, 3=文字コード, 4=改行コード
        HMENU hEncMenu    = GetSubMenu(hFormatMenu, 3);
        HMENU hLeMenu     = GetSubMenu(hFormatMenu, 4);

        if ((HMENU)wParam == hEditMenu) {
            DWORD dwStart = 0, dwEnd = 0;
            SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);
            BOOL bHasSel   = (dwStart != dwEnd);
            BOOL bCanUndo  = (BOOL)SendMessageW(g_hEdit, EM_CANUNDO, 0, 0);
            BOOL bCanPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) ||
                             IsClipboardFormatAvailable(CF_TEXT);

            EnableMenuItem(hEditMenu, IDM_EDIT_UNDO,   MF_BYCOMMAND | (bCanUndo  ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_CUT,    MF_BYCOMMAND | (bHasSel   ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_COPY,   MF_BYCOMMAND | (bHasSel   ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_PASTE,  MF_BYCOMMAND | (bCanPaste ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(hEditMenu, IDM_EDIT_DELETE, MF_BYCOMMAND | (bHasSel   ? MF_ENABLED : MF_GRAYED));
        } else if ((HMENU)wParam == hEncMenu) {
            // 文字コードチェックマーク
            static const UINT encIds[] = {
                IDM_ENC_SJIS, IDM_ENC_JIS,
                IDM_ENC_UTF8, IDM_ENC_UTF8_BOM,
                IDM_ENC_UTF16_LE, IDM_ENC_UTF16_LE_BOM,
                IDM_ENC_UTF16_BE, IDM_ENC_UTF16_BE_BOM,
                IDM_ENC_EUCJP
            };
            static const TextEncoding encVals[] = {
                ENC_SJIS, ENC_JIS,
                ENC_UTF8, ENC_UTF8_BOM,
                ENC_UTF16_LE, ENC_UTF16_LE_BOM,
                ENC_UTF16_BE, ENC_UTF16_BE_BOM,
                ENC_EUCJP
            };
            for (int i = 0; i < 9; i++) {
                CheckMenuItem(hEncMenu, encIds[i],
                    MF_BYCOMMAND | (g_enc == encVals[i] ? MF_CHECKED : MF_UNCHECKED));
            }
        } else if ((HMENU)wParam == hLeMenu) {
            // 改行コードチェックマーク
            CheckMenuItem(hLeMenu, IDM_LE_CRLF,
                MF_BYCOMMAND | (g_le == LE_CRLF ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(hLeMenu, IDM_LE_LF,
                MF_BYCOMMAND | (g_le == LE_LF   ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(hLeMenu, IDM_LE_CR,
                MF_BYCOMMAND | (g_le == LE_CR   ? MF_CHECKED : MF_UNCHECKED));
        }
        return 0;
    }

    case WM_CLOSE:
        if (ConfirmSave(hWnd)) {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
