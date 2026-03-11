#include "FileIO.h"

// ============================================================
// 改行コード検出 (UTF-16 変換後のテキストを走査)
// 最初に見つかったパターンを採用。見つからなければ CRLF。
// ============================================================

static LineEnding DetectLineEnding(const WCHAR* pText, int nLen)
{
    for (int i = 0; i < nLen; i++) {
        if (pText[i] == L'\r') {
            if (i + 1 < nLen && pText[i + 1] == L'\n')
                return LE_CRLF;
            return LE_CR;
        }
        if (pText[i] == L'\n')
            return LE_LF;
    }
    return LE_CRLF;
}

// ============================================================
// 改行コード正規化 (→ CRLF、EDIT コントロール用)
// ============================================================

static std::vector<WCHAR> NormalizeToCRLF(const WCHAR* pText, int nLen)
{
    std::vector<WCHAR> out;
    out.reserve(nLen + nLen / 10 + 2);

    for (int i = 0; i < nLen; i++) {
        if (pText[i] == L'\r') {
            out.push_back(L'\r');
            out.push_back(L'\n');
            if (i + 1 < nLen && pText[i + 1] == L'\n')
                i++; // \r\n をひとつのセットとして扱う
        } else if (pText[i] == L'\n') {
            out.push_back(L'\r');
            out.push_back(L'\n');
        } else {
            out.push_back(pText[i]);
        }
    }
    out.push_back(L'\0');
    return out;
}

// ============================================================
// マルチバイト → UTF-16 変換ヘルパー
// ============================================================

static bool MBToWide(UINT cp, const BYTE* pData, int nLen,
                     std::vector<WCHAR>& out, bool bStrict)
{
    DWORD dwFlags = bStrict ? MB_ERR_INVALID_CHARS : 0;
    int nWide = MultiByteToWideChar(cp, dwFlags,
        (LPCCH)pData, nLen, NULL, 0);
    if (nWide <= 0) return false;
    out.resize(nWide + 1, L'\0');
    MultiByteToWideChar(cp, 0, (LPCCH)pData, nLen, &out[0], nWide);
    return true;
}

// ============================================================
// DoLoadFile
// ============================================================

bool DoLoadFile(const WCHAR* szPath,
                std::vector<WCHAR>& outContent,
                TextEncoding& outEncoding,
                LineEnding& outLineEnding)
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

    // +2: UTF-16 のヌル終端確保用
    std::vector<BYTE> buf(dwFileSize + 2, 0);
    DWORD dwRead = 0;
    BOOL bOK = ReadFile(hFile, &buf[0], dwFileSize, &dwRead, NULL);
    CloseHandle(hFile);

    if (!bOK || dwRead != dwFileSize) return false;

    const BYTE* pData     = &buf[0];
    DWORD       dwDataLen = dwRead;

    // ----------------------------------------------------------------
    // BOM 検出
    // ----------------------------------------------------------------

    // UTF-16 BE BOM: FE FF
    if (dwDataLen >= 2 && pData[0] == 0xFE && pData[1] == 0xFF) {
        outEncoding = ENC_UTF16_BE_BOM;
        pData     += 2;
        dwDataLen -= 2;
        int nWChars = (int)(dwDataLen / 2);
        std::vector<WCHAR> wbuf(nWChars + 1, L'\0');
        for (int i = 0; i < nWChars; i++) {
            wbuf[i] = (WCHAR)((pData[i * 2] << 8) | pData[i * 2 + 1]);
        }
        outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
        outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
        return true;
    }

    // UTF-16 LE BOM: FF FE
    if (dwDataLen >= 2 && pData[0] == 0xFF && pData[1] == 0xFE) {
        outEncoding = ENC_UTF16_LE_BOM;
        pData     += 2;
        dwDataLen -= 2;
        int nWChars = (int)(dwDataLen / 2);
        std::vector<WCHAR> wbuf(nWChars + 1, L'\0');
        for (int i = 0; i < nWChars; i++) {
            wbuf[i] = (WCHAR)(pData[i * 2] | (pData[i * 2 + 1] << 8));
        }
        outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
        outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
        return true;
    }

    // UTF-8 BOM: EF BB BF
    if (dwDataLen >= 3 &&
        pData[0] == 0xEF && pData[1] == 0xBB && pData[2] == 0xBF)
    {
        outEncoding = ENC_UTF8_BOM;
        pData     += 3;
        dwDataLen -= 3;
        std::vector<WCHAR> wbuf;
        if (dwDataLen == 0) {
            wbuf.push_back(L'\0');
        } else if (!MBToWide(CP_UTF8, pData, (int)dwDataLen, wbuf, false)) {
            return false;
        }
        int nWChars = (int)wbuf.size() - 1;
        outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
        outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
        return true;
    }

    // ----------------------------------------------------------------
    // BOM なし: ヒューリスティック判定 (Shift-JIS 優先)
    // ----------------------------------------------------------------

    // 空ファイル
    if (dwDataLen == 0) {
        outEncoding   = ENC_UTF8_BOM;
        outLineEnding = LE_CRLF;
        outContent.assign(1, L'\0');
        return true;
    }

    // 1. JIS: ESC シーケンス検出 (0x1B 0x24 or 0x1B 0x28)
    {
        bool bIsJIS = false;
        for (DWORD i = 0; i + 1 < dwDataLen; i++) {
            if (pData[i] == 0x1B &&
                (pData[i + 1] == 0x24 || pData[i + 1] == 0x28))
            {
                bIsJIS = true;
                break;
            }
        }
        if (bIsJIS) {
            std::vector<WCHAR> wbuf;
            if (MBToWide(50220, pData, (int)dwDataLen, wbuf, false)) {
                outEncoding   = ENC_JIS;
                int nWChars   = (int)wbuf.size() - 1;
                outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
                outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
                return true;
            }
            // JIS 変換失敗時は Shift-JIS フォールバックへ
        }
    }

    // 2. UTF-8 (BOM なし)
    {
        std::vector<WCHAR> wbuf;
        if (MBToWide(CP_UTF8, pData, (int)dwDataLen, wbuf, true)) {
            outEncoding   = ENC_UTF8;
            int nWChars   = (int)wbuf.size() - 1;
            outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
            outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
            return true;
        }
    }

    // 3. EUC-JP
    {
        std::vector<WCHAR> wbuf;
        if (MBToWide(20932, pData, (int)dwDataLen, wbuf, true)) {
            outEncoding   = ENC_EUCJP;
            int nWChars   = (int)wbuf.size() - 1;
            outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
            outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
            return true;
        }
    }

    // 4. Shift-JIS (デフォルト)
    {
        outEncoding = ENC_SJIS;
        std::vector<WCHAR> wbuf;
        if (!MBToWide(932, pData, (int)dwDataLen, wbuf, false)) {
            return false;
        }
        int nWChars   = (int)wbuf.size() - 1;
        outLineEnding = DetectLineEnding(&wbuf[0], nWChars);
        outContent    = NormalizeToCRLF(&wbuf[0], nWChars);
        return true;
    }
}

