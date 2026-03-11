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
