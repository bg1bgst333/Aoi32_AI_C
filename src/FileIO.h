#pragma once

#include "common.h"
#include <windows.h>
#include <vector>

// ============================================================
// ファイル I/O
// UI への依存なし。テキストの読み書きのみを行う。
// ============================================================

/**
 * ファイルを読み込み、UTF-16 文字列として返す。
 * BOM 付き UTF-8 / BOM なし UTF-8 / ANSI に対応。
 */
bool DoLoadFile(const WCHAR* szPath, std::vector<WCHAR>& outContent);

/**
 * UTF-16 文字列を UTF-8 BOM 付きでファイルに書き込む。
 */
bool DoSaveFile(const WCHAR* szPath, const WCHAR* pText, int nLen);
