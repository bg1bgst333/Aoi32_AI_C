#pragma once

// ============================================================
// プラットフォーム共通定義
// 各 .cpp ファイルで windows.h より前にインクルードすること
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WINVER
#define WINVER       0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef _WIN32_IE
#define _WIN32_IE    0x0501
#endif

// ============================================================
// 文字コード・改行コード 列挙型
// ============================================================

enum TextEncoding {
    ENC_SJIS,
    ENC_JIS,
    ENC_UTF8,
    ENC_UTF8_BOM,
    ENC_UTF16_LE,
    ENC_UTF16_LE_BOM,
    ENC_UTF16_BE,
    ENC_UTF16_BE_BOM,
    ENC_EUCJP
};

enum LineEnding {
    LE_CRLF,
    LE_LF,
    LE_CR
};
