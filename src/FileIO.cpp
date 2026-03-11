#include "FileIO.h"

// UTF-8 BOM
static const BYTE UTF8_BOM[3] = { 0xEF, 0xBB, 0xBF };

// ============================================================
// DoLoadFile
// ============================================================

bool DoLoadFile(const WCHAR* szPath, std::vector<WCHAR>& outContent)
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

    // BOM 検出
    const BYTE* pData     = &buf[0];
    DWORD       dwDataLen = dwRead;
    UINT        nCodePage = CP_UTF8;

    if (dwRead >= 3 &&
        pData[0] == UTF8_BOM[0] &&
        pData[1] == UTF8_BOM[1] &&
        pData[2] == UTF8_BOM[2])
    {
        // UTF-8 BOM あり: BOM をスキップ
        pData     += 3;
        dwDataLen -= 3;
    }

    // UTF-8 → UTF-16 変換
    int nWideLen = MultiByteToWideChar(
        nCodePage, 0,
        (LPCCH)pData, (int)dwDataLen,
        NULL, 0);

    if (nWideLen <= 0) {
        // UTF-8 変換失敗 → ANSI フォールバック
        nCodePage = CP_ACP;
        nWideLen  = MultiByteToWideChar(
            nCodePage, 0,
            (LPCCH)pData, (int)dwDataLen,
            NULL, 0);
    }
    if (nWideLen <= 0) return false;

    outContent.assign(nWideLen + 1, L'\0');
    MultiByteToWideChar(
        nCodePage, 0,
        (LPCCH)pData, (int)dwDataLen,
        &outContent[0], nWideLen);

    return true;
}

// ============================================================
// DoSaveFile
// ============================================================

bool DoSaveFile(const WCHAR* szPath, const WCHAR* pText, int nLen)
{
    // UTF-16 → UTF-8 変換
    int nUtf8Len = WideCharToMultiByte(
        CP_UTF8, 0,
        pText, nLen,
        NULL, 0, NULL, NULL);
    if (nUtf8Len < 0) return false;

    // BOM (3バイト) + 本文
    std::vector<BYTE> outBuf(3 + nUtf8Len);
    outBuf[0] = UTF8_BOM[0];
    outBuf[1] = UTF8_BOM[1];
    outBuf[2] = UTF8_BOM[2];

    WideCharToMultiByte(
        CP_UTF8, 0,
        pText, nLen,
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
