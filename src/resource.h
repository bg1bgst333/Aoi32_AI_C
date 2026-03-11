#pragma once

// ============================================================
// メニューコマンド ID
// ============================================================

// ファイルメニュー
#define IDM_FILE_NEW        1001
#define IDM_FILE_OPEN       1002
#define IDM_FILE_SAVE       1003
#define IDM_FILE_SAVEAS     1004
#define IDM_FILE_EXIT       1005

// 編集メニュー
#define IDM_EDIT_UNDO       2001
#define IDM_EDIT_CUT        2002
#define IDM_EDIT_COPY       2003
#define IDM_EDIT_PASTE      2004
#define IDM_EDIT_DELETE     2005
#define IDM_EDIT_SELECTALL  2006

// 書式メニュー
#define IDM_FORMAT_WORDWRAP  3001
#define IDM_FORMAT_FONT      3002

// 書式 > 文字コード
#define IDM_ENC_SJIS         3101
#define IDM_ENC_JIS          3102
#define IDM_ENC_UTF8         3103
#define IDM_ENC_UTF8_BOM     3104
#define IDM_ENC_UTF16_LE     3105
#define IDM_ENC_UTF16_LE_BOM 3106
#define IDM_ENC_UTF16_BE     3107
#define IDM_ENC_UTF16_BE_BOM 3108
#define IDM_ENC_EUCJP        3109

// 書式 > 改行コード
#define IDM_LE_CRLF          3201
#define IDM_LE_LF            3202
#define IDM_LE_CR            3203

// ヘルプメニュー
#define IDM_HELP_ABOUT      4001

// ============================================================
// コントロール ID
// ============================================================

#define IDC_EDIT            5001
#define IDC_STATUS          5002