// ============================================================
// 改行コード変換 (CRLF → 指定の改行コード、保存用)
// ============================================================

static std::vector<WCHAR> ConvertLineEnding(const WCHAR* pText, int nLen,
                                             LineEnding le)
{
    std::vector<WCHAR> out;
    if (le == LE_CRLF) {
        out.assign(pText, pText + nLen);
        out.push_back(L'\0');
        return out;
    }

    out.reserve(nLen);
    for (int i = 0; i < nLen; i++) {
        if (pText[i] == L'\r' &&
            i + 1 < nLen && pText[i + 1] == L'\n')
        {
            // CRLF → target
            if (le == LE_LF)
                out.push_back(L'\n');
            else
                out.push_back(L'\r');
            i++; // \n をスキップ
        } else {
            out.push_back(pText[i]);
        }
    }
    out.push_back(L'\0');
    return out;
}

// ============================================================
// DoSaveFile
// ============================================================

bool DoSaveFile(const WCHAR* szPath,
                const WCHAR* pText, int nLen,
                TextEncoding enc, LineEnding le)
{
    // 改行コード変換
    std::vector<WCHAR> converted = ConvertLineEnding(pText, nLen, le);
    const WCHAR* pConv  = &converted[0];
    int          nConvLen = (int)converted.size() - 1; // null を除く

    std::vector<BYTE> outBuf;

    if (enc == ENC_UTF16_LE || enc == ENC_UTF16_LE_BOM) {
        // UTF-16 LE
        if (enc == ENC_UTF16_LE_BOM) {
            outBuf.push_back(0xFF);
            outBuf.push_back(0xFE);
        }
        for (int i = 0; i < nConvLen; i++) {
            WCHAR ch = pConv[i];
            outBuf.push_back((BYTE)(ch & 0xFF));
            outBuf.push_back((BYTE)((ch >> 8) & 0xFF));
        }
    } else if (enc == ENC_UTF16_BE || enc == ENC_UTF16_BE_BOM) {
        // UTF-16 BE
        if (enc == ENC_UTF16_BE_BOM) {
            outBuf.push_back(0xFE);
            outBuf.push_back(0xFF);
        }
        for (int i = 0; i < nConvLen; i++) {
            WCHAR ch = pConv[i];
            outBuf.push_back((BYTE)((ch >> 8) & 0xFF));
            outBuf.push_back((BYTE)(ch & 0xFF));
        }
    } else {
        // マルチバイト系
        UINT cp;
        switch (enc) {
        case ENC_SJIS:     cp = 932;     break;
        case ENC_JIS:      cp = 50220;   break;
        case ENC_UTF8:     cp = CP_UTF8; break;
        case ENC_UTF8_BOM: cp = CP_UTF8; break;
        case ENC_EUCJP:    cp = 20932;   break;
        default:           cp = CP_UTF8; break;
        }

        // UTF-8 BOM
        if (enc == ENC_UTF8_BOM) {
            outBuf.push_back(0xEF);
            outBuf.push_back(0xBB);
            outBuf.push_back(0xBF);
        }

        if (nConvLen > 0) {
            int nMBLen = WideCharToMultiByte(
                cp, 0, pConv, nConvLen, NULL, 0, NULL, NULL);
            if (nMBLen < 0) return false;

            int nOffset = (int)outBuf.size();
            outBuf.resize(nOffset + nMBLen);
            if (nMBLen > 0) {
                WideCharToMultiByte(
                    cp, 0, pConv, nConvLen,
                    (LPSTR)&outBuf[nOffset], nMBLen,
                    NULL, NULL);
            }
        }
    }

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
    BOOL  bOK       = TRUE;
    if (!outBuf.empty()) {
        bOK = WriteFile(hFile, &outBuf[0], (DWORD)outBuf.size(),
                        &dwWritten, NULL);
    }
    CloseHandle(hFile);

    return bOK && (dwWritten == (DWORD)outBuf.size());
}
