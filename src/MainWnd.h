#pragma once

#include "common.h"
#include <windows.h>

// ============================================================
// メインウィンドウ
// ============================================================

/**
 * ウィンドウクラス登録・ウィンドウ作成を行う。
 * 成功時は *phWnd に HWND を格納して true を返す。
 */
bool InitMainWindow(HINSTANCE hInst, int nCmdShow, HWND* phWnd);

/**
 * 終了時のリソース解放 (フォントなど)。
 */
void CleanupMainWindow();

// WinMain がアクセラレータ翻訳のために使用するウィンドウプロシージャ宣言
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
