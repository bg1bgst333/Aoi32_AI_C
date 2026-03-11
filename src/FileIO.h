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
 * BOM 検出・ヒューリスティック判定 (Shift-JIS 優先) に対応。
 * 改行コードはすべて CRLF に正規化して返す。
 */
bool DoLoadFile(const WCHAR* szPath,
                std::vector<WCHAR>& outContent,
                TextEncoding& outEncoding,
                LineEnding& outLineEnding);

/**
 * UTF-16 文字列を指定の文字コード・改行コードでファイルに書き込む。
 */
bool DoSaveFile(const WCHAR* szPath,
                const WCHAR* pText, int nLen,
                TextEncoding enc, LineEnding le);
