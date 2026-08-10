#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <wininet.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "text_normalization.h"

class ToolTypeApp;
static ToolTypeApp* g_app = nullptr;

namespace {

constexpr int kWindowWidth = 312;
constexpr int kExpandedWindowWidth = 410;
constexpr int kWindowHeight = 282;
constexpr int kLineHeight = 18;
constexpr int kScrollSize = 8;
constexpr int kMinScrollThumb = 22;
constexpr BYTE kWindowAlpha = 236;
constexpr COLORREF kBackColor = RGB(0, 0, 0);
constexpr COLORREF kPanelColor = RGB(0, 0, 0);
constexpr COLORREF kButtonColor = RGB(14, 14, 14);
constexpr COLORREF kButtonHoverColor = RGB(255, 255, 255);
constexpr COLORREF kBorderColor = RGB(130, 140, 143);
constexpr COLORREF kTextColor = RGB(255, 255, 255);
constexpr COLORREF kMutedTextColor = RGB(205, 214, 216);
constexpr COLORREF kSelectColor = RGB(0, 120, 215);
constexpr COLORREF kScrollTrackColor = RGB(10, 10, 10);
constexpr COLORREF kScrollThumbColor = RGB(86, 92, 94);
constexpr COLORREF kScrollThumbHotColor = RGB(255, 255, 255);
constexpr COLORREF kCommentTextColor = RGB(135, 142, 145);
constexpr COLORREF kStarTextColor = RGB(158, 255, 176);
constexpr COLORREF kDashTextColor = RGB(145, 210, 255);
constexpr COLORREF kQuoteTextColor = RGB(255, 236, 145);
constexpr COLORREF kTooltipBackColor = RGB(18, 18, 18);
constexpr COLORREF kTooltipBorderColor = RGB(255, 255, 255);
constexpr UINT WM_APP_PASTE_NEXT = WM_APP + 24;
constexpr UINT WM_APP_UPDATE_CHECK_DONE = WM_APP + 25;
constexpr UINT WM_APP_PTS_PROGRESS = WM_APP + 26;
constexpr UINT WM_APP_PTS_OPERATION_DONE = WM_APP + 27;

constexpr const wchar_t* kToolVersion = L"1.0";
constexpr const wchar_t* kUpdateCheckUrl =
    L"https://github.com/dex593/tool-type/raw/refs/heads/master/check.ini";
constexpr const wchar_t* kLatestVersionMessage =
    L"Bạn đang sử dụng version mới nhất.";
constexpr const wchar_t* kUpdateAvailableMessage =
    L"Đã có version mới, click để tải ngay.";

constexpr int ID_LIST = 1001;
constexpr int ID_ADD = 1101;
constexpr int ID_PIN = 1102;
constexpr int ID_SAVE = 1103;
constexpr int ID_OPEN = 1104;
constexpr int ID_ONOFF = 1105;
constexpr int ID_EXPAND = 1106;
constexpr int ID_GDOCS = 1107;
constexpr int ID_HOTKEY = 1108;
constexpr int ID_GUIDE = 1109;
constexpr int ID_DEFAULT_HOTKEY = 1110;
constexpr int ID_PTS = 1111;
constexpr int ID_STATUS = 1201;
constexpr int ID_PTS_BACKUP_SETTINGS = 1301;
constexpr int ID_PTS_BACKUP_FONTS = 1302;
constexpr int ID_PTS_BACKUP_ALL = 1303;
constexpr int ID_PTS_RESTORE = 1304;
constexpr int IDI_TOOLTYPE = 101;

struct UpdateCheckResult {
    bool updateAvailable = false;
    std::wstring downloadUrl;
};

uint16_t ReadU16(const std::vector<uint8_t>& data, size_t off) {
    if (off + 2 > data.size()) return 0;
    return static_cast<uint16_t>(data[off] | (data[off + 1] << 8));
}

uint32_t ReadU32(const std::vector<uint8_t>& data, size_t off) {
    if (off + 4 > data.size()) return 0;
    return static_cast<uint32_t>(data[off] |
                                 (data[off + 1] << 8) |
                                 (data[off + 2] << 16) |
                                 (data[off + 3] << 24));
}

std::wstring Utf8ToWide(const std::string& input, bool strict = false) {
    if (input.empty()) return L"";
    DWORD flags = strict ? MB_ERR_INVALID_CHARS : 0;
    int needed = MultiByteToWideChar(CP_UTF8, flags, input.data(),
                                     static_cast<int>(input.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, flags, input.data(), static_cast<int>(input.size()),
                        out.data(), needed);
    return out;
}

COLORREF BlendColor(COLORREF from, COLORREF to, int percentTo) {
    percentTo = std::clamp(percentTo, 0, 100);
    int inv = 100 - percentTo;
    return RGB((GetRValue(from) * inv + GetRValue(to) * percentTo) / 100,
               (GetGValue(from) * inv + GetGValue(to) * percentTo) / 100,
               (GetBValue(from) * inv + GetBValue(to) * percentTo) / 100);
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, input.data(),
                                     static_cast<int>(input.size()), nullptr, 0,
                                     nullptr, nullptr);
    if (needed <= 0) return "";
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring AnsiToWide(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return L"";
    int needed = MultiByteToWideChar(CP_ACP, 0,
                                     reinterpret_cast<const char*>(bytes.data()),
                                     static_cast<int>(bytes.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(bytes.data()),
                        static_cast<int>(bytes.size()), out.data(), needed);
    return out;
}

std::wstring DecodeTextBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return L"";

    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return Utf8ToWide(std::string(reinterpret_cast<const char*>(bytes.data() + 3),
                                      reinterpret_cast<const char*>(bytes.data() + bytes.size())));
    }

    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        std::wstring out;
        out.reserve((bytes.size() - 2) / 2);
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            out.push_back(static_cast<wchar_t>(bytes[i] | (bytes[i + 1] << 8)));
        }
        return out;
    }

    if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        std::wstring out;
        out.reserve((bytes.size() - 2) / 2);
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            out.push_back(static_cast<wchar_t>((bytes[i] << 8) | bytes[i + 1]));
        }
        return out;
    }

    std::string raw(reinterpret_cast<const char*>(bytes.data()),
                    reinterpret_cast<const char*>(bytes.data() + bytes.size()));
    std::wstring utf8 = Utf8ToWide(raw, true);
    if (!utf8.empty()) return utf8;
    return AnsiToWide(bytes);
}

std::wstring XmlEntitiesToWide(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != L'&') {
            out.push_back(text[i]);
            continue;
        }

        size_t semi = text.find(L';', i + 1);
        if (semi == std::wstring::npos || semi - i > 16) {
            out.push_back(text[i]);
            continue;
        }

        std::wstring entity = text.substr(i + 1, semi - i - 1);
        wchar_t decoded = 0;
        if (entity == L"amp") decoded = L'&';
        else if (entity == L"lt") decoded = L'<';
        else if (entity == L"gt") decoded = L'>';
        else if (entity == L"quot") decoded = L'"';
        else if (entity == L"apos") decoded = L'\'';
        else if (!entity.empty() && entity[0] == L'#') {
            int base = 10;
            size_t pos = 1;
            if (entity.size() > 2 && (entity[1] == L'x' || entity[1] == L'X')) {
                base = 16;
                pos = 2;
            }
            wchar_t* end = nullptr;
            unsigned long value = std::wcstoul(entity.c_str() + pos, &end, base);
            if (end && *end == L'\0' && value <= 0xFFFF) {
                decoded = static_cast<wchar_t>(value);
            }
        }

        if (decoded) {
            out.push_back(decoded);
            i = semi;
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

std::vector<std::wstring> SplitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring current;
    current.reserve(160);

    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            lines.push_back(current);
            current.clear();
        } else if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    if (!current.empty() || text.empty() || (!text.empty() && text.back() != L'\n' && text.back() != L'\r')) {
        lines.push_back(current);
    }
    if (lines.size() == 1 && lines[0].empty() && !text.empty()) {
        lines.clear();
    }
    return lines;
}

std::wstring TrimLeftCopy(const std::wstring& text) {
    size_t first = 0;
    while (first < text.size() && iswspace(text[first])) ++first;
    return text.substr(first);
}

std::wstring TrimCopy(const std::wstring& text) {
    size_t first = 0;
    while (first < text.size() && iswspace(text[first])) ++first;
    size_t last = text.size();
    while (last > first && iswspace(text[last - 1])) --last;
    return text.substr(first, last - first);
}

std::wstring LowerAsciiCopy(std::wstring text) {
    for (auto& ch : text) {
        if (ch >= L'A' && ch <= L'Z') {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
    }
    return text;
}

bool IsHttpUrl(const std::wstring& url) {
    return url.rfind(L"https://", 0) == 0 || url.rfind(L"http://", 0) == 0;
}

bool ReadUpdateIni(const std::wstring& text, std::wstring& version, std::wstring& downloadUrl) {
    version.clear();
    downloadUrl.clear();
    for (const auto& rawLine : SplitLines(text)) {
        std::wstring line = TrimCopy(rawLine);
        if (line.empty() || line[0] == L';' || line[0] == L'#' || line[0] == L'[') {
            continue;
        }

        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;

        std::wstring key = LowerAsciiCopy(TrimCopy(line.substr(0, eq)));
        std::wstring value = TrimCopy(line.substr(eq + 1));
        if (key == L"version") {
            version = value;
        } else if (key == L"download" || key == L"download_url" ||
                   key == L"downloadurl" || key == L"url" || key == L"link") {
            downloadUrl = value;
        }
    }
    return !version.empty() && IsHttpUrl(downloadUrl);
}

std::vector<unsigned long long> ParseVersionParts(const std::wstring& version) {
    std::vector<unsigned long long> parts;
    unsigned long long value = 0;
    bool inNumber = false;
    for (wchar_t ch : version) {
        if (ch >= L'0' && ch <= L'9') {
            inNumber = true;
            value = value * 10 + static_cast<unsigned long long>(ch - L'0');
        } else if (inNumber) {
            parts.push_back(value);
            value = 0;
            inNumber = false;
        }
    }
    if (inNumber) parts.push_back(value);
    return parts;
}

int CompareVersions(const std::wstring& left, const std::wstring& right) {
    std::vector<unsigned long long> a = ParseVersionParts(left);
    std::vector<unsigned long long> b = ParseVersionParts(right);
    size_t count = std::max(a.size(), b.size());
    for (size_t i = 0; i < count; ++i) {
        unsigned long long av = i < a.size() ? a[i] : 0;
        unsigned long long bv = i < b.size() ? b[i] : 0;
        if (av > bv) return 1;
        if (av < bv) return -1;
    }
    return 0;
}

COLORREF TextColorForLine(const std::wstring& line) {
    switch (ClassifyTextLine(line)) {
        case TextLineKind::Comment: return kCommentTextColor;
        case TextLineKind::Star: return kStarTextColor;
        case TextLineKind::Dash: return kDashTextColor;
        case TextLineKind::Quote: return kQuoteTextColor;
        case TextLineKind::Blank: return kMutedTextColor;
        case TextLineKind::Normal:
        default: return kTextColor;
    }
}

bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out, std::wstring& error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Không mở được file.";
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 512LL * 1024LL * 1024LL) {
        CloseHandle(file);
        error = L"File quá lớn hoặc không đọc được kích thước.";
        return false;
    }

    out.assign(static_cast<size_t>(size.QuadPart), 0);
    uint8_t* write = out.data();
    size_t remaining = out.size();
    while (remaining > 0) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 1u << 20));
        DWORD read = 0;
        if (!ReadFile(file, write, chunk, &read, nullptr)) {
            CloseHandle(file);
            error = L"Lỗi khi đọc file.";
            return false;
        }
        if (read == 0) break;
        write += read;
        remaining -= read;
    }
    CloseHandle(file);
    return true;
}

bool DownloadUpdateBytes(const std::wstring& url, std::vector<uint8_t>& out) {
    out.clear();
    std::wstring userAgent = L"ToolType/";
    userAgent += kToolVersion;
    HINTERNET internet = InternetOpenW(userAgent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG,
                                       nullptr, nullptr, 0);
    if (!internet) return false;

    DWORD timeoutMs = 7000;
    InternetSetOptionW(internet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(internet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(internet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                  INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE;
    HINTERNET request = InternetOpenUrlW(internet, url.c_str(), nullptr, 0, flags, 0);
    if (!request) {
        InternetCloseHandle(internet);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &status, &statusSize, nullptr) && status >= 400) {
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        return false;
    }

    uint8_t buffer[16 * 1024];
    for (;;) {
        DWORD read = 0;
        if (!InternetReadFile(request, buffer, sizeof(buffer), &read)) {
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            return false;
        }
        if (read == 0) break;
        if (out.size() + read > 256u * 1024u) {
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            return false;
        }
        out.insert(out.end(), buffer, buffer + read);
    }

    InternetCloseHandle(request);
    InternetCloseHandle(internet);
    return !out.empty();
}

DWORD WINAPI UpdateCheckThreadProc(LPVOID rawArgs) {
    HWND hwnd = static_cast<HWND>(rawArgs);

    auto* result = new UpdateCheckResult();
    std::vector<uint8_t> bytes;
    if (DownloadUpdateBytes(kUpdateCheckUrl, bytes)) {
        std::wstring text = DecodeTextBytes(bytes);
        std::wstring remoteVersion;
        std::wstring downloadUrl;
        if (ReadUpdateIni(text, remoteVersion, downloadUrl) &&
            CompareVersions(remoteVersion, kToolVersion) > 0) {
            result->updateAvailable = true;
            result->downloadUrl = downloadUrl;
        }
    }

    if (!hwnd || !PostMessageW(hwnd, WM_APP_UPDATE_CHECK_DONE, 0,
                               reinterpret_cast<LPARAM>(result))) {
        delete result;
    }
    return 0;
}

void StartUpdateCheck(HWND hwnd) {
    HANDLE thread = CreateThread(nullptr, 0, UpdateCheckThreadProc, hwnd, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    }
}

std::wstring ExtensionLower(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return L"";
    std::wstring ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return ext;
}

bool InflateRawDeflate(const uint8_t* input, size_t inputSize, size_t outputSize, std::string& out) {
    out.assign(outputSize, '\0');
    if (outputSize == 0) return true;

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input));
    stream.avail_in = static_cast<uInt>(inputSize);
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
    int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    return ret == Z_STREAM_END;
}

bool ExtractZipFile(const std::vector<uint8_t>& zip, const std::string& wantedName,
                    std::string& content, std::wstring& error) {
    if (zip.size() < 22) {
        error = L"DOCX không hợp lệ.";
        return false;
    }

    size_t searchStart = zip.size() > (22 + 65535) ? zip.size() - (22 + 65535) : 0;
    size_t eocd = std::wstring::npos;
    for (size_t i = zip.size() - 22 + 1; i-- > searchStart;) {
        if (ReadU32(zip, i) == 0x06054b50) {
            eocd = i;
            break;
        }
        if (i == 0) break;
    }
    if (eocd == std::wstring::npos) {
        error = L"Không tìm thấy cấu trúc ZIP trong DOCX.";
        return false;
    }

    uint16_t entries = ReadU16(zip, eocd + 10);
    uint32_t cdSize = ReadU32(zip, eocd + 12);
    uint32_t cdOffset = ReadU32(zip, eocd + 16);
    if (cdOffset >= zip.size() || cdOffset + cdSize > zip.size()) {
        error = L"DOCX ZIP bị lỗi.";
        return false;
    }

    size_t pos = cdOffset;
    for (uint16_t entry = 0; entry < entries && pos + 46 <= zip.size(); ++entry) {
        if (ReadU32(zip, pos) != 0x02014b50) break;

        uint16_t method = ReadU16(zip, pos + 10);
        uint32_t compSize = ReadU32(zip, pos + 20);
        uint32_t uncompSize = ReadU32(zip, pos + 24);
        uint16_t nameLen = ReadU16(zip, pos + 28);
        uint16_t extraLen = ReadU16(zip, pos + 30);
        uint16_t commentLen = ReadU16(zip, pos + 32);
        uint32_t localOffset = ReadU32(zip, pos + 42);

        if (pos + 46 + nameLen + extraLen + commentLen > zip.size()) break;
        std::string name(reinterpret_cast<const char*>(zip.data() + pos + 46), nameLen);
        std::replace(name.begin(), name.end(), '\\', '/');

        if (name == wantedName) {
            if (localOffset + 30 > zip.size() || ReadU32(zip, localOffset) != 0x04034b50) {
                error = L"DOCX có local header bị lỗi.";
                return false;
            }
            uint16_t localNameLen = ReadU16(zip, localOffset + 26);
            uint16_t localExtraLen = ReadU16(zip, localOffset + 28);
            size_t dataStart = static_cast<size_t>(localOffset) + 30 + localNameLen + localExtraLen;
            if (dataStart + compSize > zip.size()) {
                error = L"DOCX thiếu dữ liệu nén.";
                return false;
            }

            if (method == 0) {
                content.assign(reinterpret_cast<const char*>(zip.data() + dataStart), compSize);
                return true;
            }
            if (method == 8) {
                if (!InflateRawDeflate(zip.data() + dataStart, compSize, uncompSize, content)) {
                    error = L"Không giải nén được nội dung DOCX.";
                    return false;
                }
                return true;
            }
            error = L"DOCX dùng kiểu nén chưa hỗ trợ.";
            return false;
        }

        pos += 46 + nameLen + extraLen + commentLen;
    }

    error = L"Không tìm thấy word/document.xml trong DOCX.";
    return false;
}

std::wstring XmlTagName(const std::wstring& rawTag) {
    std::wstring tag = TrimLeftCopy(rawTag);
    if (!tag.empty() && tag[0] == L'/') tag.erase(tag.begin());

    size_t end = 0;
    while (end < tag.size() && !iswspace(tag[end]) && tag[end] != L'/') ++end;
    return tag.substr(0, end);
}

std::wstring XmlLocalName(const std::wstring& rawTag) {
    std::wstring name = XmlTagName(rawTag);
    size_t colon = name.find(L':');
    return colon == std::wstring::npos ? name : name.substr(colon + 1);
}

bool IsClosingXmlTag(const std::wstring& rawTag) {
    std::wstring tag = TrimLeftCopy(rawTag);
    return !tag.empty() && tag[0] == L'/';
}

bool IsSelfClosingXmlTag(const std::wstring& rawTag) {
    size_t end = rawTag.size();
    while (end > 0 && iswspace(rawTag[end - 1])) --end;
    return end > 0 && rawTag[end - 1] == L'/';
}

std::wstring ExtractDocxTextFromXml(const std::string& xmlBytes) {
    std::wstring xml = Utf8ToWide(xmlBytes);
    std::wstring out;
    out.reserve(xml.size() / 5);

    for (size_t i = 0; i < xml.size();) {
        if (xml[i] != L'<') {
            ++i;
            continue;
        }

        size_t close = xml.find(L'>', i + 1);
        if (close == std::wstring::npos) break;
        std::wstring tag = xml.substr(i + 1, close - i - 1);
        std::wstring localName = XmlLocalName(tag);
        bool isClosing = IsClosingXmlTag(tag);
        bool isSelfClosing = IsSelfClosingXmlTag(tag);

        if (localName == L"t" && !isClosing && !isSelfClosing) {
            size_t textStart = close + 1;
            size_t textEnd = xml.find(L'<', textStart);
            if (textEnd == std::wstring::npos) textEnd = xml.size();
            out += XmlEntitiesToWide(xml.substr(textStart, textEnd - textStart));
            i = textEnd;
            continue;
        }

        if (localName == L"tab" && !isClosing) {
            out.push_back(L'\t');
        } else if (localName == L"br" && !isClosing) {
            out.push_back(L'\n');
        } else if ((isClosing || isSelfClosing) && localName == L"p") {
            out.push_back(L'\n');
        } else if (isClosing && localName == L"tc") {
            if (!out.empty() && out.back() != L'\n') out.push_back(L'\n');
        }
        i = close + 1;
    }

    return out;
}

bool LoadDocumentLines(const std::wstring& path, std::vector<std::wstring>& lines, std::wstring& error) {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes, error)) return false;

    std::wstring ext = ExtensionLower(path);
    std::wstring text;
    if (ext == L".docx") {
        std::string xml;
        if (!ExtractZipFile(bytes, "word/document.xml", xml, error)) return false;
        text = ExtractDocxTextFromXml(xml);
    } else {
        text = DecodeTextBytes(bytes);
    }

    lines = SplitLines(text);
    bool hasPasteableLine = std::any_of(lines.begin(), lines.end(), [](const std::wstring& line) {
        return IsPasteableTextLine(line);
    });
    if (!hasPasteableLine) {
        error = L"File không có dòng văn bản nào.";
        return false;
    }
    return true;
}

std::wstring GetModuleDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer, len);
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::wstring FileNameOnly(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

void WriteUtf8File(const std::wstring& path, const std::string& data) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    CloseHandle(file);
}

enum class PtsEntryKind : uint8_t {
    PhotoshopSetting = 1,
    Font = 2,
};

struct PtsBackupItem {
    PtsEntryKind kind = PtsEntryKind::PhotoshopSetting;
    std::wstring sourcePath;
    std::wstring rootToken;
    std::wstring relativePath;
    std::wstring displayName;
    uint64_t size = 0;
};

struct PtsPhotoshopRoot {
    std::wstring rootToken;
    std::wstring baseRoot;
    std::wstring scanRoot;
    std::wstring relativeRoot;
    std::wstring versionLabel;
    std::wstring versionKey;
};

struct PtsPhotoshopVersion {
    std::wstring label;
    std::vector<PtsPhotoshopRoot> roots;
};

struct PtsBackupOptions {
    std::set<std::wstring> selectedPhotoshopVersionKeys;
    std::set<std::wstring> selectedPhotoshopVersionLabels;
    std::wstring selectedPhotoshopVersionLabel;
};

struct PtsArchiveVersion {
    std::wstring label;
    std::set<std::wstring> rootKeys;
};

struct PtsArchiveScanResult {
    std::vector<PtsArchiveVersion> versions;
    bool hasSettings = false;
    bool hasFonts = false;
    uint32_t entryCount = 0;
};

struct PtsRestoreOptions {
    std::set<std::wstring> sourceVersionKeys;
    std::wstring sourceVersionLabel;
    std::wstring targetVersionLabel;
    std::wstring targetAppDataRelativeRoot;
    std::wstring targetLocalAppDataRelativeRoot;
    bool archiveHasSettings = false;
    bool archiveValidated = false;

    bool HasSourceFilter() const { return !sourceVersionKeys.empty(); }
    bool HasTargetMapping() const { return !targetVersionLabel.empty(); }
};

using PtsProgressCallback = std::function<void(int, const std::wstring&)>;
using PtsCancellationCallback = std::function<bool()>;
using PtsPhotoshopRunningCallback = std::function<bool()>;
using PtsSettingsRegistryUpdateCallback =
    std::function<uint32_t(const std::wstring&, const std::set<std::wstring>&)>;
using PtsReplaceFileCallback =
    std::function<BOOL(const std::wstring&, const std::wstring&,
                       const std::wstring&, DWORD)>;
using PtsCommitCallback = std::function<bool()>;

enum class PtsCancellationPhase : int {
    Running,
    CancelRequested,
    CommitStarted,
};

struct PtsCancellationState {
    std::atomic<int> phase{static_cast<int>(PtsCancellationPhase::Running)};
};

using PtsCancellationHandle = std::shared_ptr<PtsCancellationState>;

PtsCancellationHandle CreatePtsCancellationHandle() {
    return std::make_shared<PtsCancellationState>();
}

bool RequestPtsCancellation(const PtsCancellationHandle& cancellation) {
    if (!cancellation) return false;
    int expected = static_cast<int>(PtsCancellationPhase::Running);
    if (cancellation->phase.compare_exchange_strong(
            expected, static_cast<int>(PtsCancellationPhase::CancelRequested),
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        return true;
    }
    return expected == static_cast<int>(PtsCancellationPhase::CancelRequested);
}

bool IsPtsCancellationRequested(const PtsCancellationHandle& cancellation) {
    return cancellation &&
           cancellation->phase.load(std::memory_order_acquire) ==
               static_cast<int>(PtsCancellationPhase::CancelRequested);
}

bool BeginPtsCommit(const PtsCancellationHandle& cancellation) {
    if (!cancellation) return false;
    int expected = static_cast<int>(PtsCancellationPhase::Running);
    if (cancellation->phase.compare_exchange_strong(
            expected, static_cast<int>(PtsCancellationPhase::CommitStarted),
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        return true;
    }
    return expected == static_cast<int>(PtsCancellationPhase::CommitStarted);
}

struct PtsProgressMessage {
    int percent = 0;
    std::wstring message;
};

struct PtsBackgroundResult {
    bool ok = false;
    bool cancelled = false;
    DWORD workerThreadId = 0;
    std::wstring summary;
    std::wstring error;
};

using PtsBackgroundOperation = std::function<bool(
    const PtsProgressCallback&, const PtsCancellationCallback&, const PtsCommitCallback&,
    std::wstring&, std::wstring&)>;

enum class PtsOperationKind {
    None,
    Backup,
    Restore,
};

struct PtsBackgroundTaskContext {
    HWND notifyWindow = nullptr;
    PtsBackgroundOperation operation;
    PtsCancellationHandle cancellation;
};

bool RunPtsDialogMessageLoop(HWND dialog) {
    MSG message{};
    BOOL getMessageResult = TRUE;
    while (IsWindow(dialog) &&
           (getMessageResult = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (getMessageResult == 0) {
        int exitCode = static_cast<int>(message.wParam);
        if (IsWindow(dialog)) DestroyWindow(dialog);
        PostQuitMessage(exitCode);
        return false;
    }
    if (getMessageResult < 0) {
        if (IsWindow(dialog)) DestroyWindow(dialog);
        return false;
    }
    return true;
}

DWORD WINAPI PtsBackgroundTaskThreadProc(void* parameter) {
    std::unique_ptr<PtsBackgroundTaskContext> context(
        static_cast<PtsBackgroundTaskContext*>(parameter));
    auto* result = new (std::nothrow) PtsBackgroundResult{};
    if (!result) {
        PostMessageW(context->notifyWindow, WM_APP_PTS_OPERATION_DONE, 1, 0);
        return 1;
    }
    result->workerThreadId = GetCurrentThreadId();

    const HWND notifyWindow = context->notifyWindow;
    const PtsCancellationHandle cancellation = context->cancellation;
    int lastPercent = -1;
    ULONGLONG lastPost = 0;
    PtsProgressCallback progress = [notifyWindow, lastPercent, lastPost](
                                       int percent, const std::wstring& message) mutable {
        int boundedPercent = std::clamp(percent, 0, 100);
        ULONGLONG now = GetTickCount64();
        bool terminal = boundedPercent == 0 || boundedPercent == 100;
        if (!terminal && boundedPercent == lastPercent && now - lastPost < 75) return;
        lastPercent = boundedPercent;
        lastPost = now;
        try {
            auto* update = new PtsProgressMessage{boundedPercent, message};
            if (!PostMessageW(notifyWindow, WM_APP_PTS_PROGRESS, 0,
                              reinterpret_cast<LPARAM>(update))) {
                delete update;
            }
        } catch (...) {
        }
    };
    PtsCancellationCallback cancelled = [cancellation] {
        return IsPtsCancellationRequested(cancellation);
    };
    PtsCommitCallback beginCommit = [cancellation] {
        return BeginPtsCommit(cancellation);
    };

    try {
        result->ok = context->operation(progress, cancelled, beginCommit,
                                        result->summary, result->error);
    } catch (...) {
        result->ok = false;
        result->error = L"Lỗi không xác định trong thao tác Pts.";
    }
    result->cancelled = !result->ok && IsPtsCancellationRequested(cancellation);
    if (!PostMessageW(notifyWindow, WM_APP_PTS_OPERATION_DONE, 0,
                      reinterpret_cast<LPARAM>(result))) {
        delete result;
    }
    return 0;
}

bool StartPtsBackgroundTask(HWND notifyWindow, const PtsBackgroundOperation& operation,
                            const PtsCancellationHandle& cancellation,
                            HANDLE& workerThread, std::wstring& error) {
    workerThread = nullptr;
    if (!notifyWindow || !IsWindow(notifyWindow) || !operation || !cancellation) {
        error = L"Không thể bắt đầu thao tác Pts chạy nền.";
        return false;
    }

    PtsBackgroundTaskContext* context = nullptr;
    try {
        context = new PtsBackgroundTaskContext{notifyWindow, operation, cancellation};
    } catch (...) {
        context = nullptr;
    }
    if (!context) {
        error = L"Không đủ bộ nhớ để bắt đầu thao tác Pts.";
        return false;
    }
    workerThread = CreateThread(nullptr, 0, PtsBackgroundTaskThreadProc, context, 0, nullptr);
    if (!workerThread) {
        delete context;
        error = L"Không tạo được worker cho thao tác Pts.";
        return false;
    }
    return true;
}

constexpr char kAfangMagic[8] = {'A', 'F', 'A', 'N', 'G', 'P', 'S', '1'};
constexpr uint32_t kAfangVersion = 1;
constexpr uint64_t kPtsMaxSingleFileBytes = 512ull * 1024ull * 1024ull;

std::wstring LowerWide(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return text;
}

std::wstring TrimTrailingSlashes(std::wstring path) {
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) {
        path.pop_back();
    }
    return path;
}

std::wstring GetEnvPath(const wchar_t* name) {
    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) return L"";
    std::wstring value(static_cast<size_t>(needed), L'\0');
    DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0) return L"";
    value.resize(static_cast<size_t>(written));
    return TrimTrailingSlashes(value);
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (right.size() > 1 && right[1] == L':') return right;
    if (right.front() == L'\\' || right.front() == L'/') return right;
    wchar_t sep = (left.back() == L'\\' || left.back() == L'/') ? L'\0' : L'\\';
    return sep ? left + sep + right : left + right;
}

std::wstring ParentPath(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    if (pos == 2 && path.size() >= 3 && path[1] == L':') return path.substr(0, 3);
    if (pos == 0) return path.substr(0, 1);
    return path.substr(0, pos);
}

bool DirectoryExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsProcessRunningByBaseName(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::wstring expected = LowerWide(processName);
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (LowerWide(entry.szExeFile) == expected) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

bool IsPhotoshopRunning() {
    return IsProcessRunningByBaseName(L"Photoshop.exe");
}

bool EnsureDirectoryExists(const std::wstring& path) {
    if (path.empty()) return false;
    if (DirectoryExists(path)) return true;
    std::wstring parent = ParentPath(path);
    if (!parent.empty() && parent != path && !DirectoryExists(parent)) {
        if (!EnsureDirectoryExists(parent)) return false;
    }
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS && DirectoryExists(path);
}

bool EnsureParentDirectory(const std::wstring& path) {
    std::wstring parent = ParentPath(path);
    return parent.empty() || EnsureDirectoryExists(parent);
}

bool GetFileSize64(const std::wstring& path, uint64_t& sizeOut) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
    LARGE_INTEGER size{};
    size.HighPart = static_cast<LONG>(data.nFileSizeHigh);
    size.LowPart = data.nFileSizeLow;
    if (size.QuadPart < 0) return false;
    sizeOut = static_cast<uint64_t>(size.QuadPart);
    return true;
}

std::wstring RelativePathFromRoot(const std::wstring& root, const std::wstring& path) {
    std::wstring cleanRoot = TrimTrailingSlashes(root);
    if (cleanRoot.empty()) return FileNameOnly(path);
    std::wstring lowerRoot = LowerWide(cleanRoot);
    std::wstring lowerPath = LowerWide(path);
    if (lowerPath == lowerRoot) return L"";
    if (lowerPath.rfind(lowerRoot, 0) == 0 &&
        path.size() > cleanRoot.size() &&
        (path[cleanRoot.size()] == L'\\' || path[cleanRoot.size()] == L'/')) {
        return path.substr(cleanRoot.size() + 1);
    }
    return FileNameOnly(path);
}

bool IsValidWindowsBaseFileName(const std::wstring& path);

bool IsSafeRelativePath(const std::wstring& path) {
    if (path.empty()) return false;
    if (path.find(L':') != std::wstring::npos) return false;
    if (path.front() == L'\\' || path.front() == L'/') return false;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find_first_of(L"\\/", start);
        std::wstring part = path.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (part == L"." || part == L".." || part.empty() ||
            !IsValidWindowsBaseFileName(part)) {
            return false;
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return true;
}

bool IsBaseFileNameOnly(const std::wstring& path) {
    return !path.empty() &&
           path.find_first_of(L"\\/") == std::wstring::npos &&
           path.find(L':') == std::wstring::npos &&
           path != L"." &&
           path != L"..";
}

bool IsValidWindowsBaseFileName(const std::wstring& path) {
    if (!IsBaseFileNameOnly(path)) return false;
    if (path.back() == L'.' || path.back() == L' ') return false;
    if (path.find_first_of(L"<>\"|?*") != std::wstring::npos) return false;
    for (wchar_t ch : path) {
        if (ch < 0x20) return false;
    }

    std::wstring stem = path;
    size_t dot = stem.find(L'.');
    if (dot != std::wstring::npos) stem.resize(dot);
    while (!stem.empty() && (stem.back() == L'.' || stem.back() == L' ')) {
        stem.pop_back();
    }
    stem = LowerWide(stem);
    const wchar_t* reserved[] = {
        L"con", L"prn", L"aux", L"nul",
        L"com1", L"com2", L"com3", L"com4", L"com5", L"com6", L"com7", L"com8", L"com9",
        L"lpt1", L"lpt2", L"lpt3", L"lpt4", L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9"
    };
    for (const wchar_t* name : reserved) {
        if (stem == name) return false;
    }
    return true;
}

std::wstring NormalizeRelativePathSlashes(std::wstring path) {
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
}

std::vector<std::wstring> SplitRelativePathParts(const std::wstring& path) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find_first_of(L"\\/", start);
        std::wstring part = path.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!part.empty()) parts.push_back(part);
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return parts;
}

std::wstring JoinRelativePathParts(const std::vector<std::wstring>& parts, size_t count) {
    std::wstring out;
    count = std::min(count, parts.size());
    for (size_t i = 0; i < count; ++i) {
        if (!out.empty()) out += L"\\";
        out += parts[i];
    }
    return out;
}

std::wstring NormalizePhotoshopVersionLabel(std::wstring label) {
    label = TrimCopy(label);
    std::wstring lower = LowerWide(label);
    const std::wstring settingsSuffix = L" settings";
    if (lower.size() > settingsSuffix.size() &&
        lower.rfind(settingsSuffix) == lower.size() - settingsSuffix.size()) {
        label.resize(label.size() - settingsSuffix.size());
        label = TrimCopy(label);
        lower = LowerWide(label);
    }
    const wchar_t* suffixes[] = {L" (64 bit)", L" (32 bit)"};
    for (const wchar_t* suffix : suffixes) {
        std::wstring suffixText = suffix;
        if (lower.size() > suffixText.size() &&
            lower.rfind(suffixText) == lower.size() - suffixText.size()) {
            label.resize(label.size() - suffixText.size());
            label = TrimCopy(label);
            break;
        }
    }
    return label;
}

bool LooksLikePhotoshopVersionLabel(const std::wstring& label) {
    std::wstring normalized = NormalizePhotoshopVersionLabel(label);
    std::wstring lower = LowerWide(normalized);
    if (normalized.empty() || lower.find(L"photoshop") == std::wstring::npos) return false;
    if (lower.find(L"camera raw") != std::wstring::npos) return false;
    if (lower.find(L"plugin") != std::wstring::npos) return false;
    if (lower.find(L"_gude") != std::wstring::npos) return false;
    return IsBaseFileNameOnly(normalized);
}

bool ExtractPhotoshopVersionInfo(const std::wstring& path, std::wstring& relativeRoot,
                                  std::wstring& label) {
    if (!IsSafeRelativePath(path)) return false;
    std::wstring lower = LowerWide(path);
    if (lower.rfind(L"adobe\\", 0) != 0 && lower.rfind(L"adobe/", 0) != 0) {
        return false;
    }

    std::vector<std::wstring> parts = SplitRelativePathParts(path);
    for (size_t i = 0; i < parts.size(); ++i) {
        if (LowerWide(parts[i]).find(L"photoshop") != std::wstring::npos) {
            relativeRoot = JoinRelativePathParts(parts, i + 1);
            label = NormalizePhotoshopVersionLabel(parts[i]);
            return IsSafeRelativePath(relativeRoot) && !label.empty();
        }
    }
    return false;
}

bool IsAllowedPhotoshopSettingsRelativePath(const std::wstring& path) {
    std::wstring relativeRoot;
    std::wstring label;
    return ExtractPhotoshopVersionInfo(path, relativeRoot, label);
}

std::wstring PtsPhotoshopVersionKey(const std::wstring& rootToken,
                                    const std::wstring& relativeRoot) {
    return LowerWide(rootToken + L"\\" + NormalizeRelativePathSlashes(relativeRoot));
}

bool HasFontExtension(const std::wstring& path) {
    std::wstring ext = ExtensionLower(path);
    return ext == L".ttf" || ext == L".otf" || ext == L".ttc" ||
           ext == L".otc" || ext == L".fon";
}

bool HasLikelyCacheDirectoryName(const std::wstring& name) {
    std::wstring lower = LowerWide(name);
    return lower == L"cache" || lower == L"caches" || lower == L"temp" ||
           lower == L"tmp" || lower == L"logs" || lower == L"log" ||
           lower.find(L"autorecover") != std::wstring::npos ||
           lower.find(L"media cache") != std::wstring::npos;
}

bool CollectFilesRecursive(const std::wstring& directory, std::vector<std::wstring>& files,
                           bool skipLikelyCaches,
                           const PtsProgressCallback& progress = {},
                           int progressPercent = 0,
                           const std::wstring& progressLabel = L"",
                           size_t* visited = nullptr,
                           const PtsCancellationCallback& cancelled = {}) {
    if (cancelled && cancelled()) return false;
    std::wstring pattern = JoinPath(directory, L"*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return !(cancelled && cancelled());

    do {
        if (cancelled && cancelled()) {
            FindClose(find);
            return false;
        }
        std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") continue;
        if (visited) {
            ++(*visited);
            if (progress && (*visited == 1 || *visited % 64 == 0)) {
                progress(progressPercent, progressLabel + L" (" +
                                             std::to_wstring(*visited) + L" mục)...");
            }
        }
        std::wstring full = JoinPath(directory, name);
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (skipLikelyCaches && HasLikelyCacheDirectoryName(name)) continue;
            if (!CollectFilesRecursive(full, files, skipLikelyCaches, progress,
                                       progressPercent, progressLabel, visited, cancelled)) {
                FindClose(find);
                return false;
            }
        } else {
            files.push_back(full);
        }
    } while (FindNextFileW(find, &data));

    FindClose(find);
    return true;
}

void FindPhotoshopDirectoriesRecursive(const std::wstring& directory, int depth,
                                       std::vector<std::wstring>& roots) {
    if (depth < 0) return;
    std::wstring pattern = JoinPath(directory, L"*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") continue;
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        std::wstring full = JoinPath(directory, name);
        std::wstring lower = LowerWide(name);
        if (lower.find(L"photoshop") != std::wstring::npos) {
            roots.push_back(full);
            continue;
        }
        FindPhotoshopDirectoriesRecursive(full, depth - 1, roots);
    } while (FindNextFileW(find, &data));

    FindClose(find);
}

void FindImmediatePhotoshopDirectories(const std::wstring& directory,
                                       std::vector<std::wstring>& roots) {
    std::wstring pattern = JoinPath(directory, L"*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") continue;
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (!LooksLikePhotoshopVersionLabel(name)) continue;
        roots.push_back(JoinPath(directory, name));
    } while (FindNextFileW(find, &data));

    FindClose(find);
}

void AddPhotoshopVersionRoot(const std::wstring& baseRoot, const std::wstring& rootToken,
                             const std::wstring& scanRoot,
                             std::vector<PtsPhotoshopRoot>& roots,
                             std::set<std::wstring>& seen) {
    std::wstring relativeRootCandidate = RelativePathFromRoot(baseRoot, scanRoot);
    std::wstring relativeRoot;
    std::wstring label;
    if (!ExtractPhotoshopVersionInfo(relativeRootCandidate, relativeRoot, label)) return;

    std::wstring key = PtsPhotoshopVersionKey(rootToken, relativeRoot);
    if (!seen.insert(key).second) return;

    PtsPhotoshopRoot root{};
    root.rootToken = rootToken;
    root.baseRoot = baseRoot;
    root.scanRoot = scanRoot;
    root.relativeRoot = NormalizeRelativePathSlashes(relativeRoot);
    root.versionLabel = label;
    root.versionKey = key;
    roots.push_back(std::move(root));
}

std::vector<PtsPhotoshopRoot> DiscoverPhotoshopRoots() {
    std::vector<PtsPhotoshopRoot> roots;
    std::set<std::wstring> seen;
    struct RootSpec {
        const wchar_t* envName;
        const wchar_t* token;
    };
    RootSpec specs[] = {{L"APPDATA", L"APPDATA"}, {L"LOCALAPPDATA", L"LOCALAPPDATA"}};
    for (const auto& spec : specs) {
        std::wstring base = GetEnvPath(spec.envName);
        if (base.empty()) continue;
        std::wstring adobe = JoinPath(base, L"Adobe");
        if (!DirectoryExists(adobe)) continue;
        std::vector<std::wstring> photoshopRoots;
        FindImmediatePhotoshopDirectories(adobe, photoshopRoots);
        if (photoshopRoots.empty()) {
            FindPhotoshopDirectoriesRecursive(adobe, 1, photoshopRoots);
        }
        for (const auto& root : photoshopRoots) {
            AddPhotoshopVersionRoot(base, spec.token, root, roots, seen);
        }
    }
    return roots;
}

std::vector<PtsPhotoshopVersion> GroupPhotoshopVersions(
    const std::vector<PtsPhotoshopRoot>& roots) {
    std::vector<PtsPhotoshopVersion> versions;
    for (const auto& root : roots) {
        std::wstring groupKey = LowerWide(root.versionLabel);
        auto it = std::find_if(versions.begin(), versions.end(), [&](const PtsPhotoshopVersion& v) {
            return LowerWide(v.label) == groupKey;
        });
        if (it == versions.end()) {
            PtsPhotoshopVersion version{};
            version.label = root.versionLabel;
            version.roots.push_back(root);
            versions.push_back(std::move(version));
        } else {
            it->roots.push_back(root);
        }
    }
    return versions;
}

void AddInstalledPhotoshopLabel(const std::wstring& rawLabel, std::vector<std::wstring>& labels,
                                std::set<std::wstring>& seen) {
    std::wstring label = NormalizePhotoshopVersionLabel(rawLabel);
    if (!LooksLikePhotoshopVersionLabel(label)) return;
    std::wstring key = LowerWide(label);
    if (seen.insert(key).second) labels.push_back(label);
}

void AddInstalledPhotoshopLabelsFromDirectory(const std::wstring& adobeDirectory,
                                              std::vector<std::wstring>& labels,
                                              std::set<std::wstring>& seen) {
    if (!DirectoryExists(adobeDirectory)) return;
    std::wstring pattern = JoinPath(adobeDirectory, L"*");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") continue;
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        std::wstring installRoot = JoinPath(adobeDirectory, name);
        if (!FileExists(JoinPath(installRoot, L"Photoshop.exe"))) continue;
        AddInstalledPhotoshopLabel(name, labels, seen);
    } while (FindNextFileW(find, &data));
    FindClose(find);
}

bool ReadRegistryStringValue(HKEY key, const wchar_t* valueName, std::wstring& value) {
    DWORD type = 0;
    DWORD bytes = 0;
    LONG rc = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        return false;
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 2, L'\0');
    rc = RegQueryValueExW(key, valueName, nullptr, &type,
                          reinterpret_cast<LPBYTE>(buffer.data()), &bytes);
    if (rc != ERROR_SUCCESS) return false;
    value.assign(buffer.data());
    value = TrimCopy(value);
    return !value.empty();
}

std::wstring ExpandEnvironmentPath(std::wstring path) {
    path = TrimCopy(path);
    if (path.empty()) return L"";

    DWORD needed = ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);
    if (needed <= 1) return path;
    std::wstring expanded(static_cast<size_t>(needed), L'\0');
    DWORD written = ExpandEnvironmentStringsW(path.c_str(), expanded.data(), needed);
    if (written == 0 || written >= needed) return path;
    expanded.resize(static_cast<size_t>(written - 1));
    return TrimCopy(expanded);
}

size_t FindCaseInsensitive(const std::wstring& text, const std::wstring& needle) {
    if (needle.empty()) return 0;
    return LowerWide(text).find(LowerWide(needle));
}

std::wstring ExecutablePathFromRegistryHint(std::wstring hint) {
    hint = TrimCopy(hint);
    if (!hint.empty() && hint.front() == L'@') {
        hint.erase(hint.begin());
        hint = TrimCopy(hint);
    }
    if (hint.empty()) return L"";

    std::wstring candidate;
    if (hint.front() == L'"') {
        size_t endQuote = hint.find(L'"', 1);
        if (endQuote != std::wstring::npos) {
            candidate = hint.substr(1, endQuote - 1);
        }
    }
    if (candidate.empty()) {
        size_t exePos = FindCaseInsensitive(hint, L".exe");
        if (exePos != std::wstring::npos) {
            candidate = hint.substr(0, exePos + 4);
        } else {
            candidate = hint;
            size_t comma = candidate.find(L',');
            if (comma != std::wstring::npos) candidate.resize(comma);
        }
    }
    return ExpandEnvironmentPath(candidate);
}

bool RegistryValuePointsToExistingPhotoshopExe(HKEY key, const wchar_t* valueName) {
    std::wstring raw;
    if (!ReadRegistryStringValue(key, valueName, raw)) return false;

    std::wstring path = ExecutablePathFromRegistryHint(raw);
    if (path.empty()) return false;
    if (DirectoryExists(path)) path = JoinPath(path, L"Photoshop.exe");
    if (!FileExists(path)) return false;

    std::wstring file = LowerWide(FileNameOnly(path));
    return file == L"photoshop.exe";
}

bool RegistryEntryHasExistingPhotoshopExecutable(HKEY appKey) {
    const wchar_t* pathValues[] = {
        L"InstallLocation",
        L"InstallPath",
        L"DisplayIcon",
        L"UninstallString",
        L"QuietUninstallString",
    };
    for (const wchar_t* valueName : pathValues) {
        if (RegistryValuePointsToExistingPhotoshopExe(appKey, valueName)) return true;
    }
    return false;
}

void AddInstalledPhotoshopLabelsFromUninstall(HKEY hive, REGSAM viewFlags,
                                               std::vector<std::wstring>& labels,
                                               std::set<std::wstring>& seen) {
    HKEY uninstall = nullptr;
    REGSAM access = KEY_READ | viewFlags;
    if (RegOpenKeyExW(hive, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                      0, access, &uninstall) != ERROR_SUCCESS) {
        return;
    }

    for (DWORD index = 0;; ++index) {
        wchar_t subKeyName[512] = {};
        DWORD subKeyLen = static_cast<DWORD>(std::size(subKeyName));
        LONG rc = RegEnumKeyExW(uninstall, index, subKeyName, &subKeyLen,
                                nullptr, nullptr, nullptr, nullptr);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;

        HKEY appKey = nullptr;
        if (RegOpenKeyExW(uninstall, subKeyName, 0, access, &appKey) != ERROR_SUCCESS) {
            continue;
        }
        std::wstring displayName;
        if (ReadRegistryStringValue(appKey, L"DisplayName", displayName) &&
            RegistryEntryHasExistingPhotoshopExecutable(appKey)) {
            AddInstalledPhotoshopLabel(displayName, labels, seen);
        }
        RegCloseKey(appKey);
    }

    RegCloseKey(uninstall);
}

std::vector<std::wstring> DiscoverInstalledPhotoshopVersionLabels() {
    std::vector<std::wstring> labels;
    std::set<std::wstring> seen;

    const wchar_t* envNames[] = {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432"};
    for (const wchar_t* envName : envNames) {
        std::wstring base = GetEnvPath(envName);
        if (!base.empty()) AddInstalledPhotoshopLabelsFromDirectory(JoinPath(base, L"Adobe"), labels, seen);
    }

    AddInstalledPhotoshopLabelsFromUninstall(HKEY_CURRENT_USER, 0, labels, seen);
    AddInstalledPhotoshopLabelsFromUninstall(HKEY_LOCAL_MACHINE, 0, labels, seen);
    AddInstalledPhotoshopLabelsFromUninstall(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, labels, seen);
    AddInstalledPhotoshopLabelsFromUninstall(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, labels, seen);

    std::sort(labels.begin(), labels.end(), [](const std::wstring& a, const std::wstring& b) {
        return LowerWide(a) < LowerWide(b);
    });
    return labels;
}

void MergeInstalledPhotoshopLabels(std::vector<PtsPhotoshopVersion>& versions,
                                   const std::vector<std::wstring>& labels) {
    for (const auto& label : labels) {
        std::wstring key = LowerWide(label);
        auto it = std::find_if(versions.begin(), versions.end(), [&](const PtsPhotoshopVersion& v) {
            return LowerWide(v.label) == key;
        });
        if (it == versions.end()) {
            PtsPhotoshopVersion version{};
            version.label = label;
            versions.push_back(std::move(version));
        }
    }
}

bool IsInstalledPhotoshopLabel(const std::wstring& label,
                               const std::vector<std::wstring>& installedLabels) {
    if (installedLabels.empty()) return true;
    std::wstring normalized = LowerWide(NormalizePhotoshopVersionLabel(label));
    return std::any_of(installedLabels.begin(), installedLabels.end(),
                       [&](const std::wstring& installed) {
                           return LowerWide(NormalizePhotoshopVersionLabel(installed)) ==
                                  normalized;
                       });
}

std::vector<PtsPhotoshopRoot> FilterPhotoshopRootsByInstalledLabels(
    const std::vector<PtsPhotoshopRoot>& roots,
    const std::vector<std::wstring>& installedLabels) {
    if (installedLabels.empty()) return {};

    std::vector<PtsPhotoshopRoot> filtered;
    for (const auto& root : roots) {
        if (IsInstalledPhotoshopLabel(root.versionLabel, installedLabels)) {
            filtered.push_back(root);
        }
    }
    return filtered;
}

std::vector<PtsPhotoshopRoot> DiscoverInstalledPhotoshopRoots() {
    return FilterPhotoshopRootsByInstalledLabels(DiscoverPhotoshopRoots(),
                                                 DiscoverInstalledPhotoshopVersionLabels());
}

std::vector<PtsPhotoshopVersion> DiscoverPhotoshopVersions() {
    std::vector<std::wstring> installedLabels = DiscoverInstalledPhotoshopVersionLabels();
    std::vector<PtsPhotoshopVersion> versions =
        GroupPhotoshopVersions(FilterPhotoshopRootsByInstalledLabels(DiscoverPhotoshopRoots(),
                                                                     installedLabels));
    MergeInstalledPhotoshopLabels(versions, installedLabels);
    std::sort(versions.begin(), versions.end(), [](const PtsPhotoshopVersion& a,
                                                   const PtsPhotoshopVersion& b) {
        return LowerWide(a.label) < LowerWide(b.label);
    });
    return versions;
}

std::wstring RootForToken(const PtsPhotoshopVersion& version, const std::wstring& token) {
    for (const auto& root : version.roots) {
        if (root.rootToken == token) return root.relativeRoot;
    }
    return L"";
}

std::wstring DefaultPhotoshopRelativeRootForLabel(const std::wstring& label) {
    return IsValidWindowsBaseFileName(label) ? JoinPath(L"Adobe", label) : L"";
}

bool AddPhotoshopItemsFromRoot(const PtsPhotoshopRoot& root,
                               std::vector<PtsBackupItem>& items,
                               std::set<std::wstring>& seen,
                               const PtsProgressCallback& progress,
                               size_t& visited,
                               const PtsCancellationCallback& cancelled) {
    std::vector<std::wstring> files;
    if (!CollectFilesRecursive(root.scanRoot, files, true, progress, 8,
                               L"Đang quét cài đặt Photoshop", &visited, cancelled)) {
        return false;
    }
    size_t processed = 0;
    for (const auto& file : files) {
        if (cancelled && cancelled()) return false;
        ++processed;
        if (progress && (processed == 1 || processed % 64 == 0)) {
            progress(8, L"Đang kiểm tra cài đặt của " + root.versionLabel + L" (" +
                             std::to_wstring(processed) + L" file)...");
        }
        uint64_t size = 0;
        if (!GetFileSize64(file, size) || size > kPtsMaxSingleFileBytes) continue;
        std::wstring rel = RelativePathFromRoot(root.baseRoot, file);
        if (!IsAllowedPhotoshopSettingsRelativePath(rel)) continue;
        std::wstring key = LowerWide(root.rootToken + L"\\" + rel);
        if (!seen.insert(key).second) continue;
        PtsBackupItem item{};
        item.kind = PtsEntryKind::PhotoshopSetting;
        item.sourcePath = file;
        item.rootToken = root.rootToken;
        item.relativePath = NormalizeRelativePathSlashes(rel);
        item.displayName = root.versionLabel.empty() ? L"Photoshop settings" : root.versionLabel;
        item.size = size;
        items.push_back(std::move(item));
    }
    return true;
}

bool GatherPhotoshopSettingItems(std::vector<PtsBackupItem>& items,
                                 const PtsBackupOptions& options,
                                 const PtsProgressCallback& progress = {},
                                 const PtsCancellationCallback& cancelled = {}) {
    std::set<std::wstring> seen;
    bool hasVersionFilter = !options.selectedPhotoshopVersionKeys.empty() ||
                            !options.selectedPhotoshopVersionLabels.empty();
    if (progress) progress(5, L"Đang dò thư mục cài đặt Photoshop...");
    std::vector<PtsPhotoshopRoot> roots = DiscoverInstalledPhotoshopRoots();
    size_t visited = 0;
    for (const auto& root : roots) {
        if (cancelled && cancelled()) return false;
        if (hasVersionFilter &&
            options.selectedPhotoshopVersionKeys.count(root.versionKey) == 0 &&
            options.selectedPhotoshopVersionLabels.count(LowerWide(root.versionLabel)) == 0) {
            continue;
        }
        if (progress) {
            progress(8, L"Đang quét " + root.versionLabel + L"...");
        }
        if (!AddPhotoshopItemsFromRoot(root, items, seen, progress, visited, cancelled)) {
            return false;
        }
    }
    if (progress) {
        progress(12, L"Đã tìm thấy " + std::to_wstring(items.size()) +
                         L" file cài đặt phù hợp.");
    }
    return !(cancelled && cancelled());
}

bool IsDefaultWindowsFont(const std::wstring& fileName, const std::wstring& displayName) {
    std::wstring file = LowerWide(fileName);
    std::wstring name = LowerWide(displayName);
    const wchar_t* defaultPrefixes[] = {
        L"arial", L"bahnschrift", L"calibri", L"cambria", L"candara",
        L"comic", L"consola", L"constan", L"corbel", L"cour", L"ebrima",
        L"framd", L"gadugi", L"georgia", L"holomdl2", L"impact", L"javatext",
        L"leelawad", L"lucon", L"malgun", L"marlett", L"micross", L"mingliu",
        L"mmrtext", L"msgothic", L"msjh", L"msyi", L"mvboli", L"nirmala",
        L"ntailu", L"palab", L"phagspa", L"seg", L"simsun", L"sitka",
        L"sylfaen", L"symbol", L"tahoma", L"times", L"trebuc", L"verdana",
        L"webdings", L"wingding", L"yugoth", L"cascadia", L"inkfree",
        L"segoe", L"msmincho", L"meiryo", L"batang", L"gulim"
    };
    for (const wchar_t* prefix : defaultPrefixes) {
        if (file.rfind(prefix, 0) == 0) return true;
    }
    const wchar_t* defaultNames[] = {
        L"arial", L"bahnschrift", L"calibri", L"cambria", L"candara",
        L"comic sans", L"consolas", L"constantia", L"corbel", L"courier",
        L"ebrima", L"gadugi", L"georgia", L"impact", L"leelawadee",
        L"lucida", L"malgun", L"microsoft", L"nirmala", L"palatino",
        L"segoe", L"sim", L"sitka", L"sylfaen", L"symbol", L"tahoma",
        L"times new roman", L"trebuchet", L"verdana", L"webdings",
        L"wingdings", L"yu gothic", L"cascadia"
    };
    for (const wchar_t* text : defaultNames) {
        if (name.find(text) != std::wstring::npos) return true;
    }
    return false;
}

std::wstring ResolveFontRegistryPath(const std::wstring& rawPath, bool userScope) {
    if (rawPath.empty()) return L"";
    wchar_t expanded[MAX_PATH * 4] = {};
    DWORD expandedLen = ExpandEnvironmentStringsW(rawPath.c_str(), expanded,
                                                  static_cast<DWORD>(std::size(expanded)));
    std::wstring path = (expandedLen > 0 && expandedLen < std::size(expanded))
                            ? std::wstring(expanded)
                            : rawPath;
    if (path.size() > 1 && path[1] == L':') return path;
    if (!path.empty() && (path.front() == L'\\' || path.front() == L'/')) return path;

    std::wstring base = userScope
                            ? JoinPath(GetEnvPath(L"LOCALAPPDATA"), L"Microsoft\\Windows\\Fonts")
                            : JoinPath(GetEnvPath(L"WINDIR"), L"Fonts");
    return JoinPath(base, path);
}

void AddFontItem(const std::wstring& path, const std::wstring& displayName,
                 bool alwaysCustom, std::vector<PtsBackupItem>& items,
                 std::set<std::wstring>& seen) {
    if (!FileExists(path) || !HasFontExtension(path)) return;
    std::wstring fileName = FileNameOnly(path);
    if (!alwaysCustom && IsDefaultWindowsFont(fileName, displayName)) return;
    uint64_t size = 0;
    if (!GetFileSize64(path, size) || size > kPtsMaxSingleFileBytes) return;
    std::wstring key = LowerWide(path);
    if (!seen.insert(key).second) return;
    PtsBackupItem item{};
    item.kind = PtsEntryKind::Font;
    item.sourcePath = path;
    item.rootToken = L"FONT";
    item.relativePath = fileName;
    item.displayName = displayName.empty() ? fileName : displayName;
    item.size = size;
    items.push_back(std::move(item));
}

bool GatherFontsFromRegistry(HKEY hive, bool userScope, std::vector<PtsBackupItem>& items,
                             std::set<std::wstring>& seen,
                             const PtsProgressCallback& progress,
                             int progressPercent,
                             size_t& visited,
                             const PtsCancellationCallback& cancelled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
        return !(cancelled && cancelled());
    }

    for (DWORD index = 0;; ++index) {
        if (cancelled && cancelled()) {
            RegCloseKey(key);
            return false;
        }
        wchar_t valueName[512] = {};
        DWORD valueNameLen = static_cast<DWORD>(std::size(valueName));
        wchar_t data[2048] = {};
        DWORD dataSize = sizeof(data);
        DWORD type = 0;
        LONG rc = RegEnumValueW(key, index, valueName, &valueNameLen, nullptr,
                                &type, reinterpret_cast<LPBYTE>(data), &dataSize);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;
        ++visited;
        if (progress && (visited == 1 || visited % 64 == 0)) {
            progress(progressPercent, L"Đang kiểm tra danh sách font (" +
                                          std::to_wstring(visited) + L" mục)...");
        }
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
        std::wstring raw(data, data + (dataSize / sizeof(wchar_t)));
        if (!raw.empty() && raw.back() == L'\0') raw.pop_back();
        std::wstring path = ResolveFontRegistryPath(raw, userScope);
        AddFontItem(path, std::wstring(valueName, valueNameLen), userScope, items, seen);
    }

    RegCloseKey(key);
    return true;
}

bool GatherFontItems(std::vector<PtsBackupItem>& items,
                     const PtsProgressCallback& progress = {},
                     const PtsCancellationCallback& cancelled = {}) {
    std::set<std::wstring> seen;
    size_t registryValues = 0;
    if (progress) progress(14, L"Đang đọc font của người dùng hiện tại...");
    if (!GatherFontsFromRegistry(HKEY_CURRENT_USER, true, items, seen, progress, 15,
                                 registryValues, cancelled)) {
        return false;
    }
    if (progress) progress(16, L"Đang đọc font đã cài trên máy...");
    if (!GatherFontsFromRegistry(HKEY_LOCAL_MACHINE, false, items, seen, progress, 17,
                                 registryValues, cancelled)) {
        return false;
    }

    std::wstring userFonts = JoinPath(GetEnvPath(L"LOCALAPPDATA"), L"Microsoft\\Windows\\Fonts");
    if (DirectoryExists(userFonts)) {
        std::vector<std::wstring> files;
        size_t visited = 0;
        if (!CollectFilesRecursive(userFonts, files, false, progress, 18,
                                   L"Đang quét thư mục font người dùng", &visited,
                                   cancelled)) {
            return false;
        }
        size_t processed = 0;
        for (const auto& file : files) {
            if (cancelled && cancelled()) return false;
            ++processed;
            if (progress && (processed == 1 || processed % 64 == 0)) {
                progress(19, L"Đang kiểm tra font người dùng (" +
                                 std::to_wstring(processed) + L" file)...");
            }
            AddFontItem(file, FileNameOnly(file), true, items, seen);
        }
    }
    return !(cancelled && cancelled());
}

bool WriteAll(HANDLE file, const void* data, DWORD bytes) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    DWORD remaining = bytes;
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(file, ptr, remaining, &written, nullptr) || written == 0) return false;
        ptr += written;
        remaining -= written;
    }
    return true;
}

bool ReadAll(HANDLE file, void* data, DWORD bytes) {
    uint8_t* ptr = static_cast<uint8_t*>(data);
    DWORD remaining = bytes;
    while (remaining > 0) {
        DWORD read = 0;
        if (!ReadFile(file, ptr, remaining, &read, nullptr) || read == 0) return false;
        ptr += read;
        remaining -= read;
    }
    return true;
}

bool WriteU8(HANDLE file, uint8_t value) {
    return WriteAll(file, &value, 1);
}

bool ReadU8(HANDLE file, uint8_t& value) {
    return ReadAll(file, &value, 1);
}

bool WriteU32File(HANDLE file, uint32_t value) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>(value & 0xff),
        static_cast<uint8_t>((value >> 8) & 0xff),
        static_cast<uint8_t>((value >> 16) & 0xff),
        static_cast<uint8_t>((value >> 24) & 0xff),
    };
    return WriteAll(file, bytes, 4);
}

bool ReadU32File(HANDLE file, uint32_t& value) {
    uint8_t bytes[4] = {};
    if (!ReadAll(file, bytes, 4)) return false;
    value = static_cast<uint32_t>(bytes[0]) |
            (static_cast<uint32_t>(bytes[1]) << 8) |
            (static_cast<uint32_t>(bytes[2]) << 16) |
            (static_cast<uint32_t>(bytes[3]) << 24);
    return true;
}

bool WriteU64File(HANDLE file, uint64_t value) {
    uint8_t bytes[8] = {};
    for (int i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
    return WriteAll(file, bytes, 8);
}

bool ReadU64File(HANDLE file, uint64_t& value) {
    uint8_t bytes[8] = {};
    if (!ReadAll(file, bytes, 8)) return false;
    value = 0;
    for (int i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    return true;
}

bool WriteUtf8String(HANDLE file, const std::wstring& value) {
    std::string utf8 = WideToUtf8(value);
    if (utf8.size() > 1024u * 1024u) return false;
    return WriteU32File(file, static_cast<uint32_t>(utf8.size())) &&
           (utf8.empty() || WriteAll(file, utf8.data(), static_cast<DWORD>(utf8.size())));
}

bool ReadUtf8String(HANDLE file, std::wstring& value) {
    uint32_t size = 0;
    if (!ReadU32File(file, size) || size > 1024u * 1024u) return false;
    std::string utf8(size, '\0');
    if (size > 0 && !ReadAll(file, utf8.data(), size)) return false;
    value = Utf8ToWide(utf8, false);
    return true;
}

HANDLE CreateTemporarySiblingFile(const std::wstring& path, DWORD desiredAccess,
                                  DWORD attributes, std::wstring& tempPath) {
    static volatile LONG tempSequence = 0;
    DWORD lastError = ERROR_FILE_EXISTS;
    for (int attempt = 0; attempt < 32; ++attempt) {
        LONG sequence = InterlockedIncrement(&tempSequence);
        tempPath = path + L".tooltype-tmp-" + std::to_wstring(GetCurrentProcessId()) +
                   L"-" + std::to_wstring(sequence);
        HANDLE file = CreateFileW(tempPath.c_str(), desiredAccess, 0, nullptr, CREATE_NEW,
                                  attributes, nullptr);
        if (file != INVALID_HANDLE_VALUE) return file;
        lastError = GetLastError();
        if (lastError != ERROR_FILE_EXISTS && lastError != ERROR_ALREADY_EXISTS) break;
    }
    SetLastError(lastError);
    return INVALID_HANDLE_VALUE;
}

void ClearTemporaryFileAttribute(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_TEMPORARY)) {
        SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_TEMPORARY);
    }
}

bool RestoreReplacementBackup(const std::wstring& path, const std::wstring& backupPath) {
    if (!FileExists(backupPath)) return FileExists(path);
    if (MoveFileExW(backupPath.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }

    if (!CopyFileW(backupPath.c_str(), path.c_str(), FALSE)) return false;
    HANDLE restored = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (restored != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(restored);
        CloseHandle(restored);
    }
    DeleteFileW(backupPath.c_str());
    return true;
}

bool CommitTemporarySiblingFile(const std::wstring& path, const std::wstring& tempPath,
                                DWORD& errorCode,
                                const PtsReplaceFileCallback& replaceFile = {},
                                std::wstring* recoveryBackupPath = nullptr) {
    if (recoveryBackupPath) recoveryBackupPath->clear();
    DWORD attrs = GetFileAttributesW(path.c_str());
    bool destinationExists = attrs != INVALID_FILE_ATTRIBUTES;
    bool clearedReadOnly = destinationExists && !(attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                           (attrs & FILE_ATTRIBUTE_READONLY);
    if (clearedReadOnly) SetFileAttributesW(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);

    if (!destinationExists) {
        if (MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
            ClearTemporaryFileAttribute(path);
            return true;
        }
        errorCode = GetLastError();
        return false;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        errorCode = ERROR_ACCESS_DENIED;
        return false;
    }

    const std::wstring backupPath = tempPath + L".previous";
    if (GetFileAttributesW(backupPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        errorCode = ERROR_FILE_EXISTS;
        if (clearedReadOnly) SetFileAttributesW(path.c_str(), attrs);
        return false;
    }
    BOOL replaced = replaceFile
                        ? replaceFile(path, tempPath, backupPath,
                                      REPLACEFILE_IGNORE_MERGE_ERRORS)
                        : ReplaceFileW(path.c_str(), tempPath.c_str(), backupPath.c_str(),
                                       REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr);
    if (replaced) {
        DeleteFileW(backupPath.c_str());
        ClearTemporaryFileAttribute(path);
        return true;
    }

    errorCode = GetLastError();
    if (FileExists(backupPath)) {
        if (!RestoreReplacementBackup(path, backupPath) && recoveryBackupPath) {
            *recoveryBackupPath = backupPath;
        }
    }
    if (clearedReadOnly) SetFileAttributesW(path.c_str(), attrs);
    return false;
}

bool WriteBytesToFile(const std::wstring& path, const std::vector<uint8_t>& bytes,
                      std::wstring& error) {
    if (!EnsureParentDirectory(path)) {
        error = L"Không tạo được thư mục đích.";
        return false;
    }

    std::wstring tempPath;
    HANDLE file = CreateTemporarySiblingFile(path, GENERIC_WRITE, FILE_ATTRIBUTE_TEMPORARY,
                                             tempPath);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD rc = GetLastError();
        error = L"Không tạo được file tạm để khôi phục.\nMã lỗi: " +
                std::to_wstring(rc) + L".";
        return false;
    }

    bool ok = (bytes.empty() ||
               WriteAll(file, bytes.data(), static_cast<DWORD>(bytes.size()))) &&
              FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) {
        DWORD rc = GetLastError();
        DeleteFileW(tempPath.c_str());
        error = L"Lỗi ghi file tạm khi khôi phục.\nMã lỗi: " +
                std::to_wstring(rc) + L".";
        return false;
    }

    DWORD rc = ERROR_SUCCESS;
    std::wstring recoveryBackupPath;
    if (!CommitTemporarySiblingFile(path, tempPath, rc, {}, &recoveryBackupPath)) {
        DeleteFileW(tempPath.c_str());
        error = L"Không thay được file đích khi khôi phục.\nMã lỗi: " +
                std::to_wstring(rc) + L".";
        if (!recoveryBackupPath.empty()) {
            error += L"\nBản gốc an toàn còn tại: " + recoveryBackupPath;
        }
        return false;
    }
    return true;
}

void CompressForArchive(const std::vector<uint8_t>& input, uint8_t& method,
                        std::vector<uint8_t>& output) {
    method = 0;
    output = input;
    if (input.empty()) return;
    uLongf bound = compressBound(static_cast<uLong>(input.size()));
    std::vector<uint8_t> compressed(static_cast<size_t>(bound));
    int rc = compress2(compressed.data(), &bound, input.data(), static_cast<uLong>(input.size()),
                       Z_BEST_COMPRESSION);
    if (rc == Z_OK && bound < input.size()) {
        compressed.resize(static_cast<size_t>(bound));
        output.swap(compressed);
        method = 1;
    }
}

bool DecompressFromArchive(const std::vector<uint8_t>& input, uint8_t method,
                           uint64_t originalSize, std::vector<uint8_t>& output) {
    if (originalSize > kPtsMaxSingleFileBytes) return false;
    if (method == 0) {
        if (input.size() != originalSize) return false;
        output = input;
        return true;
    }
    if (method != 1) return false;
    output.assign(static_cast<size_t>(originalSize), 0);
    uLongf outSize = static_cast<uLongf>(output.size());
    int rc = uncompress(output.data(), &outSize, input.data(), static_cast<uLong>(input.size()));
    if (rc != Z_OK || outSize != originalSize) return false;
    return true;
}

std::wstring TruncatePtsNoticeLine(const std::wstring& line, size_t maxChars) {
    if (line.size() <= maxChars) return line;
    if (maxChars <= 3) return line.substr(0, maxChars);
    return TrimCopy(line.substr(0, maxChars - 3)) + L"...";
}

std::wstring CollapsePtsWhitespace(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    bool pendingSpace = false;
    for (wchar_t ch : text) {
        if (iswspace(ch)) {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) out.push_back(L' ');
        pendingSpace = false;
        out.push_back(ch);
    }
    return out;
}

bool AddCompactPtsNoticeLine(std::vector<std::wstring>& lines, const std::wstring& rawLine) {
    constexpr size_t kPtsNoticeLineLimit = 40;
    std::wstring remaining = CollapsePtsWhitespace(rawLine);
    while (!remaining.empty() && lines.size() < 2) {
        if (remaining.size() <= kPtsNoticeLineLimit) {
            lines.push_back(remaining);
            return true;
        }

        size_t split = remaining.rfind(L' ', kPtsNoticeLineLimit);
        if (split == std::wstring::npos || split < 16) {
            lines.push_back(TruncatePtsNoticeLine(remaining, kPtsNoticeLineLimit));
            return false;
        }

        lines.push_back(TrimCopy(remaining.substr(0, split)));
        remaining = TrimCopy(remaining.substr(split + 1));
    }
    return remaining.empty();
}

void MarkCompactPtsNoticeAsTruncated(std::wstring& line) {
    constexpr size_t kPtsNoticeLineLimit = 40;
    if (line.size() >= 3 && line.compare(line.size() - 3, 3, L"...") == 0) return;
    if (line.size() + 3 <= kPtsNoticeLineLimit) {
        line += L"...";
        return;
    }
    line = TrimCopy(line.substr(0, kPtsNoticeLineLimit - 3)) + L"...";
}

std::wstring CompactPtsNotice(const std::wstring& text) {
    std::vector<std::wstring> lines;
    const auto rawLines = SplitLines(text);
    bool truncated = false;
    for (size_t index = 0; index < rawLines.size(); ++index) {
        if (!AddCompactPtsNoticeLine(lines, rawLines[index])) {
            truncated = true;
            break;
        }
        if (lines.size() >= 2) {
            for (size_t remaining = index + 1; remaining < rawLines.size(); ++remaining) {
                if (!CollapsePtsWhitespace(rawLines[remaining]).empty()) {
                    truncated = true;
                    break;
                }
            }
            break;
        }
    }
    if (lines.empty()) return L"...";
    if (truncated) MarkCompactPtsNoticeAsTruncated(lines.back());
    if (lines.size() == 1) return lines.front();
    return lines[0] + L"\n" + lines[1];
}

int ScalePtsMetric(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi == 0 ? 96 : dpi), 96);
}

int PtsDialogFontHeight(UINT dpi) {
    return -ScalePtsMetric(13, dpi);
}

struct PtsDialogLayout {
    int clientWidth = 0;
    int clientHeight = 0;
    RECT title{};
    RECT backupSettings{};
    RECT backupFonts{};
    RECT backupAll{};
    RECT restore{};
    RECT status{};
    RECT percent{};
    RECT progress{};
    RECT close{};
};

PtsDialogLayout CalculatePtsDialogLayout(UINT dpi) {
    auto scaledRect = [dpi](LONG left, LONG top, LONG right, LONG bottom) {
        return RECT{ScalePtsMetric(left, dpi), ScalePtsMetric(top, dpi),
                    ScalePtsMetric(right, dpi), ScalePtsMetric(bottom, dpi)};
    };

    PtsDialogLayout layout{};
    layout.clientWidth = ScalePtsMetric(520, dpi);
    layout.clientHeight = ScalePtsMetric(330, dpi);
    layout.title = scaledRect(18, 16, 502, 40);
    layout.backupSettings = scaledRect(18, 58, 254, 94);
    layout.backupFonts = scaledRect(266, 58, 502, 94);
    layout.backupAll = scaledRect(18, 104, 254, 140);
    layout.restore = scaledRect(266, 104, 502, 140);
    layout.status = scaledRect(18, 156, 410, 210);
    layout.percent = scaledRect(422, 156, 502, 180);
    layout.progress = scaledRect(18, 220, 502, 240);
    layout.close = scaledRect(390, 280, 502, 312);
    return layout;
}

DWORD PtsDialogExtendedStyle(bool topMost) {
    return WS_EX_TOOLWINDOW | (topMost ? WS_EX_TOPMOST : 0);
}

UINT QueryPtsDialogDpi(HWND owner) {
    HDC dc = GetDC(owner);
    if (!dc) return 96;
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(owner, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}

HFONT CreatePtsDialogFont(HFONT baseFont, UINT dpi) {
    LOGFONTW font{};
    if (!baseFont || GetObjectW(baseFont, sizeof(font), &font) != sizeof(font)) {
        font.lfCharSet = DEFAULT_CHARSET;
        std::wcscpy(font.lfFaceName, L"Segoe UI");
    }
    font.lfHeight = PtsDialogFontHeight(dpi);
    return CreateFontIndirectW(&font);
}

std::wstring FormatBytes(uint64_t bytes) {
    wchar_t text[64] = {};
    if (bytes >= 1024ull * 1024ull * 1024ull) {
        swprintf(text, 64, L"%.1f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ull * 1024ull) {
        swprintf(text, 64, L"%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024ull) {
        swprintf(text, 64, L"%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        swprintf(text, 64, L"%llu B", static_cast<unsigned long long>(bytes));
    }
    return text;
}

bool CreatePtsBackupArchive(const std::wstring& path, bool includeSettings, bool includeFonts,
                             const PtsBackupOptions& options,
                             const PtsProgressCallback& progress, std::wstring& summary,
                             std::wstring& error,
                             const PtsCancellationCallback& cancelled = {},
                             const PtsPhotoshopRunningCallback& photoshopRunning = {},
                             const PtsCommitCallback& beginCommit = {}) {
    std::vector<PtsBackupItem> items;
    auto cancellationRequested = [&] { return cancelled && cancelled(); };
    if (includeSettings &&
        (photoshopRunning ? photoshopRunning() : IsPhotoshopRunning())) {
        error = L"Photoshop đang chạy. Hãy đóng Photoshop trước khi sao lưu cài đặt "
                L"để workspace/layout hiện tại được ghi đầy đủ xuống đĩa.";
        return false;
    }
    if (includeSettings) {
        progress(5, options.selectedPhotoshopVersionLabel.empty()
                        ? L"Đang tìm cài đặt Photoshop trong AppData..."
                        : L"Đang tìm cài đặt của " + options.selectedPhotoshopVersionLabel + L"...");
        if (!GatherPhotoshopSettingItems(items, options, progress, cancelled)) {
            error = L"Đã hủy sao lưu Pts.";
            return false;
        }
    }
    size_t settingsCount = items.size();
    if (includeFonts) {
        progress(includeSettings ? 14 : 8, L"Đang tìm font tùy chỉnh...");
        if (!GatherFontItems(items, progress, cancelled)) {
            error = L"Đã hủy sao lưu Pts.";
            return false;
        }
    }
    size_t fontCount = items.size() - settingsCount;
    if (items.empty()) {
        error = L"Không tìm thấy cài đặt Photoshop hoặc font tùy chỉnh để sao lưu.";
        return false;
    }

    progress(20, L"Đang tạo file sao lưu .afang...");
    if (cancellationRequested()) {
        error = L"Đã hủy sao lưu Pts.";
        return false;
    }
    std::wstring tempPath;
    HANDLE file = CreateTemporarySiblingFile(path, GENERIC_WRITE | GENERIC_READ,
                                             FILE_ATTRIBUTE_TEMPORARY, tempPath);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Không tạo được file sao lưu .afang.";
        return false;
    }

    bool ok = WriteAll(file, kAfangMagic, sizeof(kAfangMagic)) &&
              WriteU32File(file, kAfangVersion) &&
              WriteU32File(file, 0);
    uint32_t writtenEntries = 0;
    uint32_t skipped = 0;
    uint64_t sourceBytes = 0;
    uint64_t archiveBytes = 0;
    bool cancellationObserved = false;

    for (size_t i = 0; ok && i < items.size(); ++i) {
        if (cancellationRequested()) {
            cancellationObserved = true;
            break;
        }
        const auto& item = items[i];
        int percent = 20 + static_cast<int>((i * 72) / std::max<size_t>(1, items.size()));
        progress(percent, L"Đang nén " + FileNameOnly(item.sourcePath) +
                          L" (" + std::to_wstring(i + 1) + L"/" +
                          std::to_wstring(items.size()) + L")...");

        std::vector<uint8_t> raw;
        std::wstring readError;
        if (!ReadFileBytes(item.sourcePath, raw, readError)) {
            ++skipped;
            continue;
        }
        uint8_t method = 0;
        std::vector<uint8_t> stored;
        CompressForArchive(raw, method, stored);
        if (cancellationRequested()) {
            cancellationObserved = true;
            break;
        }
        sourceBytes += static_cast<uint64_t>(raw.size());
        archiveBytes += static_cast<uint64_t>(stored.size());

        ok = WriteU8(file, static_cast<uint8_t>(item.kind)) &&
             WriteUtf8String(file, item.rootToken) &&
             WriteUtf8String(file, item.relativePath) &&
             WriteUtf8String(file, item.displayName) &&
             WriteU64File(file, static_cast<uint64_t>(raw.size())) &&
             WriteU64File(file, static_cast<uint64_t>(stored.size())) &&
             WriteU8(file, method) &&
             (stored.empty() || WriteAll(file, stored.data(), static_cast<DWORD>(stored.size())));
        if (ok) {
            ++writtenEntries;
        }
    }

    if (cancellationRequested()) cancellationObserved = true;
    if (ok && !cancellationObserved) {
        LARGE_INTEGER pos{};
        pos.QuadPart = sizeof(kAfangMagic) + sizeof(uint32_t);
        ok = SetFilePointerEx(file, pos, nullptr, FILE_BEGIN) &&
             WriteU32File(file, writtenEntries) &&
             FlushFileBuffers(file);
    }
    CloseHandle(file);

    if (cancellationRequested()) cancellationObserved = true;
    if (cancellationObserved) {
        DeleteFileW(tempPath.c_str());
        error = L"Đã hủy sao lưu Pts. File chưa hoàn tất đã được xóa; file sao lưu cũ (nếu có) vẫn được giữ nguyên.";
        return false;
    }

    if (!ok || writtenEntries == 0) {
        DeleteFileW(tempPath.c_str());
        error = L"Lỗi khi ghi file sao lưu .afang.";
        return false;
    }

    if ((beginCommit && !beginCommit()) || (!beginCommit && cancellationRequested())) {
        DeleteFileW(tempPath.c_str());
        error = L"Đã hủy sao lưu Pts trước khi thay file đích; "
                L"file sao lưu cũ (nếu có) vẫn được giữ nguyên.";
        return false;
    }

    DWORD commitError = ERROR_SUCCESS;
    std::wstring recoveryBackupPath;
    if (!CommitTemporarySiblingFile(path, tempPath, commitError, {},
                                    &recoveryBackupPath)) {
        DeleteFileW(tempPath.c_str());
        error = L"Không thay được file sao lưu .afang đích.\nMã lỗi: " +
                std::to_wstring(commitError) + L".";
        if (!recoveryBackupPath.empty()) {
            error += L"\nFile sao lưu cũ an toàn còn tại: " + recoveryBackupPath;
        }
        return false;
    }

    progress(100, L"Đã tạo xong file sao lưu .afang.");
    summary = L"Đã sao lưu " + std::to_wstring(writtenEntries) + L" file (" +
              FormatBytes(sourceBytes) + L" → " + FormatBytes(archiveBytes) + L").";
    if (settingsCount > 0 || fontCount > 0) {
        summary += L" Cài đặt: " + std::to_wstring(settingsCount) +
                   L", font tùy chỉnh: " + std::to_wstring(fontCount) + L".";
    }
    if (includeSettings && !options.selectedPhotoshopVersionLabel.empty()) {
        summary += L" Phiên bản Photoshop: " + options.selectedPhotoshopVersionLabel + L".";
    }
    if (skipped > 0) {
        summary += L" Bỏ qua " + std::to_wstring(skipped) + L" file không đọc được.";
    }
    return true;
}

std::wstring FontRegistryNameFromFile(const std::wstring& path) {
    std::wstring file = FileNameOnly(path);
    std::wstring ext = ExtensionLower(file);
    std::wstring stem = file;
    size_t dot = stem.find_last_of(L'.');
    if (dot != std::wstring::npos) stem.resize(dot);
    if (ext == L".otf") return stem + L" (OpenType)";
    return stem + L" (TrueType)";
}

struct RestoredUserFont {
    std::wstring path;
    std::wstring displayName;
};

struct FontRegistrationResult {
    uint32_t resourceRegistrations = 0;
    uint32_t registryUpdates = 0;
};

std::wstring SafeFontRegistryValueName(const RestoredUserFont& font) {
    std::wstring name = font.displayName.empty()
                            ? FontRegistryNameFromFile(font.path)
                            : font.displayName;
    name.erase(std::remove_if(name.begin(), name.end(), [](wchar_t ch) {
                   return ch == L'\r' || ch == L'\n' || ch == L'\t' || ch < 32;
               }),
               name.end());
    if (name.size() > 200) name.resize(200);
    if (name.empty()) name = FontRegistryNameFromFile(font.path);
    return name;
}

std::wstring NonConflictingFontRegistryValueName(HKEY key, const RestoredUserFont& font) {
    std::wstring base = SafeFontRegistryValueName(font);
    for (int i = 0; i <= 999; ++i) {
        std::wstring candidate = i == 0
                                     ? base
                                     : base + L" (ToolType restored " +
                                           std::to_wstring(i) + L")";
        DWORD type = 0;
        DWORD bytes = 0;
        LONG rc = RegQueryValueExW(key, candidate.c_str(), nullptr, &type, nullptr, &bytes);
        if (rc == ERROR_FILE_NOT_FOUND) return candidate;
        if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) ||
            bytes < sizeof(wchar_t)) {
            continue;
        }
        std::wstring existing(bytes / sizeof(wchar_t), L'\0');
        if (RegQueryValueExW(key, candidate.c_str(), nullptr, &type,
                             reinterpret_cast<BYTE*>(existing.data()), &bytes) != ERROR_SUCCESS) {
            continue;
        }
        while (!existing.empty() && existing.back() == L'\0') existing.pop_back();
        if (LowerWide(existing) == LowerWide(font.path)) return candidate;
    }
    return L"";
}

FontRegistrationResult RegisterRestoredUserFonts(
    const std::vector<RestoredUserFont>& fonts,
    const PtsProgressCallback& progress = {},
    const PtsCancellationCallback& cancelled = {}) {
    FontRegistrationResult result{};
    HKEY key = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                    0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr);

    for (size_t index = 0; index < fonts.size(); ++index) {
        if (cancelled && cancelled()) break;
        const auto& font = fonts[index];
        if (AddFontResourceExW(font.path.c_str(), 0, nullptr) > 0) {
            ++result.resourceRegistrations;
        }
        if (key) {
            std::wstring valueName = NonConflictingFontRegistryValueName(key, font);
            if (!valueName.empty() &&
                RegSetValueExW(key, valueName.c_str(), 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(font.path.c_str()),
                               static_cast<DWORD>((font.path.size() + 1) * sizeof(wchar_t))) ==
                    ERROR_SUCCESS) {
                ++result.registryUpdates;
            }
        }
        if (progress && (index == 0 || (index + 1) % 8 == 0 || index + 1 == fonts.size())) {
            int percent = 95 + static_cast<int>(((index + 1) * 2) / fonts.size());
            progress(percent, L"Đang đăng ký font " + std::to_wstring(index + 1) + L"/" +
                                  std::to_wstring(fonts.size()) + L"...");
        }
    }

    if (key) RegCloseKey(key);
    return result;
}

bool FileMatchesBytes(const std::wstring& path, const std::vector<uint8_t>& bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<uint64_t>(size.QuadPart) != bytes.size()) {
        CloseHandle(file);
        return false;
    }

    uint8_t buffer[64 * 1024] = {};
    size_t offset = 0;
    bool equal = true;
    while (offset < bytes.size()) {
        DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(bytes.size() - offset, sizeof(buffer)));
        DWORD read = 0;
        if (!ReadFile(file, buffer, chunk, &read, nullptr) || read != chunk ||
            std::memcmp(buffer, bytes.data() + offset, chunk) != 0) {
            equal = false;
            break;
        }
        offset += read;
    }
    CloseHandle(file);
    return equal;
}

std::wstring ReusableOrNonConflictingFontPath(const std::wstring& desiredPath,
                                              const std::vector<uint8_t>& bytes,
                                              bool& identicalFileExists) {
    identicalFileExists = false;
    if (!FileExists(desiredPath)) return desiredPath;
    if (FileMatchesBytes(desiredPath, bytes)) {
        identicalFileExists = true;
        return desiredPath;
    }

    const std::wstring directory = ParentPath(desiredPath);
    const std::wstring name = FileNameOnly(desiredPath);
    std::wstring stem = name;
    std::wstring extension;
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        stem = name.substr(0, dot);
        extension = name.substr(dot);
    }

    for (int i = 1; i <= 999; ++i) {
        std::wstring candidate = JoinPath(
            directory, stem + L"-restored-" + std::to_wstring(i) + extension);
        if (!FileExists(candidate)) return candidate;
        if (FileMatchesBytes(candidate, bytes)) {
            identicalFileExists = true;
            return candidate;
        }
    }
    return L"";
}

bool ReadPtsArchiveHeader(HANDLE file, uint32_t& count, std::wstring& error) {
    char magic[8] = {};
    uint32_t version = 0;
    count = 0;
    bool ok = ReadAll(file, magic, sizeof(magic)) &&
              std::memcmp(magic, kAfangMagic, sizeof(kAfangMagic)) == 0 &&
              ReadU32File(file, version) &&
              ReadU32File(file, count) &&
              version == kAfangVersion &&
              count < 200000;
    if (!ok) {
        error = L"File .afang không đúng định dạng hoặc không hỗ trợ phiên bản này.";
    }
    return ok;
}

bool ValidatePtsArchiveEntryMetadata(uint8_t kindByte, const std::wstring& rootToken,
                                     const std::wstring& relativePath,
                                     uint64_t originalSize, uint64_t storedSize,
                                     uint8_t method, std::wstring& error) {
    if (kindByte != static_cast<uint8_t>(PtsEntryKind::PhotoshopSetting) &&
        kindByte != static_cast<uint8_t>(PtsEntryKind::Font)) {
        error = L"File .afang chứa loại dữ liệu không được hỗ trợ.";
        return false;
    }
    if (originalSize > kPtsMaxSingleFileBytes || storedSize > kPtsMaxSingleFileBytes ||
        (method != 0 && method != 1)) {
        error = L"File .afang bị lỗi ở phần metadata.";
        return false;
    }
    if (kindByte == static_cast<uint8_t>(PtsEntryKind::Font)) {
        if (rootToken != L"FONT" || !IsValidWindowsBaseFileName(relativePath) ||
            !HasFontExtension(relativePath)) {
            error = L"File .afang chứa font có tên file không hợp lệ.";
            return false;
        }
        return true;
    }

    if ((rootToken != L"APPDATA" && rootToken != L"LOCALAPPDATA") ||
        !IsAllowedPhotoshopSettingsRelativePath(relativePath)) {
        error = L"File .afang chứa đường dẫn cài đặt Photoshop không hợp lệ.";
        return false;
    }
    return true;
}

bool ReadPtsArchiveEntryMetadata(HANDLE file, uint8_t& kindByte, std::wstring& rootToken,
                                 std::wstring& relativePath, std::wstring& displayName,
                                 uint64_t& originalSize, uint64_t& storedSize,
                                 uint8_t& method, std::wstring& error) {
    if (!ReadU8(file, kindByte) ||
        !ReadUtf8String(file, rootToken) ||
        !ReadUtf8String(file, relativePath) ||
        !ReadUtf8String(file, displayName) ||
        !ReadU64File(file, originalSize) ||
        !ReadU64File(file, storedSize) ||
        !ReadU8(file, method)) {
        error = L"File .afang bị lỗi ở phần metadata.";
        return false;
    }
    relativePath = NormalizeRelativePathSlashes(relativePath);
    return ValidatePtsArchiveEntryMetadata(kindByte, rootToken, relativePath,
                                           originalSize, storedSize, method, error);
}

bool SkipFileBytes(HANDLE file, uint64_t bytes) {
    LARGE_INTEGER zero{};
    LARGE_INTEGER current{};
    LARGE_INTEGER fileSize{};
    if (!SetFilePointerEx(file, zero, &current, FILE_CURRENT) ||
        !GetFileSizeEx(file, &fileSize) || current.QuadPart < 0 ||
        fileSize.QuadPart < current.QuadPart) {
        return false;
    }
    uint64_t remaining = static_cast<uint64_t>(fileSize.QuadPart - current.QuadPart);
    if (bytes > remaining || bytes > static_cast<uint64_t>(INT64_MAX)) return false;
    LARGE_INTEGER move{};
    move.QuadPart = static_cast<LONGLONG>(bytes);
    return SetFilePointerEx(file, move, nullptr, FILE_CURRENT) != FALSE;
}

void AddArchiveVersion(PtsArchiveScanResult& scan, const std::wstring& label,
                       const std::wstring& versionKey) {
    std::wstring groupKey = LowerWide(label);
    auto it = std::find_if(scan.versions.begin(), scan.versions.end(), [&](const PtsArchiveVersion& v) {
        return LowerWide(v.label) == groupKey;
    });
    if (it == scan.versions.end()) {
        PtsArchiveVersion version{};
        version.label = label;
        version.rootKeys.insert(versionKey);
        scan.versions.push_back(std::move(version));
    } else {
        it->rootKeys.insert(versionKey);
    }
}

bool InspectPtsArchiveVersions(const std::wstring& path, PtsArchiveScanResult& scan,
                               std::wstring& error,
                               const PtsProgressCallback& progress = {},
                               const PtsCancellationCallback& cancelled = {}) {
    scan = PtsArchiveScanResult{};
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Không mở được file .afang.";
        return false;
    }

    uint32_t count = 0;
    if (!ReadPtsArchiveHeader(file, count, error)) {
        CloseHandle(file);
        return false;
    }
    scan.entryCount = count;
    if (progress) progress(1, L"Đang đọc metadata file .afang...");

    for (uint32_t i = 0; i < count; ++i) {
        if (cancelled && cancelled()) {
            CloseHandle(file);
            error = L"Đã hủy kiểm tra file .afang.";
            return false;
        }
        uint8_t kindByte = 0;
        std::wstring rootToken;
        std::wstring relativePath;
        std::wstring displayName;
        uint64_t originalSize = 0;
        uint64_t storedSize = 0;
        uint8_t method = 0;
        if (!ReadPtsArchiveEntryMetadata(file, kindByte, rootToken, relativePath, displayName,
                                         originalSize, storedSize, method, error)) {
            CloseHandle(file);
            return false;
        }

        if (kindByte == static_cast<uint8_t>(PtsEntryKind::PhotoshopSetting)) {
            scan.hasSettings = true;
            std::wstring relativeRoot;
            std::wstring label;
            if (!ExtractPhotoshopVersionInfo(relativePath, relativeRoot, label)) {
                CloseHandle(file);
                error = L"File .afang chứa cài đặt Photoshop không xác định được phiên bản.";
                return false;
            }
            AddArchiveVersion(scan, label, PtsPhotoshopVersionKey(rootToken, relativeRoot));
        } else if (kindByte == static_cast<uint8_t>(PtsEntryKind::Font)) {
            scan.hasFonts = true;
        }

        if (!SkipFileBytes(file, storedSize)) {
            CloseHandle(file);
            error = L"File .afang bị thiếu dữ liệu.";
            return false;
        }
        if (progress && (i == 0 || (i + 1) % 50 == 0 || i + 1 == count)) {
            int percent = count == 0
                              ? 8
                              : 1 + static_cast<int>((static_cast<uint64_t>(i + 1) * 7) / count);
            progress(percent, L"Đang kiểm tra mục " + std::to_wstring(i + 1) + L"/" +
                                  std::to_wstring(count) + L" trong .afang...");
        }
    }

    if (count == 0 && progress) progress(8, L"Đã kiểm tra metadata file .afang.");

    CloseHandle(file);
    return true;
}

bool EndsWithCaseInsensitive(const std::wstring& text, const std::wstring& suffix) {
    if (text.size() < suffix.size()) return false;
    return LowerWide(text.substr(text.size() - suffix.size())) == LowerWide(suffix);
}

bool ShouldRewritePhotoshopVersionLabel(const std::wstring& sourceLabel,
                                        const std::wstring& targetLabel) {
    std::wstring source = NormalizePhotoshopVersionLabel(sourceLabel);
    std::wstring target = NormalizePhotoshopVersionLabel(targetLabel);
    if (source.empty() || target.empty() || LowerWide(source) == LowerWide(target)) {
        return false;
    }
    if (LowerWide(source) == L"photoshop") {
        return false;
    }
    return LooksLikePhotoshopVersionLabel(source) && LooksLikePhotoshopVersionLabel(target);
}

std::wstring PhotoshopSettingsFolderName(const std::wstring& label) {
    std::wstring normalized = NormalizePhotoshopVersionLabel(label);
    return normalized.empty() ? L"" : normalized + L" Settings";
}

bool IsPhotoshopSettingsFolderSegment(const std::wstring& segment) {
    const std::wstring settingsSuffix = L" settings";
    if (!EndsWithCaseInsensitive(segment, settingsSuffix)) return false;
    return LooksLikePhotoshopVersionLabel(NormalizePhotoshopVersionLabel(segment));
}

bool IsPhotoshopSettingsFolderForLabel(const std::wstring& segment,
                                       const std::wstring& label) {
    if (!IsPhotoshopSettingsFolderSegment(segment)) return false;
    std::wstring segmentLabel = NormalizePhotoshopVersionLabel(segment);
    std::wstring normalizedLabel = NormalizePhotoshopVersionLabel(label);
    return !normalizedLabel.empty() && LowerWide(segmentLabel) == LowerWide(normalizedLabel);
}

std::wstring ReplaceAllCaseInsensitive(std::wstring text, const std::wstring& oldText,
                                       const std::wstring& newText) {
    if (oldText.empty()) return text;
    std::wstring lowerText = LowerWide(text);
    std::wstring lowerOld = LowerWide(oldText);
    size_t pos = 0;
    while ((pos = lowerText.find(lowerOld, pos)) != std::wstring::npos) {
        text.replace(pos, oldText.size(), newText);
        lowerText.replace(pos, oldText.size(), LowerWide(newText));
        pos += newText.size();
    }
    return text;
}

std::wstring RewritePhotoshopVersionLabelInSegment(const std::wstring& segment,
                                                   const std::wstring& sourceLabel,
                                                   const std::wstring& targetLabel) {
    std::wstring source = NormalizePhotoshopVersionLabel(sourceLabel);
    std::wstring target = NormalizePhotoshopVersionLabel(targetLabel);
    if (source.empty() || target.empty()) return segment;
    if (LowerWide(segment) == LowerWide(source)) return target;
    if (IsPhotoshopSettingsFolderForLabel(segment, source)) {
        return PhotoshopSettingsFolderName(target);
    }
    if (LowerWide(source) == L"photoshop" && IsPhotoshopSettingsFolderSegment(segment)) {
        return PhotoshopSettingsFolderName(target);
    }
    if (!ShouldRewritePhotoshopVersionLabel(sourceLabel, targetLabel)) {
        return segment;
    }
    return ReplaceAllCaseInsensitive(segment, source, target);
}

bool BuildMappedPhotoshopSettingsRelativePath(const std::wstring& sourcePath,
                                               const std::wstring& sourceRoot,
                                               const std::wstring& targetRoot,
                                               const std::wstring& sourceLabel,
                                               const std::wstring& targetLabel,
                                              std::wstring& output) {
    std::vector<std::wstring> sourceParts = SplitRelativePathParts(NormalizeRelativePathSlashes(sourceRoot));
    std::vector<std::wstring> targetParts = SplitRelativePathParts(NormalizeRelativePathSlashes(targetRoot));
    std::vector<std::wstring> pathParts = SplitRelativePathParts(NormalizeRelativePathSlashes(sourcePath));
    if (sourceParts.empty() || targetParts.empty() || pathParts.size() < sourceParts.size()) {
        return false;
    }
    for (size_t i = 0; i < sourceParts.size(); ++i) {
        if (LowerWide(pathParts[i]) != LowerWide(sourceParts[i])) return false;
    }

    std::vector<std::wstring> suffix(pathParts.begin() + sourceParts.size(), pathParts.end());
    std::wstring source = NormalizePhotoshopVersionLabel(sourceLabel);
    std::wstring targetSettingsFolder = PhotoshopSettingsFolderName(targetLabel);
    if (!suffix.empty() &&
        (IsPhotoshopSettingsFolderForLabel(suffix.front(), source) ||
         (LowerWide(source) == L"photoshop" && IsPhotoshopSettingsFolderSegment(suffix.front())))) {
        if (!targetSettingsFolder.empty() &&
            !targetParts.empty() &&
            IsPhotoshopSettingsFolderForLabel(targetParts.back(), targetLabel)) {
            suffix.erase(suffix.begin());
        } else if (!targetSettingsFolder.empty()) {
            suffix.front() = targetSettingsFolder;
        }
    }

    for (auto& part : suffix) {
        part = RewritePhotoshopVersionLabelInSegment(part, sourceLabel, targetLabel);
    }

    std::vector<std::wstring> result = targetParts;
    result.insert(result.end(), suffix.begin(), suffix.end());
    output = JoinRelativePathParts(result, result.size());
    return IsSafeRelativePath(output) && IsAllowedPhotoshopSettingsRelativePath(output);
}

void AddUniquePhotoshopRestoreRelativePath(std::vector<std::wstring>& paths,
                                           const std::wstring& path) {
    std::wstring normalized = NormalizeRelativePathSlashes(path);
    if (!IsSafeRelativePath(normalized) || !IsAllowedPhotoshopSettingsRelativePath(normalized)) {
        return;
    }
    std::wstring key = LowerWide(normalized);
    for (const auto& existing : paths) {
        if (LowerWide(existing) == key) return;
    }
    paths.push_back(std::move(normalized));
}

std::wstring PhotoshopLegacyCsSuffix(const std::wstring& label) {
    std::wstring normalized = NormalizePhotoshopVersionLabel(label);
    std::wstring lower = LowerWide(normalized);
    size_t photoshop = lower.find(L"photoshop");
    if (photoshop == std::wstring::npos) return L"";

    std::wstring suffix = TrimCopy(normalized.substr(photoshop + 9));
    std::wstring lowerSuffix = LowerWide(suffix);
    if (lowerSuffix.rfind(L"cs", 0) != 0) return L"";

    size_t firstSpace = suffix.find_first_of(L" \t");
    std::wstring firstToken = firstSpace == std::wstring::npos
                                  ? suffix
                                  : suffix.substr(0, firstSpace);
    return LowerWide(firstToken).rfind(L"cs", 0) == 0 ? firstToken : L"";
}

std::vector<std::wstring> PhotoshopFilenameLabelsForTarget(const std::wstring& targetLabel) {
    std::vector<std::wstring> labels;
    std::wstring normalized = NormalizePhotoshopVersionLabel(targetLabel);
    if (!normalized.empty()) labels.push_back(normalized);

    std::wstring csSuffix = PhotoshopLegacyCsSuffix(targetLabel);
    if (!csSuffix.empty()) {
        labels.push_back(L"Adobe Photoshop X64 " + csSuffix);
    }
    return labels;
}

bool UsesLegacyPhotoshopWorkspaceNames(const std::wstring& targetLabel) {
    return !PhotoshopLegacyCsSuffix(targetLabel).empty();
}

bool IsPhotoshopPreferenceFileName(const std::wstring& fileName) {
    std::wstring lower = LowerWide(fileName);
    std::wstring ext = ExtensionLower(fileName);
    if (ext != L".psp" && ext != L".psw" && ext != L".xml" && ext != L".kys" &&
        ext != L".mnu") {
        return false;
    }
    return lower.find(L"prefs") != std::wstring::npos ||
           lower.find(L"preferences") != std::wstring::npos ||
           lower.find(L"workspace") != std::wstring::npos ||
           lower.find(L"toolbar") != std::wstring::npos ||
           lower.find(L"tool") != std::wstring::npos ||
           lower.find(L"photoshop") != std::wstring::npos;
}

void AddPhotoshopVersionFilenameAliases(std::vector<std::wstring>& paths,
                                        const std::wstring& primaryPath,
                                        const std::wstring& targetLabel) {
    std::vector<std::wstring> parts = SplitRelativePathParts(primaryPath);
    if (parts.empty() || !IsPhotoshopPreferenceFileName(parts.back())) return;

    std::vector<std::wstring> labels = PhotoshopFilenameLabelsForTarget(targetLabel);
    if (labels.size() <= 1) return;

    std::wstring primaryLabel = labels.front();
    if (primaryLabel.empty()) return;

    for (size_t i = 1; i < labels.size(); ++i) {
        std::wstring aliasName = ReplaceAllCaseInsensitive(parts.back(), primaryLabel, labels[i]);
        if (aliasName == parts.back()) continue;
        std::vector<std::wstring> aliasParts = parts;
        aliasParts.back() = aliasName;
        AddUniquePhotoshopRestoreRelativePath(paths,
                                             JoinRelativePathParts(aliasParts, aliasParts.size()));
    }
}

bool IsWorkspaceDirectoryName(const std::wstring& part) {
    std::wstring lower = LowerWide(part);
    return lower == L"workspaces" || lower == L"workspaces (modified)";
}

void AddLegacyWorkspaceNoExtensionAlias(std::vector<std::wstring>& paths,
                                        const std::vector<std::wstring>& parts,
                                        const std::wstring& targetLabel) {
    if (!UsesLegacyPhotoshopWorkspaceNames(targetLabel) || parts.size() < 2) return;
    if (ExtensionLower(parts.back()) != L".psw") return;

    bool insideWorkspaceDirectory = false;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        if (IsWorkspaceDirectoryName(parts[i])) {
            insideWorkspaceDirectory = true;
            break;
        }
    }
    if (!insideWorkspaceDirectory) return;

    std::vector<std::wstring> aliasParts = parts;
    std::wstring fileName = aliasParts.back();
    aliasParts.back() = fileName.substr(0, fileName.size() - 4);
    AddUniquePhotoshopRestoreRelativePath(paths,
                                          JoinRelativePathParts(aliasParts, aliasParts.size()));
}

void AddWorkspaceCompatibilityAliases(std::vector<std::wstring>& paths,
                                      const std::wstring& primaryPath,
                                      const std::wstring& targetLabel) {
    std::vector<std::wstring> parts = SplitRelativePathParts(primaryPath);
    if (parts.size() < 2) return;

    bool touchedWorkspace = false;
    std::vector<size_t> workspaceIndexes;
    AddLegacyWorkspaceNoExtensionAlias(paths, parts, targetLabel);
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        std::wstring lower = LowerWide(parts[i]);
        if (!IsWorkspaceDirectoryName(parts[i])) continue;
        touchedWorkspace = true;
        workspaceIndexes.push_back(i);

        std::vector<std::wstring> aliasParts = parts;
        aliasParts[i] = lower == L"workspaces (modified)"
                            ? L"WorkSpaces"
                            : L"WorkSpaces (Modified)";
        AddUniquePhotoshopRestoreRelativePath(paths,
                                              JoinRelativePathParts(aliasParts, aliasParts.size()));
        AddLegacyWorkspaceNoExtensionAlias(paths, aliasParts, targetLabel);
    }

    std::wstring ext = ExtensionLower(parts.back());
    if (!touchedWorkspace || ext != L".psw") return;

    std::wstring fileName = parts.back();
    std::wstring stem = fileName;
    size_t dot = fileName.find_last_of(L'.');
    if (dot != std::wstring::npos) stem = fileName.substr(0, dot);
    if (EndsWithCaseInsensitive(stem, L" (Modified)")) {
        std::vector<std::wstring> aliasParts = parts;
        std::wstring unmodifiedFileName = stem.substr(0, stem.size() - 11) + ext;
        aliasParts.back() = unmodifiedFileName;
        AddUniquePhotoshopRestoreRelativePath(paths,
                                              JoinRelativePathParts(aliasParts, aliasParts.size()));
        AddLegacyWorkspaceNoExtensionAlias(paths, aliasParts, targetLabel);
        for (size_t index : workspaceIndexes) {
            std::vector<std::wstring> combinedParts = aliasParts;
            std::wstring lower = LowerWide(combinedParts[index]);
            combinedParts[index] = lower == L"workspaces (modified)"
                                       ? L"WorkSpaces"
                                       : L"WorkSpaces (Modified)";
            AddUniquePhotoshopRestoreRelativePath(
                paths, JoinRelativePathParts(combinedParts, combinedParts.size()));
            AddLegacyWorkspaceNoExtensionAlias(paths, combinedParts, targetLabel);
        }
    } else {
        std::vector<std::wstring> aliasParts = parts;
        aliasParts.back() = stem + L" (Modified)" + ext;
        AddUniquePhotoshopRestoreRelativePath(paths,
                                              JoinRelativePathParts(aliasParts, aliasParts.size()));
        AddLegacyWorkspaceNoExtensionAlias(paths, aliasParts, targetLabel);
        for (size_t index : workspaceIndexes) {
            std::vector<std::wstring> combinedParts = aliasParts;
            std::wstring lower = LowerWide(combinedParts[index]);
            combinedParts[index] = lower == L"workspaces (modified)"
                                       ? L"WorkSpaces"
                                       : L"WorkSpaces (Modified)";
            AddUniquePhotoshopRestoreRelativePath(
                paths, JoinRelativePathParts(combinedParts, combinedParts.size()));
            AddLegacyWorkspaceNoExtensionAlias(paths, combinedParts, targetLabel);
        }
    }
}

std::vector<std::wstring> BuildPhotoshopSettingsRestoreRelativePaths(
    const std::wstring& primaryPath, const std::wstring& sourceLabel,
    const std::wstring& targetLabel) {
    std::vector<std::wstring> paths;
    AddUniquePhotoshopRestoreRelativePath(paths, primaryPath);

    const bool crossVersion =
        ShouldRewritePhotoshopVersionLabel(sourceLabel, targetLabel);
    if (crossVersion) {
        AddPhotoshopVersionFilenameAliases(paths, primaryPath, targetLabel);
    }
    if (crossVersion || UsesLegacyPhotoshopWorkspaceNames(targetLabel)) {
        AddWorkspaceCompatibilityAliases(paths, primaryPath, targetLabel);
    }
    return paths;
}

bool LooksLikePhotoshopWorkspaceXml(const std::wstring& text) {
    return text.find(L"<photoshop-workspace") != std::wstring::npos;
}

std::wstring LegacyWorkspaceXmlIdForValue(const std::wstring& value,
                                          std::map<std::wstring, std::wstring>& idMap) {
    if (value.empty()) return L"";
    if (value.size() > 8 || (value.size() > 1 && value.front() == L'0')) return L"";
    unsigned long long numeric = 0;
    for (wchar_t ch : value) {
        if (!iswdigit(ch)) return L"";
        numeric = numeric * 10ull + static_cast<unsigned long long>(ch - L'0');
        if (numeric > 0x00FFFFFFull) return L"";
    }

    auto existing = idMap.find(value);
    if (existing != idMap.end()) return existing->second;

    wchar_t buffer[32] = {};
    swprintf(buffer, std::size(buffer), L"%016llX",
             static_cast<unsigned long long>(0x01000000ull + numeric));
    std::wstring mapped = buffer;
    idMap[value] = mapped;
    return mapped;
}

bool RewriteDecimalXmlAttributeValues(std::wstring& text, const std::wstring& attribute,
                                      std::map<std::wstring, std::wstring>& idMap) {
    bool changed = false;
    std::wstring needle = attribute + L"=\"";
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::wstring::npos) {
        size_t valueStart = pos + needle.size();
        size_t valueEnd = text.find(L'"', valueStart);
        if (valueEnd == std::wstring::npos) break;

        std::wstring value = text.substr(valueStart, valueEnd - valueStart);
        std::wstring mapped = LegacyWorkspaceXmlIdForValue(value, idMap);
        if (!mapped.empty() && mapped != value) {
            text.replace(valueStart, value.size(), mapped);
            valueEnd = valueStart + mapped.size();
            changed = true;
        }
        pos = valueEnd + 1;
    }
    return changed;
}

bool HasRewriteableDecimalXmlAttributeValue(const std::wstring& text,
                                            const std::wstring& attribute) {
    std::map<std::wstring, std::wstring> scratchMap;
    std::wstring needle = attribute + L"=\"";
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::wstring::npos) {
        size_t valueStart = pos + needle.size();
        size_t valueEnd = text.find(L'"', valueStart);
        if (valueEnd == std::wstring::npos) break;

        std::wstring value = text.substr(valueStart, valueEnd - valueStart);
        if (!LegacyWorkspaceXmlIdForValue(value, scratchMap).empty()) return true;
        pos = valueEnd + 1;
    }
    return false;
}

bool RemoveXmlAttributeFromTags(std::wstring& text, const std::wstring& tagName,
                                const std::wstring& attribute) {
    bool changed = false;
    std::wstring tagNeedle = L"<" + tagName;
    std::wstring attributeNeedle = L" " + attribute + L"=\"";
    size_t pos = 0;
    while ((pos = text.find(tagNeedle, pos)) != std::wstring::npos) {
        size_t nameEnd = pos + tagNeedle.size();
        if (nameEnd < text.size() && !iswspace(text[nameEnd]) &&
            text[nameEnd] != L'>' && text[nameEnd] != L'/') {
            pos = nameEnd;
            continue;
        }

        size_t tagEnd = text.find(L'>', nameEnd);
        if (tagEnd == std::wstring::npos) break;
        size_t attributePos = text.find(attributeNeedle, nameEnd);
        if (attributePos == std::wstring::npos || attributePos > tagEnd) {
            pos = tagEnd + 1;
            continue;
        }
        size_t valueEnd = text.find(L'"', attributePos + attributeNeedle.size());
        if (valueEnd == std::wstring::npos || valueEnd > tagEnd) {
            pos = tagEnd + 1;
            continue;
        }
        text.erase(attributePos, valueEnd - attributePos + 1);
        changed = true;
        pos = attributePos;
    }
    return changed;
}

bool ClampNegativeOriginAttributes(std::wstring& text) {
    bool changed = false;
    std::wstring needle = L"origin=\"";
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::wstring::npos) {
        size_t valueStart = pos + needle.size();
        size_t valueEnd = text.find(L'"', valueStart);
        if (valueEnd == std::wstring::npos) break;

        std::wstring value = text.substr(valueStart, valueEnd - valueStart);
        wchar_t* end = nullptr;
        long x = wcstol(value.c_str(), &end, 10);
        if (end == value.c_str()) {
            pos = valueEnd + 1;
            continue;
        }
        while (*end && iswspace(*end)) ++end;
        wchar_t* end2 = nullptr;
        long y = wcstol(end, &end2, 10);
        if (end2 == end) {
            pos = valueEnd + 1;
            continue;
        }
        while (*end2 && iswspace(*end2)) ++end2;
        if (*end2 != L'\0') {
            pos = valueEnd + 1;
            continue;
        }

        long clampedX = std::max<long>(0, x);
        long clampedY = std::max<long>(0, y);
        if (clampedX != x || clampedY != y) {
            std::wstring replacement = std::to_wstring(clampedX) + L" " +
                                       std::to_wstring(clampedY);
            text.replace(valueStart, value.size(), replacement);
            valueEnd = valueStart + replacement.size();
            changed = true;
        }
        pos = valueEnd + 1;
    }
    return changed;
}

int Base64Value(wchar_t ch) {
    if (ch >= L'A' && ch <= L'Z') return static_cast<int>(ch - L'A');
    if (ch >= L'a' && ch <= L'z') return static_cast<int>(ch - L'a') + 26;
    if (ch >= L'0' && ch <= L'9') return static_cast<int>(ch - L'0') + 52;
    if (ch == L'+') return 62;
    if (ch == L'/') return 63;
    return -1;
}

bool DecodeBase64Value(const std::wstring& value, std::vector<uint8_t>& output) {
    output.clear();
    uint32_t accumulator = 0;
    int bits = -8;
    for (wchar_t ch : value) {
        if (ch == L'=') break;
        if (iswspace(ch)) continue;
        int decoded = Base64Value(ch);
        if (decoded < 0) return false;
        accumulator = ((accumulator << 6) | static_cast<uint32_t>(decoded)) & 0x00FFFFFFu;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return true;
}

bool IsPanelIdChar(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') ||
           (ch >= L'A' && ch <= L'Z') ||
           (ch >= L'0' && ch <= L'9') ||
           ch == L'.' || ch == L'_' || ch == L'-';
}

bool MatchesUtf16LeAt(const std::vector<uint8_t>& bytes, size_t pos,
                      const std::wstring& needle) {
    if (pos + needle.size() * 2 > bytes.size()) return false;
    for (size_t i = 0; i < needle.size(); ++i) {
        wchar_t actual = static_cast<wchar_t>(bytes[pos + i * 2] |
                                             (bytes[pos + i * 2 + 1] << 8));
        if (actual != needle[i]) return false;
    }
    return true;
}

std::wstring ExtractWorkspacePanelIdFromAppData(const std::wstring& appData) {
    std::vector<uint8_t> decoded;
    if (!DecodeBase64Value(appData, decoded)) return L"";

    const std::wstring needle = L"panelid.";
    for (size_t pos = 0; pos + needle.size() * 2 <= decoded.size(); ++pos) {
        if (!MatchesUtf16LeAt(decoded, pos, needle)) continue;

        std::wstring panelId;
        for (size_t i = pos; i + 1 < decoded.size(); i += 2) {
            wchar_t ch = static_cast<wchar_t>(decoded[i] | (decoded[i + 1] << 8));
            if (ch == L'\0' || !IsPanelIdChar(ch)) break;
            panelId.push_back(ch);
        }
        return panelId;
    }
    return L"";
}

std::wstring XmlAttributeValue(const std::wstring& tag, const std::wstring& attribute) {
    std::wstring needle = attribute + L"=\"";
    size_t pos = tag.find(needle);
    if (pos == std::wstring::npos) return L"";
    size_t valueStart = pos + needle.size();
    size_t valueEnd = tag.find(L'"', valueStart);
    if (valueEnd == std::wstring::npos) return L"";
    return tag.substr(valueStart, valueEnd - valueStart);
}

bool IsXmlTagAt(const std::wstring& text, size_t pos, const std::wstring& tagName) {
    std::wstring needle = L"<" + tagName;
    if (text.compare(pos, needle.size(), needle) != 0) return false;
    size_t nameEnd = pos + needle.size();
    return nameEnd >= text.size() || iswspace(text[nameEnd]) ||
           text[nameEnd] == L'>' || text[nameEnd] == L'/';
}

bool ReplaceXmlAttributeValueInRange(std::wstring& text, size_t begin, size_t end,
                                     const std::wstring& attribute,
                                     const std::wstring& value);

std::wstring LegacyWorkspaceXmlIdForPanelId(const std::wstring& panelId) {
    struct PanelIdMap {
        const wchar_t* panelId;
        const wchar_t* legacyId;
    };
    static const PanelIdMap kPanelIds[] = {
        {L"panelid.static.options", L"0000000000E711EA"},
        {L"panelid.static.animation", L"0000000000340FDC"},
        {L"panelid.static.toolbar", L"00000000000616F0"},
        {L"panelid.static.layers", L"0000000000090F0E"},
        {L"panelid.static.navigator", L"000000000004158A"},
        {L"panelid.static.histogram", L"0000000000500DAA"},
        {L"panelid.static.properties", L"0000000000121586"},
        {L"panelid.static.info", L"0000000000120ED4"},
        {L"panelid.static.brushstyler", L"00000000002B1206"},
        {L"panelid.static.brushpresets", L"00000000001E128E"},
        {L"panelid.static.clonesource", L"00000000000F11A0"},
        {L"panelid.static.textcharacter", L"0000000000180DD2"},
        {L"panelid.static.textparagraph", L"0000000000101144"},
        {L"panelid.static.textparastyle", L"0000000000061118"},
        {L"panelid.static.textcharstyle", L"0000000000031502"},
        {L"panelid.static.comps", L"0000000000081554"},
        {L"panelid.static.annotation", L"0000000000061534"},
        {L"panelid.dynamic.swf.csxs.klr", L"00000000003416EC"},
        {L"panelid.static.blrb", L"0000000000051606"},
        {L"panelid.static.blb2", L"00000000002714A2"},
        {L"panelid.static.3d", L"0000000000071034"},
        {L"panelid.static.toolpresets", L"00000000008714D6"},
        {L"panelid.static.picker", L"0000000000071322"},
        {L"panelid.static.swatches", L"0000000000490E08"},
        {L"panelid.static.create", L"00000000000C1558"},
        {L"panelid.static.styles", L"000000000006151E"},
        {L"panelid.static.channels", L"0000000000120EDA"},
        {L"panelid.static.paths", L"00000000000D1054"},
        {L"panelid.static.history", L"00000000000A101E"},
        {L"panelid.static.actions", L"0000000000070E42"},
        {L"panelid.dynamic.swf.csxs.minibr", L"0000000000080D1C"},
    };

    std::wstring lower = LowerWide(panelId);
    for (const auto& item : kPanelIds) {
        if (lower == item.panelId) return item.legacyId;
    }
    return L"";
}

bool RewriteWorkspaceElementIdsFromPanelIds(std::wstring& text,
                                            std::map<std::wstring, std::wstring>& idMap) {
    bool changed = false;
    const std::wstring tagNames[] = {L"control-bar", L"toolbar", L"palette"};
    for (const auto& tagName : tagNames) {
        size_t pos = 0;
        while ((pos = text.find(L"<" + tagName, pos)) != std::wstring::npos) {
            if (!IsXmlTagAt(text, pos, tagName)) {
                pos += tagName.size() + 1;
                continue;
            }

            size_t tagEnd = text.find(L'>', pos);
            if (tagEnd == std::wstring::npos) break;
            std::wstring tag = text.substr(pos, tagEnd - pos + 1);
            std::wstring id = XmlAttributeValue(tag, L"id");
            std::wstring appData = XmlAttributeValue(tag, L"app-data");
            std::wstring panelId = ExtractWorkspacePanelIdFromAppData(appData);
            std::wstring legacyId = LegacyWorkspaceXmlIdForPanelId(panelId);
            if (!id.empty() && !legacyId.empty()) {
                idMap[id] = legacyId;
                if (id != legacyId &&
                    ReplaceXmlAttributeValueInRange(text, pos, tagEnd, L"id", legacyId)) {
                    changed = true;
                    tagEnd = text.find(L'>', pos);
                    if (tagEnd == std::wstring::npos) break;
                }
            }
            pos = tagEnd + 1;
        }
    }
    return changed;
}

bool IsUnsupportedLegacyWorkspacePanelId(const std::wstring& panelId) {
    std::wstring lower = LowerWide(panelId);
    if (lower.empty()) return false;

    if (lower == L"panelid.dynamic.uxp" ||
        lower.rfind(L"panelid.dynamic.uxp.", 0) == 0) {
        return true;
    }
    if (lower.rfind(L"panelid.dynamic.swf.csxs.com.adobe.designlibraries", 0) == 0 ||
        lower.rfind(L"panelid.dynamic.swf.csxs.typer", 0) == 0) {
        return true;
    }

    const wchar_t* unsupportedStaticPanels[] = {
        L"panelid.static.measurement",
        L"panelid.static.ocio",
        L"panelid.static.customshapes",
        L"panelid.static.textglyphspanel",
        L"panelid.static.patchmatchfillpreview",
        L"panelid.static.smartbrush",
        L"panelid.static.patchmatch",
        L"panelid.static.gradients",
        L"panelid.static.patterns",
    };
    for (const wchar_t* unsupported : unsupportedStaticPanels) {
        if (lower == unsupported) return true;
    }
    return false;
}

void EraseXmlTagWithLineWhitespace(std::wstring& text, size_t tagStart, size_t tagEnd) {
    size_t removeStart = tagStart;
    size_t lineStart = text.rfind(L'\n', tagStart);
    lineStart = lineStart == std::wstring::npos ? 0 : lineStart + 1;
    bool onlyIndentBefore = true;
    for (size_t i = lineStart; i < tagStart; ++i) {
        if (!iswspace(text[i])) {
            onlyIndentBefore = false;
            break;
        }
    }
    if (onlyIndentBefore) removeStart = lineStart;

    size_t removeEnd = tagEnd + 1;
    size_t lineEnd = text.find(L'\n', removeEnd);
    if (lineEnd != std::wstring::npos) {
        bool onlyWhitespaceAfter = true;
        for (size_t i = removeEnd; i < lineEnd; ++i) {
            if (!iswspace(text[i])) {
                onlyWhitespaceAfter = false;
                break;
            }
        }
        if (onlyWhitespaceAfter) removeEnd = lineEnd + 1;
    }
    text.erase(removeStart, removeEnd - removeStart);
}

std::wstring FirstPaletteIdInRange(const std::wstring& text, size_t begin, size_t end) {
    size_t pos = begin;
    while ((pos = text.find(L"<palette", pos)) != std::wstring::npos && pos < end) {
        if (!IsXmlTagAt(text, pos, L"palette")) {
            pos += 8;
            continue;
        }
        size_t tagEnd = text.find(L'>', pos);
        if (tagEnd == std::wstring::npos || tagEnd > end) return L"";
        std::wstring id = XmlAttributeValue(text.substr(pos, tagEnd - pos + 1), L"id");
        if (!id.empty()) return id;
        pos = tagEnd + 1;
    }
    return L"";
}

bool ReplaceXmlAttributeValueInRange(std::wstring& text, size_t begin, size_t end,
                                     const std::wstring& attribute,
                                     const std::wstring& value) {
    std::wstring needle = attribute + L"=\"";
    size_t pos = text.find(needle, begin);
    if (pos == std::wstring::npos || pos >= end) return false;
    size_t valueStart = pos + needle.size();
    size_t valueEnd = text.find(L'"', valueStart);
    if (valueEnd == std::wstring::npos || valueEnd > end) return false;
    text.replace(valueStart, valueEnd - valueStart, value);
    return true;
}

bool FixLegacyWorkspaceTabGroups(std::wstring& text) {
    bool changed = false;
    size_t pos = 0;
    while ((pos = text.find(L"<tab-group", pos)) != std::wstring::npos) {
        if (!IsXmlTagAt(text, pos, L"tab-group")) {
            pos += 10;
            continue;
        }
        size_t startTagEnd = text.find(L'>', pos);
        if (startTagEnd == std::wstring::npos) break;
        size_t closeStart = text.find(L"</tab-group>", startTagEnd + 1);
        if (closeStart == std::wstring::npos) break;
        size_t closeEnd = closeStart + std::wstring(L"</tab-group>").size();
        std::wstring firstPaletteId = FirstPaletteIdInRange(text, startTagEnd + 1, closeStart);
        if (firstPaletteId.empty()) {
            EraseXmlTagWithLineWhitespace(text, pos, closeEnd - 1);
            changed = true;
            continue;
        }

        std::wstring startTag = text.substr(pos, startTagEnd - pos + 1);
        std::wstring activePalette = XmlAttributeValue(startTag, L"active-palette");
        bool activeExists = false;
        if (!activePalette.empty()) {
            size_t search = startTagEnd + 1;
            while ((search = text.find(L"<palette", search)) != std::wstring::npos &&
                   search < closeStart) {
                if (!IsXmlTagAt(text, search, L"palette")) {
                    search += 8;
                    continue;
                }
                size_t paletteEnd = text.find(L'>', search);
                if (paletteEnd == std::wstring::npos || paletteEnd > closeStart) break;
                std::wstring id =
                    XmlAttributeValue(text.substr(search, paletteEnd - search + 1), L"id");
                if (id == activePalette) {
                    activeExists = true;
                    break;
                }
                search = paletteEnd + 1;
            }
        }
        if (!activeExists &&
            ReplaceXmlAttributeValueInRange(text, pos, startTagEnd, L"active-palette",
                                            firstPaletteId)) {
            changed = true;
            startTagEnd = text.find(L'>', pos);
            closeStart = text.find(L"</tab-group>", startTagEnd + 1);
            closeEnd = closeStart == std::wstring::npos
                           ? closeEnd
                           : closeStart + std::wstring(L"</tab-group>").size();
        }
        pos = closeEnd;
    }
    return changed;
}

bool RemoveEmptyXmlElements(std::wstring& text, const std::wstring& tagName,
                            const std::wstring& requiredNeedle) {
    bool changed = false;
    size_t pos = 0;
    std::wstring closeTag = L"</" + tagName + L">";
    while ((pos = text.find(L"<" + tagName, pos)) != std::wstring::npos) {
        if (!IsXmlTagAt(text, pos, tagName)) {
            pos += tagName.size() + 1;
            continue;
        }
        size_t startTagEnd = text.find(L'>', pos);
        if (startTagEnd == std::wstring::npos) break;
        size_t closeStart = text.find(closeTag, startTagEnd + 1);
        if (closeStart == std::wstring::npos) break;
        size_t closeEnd = closeStart + closeTag.size();
        if (text.find(requiredNeedle, startTagEnd + 1) == std::wstring::npos ||
            text.find(requiredNeedle, startTagEnd + 1) >= closeStart) {
            EraseXmlTagWithLineWhitespace(text, pos, closeEnd - 1);
            changed = true;
            continue;
        }
        pos = closeEnd;
    }
    return changed;
}

bool RemoveUnsupportedLegacyWorkspacePalettes(std::wstring& text) {
    bool changed = false;
    size_t pos = 0;
    while ((pos = text.find(L"<palette", pos)) != std::wstring::npos) {
        if (!IsXmlTagAt(text, pos, L"palette")) {
            pos += 8;
            continue;
        }
        size_t tagEnd = text.find(L'>', pos);
        if (tagEnd == std::wstring::npos) break;
        std::wstring tag = text.substr(pos, tagEnd - pos + 1);
        std::wstring appData = XmlAttributeValue(tag, L"app-data");
        std::wstring panelId = ExtractWorkspacePanelIdFromAppData(appData);
        if (IsUnsupportedLegacyWorkspacePanelId(panelId)) {
            EraseXmlTagWithLineWhitespace(text, pos, tagEnd);
            changed = true;
            continue;
        }
        pos = tagEnd + 1;
    }

    if (changed) {
        changed = FixLegacyWorkspaceTabGroups(text) || changed;
        changed = RemoveEmptyXmlElements(text, L"tab-pane", L"<palette") || changed;
    }
    return changed;
}

bool RemoveEmptyWorkspaceDocks(std::wstring& text) {
    bool changed = false;
    size_t pos = 0;
    const std::wstring closeTag = L"</dock>";
    while ((pos = text.find(L"<dock", pos)) != std::wstring::npos) {
        if (!IsXmlTagAt(text, pos, L"dock")) {
            pos += 5;
            continue;
        }
        size_t startTagEnd = text.find(L'>', pos);
        if (startTagEnd == std::wstring::npos) break;
        size_t closeStart = text.find(closeTag, startTagEnd + 1);
        if (closeStart == std::wstring::npos) break;
        size_t closeEnd = closeStart + closeTag.size();

        bool hasUsableChild =
            text.find(L"<palette", startTagEnd + 1) < closeStart ||
            text.find(L"<toolbar", startTagEnd + 1) < closeStart ||
            text.find(L"<control-bar", startTagEnd + 1) < closeStart;
        if (!hasUsableChild) {
            EraseXmlTagWithLineWhitespace(text, pos, closeEnd - 1);
            changed = true;
            continue;
        }
        pos = closeEnd;
    }
    return changed;
}

bool NormalizeLegacyControlBarOrigin(std::wstring& text) {
    bool changed = false;
    size_t pos = 0;
    while ((pos = text.find(L"<control-bar", pos)) != std::wstring::npos) {
        if (!IsXmlTagAt(text, pos, L"control-bar")) {
            pos += 12;
            continue;
        }
        size_t tagEnd = text.find(L'>', pos);
        if (tagEnd == std::wstring::npos) break;
        std::wstring tag = text.substr(pos, tagEnd - pos + 1);
        std::wstring origin = XmlAttributeValue(tag, L"origin");
        wchar_t* end = nullptr;
        long x = wcstol(origin.c_str(), &end, 10);
        if (end == origin.c_str()) {
            pos = tagEnd + 1;
            continue;
        }
        while (*end && iswspace(*end)) ++end;
        wchar_t* end2 = nullptr;
        long y = wcstol(end, &end2, 10);
        if (end2 == end) {
            pos = tagEnd + 1;
            continue;
        }
        while (*end2 && iswspace(*end2)) ++end2;
        if (*end2 == L'\0' && (x < 0 || y < 0) &&
            ReplaceXmlAttributeValueInRange(text, pos, tagEnd, L"origin", L"0 28")) {
            changed = true;
            tagEnd = text.find(L'>', pos);
        }
        pos = tagEnd == std::wstring::npos ? text.size() : tagEnd + 1;
    }
    return changed;
}

std::vector<uint8_t> EncodeUtf8LikeOriginal(const std::vector<uint8_t>& original,
                                            const std::wstring& text) {
    std::string utf8 = WideToUtf8(text);
    std::vector<uint8_t> encoded;
    bool hadBom = original.size() >= 3 && original[0] == 0xEF &&
                  original[1] == 0xBB && original[2] == 0xBF;
    if (hadBom) {
        encoded.push_back(0xEF);
        encoded.push_back(0xBB);
        encoded.push_back(0xBF);
    }
    encoded.insert(encoded.end(), utf8.begin(), utf8.end());
    return encoded;
}

bool NormalizeLegacyWorkspaceXmlText(std::wstring& text) {
    bool changed = false;
    std::wstring before = text;
    bool hasModernDecimalIds =
        HasRewriteableDecimalXmlAttributeValue(text, L"id") ||
        HasRewriteableDecimalXmlAttributeValue(text, L"active-palette");

    std::wstring rewritten =
        ReplaceAllCaseInsensitive(text,
                                  L"<photoshop-workspace version=\"2.0\"",
                                  L"<photoshop-workspace version=\"1.0\"");
    rewritten = ReplaceAllCaseInsensitive(rewritten,
                                           L"<photoshop-workspace version=\"2\"",
                                           L"<photoshop-workspace version=\"1\"");
    if (rewritten != text) {
        text = std::move(rewritten);
        changed = true;
    }
    rewritten = ReplaceAllCaseInsensitive(text, L"<workspace version=\"2.0\"",
                                           L"<workspace version=\"1.0\"");
    rewritten = ReplaceAllCaseInsensitive(rewritten, L"<workspace version=\"2\"",
                                           L"<workspace version=\"1\"");
    if (rewritten != text) {
        text = std::move(rewritten);
        changed = true;
    }

    changed = RemoveXmlAttributeFromTags(text, L"dock", L"origin") || changed;
    changed = NormalizeLegacyControlBarOrigin(text) || changed;
    changed = ClampNegativeOriginAttributes(text) || changed;
    changed = RemoveUnsupportedLegacyWorkspacePalettes(text) || changed;

    std::map<std::wstring, std::wstring> idMap;
    if (hasModernDecimalIds) {
        changed = RewriteWorkspaceElementIdsFromPanelIds(text, idMap) || changed;
    }
    changed = RewriteDecimalXmlAttributeValues(text, L"id", idMap) || changed;
    changed = RewriteDecimalXmlAttributeValues(text, L"active-palette", idMap) || changed;
    changed = FixLegacyWorkspaceTabGroups(text) || changed;
    changed = RemoveEmptyXmlElements(text, L"tab-pane", L"<palette") || changed;
    changed = RemoveEmptyWorkspaceDocks(text) || changed;

    return changed && text != before;
}

size_t FindAsciiBytes(const std::vector<uint8_t>& bytes, const char* needle,
                      size_t start = 0) {
    size_t needleLen = strlen(needle);
    if (needleLen == 0) return start <= bytes.size() ? start : std::string::npos;
    if (start > bytes.size() || needleLen > bytes.size() - start) return std::string::npos;

    const uint8_t first = static_cast<uint8_t>(needle[0]);
    for (size_t i = start; i <= bytes.size() - needleLen; ++i) {
        if (bytes[i] != first) continue;
        if (memcmp(bytes.data() + i, needle, needleLen) == 0) return i;
    }
    return std::string::npos;
}

bool FindEmbeddedWorkspaceXmlRange(const std::vector<uint8_t>& bytes,
                                   size_t& xmlStart, size_t& xmlEnd) {
    xmlStart = FindAsciiBytes(bytes, "<workspace");
    if (xmlStart == std::string::npos) return false;

    size_t closeStart = FindAsciiBytes(bytes, "</workspace>", xmlStart);
    if (closeStart == std::string::npos) return false;

    xmlEnd = closeStart + strlen("</workspace>");
    if (xmlEnd + 1 < bytes.size() && bytes[xmlEnd] == '\r' && bytes[xmlEnd + 1] == '\n') {
        xmlEnd += 2;
    } else if (xmlEnd < bytes.size() && (bytes[xmlEnd] == '\n' || bytes[xmlEnd] == '\r')) {
        ++xmlEnd;
    }
    return xmlEnd > xmlStart && xmlEnd <= bytes.size();
}

uint32_t ReadBigEndianU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0;
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<uint32_t>(bytes[offset + 3]);
}

void WriteBigEndianU32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    if (offset + 4 > bytes.size()) return;
    bytes[offset] = static_cast<uint8_t>((value >> 24) & 0xFF);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    bytes[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[offset + 3] = static_cast<uint8_t>(value & 0xFF);
}

void AppendBigEndianU32(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(value & 0xFF));
}

void AppendAscii(std::vector<uint8_t>& bytes, const char* text) {
    bytes.insert(bytes.end(), text, text + strlen(text));
}

std::vector<uint8_t> BuildLegacyWorkspacePrefsTail() {
    std::vector<uint8_t> tail;
    AppendBigEndianU32(tail, 0);
    AppendAscii(tail, "Plt bool");
    tail.push_back(1);
    tail.push_back(0);
    tail.push_back(0);
    tail.push_back(0);
    tail.push_back(0x15);
    AppendAscii(tail, "keyboardCustomizationbool");
    AppendBigEndianU32(tail, 0);
    tail.push_back(0x11);
    AppendAscii(tail, "menuCustomizationbool");
    AppendBigEndianU32(tail, 0);
    tail.push_back(0x19);
    AppendAscii(tail, "workspacesDisabledPresetsVlLs");
    AppendBigEndianU32(tail, 0);
    return tail;
}

bool ByteRangeContainsAscii(const std::vector<uint8_t>& bytes, size_t begin,
                            size_t end, const char* needle) {
    if (begin > bytes.size()) return false;
    end = std::min(end, bytes.size());
    size_t found = FindAsciiBytes(bytes, needle, begin);
    return found != std::string::npos && found + strlen(needle) <= end;
}

bool WorkspacePrefsTailHasModernOnlyKeys(const std::vector<uint8_t>& bytes,
                                         size_t xmlEnd) {
    return ByteRangeContainsAscii(bytes, xmlEnd, bytes.size(), "toolbarCustomizationbool");
}

bool WorkspacePrefsTemplateLooksLegacy(const std::vector<uint8_t>& bytes,
                                       size_t xmlEnd) {
    return bytes.size() >= 2 && bytes[0] == 0x00 && bytes[1] == 0x01 &&
           !WorkspacePrefsTailHasModernOnlyKeys(bytes, xmlEnd);
}

bool PrefixEndsWithPspWorkspaceLengthField(const std::vector<uint8_t>& prefix,
                                           size_t oldXmlLength) {
    if (prefix.size() < 4) return false;
    uint32_t storedLength = ReadBigEndianU32(prefix, prefix.size() - 4);
    if (storedLength == oldXmlLength) return true;
    if (prefix.size() < 8) return false;
    const size_t token = prefix.size() - 8;
    return prefix[token] == 't' && prefix[token + 1] == 'd' &&
           prefix[token + 2] == 't' && prefix[token + 3] == 'a';
}

bool NormalizeEmbeddedWorkspacePrefsBytesForTarget(const std::vector<uint8_t>& raw,
                                                   const std::wstring& targetPath,
                                                   std::vector<uint8_t>& normalized) {
    size_t sourceXmlStart = 0;
    size_t sourceXmlEnd = 0;
    if (!FindEmbeddedWorkspaceXmlRange(raw, sourceXmlStart, sourceXmlEnd) ||
        sourceXmlStart == 0) {
        return false;
    }

    std::string xmlUtf8(reinterpret_cast<const char*>(raw.data() + sourceXmlStart),
                        reinterpret_cast<const char*>(raw.data() + sourceXmlEnd));
    std::wstring xml = Utf8ToWide(xmlUtf8, true);
    if (xml.empty()) return false;
    if (xml.find(L"<workspace") == std::wstring::npos) return false;

    bool xmlChanged = NormalizeLegacyWorkspaceXmlText(xml);
    bool containerNeedsLegacyRewrite =
        !WorkspacePrefsTemplateLooksLegacy(raw, sourceXmlEnd);
    if (!xmlChanged && !containerNeedsLegacyRewrite) return false;
    std::string normalizedXmlUtf8 = WideToUtf8(xml);

    std::vector<uint8_t> templateBytes;
    std::wstring ignoredError;
    size_t templateXmlStart = sourceXmlStart;
    size_t templateXmlEnd = sourceXmlEnd;
    if (!targetPath.empty() && ReadFileBytes(targetPath, templateBytes, ignoredError)) {
        size_t targetXmlStart = 0;
        size_t targetXmlEnd = 0;
        if (FindEmbeddedWorkspaceXmlRange(templateBytes, targetXmlStart, targetXmlEnd) &&
            targetXmlStart > 0 &&
            WorkspacePrefsTemplateLooksLegacy(templateBytes, targetXmlEnd)) {
            templateXmlStart = targetXmlStart;
            templateXmlEnd = targetXmlEnd;
        } else {
            templateBytes.clear();
        }
    }
    const std::vector<uint8_t>& templateSource = templateBytes.empty() ? raw : templateBytes;

    std::vector<uint8_t> prefix(templateSource.begin(),
                                templateSource.begin() + templateXmlStart);
    std::vector<uint8_t> tail(templateSource.begin() + templateXmlEnd,
                              templateSource.end());
    if (templateBytes.empty() && !WorkspacePrefsTemplateLooksLegacy(raw, sourceXmlEnd)) {
        tail = BuildLegacyWorkspacePrefsTail();
    }

    if (prefix.size() >= 2) {
        prefix[0] = 0x00;
        prefix[1] = 0x01;
    }
    size_t oldTemplateXmlLength = templateXmlEnd - templateXmlStart;
    if (PrefixEndsWithPspWorkspaceLengthField(prefix, oldTemplateXmlLength)) {
        WriteBigEndianU32(prefix, prefix.size() - 4,
                          static_cast<uint32_t>(normalizedXmlUtf8.size()));
    }

    normalized.clear();
    normalized.reserve(prefix.size() + normalizedXmlUtf8.size() + tail.size());
    normalized.insert(normalized.end(), prefix.begin(), prefix.end());
    normalized.insert(normalized.end(), normalizedXmlUtf8.begin(), normalizedXmlUtf8.end());
    normalized.insert(normalized.end(), tail.begin(), tail.end());
    return normalized != raw;
}

enum class WorkspaceCompatibilityResult {
    NotNeeded,
    AlreadyCompatible,
    Converted,
    Incompatible,
};

bool IsWorkspaceTargetPath(const std::wstring& targetPath) {
    std::vector<std::wstring> parts = SplitRelativePathParts(targetPath);
    for (const auto& part : parts) {
        if (IsWorkspaceDirectoryName(part)) return true;
    }
    std::wstring file = LowerWide(FileNameOnly(targetPath));
    return ExtensionLower(file) == L".psw" ||
           file.find(L"workspace prefs") != std::wstring::npos;
}

bool IsWorkspaceXmlNameChar(wchar_t ch) {
    return iswalnum(ch) || ch == L'_' || ch == L'-' || ch == L':' || ch == L'.';
}

bool IsWorkspaceXmlOuterPadding(wchar_t ch) {
    return iswspace(ch) || ch == L'\0' || ch == 0xFEFF;
}

bool IsStrictStandalonePhotoshopWorkspaceBytes(const std::vector<uint8_t>& raw) {
    std::wstring text = DecodeTextBytes(raw);
    std::vector<std::wstring> elements;
    bool sawRoot = false;
    bool rootClosed = false;
    bool sawWorkspace = false;
    size_t pos = 0;

    while (pos < text.size()) {
        if (text[pos] != L'<') {
            size_t next = text.find(L'<', pos);
            if (next == std::wstring::npos) next = text.size();
            if (elements.empty()) {
                for (size_t i = pos; i < next; ++i) {
                    if (!IsWorkspaceXmlOuterPadding(text[i])) return false;
                }
            }
            pos = next;
            continue;
        }

        if (text.compare(pos, 4, L"<!--") == 0) {
            size_t end = text.find(L"-->", pos + 4);
            if (end == std::wstring::npos) return false;
            pos = end + 3;
            continue;
        }
        if (text.compare(pos, 2, L"<?") == 0) {
            size_t end = text.find(L"?>", pos + 2);
            if (end == std::wstring::npos) return false;
            pos = end + 2;
            continue;
        }
        if (text.compare(pos, 9, L"<![CDATA[") == 0) {
            if (elements.empty()) return false;
            size_t end = text.find(L"]]>", pos + 9);
            if (end == std::wstring::npos) return false;
            pos = end + 3;
            continue;
        }
        if (text.compare(pos, 2, L"<!") == 0) return false;

        const bool closing = text.compare(pos, 2, L"</") == 0;
        size_t nameStart = pos + (closing ? 2 : 1);
        size_t nameEnd = nameStart;
        while (nameEnd < text.size() && IsWorkspaceXmlNameChar(text[nameEnd])) ++nameEnd;
        if (nameEnd == nameStart) return false;
        std::wstring name = text.substr(nameStart, nameEnd - nameStart);

        if (closing) {
            size_t end = nameEnd;
            while (end < text.size() && iswspace(text[end])) ++end;
            if (end >= text.size() || text[end] != L'>' || elements.empty() ||
                elements.back() != name) {
                return false;
            }
            elements.pop_back();
            pos = end + 1;
            if (elements.empty()) rootClosed = true;
            continue;
        }

        if (rootClosed) return false;
        wchar_t quote = L'\0';
        size_t end = nameEnd;
        for (; end < text.size(); ++end) {
            wchar_t ch = text[end];
            if (quote != L'\0') {
                if (ch == quote) quote = L'\0';
                continue;
            }
            if (ch == L'\'' || ch == L'"') {
                quote = ch;
            } else if (ch == L'>') {
                break;
            } else if (ch == L'<') {
                return false;
            }
        }
        if (end >= text.size() || quote != L'\0') return false;

        size_t marker = end;
        while (marker > nameEnd && iswspace(text[marker - 1])) --marker;
        const bool selfClosing = marker > nameEnd && text[marker - 1] == L'/';
        if (!sawRoot) {
            if (name != L"photoshop-workspace") return false;
            sawRoot = true;
        } else if (elements.empty()) {
            return false;
        }
        if (name == L"workspace") sawWorkspace = true;

        if (!selfClosing) {
            elements.push_back(std::move(name));
        } else if (elements.empty()) {
            rootClosed = true;
        }
        pos = end + 1;
    }

    return sawRoot && rootClosed && sawWorkspace && elements.empty();
}

bool LooksLikeLegacyWorkspaceXml(const std::wstring& text) {
    std::wstring lower = LowerWide(text);
    return lower.find(L"<photoshop-workspace version=\"1") != std::wstring::npos ||
           lower.find(L"<workspace version=\"1") != std::wstring::npos;
}

bool EmbeddedWorkspacePrefsAlreadyCompatible(const std::vector<uint8_t>& raw) {
    size_t xmlStart = 0;
    size_t xmlEnd = 0;
    if (!FindEmbeddedWorkspaceXmlRange(raw, xmlStart, xmlEnd) || xmlStart == 0 ||
        !WorkspacePrefsTemplateLooksLegacy(raw, xmlEnd)) {
        return false;
    }
    std::string xmlUtf8(reinterpret_cast<const char*>(raw.data() + xmlStart),
                        reinterpret_cast<const char*>(raw.data() + xmlEnd));
    std::wstring xml = Utf8ToWide(xmlUtf8, true);
    if (xml.empty() || !LooksLikeLegacyWorkspaceXml(xml)) return false;
    std::wstring normalized = xml;
    return !NormalizeLegacyWorkspaceXmlText(normalized);
}

WorkspaceCompatibilityResult PreparePhotoshopWorkspaceBytesForTarget(
    const std::vector<uint8_t>& raw, const std::wstring& targetLabel,
    const std::wstring& targetPath, std::vector<uint8_t>& normalized) {
    normalized = raw;
    if (!UsesLegacyPhotoshopWorkspaceNames(targetLabel) || raw.empty()) {
        return WorkspaceCompatibilityResult::NotNeeded;
    }

    std::wstring text = DecodeTextBytes(raw);
    bool standaloneWorkspace = LooksLikePhotoshopWorkspaceXml(text);
    if (!standaloneWorkspace && !IsWorkspaceTargetPath(targetPath)) {
        return WorkspaceCompatibilityResult::NotNeeded;
    }

    if (standaloneWorkspace) {
        std::wstring converted = text;
        if (NormalizeLegacyWorkspaceXmlText(converted)) {
            normalized = EncodeUtf8LikeOriginal(raw, converted);
            return WorkspaceCompatibilityResult::Converted;
        }
        return LooksLikeLegacyWorkspaceXml(text)
                   ? WorkspaceCompatibilityResult::AlreadyCompatible
                   : WorkspaceCompatibilityResult::Incompatible;
    }

    if (NormalizeEmbeddedWorkspacePrefsBytesForTarget(raw, targetPath, normalized)) {
        return WorkspaceCompatibilityResult::Converted;
    }
    return EmbeddedWorkspacePrefsAlreadyCompatible(raw)
               ? WorkspaceCompatibilityResult::AlreadyCompatible
               : WorkspaceCompatibilityResult::Incompatible;
}

bool ExtractPhotoshopSettingsFolderRelativePath(const std::wstring& fileRelativePath,
                                                std::wstring& settingsFolderRelativePath) {
    settingsFolderRelativePath.clear();
    std::wstring versionRoot;
    std::wstring label;
    if (!ExtractPhotoshopVersionInfo(fileRelativePath, versionRoot, label)) return false;

    std::vector<std::wstring> rootParts = SplitRelativePathParts(versionRoot);
    std::vector<std::wstring> pathParts = SplitRelativePathParts(fileRelativePath);
    if (pathParts.size() <= rootParts.size()) return false;
    if (!IsPhotoshopSettingsFolderSegment(pathParts[rootParts.size()])) return false;

    settingsFolderRelativePath = JoinRelativePathParts(pathParts, rootParts.size() + 1);
    return IsSafeRelativePath(settingsFolderRelativePath);
}

std::wstring EnsureTrailingBackslash(std::wstring path) {
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/') path.push_back(L'\\');
    return path;
}

int FirstFourDigitYear(const std::wstring& text) {
    for (size_t i = 0; i + 4 <= text.size(); ++i) {
        if (!iswdigit(text[i]) || !iswdigit(text[i + 1]) ||
            !iswdigit(text[i + 2]) || !iswdigit(text[i + 3])) {
            continue;
        }
        int year = (text[i] - L'0') * 1000 + (text[i + 1] - L'0') * 100 +
                   (text[i + 2] - L'0') * 10 + (text[i + 3] - L'0');
        if (year >= 2014 && year <= 2035) return year;
    }
    return 0;
}

std::wstring PhotoshopRegistryVersionKeyFromLabel(const std::wstring& label) {
    std::wstring normalized = NormalizePhotoshopVersionLabel(label);
    std::wstring lower = LowerWide(normalized);
    if (lower.find(L"cs6") != std::wstring::npos) return L"60.0";
    if (lower.find(L"cs5.1") != std::wstring::npos) return L"55.0";
    if (lower.find(L"cs5") != std::wstring::npos) return L"50.0";

    int year = FirstFourDigitYear(normalized);
    if (year > 0) return std::to_wstring((year - 2006) * 10) + L".0";
    return L"";
}

bool SetRegistryStringValue(HKEY key, const wchar_t* valueName, const std::wstring& value) {
    return RegSetValueExW(key, valueName, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) ==
           ERROR_SUCCESS;
}

bool SetPhotoshopSettingsFilePathForRegistryKey(const std::wstring& subKeyName,
                                                const std::wstring& settingsFolder) {
    if (subKeyName.empty() || settingsFolder.empty()) return false;
    std::wstring keyPath = L"Software\\Adobe\\Photoshop\\" + subKeyName;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return false;
    }
    std::wstring value = EnsureTrailingBackslash(settingsFolder);
    bool ok = SetRegistryStringValue(key, L"SettingsFilePath", value);
    RegCloseKey(key);
    return ok;
}

uint32_t UpdatePhotoshopSettingsFilePathRegistry(
    const std::wstring& targetLabel, const std::set<std::wstring>& settingsFolders) {
    if (targetLabel.empty() || settingsFolders.empty()) return 0;

    const std::wstring settingsFolder = *settingsFolders.begin();
    std::set<std::wstring> keysToUpdate;
    std::wstring mappedKey = PhotoshopRegistryVersionKeyFromLabel(targetLabel);
    if (!mappedKey.empty()) keysToUpdate.insert(mappedKey);

    HKEY photoshop = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Adobe\\Photoshop", 0,
                      KEY_READ, &photoshop) == ERROR_SUCCESS) {
        std::wstring targetLower = LowerWide(NormalizePhotoshopVersionLabel(targetLabel));
        for (DWORD index = 0;; ++index) {
            wchar_t subKeyName[256] = {};
            DWORD subKeyLen = static_cast<DWORD>(std::size(subKeyName));
            LONG rc = RegEnumKeyExW(photoshop, index, subKeyName, &subKeyLen,
                                    nullptr, nullptr, nullptr, nullptr);
            if (rc == ERROR_NO_MORE_ITEMS) break;
            if (rc != ERROR_SUCCESS) continue;

            HKEY subKey = nullptr;
            if (RegOpenKeyExW(photoshop, subKeyName, 0, KEY_READ, &subKey) != ERROR_SUCCESS) {
                continue;
            }
            std::wstring existingPath;
            if (ReadRegistryStringValue(subKey, L"SettingsFilePath", existingPath)) {
                std::wstring existingLower = LowerWide(existingPath);
                if (!targetLower.empty() && existingLower.find(targetLower) != std::wstring::npos) {
                    keysToUpdate.insert(std::wstring(subKeyName, subKeyLen));
                }
            }
            RegCloseKey(subKey);
        }
        RegCloseKey(photoshop);
    }

    uint32_t updated = 0;
    for (const auto& key : keysToUpdate) {
        if (SetPhotoshopSettingsFilePathForRegistryKey(key, settingsFolder)) ++updated;
    }
    return updated;
}

std::wstring RestoreTargetRootForToken(const PtsRestoreOptions& options,
                                        const std::wstring& rootToken) {
    if (rootToken == L"APPDATA") return options.targetAppDataRelativeRoot;
    if (rootToken == L"LOCALAPPDATA") return options.targetLocalAppDataRelativeRoot;
    return L"";
}

bool CollectPtsPrimarySettingsDestinations(
    const std::wstring& path, const PtsRestoreOptions& options,
    std::set<std::wstring>& destinations, std::wstring& error,
    const PtsCancellationCallback& cancelled = {}) {
    destinations.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Không mở được file .afang để lập kế hoạch khôi phục.";
        return false;
    }

    uint32_t count = 0;
    if (!ReadPtsArchiveHeader(file, count, error)) {
        CloseHandle(file);
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (cancelled && cancelled()) {
            CloseHandle(file);
            error = L"Đã hủy lập kế hoạch khôi phục Pts.";
            return false;
        }

        uint8_t kindByte = 0;
        std::wstring rootToken;
        std::wstring relativePath;
        std::wstring displayName;
        uint64_t originalSize = 0;
        uint64_t storedSize = 0;
        uint8_t method = 0;
        if (!ReadPtsArchiveEntryMetadata(file, kindByte, rootToken, relativePath, displayName,
                                         originalSize, storedSize, method, error)) {
            CloseHandle(file);
            return false;
        }

        if (kindByte == static_cast<uint8_t>(PtsEntryKind::PhotoshopSetting)) {
            std::wstring sourceRelativeRoot;
            std::wstring sourceLabel;
            if (!ExtractPhotoshopVersionInfo(relativePath, sourceRelativeRoot, sourceLabel)) {
                CloseHandle(file);
                error = L"Không xác định được phiên bản của cài đặt trong .afang.";
                return false;
            }
            const std::wstring sourceVersionKey =
                PtsPhotoshopVersionKey(rootToken, sourceRelativeRoot);
            if (!options.HasSourceFilter() ||
                options.sourceVersionKeys.count(sourceVersionKey) > 0) {
                std::wstring destinationRelativePath = relativePath;
                if (options.HasTargetMapping()) {
                    const std::wstring targetRoot =
                        RestoreTargetRootForToken(options, rootToken);
                    if (!BuildMappedPhotoshopSettingsRelativePath(
                            relativePath, sourceRelativeRoot, targetRoot, sourceLabel,
                            options.targetVersionLabel, destinationRelativePath)) {
                        CloseHandle(file);
                        error = L"Không ánh xạ được cài đặt sang phiên bản Photoshop đã chọn.";
                        return false;
                    }
                }
                destinations.insert(
                    LowerWide(rootToken + L"\\" + destinationRelativePath));
            }
        }

        if (!SkipFileBytes(file, storedSize)) {
            CloseHandle(file);
            error = L"File .afang bị thiếu dữ liệu.";
            return false;
        }
    }

    CloseHandle(file);
    return true;
}

bool RestorePtsBackupArchive(const std::wstring& path, const PtsRestoreOptions& options,
                              const PtsProgressCallback& progress,
                              std::wstring& summary, std::wstring& error,
                              const PtsCancellationCallback& cancelled = {},
                              const PtsSettingsRegistryUpdateCallback& registryUpdater = {}) {
    auto cancellationRequested = [&] { return cancelled && cancelled(); };
    if (cancellationRequested()) {
        error = L"Đã hủy khôi phục Pts.";
        return false;
    }
    bool archiveHasSettings = options.archiveHasSettings;
    if (!options.archiveValidated) {
        PtsArchiveScanResult preflight{};
        if (!InspectPtsArchiveVersions(path, preflight, error, progress, cancelled)) return false;
        archiveHasSettings = preflight.hasSettings;
    }
    if (archiveHasSettings && IsPhotoshopRunning()) {
        error = L"Photoshop đang chạy. Hãy đóng Photoshop trước khi khôi phục cài đặt.";
        return false;
    }

    std::set<std::wstring> primarySettingsDestinations;
    if (options.HasTargetMapping() &&
        !CollectPtsPrimarySettingsDestinations(path, options, primarySettingsDestinations,
                                               error, cancelled)) {
        return false;
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Không mở được file .afang.";
        return false;
    }

    uint32_t count = 0;
    if (!ReadPtsArchiveHeader(file, count, error)) {
        CloseHandle(file);
        return false;
    }

    const std::wstring appData = GetEnvPath(L"APPDATA");
    const std::wstring localAppData = GetEnvPath(L"LOCALAPPDATA");
    const std::wstring userFonts = JoinPath(localAppData, L"Microsoft\\Windows\\Fonts");
    bool userFontDirectoryReady = false;
    uint32_t restored = 0;
    uint32_t fontRestored = 0;
    uint32_t fontFilesWritten = 0;
    uint32_t fontsAlreadyPresent = 0;
    uint32_t settingsRestored = 0;
    uint32_t skippedByVersion = 0;
    uint32_t settingsCompatibilityAliases = 0;
    uint32_t workspaceCompatibilityConversions = 0;
    uint32_t registrySettingsPathUpdates = 0;
    uint64_t restoredBytes = 0;
    bool appliedAnyChange = false;
    std::set<std::wstring> restoredSettingsRoots;
    std::set<std::wstring> targetSettingsFoldersForRegistry;
    std::vector<RestoredUserFont> fontsToRegister;
    auto cancellationError = [&] {
        return appliedAnyChange
                   ? L"Đã hủy khôi phục sau khi áp dụng một phần. Các thay đổi không tự hoàn tác."
                   : L"Đã hủy khôi phục; chưa có thay đổi nào được áp dụng.";
    };

    for (uint32_t i = 0; i < count; ++i) {
        if (cancellationRequested()) {
            CloseHandle(file);
            error = cancellationError();
            return false;
        }
        if (i == 0 || i + 1 == count || (i % 25) == 0) {
            int percent = 5 + static_cast<int>(
                (static_cast<uint64_t>(i) * 88) / std::max<uint32_t>(1, count));
            progress(percent, L"Đang khôi phục file " + std::to_wstring(i + 1) + L"/" +
                                  std::to_wstring(count) + L"...");
        }
        if (cancellationRequested()) {
            CloseHandle(file);
            error = cancellationError();
            return false;
        }

        uint8_t kindByte = 0;
        std::wstring rootToken;
        std::wstring relativePath;
        std::wstring displayName;
        uint64_t originalSize = 0;
        uint64_t storedSize = 0;
        uint8_t method = 0;
        if (!ReadPtsArchiveEntryMetadata(file, kindByte, rootToken, relativePath, displayName,
                                         originalSize, storedSize, method, error)) {
            CloseHandle(file);
            return false;
        }

        const bool isSetting =
            kindByte == static_cast<uint8_t>(PtsEntryKind::PhotoshopSetting);
        std::wstring sourceRelativeRoot;
        std::wstring sourceLabel;
        if (isSetting) {
            if (!ExtractPhotoshopVersionInfo(relativePath, sourceRelativeRoot, sourceLabel)) {
                CloseHandle(file);
                error = L"Không xác định được phiên bản của cài đặt trong .afang.";
                return false;
            }
            std::wstring sourceVersionKey =
                PtsPhotoshopVersionKey(rootToken, sourceRelativeRoot);
            if (options.HasSourceFilter() &&
                options.sourceVersionKeys.count(sourceVersionKey) == 0) {
                if (!SkipFileBytes(file, storedSize)) {
                    CloseHandle(file);
                    error = L"File .afang bị thiếu dữ liệu.";
                    return false;
                }
                ++skippedByVersion;
                continue;
            }
        }

        std::vector<uint8_t> stored(static_cast<size_t>(storedSize));
        if (storedSize > 0 &&
            !ReadAll(file, stored.data(), static_cast<DWORD>(stored.size()))) {
            CloseHandle(file);
            error = L"File .afang bị thiếu dữ liệu.";
            return false;
        }
        std::vector<uint8_t> raw;
        if (!DecompressFromArchive(stored, method, originalSize, raw)) {
            CloseHandle(file);
            error = L"Không giải nén được dữ liệu trong .afang.";
            return false;
        }
        if (cancellationRequested()) {
            CloseHandle(file);
            error = cancellationError();
            return false;
        }

        if (kindByte == static_cast<uint8_t>(PtsEntryKind::Font)) {
            if (localAppData.empty()) {
                CloseHandle(file);
                error = L"Không tìm được thư mục font người dùng.";
                return false;
            }
            if (!userFontDirectoryReady) {
                if (!EnsureDirectoryExists(userFonts)) {
                    CloseHandle(file);
                    error = L"Không tạo được thư mục font người dùng.";
                    return false;
                }
                userFontDirectoryReady = true;
            }

            const std::wstring desiredPath = JoinPath(userFonts, relativePath);
            bool alreadyPresent = false;
            std::wstring destination = ReusableOrNonConflictingFontPath(
                desiredPath, raw, alreadyPresent);
            if (destination.empty()) {
                CloseHandle(file);
                error = L"Không tìm được tên file trống để khôi phục font.";
                return false;
            }
            if (alreadyPresent) {
                ++fontsAlreadyPresent;
            } else {
                if (cancellationRequested()) {
                    CloseHandle(file);
                    error = cancellationError();
                    return false;
                }
                if (!WriteBytesToFile(destination, raw, error)) {
                    CloseHandle(file);
                    error = L"Không ghi được font " + FileNameOnly(destination) + L".\n" + error;
                    return false;
                }
                appliedAnyChange = true;
                ++fontFilesWritten;
                restoredBytes += raw.size();
            }
            fontsToRegister.push_back({destination, displayName});
            ++fontRestored;
            ++restored;
            continue;
        }

        std::wstring destinationBasePath;
        if (rootToken == L"APPDATA") {
            destinationBasePath = appData;
        } else if (rootToken == L"LOCALAPPDATA") {
            destinationBasePath = localAppData;
        }
        if (destinationBasePath.empty()) {
            CloseHandle(file);
            error = rootToken == L"APPDATA"
                        ? L"Không tìm được AppData để khôi phục cài đặt."
                        : L"Không tìm được LocalAppData để khôi phục cài đặt.";
            return false;
        }

        std::wstring destinationRelativePath = relativePath;
        if (options.HasTargetMapping()) {
            const std::wstring targetRoot = RestoreTargetRootForToken(options, rootToken);
            if (!BuildMappedPhotoshopSettingsRelativePath(
                    relativePath, sourceRelativeRoot, targetRoot, sourceLabel,
                    options.targetVersionLabel, destinationRelativePath)) {
                CloseHandle(file);
                error = L"Không ánh xạ được cài đặt sang phiên bản Photoshop đã chọn.";
                return false;
            }
        }
        if (!IsAllowedPhotoshopSettingsRelativePath(destinationRelativePath)) {
            CloseHandle(file);
            error = L"Đường dẫn khôi phục cài đặt Photoshop không hợp lệ.";
            return false;
        }

        std::vector<std::wstring> destinationRelativePaths;
        if (options.HasTargetMapping()) {
            destinationRelativePaths = BuildPhotoshopSettingsRestoreRelativePaths(
                destinationRelativePath, sourceLabel, options.targetVersionLabel);
        } else {
            AddUniquePhotoshopRestoreRelativePath(destinationRelativePaths,
                                                  destinationRelativePath);
        }
        if (destinationRelativePaths.empty()) {
            CloseHandle(file);
            error = L"Không tạo được đường dẫn khôi phục cài đặt Photoshop.";
            return false;
        }

        const bool crossVersionRestore =
            ShouldRewritePhotoshopVersionLabel(sourceLabel,
                                               options.targetVersionLabel);
        bool convertedWorkspaceForEntry = false;
        for (size_t pathIndex = 0; pathIndex < destinationRelativePaths.size(); ++pathIndex) {
            const auto& targetRelativePath = destinationRelativePaths[pathIndex];
            if (pathIndex > 0 &&
                primarySettingsDestinations.count(
                    LowerWide(rootToken + L"\\" + targetRelativePath)) > 0) {
                continue;
            }
            if (cancellationRequested()) {
                CloseHandle(file);
                error = cancellationError();
                return false;
            }
            if (!IsAllowedPhotoshopSettingsRelativePath(targetRelativePath)) {
                CloseHandle(file);
                error = L"Đường dẫn tương thích của cài đặt Photoshop không hợp lệ.";
                return false;
            }
            const std::wstring targetPath =
                JoinPath(destinationBasePath, targetRelativePath);
            if (pathIndex > 0 && !crossVersionRestore &&
                UsesLegacyPhotoshopWorkspaceNames(options.targetVersionLabel)) {
                std::vector<uint8_t> existingAlias;
                std::wstring ignoredReadError;
                if (!ReadFileBytes(targetPath, existingAlias, ignoredReadError) ||
                    IsStrictStandalonePhotoshopWorkspaceBytes(existingAlias)) {
                    continue;
                }
            }
            std::vector<uint8_t> bytesToWrite = raw;
            if (options.HasTargetMapping() && crossVersionRestore) {
                std::vector<uint8_t> normalized;
                WorkspaceCompatibilityResult compatibility =
                    PreparePhotoshopWorkspaceBytesForTarget(
                        raw, options.targetVersionLabel, targetPath, normalized);
                if (compatibility == WorkspaceCompatibilityResult::Converted) {
                    bytesToWrite.swap(normalized);
                    convertedWorkspaceForEntry = true;
                } else if (compatibility == WorkspaceCompatibilityResult::Incompatible) {
                    CloseHandle(file);
                    error = L"Workspace không tương thích với Photoshop đích.\n" +
                            FileNameOnly(targetPath);
                    return false;
                }
            }
            if (!WriteBytesToFile(targetPath, bytesToWrite, error)) {
                CloseHandle(file);
                return false;
            }
            appliedAnyChange = true;
            if (pathIndex > 0) ++settingsCompatibilityAliases;

            if (options.HasTargetMapping() && rootToken == L"APPDATA") {
                std::wstring settingsFolderRelativePath;
                if (ExtractPhotoshopSettingsFolderRelativePath(
                        targetRelativePath, settingsFolderRelativePath)) {
                    targetSettingsFoldersForRegistry.insert(
                        JoinPath(appData, settingsFolderRelativePath));
                }
            }
        }

        if (convertedWorkspaceForEntry) ++workspaceCompatibilityConversions;
        std::wstring restoredRoot;
        std::wstring restoredLabel;
        if (ExtractPhotoshopVersionInfo(destinationRelativePath, restoredRoot, restoredLabel)) {
            restoredSettingsRoots.insert(rootToken + L"\\" + restoredRoot);
        }
        ++settingsRestored;
        ++restored;
        restoredBytes += raw.size();
    }

    CloseHandle(file);
    if (cancellationRequested()) {
        error = cancellationError();
        return false;
    }
    if (options.HasTargetMapping() && settingsRestored == 0 && archiveHasSettings) {
        error = L"Không có cài đặt phù hợp với phiên bản Photoshop đã chọn.";
        return false;
    }
    if (restored == 0) {
        error = L"Không có dữ liệu phù hợp để khôi phục.";
        return false;
    }

    FontRegistrationResult fontRegistration{};
    if (!fontsToRegister.empty()) {
        progress(95, L"Đang đăng ký font cho người dùng hiện tại...");
        fontRegistration = RegisterRestoredUserFonts(fontsToRegister, progress, cancelled);
        if (fontRegistration.resourceRegistrations > 0 ||
            fontRegistration.registryUpdates > 0) {
            appliedAnyChange = true;
        }
        if (cancellationRequested()) {
            error = cancellationError();
            return false;
        }
        DWORD_PTR unused = 0;
        SendMessageTimeoutW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0,
                            SMTO_ABORTIFHUNG, 1000, &unused);
        if (cancellationRequested()) {
            error = cancellationError();
            return false;
        }
    }

    if (options.HasTargetMapping() && settingsRestored > 0 &&
        !targetSettingsFoldersForRegistry.empty()) {
        if (cancellationRequested()) {
            error = cancellationError();
            return false;
        }
        progress(97, L"Đang cập nhật SettingsFilePath...");
        registrySettingsPathUpdates = registryUpdater
                                          ? registryUpdater(options.targetVersionLabel,
                                                            targetSettingsFoldersForRegistry)
                                           : UpdatePhotoshopSettingsFilePathRegistry(
                                                 options.targetVersionLabel,
                                                 targetSettingsFoldersForRegistry);
        if (registrySettingsPathUpdates > 0) appliedAnyChange = true;
    }

    if (cancellationRequested()) {
        error = cancellationError();
        return false;
    }

    progress(100, L"Đã khôi phục xong dữ liệu .afang.");
    summary = L"Đã khôi phục " + std::to_wstring(restored) + L" file (" +
              FormatBytes(restoredBytes) + L").";
    if (settingsRestored > 0 || fontRestored > 0) {
        summary += L" Cài đặt: " + std::to_wstring(settingsRestored) +
                   L", font: " + std::to_wstring(fontRestored) + L".";
    }
    std::wstring detail;
    if (!options.sourceVersionLabel.empty() && !options.targetVersionLabel.empty()) {
        detail = options.sourceVersionLabel + L" → " + options.targetVersionLabel + L".";
    }
    if (workspaceCompatibilityConversions > 0) {
        detail += L" Đã chuyển đổi " + std::to_wstring(workspaceCompatibilityConversions) +
                  L" workspace.";
    }
    if (settingsCompatibilityAliases > 0) {
        detail += L" Đã tạo " + std::to_wstring(settingsCompatibilityAliases) +
                  L" đường dẫn tương thích.";
    }
    if (!targetSettingsFoldersForRegistry.empty()) {
        detail += registrySettingsPathUpdates > 0
                      ? L" Đã cập nhật SettingsFilePath."
                      : L" Registry chưa có SettingsFilePath phù hợp.";
    }
    if (fontsAlreadyPresent > 0) {
        detail += L" Bỏ qua " + std::to_wstring(fontsAlreadyPresent) + L" font đã có.";
    }
    if (fontRestored > 0 && fontRegistration.resourceRegistrations == 0) {
        detail += L" Font đã chép; Windows chưa nạp ngay.";
    }
    if (fontFilesWritten > 0 && fontRegistration.registryUpdates == 0) {
        detail += L" Registry font chưa cập nhật.";
    }
    if (skippedByVersion > 0) {
        detail += L" Bỏ qua " + std::to_wstring(skippedByVersion) +
                  L" cài đặt khác phiên bản.";
    }
    if (!detail.empty()) summary += L"\n" + detail;
    return true;
}

constexpr DWORD kDefaultPasteHotkeyVk = 0;  // 0 = default dual key: Tab or F4.

bool IsModifierVk(DWORD vk) {
    switch (vk) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

bool IsSelectablePasteHotkey(DWORD vk) {
    return vk >= VK_BACK && vk <= 0xFE && vk != VK_ESCAPE && !IsModifierVk(vk);
}

bool IsExtendedVk(DWORD vk) {
    switch (vk) {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
        case VK_RCONTROL:
        case VK_RMENU:
            return true;
        default:
            return false;
    }
}

std::wstring KeyNameForVk(DWORD vk) {
    if (vk == kDefaultPasteHotkeyVk) return L"Tab/F4";
    if (vk >= L'A' && vk <= L'Z') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= L'0' && vk <= L'9') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);

    switch (vk) {
        case VK_TAB: return L"Tab";
        case VK_RETURN: return L"Enter";
        case VK_SPACE: return L"Space";
        case VK_BACK: return L"Backspace";
        case VK_DELETE: return L"Delete";
        case VK_INSERT: return L"Insert";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_PRIOR: return L"Page Up";
        case VK_NEXT: return L"Page Down";
        case VK_LEFT: return L"Left";
        case VK_RIGHT: return L"Right";
        case VK_UP: return L"Up";
        case VK_DOWN: return L"Down";
        case VK_SNAPSHOT: return L"Print Screen";
        case VK_PAUSE: return L"Pause";
        default: break;
    }

    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG keyNameParam = static_cast<LONG>(scanCode << 16);
    if (IsExtendedVk(vk)) keyNameParam |= (1L << 24);

    wchar_t name[64] = {};
    if (scanCode != 0 && GetKeyNameTextW(keyNameParam, name, 64) > 0) {
        return name;
    }
    return L"VK " + std::to_wstring(vk);
}

std::wstring ShortKeyNameForVk(DWORD vk) {
    std::wstring label = KeyNameForVk(vk);
    if (label == L"Page Up") return L"PgUp";
    if (label == L"Page Down") return L"PgDn";
    if (label == L"Print Screen") return L"PrtSc";
    if (label == L"Backspace") return L"Back";
    if (label.size() > 7) return label.substr(0, 6) + L"...";
    return label;
}

}  // namespace

class ToolTypeApp {
public:
    explicit ToolTypeApp(HINSTANCE instance) : instance_(instance) {}

    ~ToolTypeApp() {
        UninstallHook();
        if (tooltip_) DestroyWindow(tooltip_);
        if (font_) DeleteObject(font_);
        if (smallFont_) DeleteObject(smallFont_);
        if (backBrush_) DeleteObject(backBrush_);
        if (panelBrush_) DeleteObject(panelBrush_);
        if (borderBrush_) DeleteObject(borderBrush_);
    }

    bool Create(int nCmdShow) {
        SetProcessDPIAware();

        WNDCLASSW wc{};
        wc.lpfnWndProc = &ToolTypeApp::WndProc;
        wc.hInstance = instance_;
        wc.lpszClassName = L"ToolTypeWindowClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_TOOLTYPE));
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hbrBackground = nullptr;
        if (!RegisterClassW(&wc)) return false;

        WNDCLASSW listClass{};
        listClass.lpfnWndProc = &ToolTypeApp::ListWndProc;
        listClass.hInstance = instance_;
        listClass.lpszClassName = L"ToolTypeListPanel";
        listClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        listClass.hbrBackground = nullptr;
        RegisterClassW(&listClass);

        WNDCLASSW tooltipClass{};
        tooltipClass.lpfnWndProc = &ToolTypeApp::TooltipWndProc;
        tooltipClass.hInstance = instance_;
        tooltipClass.lpszClassName = L"ToolTypeTooltip";
        tooltipClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        tooltipClass.hbrBackground = nullptr;
        RegisterClassW(&tooltipClass);

        hwnd_ = CreateWindowExW(WS_EX_LAYERED, wc.lpszClassName, L"ToolType",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
                                nullptr, nullptr, instance_, this);
        if (!hwnd_) return false;
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
        HICON smallIcon = reinterpret_cast<HICON>(
            LoadImageW(instance_, MAKEINTRESOURCEW(IDI_TOOLTYPE), IMAGE_ICON,
                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        if (smallIcon) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        SetLayeredWindowAttributes(hwnd_, 0, kWindowAlpha, LWA_ALPHA);

        InitControls();
        LoadConfig(false);
        ApplyTopMost(false);
        SetEnabled(enabled_, false);

        ShowWindow(hwnd_, nCmdShow);
        UpdateWindow(hwnd_);
        StartUpdateCheck(hwnd_);
        return true;
    }

    HWND Window() const { return hwnd_; }
    bool Enabled() const { return enabled_; }
    bool HasLines() const { return !lines_.empty(); }

    void PasteCurrentAndAdvance(HWND targetWindow) {
        if (!enabled_) return;
        if (lines_.empty()) {
            SetStatus(L"Chưa có text. Bấm Add Text để nạp file.");
            return;
        }
        if (endOfTextReached_) {
            ShowEndOfTextPopup();
            return;
        }

        int idx = CurrentIndex();
        if (idx < 0 || idx >= static_cast<int>(lines_.size())) idx = 0;
        idx = FindNextTextLine(idx);
        if (idx < 0) {
            SetStatus(L"Không còn dòng có nội dung để dán.");
            return;
        }

        std::wstring pasteText = PasteTextForLine(lines_[static_cast<size_t>(idx)]);
        if (!SetClipboardText(pasteText)) {
            SetStatus(L"Không ghi được clipboard.");
            return;
        }

        SendCtrlV(targetWindow);
        int nextIdx = FindNextTextLine(idx + 1);
        if (nextIdx >= 0) {
            SelectIndex(nextIdx, false);
            endOfTextReached_ = false;
        } else {
            SelectIndex(idx, false);
            endOfTextReached_ = true;
        }

        wchar_t msg[128];
        std::swprintf(msg, 128, L"Đã dán dòng %d/%u.", idx + 1,
                      static_cast<unsigned>(lines_.size()));
        SetStatus(msg);
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        ToolTypeApp* app = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            app = reinterpret_cast<ToolTypeApp*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            app->hwnd_ = hwnd;
        } else {
            app = reinterpret_cast<ToolTypeApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (!app) return DefWindowProcW(hwnd, msg, wp, lp);
        return app->HandleMessage(msg, wp, lp);
    }

    static LRESULT CALLBACK KeyboardProc(int code, WPARAM wp, LPARAM lp) {
        if (code == HC_ACTION && g_app && g_app->Enabled() && g_app->HasLines()) {
            auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
            bool keyDown = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
            if (keyDown && !(kb->flags & LLKHF_INJECTED)) {
                bool alt = (kb->flags & LLKHF_ALTDOWN) != 0;
                bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                           (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
                bool plain = !alt && !ctrl && !shift && !win;
                if (plain && g_app->IsPasteHotkey(kb->vkCode)) {
                    PostMessageW(g_app->Window(), WM_APP_PASTE_NEXT, kb->vkCode,
                                 reinterpret_cast<LPARAM>(GetForegroundWindow()));
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, code, wp, lp);
    }

    static LRESULT CALLBACK ListWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        ToolTypeApp* app = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            app = reinterpret_cast<ToolTypeApp*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<ToolTypeApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (!app) return DefWindowProcW(hwnd, msg, wp, lp);
        return app->HandleListMessage(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK TooltipWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        ToolTypeApp* app = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            app = reinterpret_cast<ToolTypeApp*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<ToolTypeApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (app) {
            if (msg == WM_PAINT) {
                app->PaintTooltip(hwnd);
                return 0;
            }
            if (msg == WM_ERASEBKGND) return 1;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    struct InputDialogState {
        enum class Kind { TextInput, KeyCapture };
        ToolTypeApp* app = nullptr;
        Kind kind = Kind::TextInput;
        std::wstring value;
        DWORD capturedVk = kDefaultPasteHotkeyVk;
        bool accepted = false;
        bool validateGoogleDocsLink = false;
        HWND edit = nullptr;
        HWND info = nullptr;
    };

    struct PtsDialogState {
        ToolTypeApp* app = nullptr;
        HWND dialog = nullptr;
        HWND status = nullptr;
        HWND percent = nullptr;
        HWND backupSettings = nullptr;
        HWND backupFonts = nullptr;
        HWND backupAll = nullptr;
        HWND restore = nullptr;
        HWND close = nullptr;
        RECT progressRect{18, 220, 502, 240};
        int progress = 0;
        ULONGLONG lastProgressPaint = 0;
        std::wstring lastProgressMessage;
        bool busy = false;
        bool cancelRequested = false;
        HANDLE workerThread = nullptr;
        std::shared_ptr<void> cancellation;
        int operationKind = static_cast<int>(PtsOperationKind::None);
    };

    struct ChoiceDialogState {
        ToolTypeApp* app = nullptr;
        std::wstring title;
        std::wstring prompt;
        std::vector<std::wstring> options;
        HWND list = nullptr;
        int selectedIndex = 0;
        bool accepted = false;
    };

    static LRESULT CALLBACK InputDialogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* state = reinterpret_cast<InputDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            state = reinterpret_cast<InputDialogState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }

        switch (msg) {
            case WM_ERASEBKGND:
                if (state && state->app) {
                    state->app->FillDialogBackground(hwnd, reinterpret_cast<HDC>(wp));
                    return 1;
                }
                break;
            case WM_PAINT:
                if (state && state->app) {
                    state->app->PaintDialogFrame(hwnd);
                    return 0;
                }
                break;
            case WM_CTLCOLORSTATIC:
                if (state && state->app) {
                    SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                    SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                    return reinterpret_cast<LRESULT>(state->app->backBrush_);
                }
                break;
            case WM_CTLCOLOREDIT:
                if (state && state->app) {
                    SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                    SetBkColor(reinterpret_cast<HDC>(wp), kPanelColor);
                    return reinterpret_cast<LRESULT>(state->app->panelBrush_);
                }
                break;
            case WM_DRAWITEM:
                if (state && state->app) {
                    state->app->DrawDialogButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
                    return TRUE;
                }
                break;
            case WM_GETDLGCODE:
                if (state && state->kind == InputDialogState::Kind::KeyCapture) {
                    return DLGC_WANTALLKEYS;
                }
                break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (state && state->kind == InputDialogState::Kind::KeyCapture) {
                    DWORD vk = static_cast<DWORD>(wp);
                    if (vk == VK_ESCAPE) {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    if (!IsSelectablePasteHotkey(vk)) {
                        if (state->info) {
                            SetWindowTextW(state->info,
                                           L"Phím này không phù hợp. Hãy chọn một phím đơn khác.");
                        }
                        return 0;
                    }
                    state->capturedVk = vk;
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            case WM_COMMAND:
                if (LOWORD(wp) == IDOK && state && state->edit) {
                    int len = GetWindowTextLengthW(state->edit);
                    std::wstring text(static_cast<size_t>(len) + 1, L'\0');
                    if (len > 0) GetWindowTextW(state->edit, text.data(), len + 1);
                    text.resize(static_cast<size_t>(len));
                    text = TrimCopy(text);
                    if (state->validateGoogleDocsLink && state->app &&
                        state->app->GoogleDocsExportUrl(text).empty()) {
                        const std::wstring error = L"Link Google Docs không hợp lệ.";
                        state->app->SetStatus(error);
                        MessageBoxW(hwnd, error.c_str(), L"ToolType",
                                    MB_OK | MB_ICONWARNING | MB_SETFOREGROUND |
                                        (state->app->topMost_ ? MB_TOPMOST : 0));
                        SetFocus(state->edit);
                        SendMessageW(state->edit, EM_SETSEL, 0, -1);
                        return 0;
                    }
                    state->value = text;
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == ID_DEFAULT_HOTKEY && state) {
                    state->capturedVk = kDefaultPasteHotkeyVk;
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK PtsDialogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* state = reinterpret_cast<PtsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            state = reinterpret_cast<PtsDialogState*>(cs->lpCreateParams);
            state->dialog = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }

        switch (msg) {
            case WM_ERASEBKGND:
                if (state && state->app) {
                    state->app->FillDialogBackground(hwnd, reinterpret_cast<HDC>(wp));
                    return 1;
                }
                break;
            case WM_PAINT:
                if (state && state->app) {
                    state->app->PaintPtsDialogFrame(hwnd, state);
                    return 0;
                }
                break;
            case WM_CTLCOLORSTATIC:
                if (state && state->app) {
                    SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                    SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                    return reinterpret_cast<LRESULT>(state->app->backBrush_);
                }
                break;
            case WM_DRAWITEM:
                if (state && state->app) {
                    state->app->DrawDialogButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
                    return TRUE;
                }
                break;
            case WM_COMMAND:
                if (state && state->app) {
                    state->app->HandlePtsDialogCommand(state, LOWORD(wp));
                    return 0;
                }
                break;
            case WM_APP_PTS_PROGRESS: {
                auto* update = reinterpret_cast<PtsProgressMessage*>(lp);
                if (state && state->app && update) {
                    state->app->HandlePtsBackgroundProgress(state, *update);
                }
                delete update;
                return 0;
            }
            case WM_APP_PTS_OPERATION_DONE: {
                auto* result = reinterpret_cast<PtsBackgroundResult*>(lp);
                if (state && state->app) {
                    if (result) {
                        state->app->HandlePtsBackgroundCompletion(state, *result);
                    } else {
                        PtsBackgroundResult fallback{};
                        fallback.error = L"Không đủ bộ nhớ để nhận kết quả thao tác Pts.";
                        state->app->HandlePtsBackgroundCompletion(state, fallback);
                    }
                }
                delete result;
                return 0;
            }
            case WM_KEYDOWN:
                if (wp == VK_ESCAPE && state) {
                    if (state->busy && state->app) {
                        state->app->RequestPtsOperationCancellation(state);
                    } else {
                        DestroyWindow(hwnd);
                    }
                    return 0;
                }
                break;
            case WM_CLOSE:
                if (state && state->busy) {
                    state->app->RequestPtsOperationCancellation(state);
                    return 0;
                }
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                if (state && state->workerThread) {
                    RequestPtsCancellation(
                        std::static_pointer_cast<PtsCancellationState>(state->cancellation));
                    WaitForSingleObject(state->workerThread, INFINITE);
                    CloseHandle(state->workerThread);
                    state->workerThread = nullptr;
                    MSG pending{};
                    while (PeekMessageW(&pending, hwnd, WM_APP_PTS_PROGRESS,
                                        WM_APP_PTS_OPERATION_DONE, PM_REMOVE)) {
                        if (pending.message == WM_APP_PTS_PROGRESS) {
                            delete reinterpret_cast<PtsProgressMessage*>(pending.lParam);
                        } else if (pending.message == WM_APP_PTS_OPERATION_DONE) {
                            delete reinterpret_cast<PtsBackgroundResult*>(pending.lParam);
                        }
                    }
                }
                break;
            case WM_NCDESTROY:
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }


    static LRESULT CALLBACK ChoiceDialogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* state = reinterpret_cast<ChoiceDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            state = reinterpret_cast<ChoiceDialogState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }

        switch (msg) {
            case WM_ERASEBKGND:
                if (state && state->app) {
                    state->app->FillDialogBackground(hwnd, reinterpret_cast<HDC>(wp));
                    return 1;
                }
                break;
            case WM_PAINT:
                if (state && state->app) {
                    state->app->PaintDialogFrame(hwnd);
                    return 0;
                }
                break;
            case WM_CTLCOLORSTATIC:
                if (state && state->app) {
                    SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                    SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                    return reinterpret_cast<LRESULT>(state->app->backBrush_);
                }
                break;
            case WM_CTLCOLORLISTBOX:
                if (state && state->app) {
                    SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                    SetBkColor(reinterpret_cast<HDC>(wp), kPanelColor);
                    return reinterpret_cast<LRESULT>(state->app->panelBrush_);
                }
                break;
            case WM_DRAWITEM:
                if (state && state->app) {
                    state->app->DrawDialogButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
                    return TRUE;
                }
                break;
            case WM_COMMAND:
                if (!state) break;
                if (LOWORD(wp) == IDOK && state->list) {
                    LRESULT sel = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                    state->selectedIndex = sel == LB_ERR ? 0 : static_cast<int>(sel);
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (reinterpret_cast<HWND>(lp) == state->list && HIWORD(wp) == LBN_DBLCLK) {
                    LRESULT sel = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                    state->selectedIndex = sel == LB_ERR ? 0 : static_cast<int>(sel);
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            case WM_KEYDOWN:
                if (wp == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static LRESULT CALLBACK ButtonWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* app = reinterpret_cast<ToolTypeApp*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
        if (app) {
            int id = GetDlgCtrlID(hwnd);
            if (msg == WM_MOUSEMOVE) {
                app->SetHoveredButton(id);
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
            } else if (msg == WM_MOUSELEAVE) {
                if (app->hoverButtonId_ == id) app->SetHoveredButton(0);
            }
            return CallWindowProcW(app->buttonProc_, hwnd, msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_COMMAND:
                OnCommand(LOWORD(wp));
                return 0;
            case WM_SIZE:
                Layout();
                return 0;
            case WM_MEASUREITEM:
                return OnMeasureItem(reinterpret_cast<MEASUREITEMSTRUCT*>(lp));
            case WM_DRAWITEM:
                return OnDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
            case WM_CTLCOLORSTATIC:
                if (reinterpret_cast<HWND>(lp) == status_) {
                    SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                    SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                    return reinterpret_cast<LRESULT>(backBrush_);
                }
                break;
            case WM_CTLCOLORLISTBOX:
                SetTextColor(reinterpret_cast<HDC>(wp), kTextColor);
                SetBkColor(reinterpret_cast<HDC>(wp), kPanelColor);
                return reinterpret_cast<LRESULT>(panelBrush_);
            case WM_ERASEBKGND:
                FillClient(reinterpret_cast<HDC>(wp));
                return 1;
            case WM_APP_PASTE_NEXT:
                PasteCurrentAndAdvance(reinterpret_cast<HWND>(lp));
                return 0;
            case WM_APP_UPDATE_CHECK_DONE:
                HandleUpdateCheckResult(reinterpret_cast<UpdateCheckResult*>(lp));
                return 0;
            case WM_SETCURSOR:
                if (reinterpret_cast<HWND>(wp) == status_ && !updateDownloadUrl_.empty()) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
                break;
            case WM_DESTROY:
                UninstallHook();
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }

    void InitControls() {
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        ncm.lfMessageFont.lfHeight = -13;
        std::wcscpy(ncm.lfMessageFont.lfFaceName, L"Segoe UI");
        font_ = CreateFontIndirectW(&ncm.lfMessageFont);

        LOGFONTW small = ncm.lfMessageFont;
        small.lfHeight = -12;
        smallFont_ = CreateFontIndirectW(&small);

        backBrush_ = CreateSolidBrush(kBackColor);
        panelBrush_ = CreateSolidBrush(kPanelColor);
        borderBrush_ = CreateSolidBrush(kBorderColor);

        list_ = CreateWindowExW(0, L"ToolTypeListPanel", nullptr,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
                                0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_LIST),
                                instance_, this);

        addButton_ = CreateButton(L"Add Text", ID_ADD);
        pinButton_ = CreateButton(L"Pin", ID_PIN);
        saveButton_ = CreateButton(L"Save", ID_SAVE);
        openButton_ = CreateButton(L"Open", ID_OPEN);
        onoffButton_ = CreateButton(L"On/Off", ID_ONOFF);
        expandButton_ = CreateButton(L"Expand", ID_EXPAND);
        googleDocsButton_ = CreateButton(L"GDocs", ID_GDOCS);
        hotkeyButton_ = CreateButton(L"Tab/F4", ID_HOTKEY);
        guideButton_ = CreateButton(L"Guide", ID_GUIDE);
        ptsButton_ = CreateButton(L"Pts", ID_PTS);
        ShowWindow(googleDocsButton_, SW_HIDE);
        ShowWindow(hotkeyButton_, SW_HIDE);
        ShowWindow(guideButton_, SW_HIDE);
        ShowWindow(ptsButton_, SW_HIDE);

        status_ = CreateWindowExW(0, L"STATIC", kLatestVersionMessage,
                                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY, 0, 0, 0, 0,
                                  hwnd_, reinterpret_cast<HMENU>(ID_STATUS), instance_, nullptr);
        tooltip_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                   L"ToolTypeTooltip", nullptr, WS_POPUP | WS_DISABLED,
                                   0, 0, 0, 0, hwnd_, nullptr, instance_, this);

        SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(status_, WM_SETFONT, reinterpret_cast<WPARAM>(smallFont_), TRUE);

        Layout();
    }

    HWND CreateButton(const wchar_t* text, int id) {
        HWND button = CreateWindowExW(0, L"BUTTON", text,
                                      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                      0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(id),
                                      instance_, nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        WNDPROC previous = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(button, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ToolTypeApp::ButtonWndProc)));
        if (!buttonProc_) buttonProc_ = previous;
        return button;
    }

    void Layout() {
        if (!list_) return;
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        int cw = rc.right - rc.left;
        int ch = rc.bottom - rc.top;

        int buttonW = 72;
        int buttonH = 32;
        int columnGap = 7;
        int buttonColumns = expanded_ ? 2 : 1;
        int buttonBlockW = buttonColumns * buttonW + (buttonColumns - 1) * columnGap;
        int preferredButtonX = expanded_ ? 232 : 216;
        int buttonX = std::min(preferredButtonX, std::max(8, cw - buttonBlockW - 8));
        int secondButtonX = buttonX + buttonW + columnGap;
        int top = 6;
        int gap = 5;
        int statusH = 17;
        int listX = 3;
        int listY = 5;
        int listW = std::max(50, buttonX - 7);
        int listH = std::max(40, ch - statusH - 11);

        MoveWindow(list_, listX, listY, listW, listH, TRUE);
        HWND buttons[] = {addButton_, pinButton_, saveButton_, openButton_, onoffButton_, expandButton_};
        for (int i = 0; i < 6; ++i) {
            MoveWindow(buttons[i], buttonX, top + i * (buttonH + gap), buttonW, buttonH, TRUE);
        }
        HWND extraButtons[] = {googleDocsButton_, hotkeyButton_, guideButton_, ptsButton_};
        for (int i = 0; i < 4; ++i) {
            ShowWindow(extraButtons[i], expanded_ ? SW_SHOW : SW_HIDE);
            MoveWindow(extraButtons[i], secondButtonX, top + i * (buttonH + gap), buttonW, buttonH, TRUE);
        }
        MoveWindow(status_, 5, std::max(0, ch - statusH), std::max(10, cw - 10), statusH, TRUE);
    }

    LRESULT OnMeasureItem(MEASUREITEMSTRUCT* mi) {
        (void)mi;
        return FALSE;
    }

    LRESULT OnDrawItem(DRAWITEMSTRUCT* di) {
        if (!di) return FALSE;
        DrawButton(di);
        return TRUE;
    }

    void FillSolidRect(HDC hdc, const RECT& rc, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }

    void DrawCrispOnePixelBorder(HDC hdc, const RECT& rc, COLORREF color) {
        if (rc.right <= rc.left || rc.bottom <= rc.top) return;
        HBRUSH brush = CreateSolidBrush(color);
        RECT edge{rc.left, rc.top, rc.right, rc.top + 1};
        FillRect(hdc, &edge, brush);
        edge = {rc.left, rc.bottom - 1, rc.right, rc.bottom};
        FillRect(hdc, &edge, brush);
        edge = {rc.left, rc.top + 1, rc.left + 1, rc.bottom - 1};
        FillRect(hdc, &edge, brush);
        edge = {rc.right - 1, rc.top + 1, rc.right, rc.bottom - 1};
        FillRect(hdc, &edge, brush);
        DeleteObject(brush);
    }

    void DrawButton(DRAWITEMSTRUCT* di) {
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        bool disabled = (di->itemState & ODS_DISABLED) != 0;
        int id = GetDlgCtrlID(di->hwndItem);
        bool hot = hoverButtonId_ == id || (di->itemState & ODS_HOTLIGHT) != 0;
        RECT rc = di->rcItem;
        COLORREF fill = kButtonColor;
        COLORREF border = kBorderColor;
        COLORREF textColor = disabled ? RGB(140, 150, 154) : kTextColor;

        if (id == ID_PIN) {
            fill = topMost_ ? RGB(0, 72, 62) : RGB(18, 18, 18);
            border = topMost_ ? RGB(0, 218, 184) : RGB(112, 120, 122);
        } else if (id == ID_ONOFF) {
            fill = enabled_ ? RGB(0, 78, 34) : RGB(82, 25, 25);
            border = enabled_ ? RGB(0, 210, 92) : RGB(228, 80, 72);
        } else if (id == ID_EXPAND) {
            fill = expanded_ ? RGB(0, 50, 84) : RGB(18, 18, 18);
            border = expanded_ ? RGB(75, 180, 255) : RGB(112, 120, 122);
        } else if (id == ID_PTS) {
            fill = RGB(45, 27, 74);
            border = RGB(184, 134, 255);
        }

        if (hot) {
            fill = kButtonHoverColor;
            border = RGB(255, 255, 255);
            textColor = RGB(0, 0, 0);
        }
        if (pressed) fill = BlendColor(fill, RGB(0, 0, 0), 22);

        FillSolidRect(di->hDC, rc, fill);
        DrawCrispOnePixelBorder(di->hDC, rc, border);

        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, textColor);
        HFONT drawFont = (id == ID_HOTKEY && smallFont_) ? smallFont_ : font_;
        SelectObject(di->hDC, drawFont);

        wchar_t text[64] = {};
        GetWindowTextW(di->hwndItem, text, 64);
        RECT textRc = rc;
        if (pressed) OffsetRect(&textRc, 1, 1);
        InflateRect(&textRc, id == ID_HOTKEY ? -2 : 0, 0);
        DrawTextW(di->hDC, text, -1, &textRc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void FillDialogBackground(HWND hwnd, HDC hdc) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, backBrush_);
    }

    void PaintDialogFrame(HWND hwnd) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        FillDialogBackground(hwnd, hdc);

        RECT rc{};
        GetClientRect(hwnd, &rc);
        HPEN pen = CreatePen(PS_SOLID, 1, kBorderColor);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &ps);
    }

    void PaintPtsDialogFrame(HWND hwnd, const PtsDialogState* state) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        FillDialogBackground(hwnd, hdc);

        RECT rc{};
        GetClientRect(hwnd, &rc);
        HPEN pen = CreatePen(PS_SOLID, 1, kBorderColor);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        RECT bar = state ? state->progressRect : RECT{18, 220, 502, 240};
        FillSolidRect(hdc, bar, RGB(22, 22, 22));
        DrawCrispOnePixelBorder(hdc, bar, kBorderColor);
        int pct = state ? std::clamp(state->progress, 0, 100) : 0;
        RECT fill = bar;
        fill.right = fill.left + MulDiv(RectWidth(bar), pct, 100);
        if (fill.right > fill.left) {
            FillSolidRect(hdc, fill, RGB(0, 135, 95));
        }

        EndPaint(hwnd, &ps);
    }

    void DrawDialogButton(DRAWITEMSTRUCT* di) {
        if (!di || di->CtlType != ODT_BUTTON) return;
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        bool disabled = (di->itemState & ODS_DISABLED) != 0;
        bool focused = (di->itemState & ODS_FOCUS) != 0;
        bool hot = !disabled && (di->itemState & ODS_HOTLIGHT) != 0;
        RECT rc = di->rcItem;

        COLORREF fill = disabled ? RGB(8, 8, 8) : (hot ? kButtonHoverColor : kButtonColor);
        COLORREF border = disabled ? RGB(65, 70, 72)
                                   : (hot ? RGB(255, 255, 255) : kBorderColor);
        COLORREF textColor = disabled ? RGB(122, 130, 133)
                                      : (hot ? RGB(0, 0, 0) : kTextColor);
        if (pressed && !disabled) fill = BlendColor(fill, RGB(0, 0, 0), 22);

        FillSolidRect(di->hDC, rc, fill);
        DrawCrispOnePixelBorder(di->hDC, rc, border);

        wchar_t text[64] = {};
        GetWindowTextW(di->hwndItem, text, 64);
        RECT textRc = rc;
        if (pressed) OffsetRect(&textRc, 1, 1);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, textColor);
        HFONT controlFont = reinterpret_cast<HFONT>(
            SendMessageW(di->hwndItem, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = SelectObject(di->hDC, controlFont ? controlFont : font_);
        DrawTextW(di->hDC, text, -1, &textRc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (focused && !disabled) {
            RECT focus = rc;
            InflateRect(&focus, -4, -4);
            DrawFocusRect(di->hDC, &focus);
        }
        SelectObject(di->hDC, oldFont);
    }

    enum class ListPart { None, VThumb, VTrack, HThumb, HTrack };

    struct ListMetrics {
        RECT outer{};
        RECT inner{};
        RECT content{};
        RECT vbar{};
        RECT hbar{};
        RECT vthumb{};
        RECT hthumb{};
        RECT corner{};
        bool showV = false;
        bool showH = false;
        int visibleRows = 1;
        int maxTop = 0;
        int maxHorizontal = 0;
    };

    static int RectWidth(const RECT& rc) { return std::max(0, static_cast<int>(rc.right - rc.left)); }
    static int RectHeight(const RECT& rc) { return std::max(0, static_cast<int>(rc.bottom - rc.top)); }

    static bool Contains(const RECT& rc, int x, int y) {
        POINT pt{x, y};
        return PtInRect(&rc, pt) != 0;
    }

    ListMetrics BuildListMetrics() const {
        ListMetrics m{};
        GetClientRect(list_, &m.outer);
        m.inner = m.outer;
        InflateRect(&m.inner, -1, -1);
        if (m.inner.right < m.inner.left) m.inner.right = m.inner.left;
        if (m.inner.bottom < m.inner.top) m.inner.bottom = m.inner.top;

        bool showV = false;
        bool showH = false;
        for (int pass = 0; pass < 3; ++pass) {
            int availableW = std::max(1, RectWidth(m.inner) - (showV ? kScrollSize : 0));
            int availableH = std::max(1, RectHeight(m.inner) - (showH ? kScrollSize : 0));
            int visibleRows = std::max(1, availableH / kLineHeight);
            bool needV = static_cast<int>(lines_.size()) > visibleRows;
            bool needH = maxTextWidth_ > std::max(1, availableW - 8);
            if (needV == showV && needH == showH) break;
            showV = needV;
            showH = needH;
        }

        m.showV = showV;
        m.showH = showH;
        m.content = m.inner;
        if (m.showV) m.content.right -= kScrollSize;
        if (m.showH) m.content.bottom -= kScrollSize;
        if (m.content.right < m.content.left) m.content.right = m.content.left;
        if (m.content.bottom < m.content.top) m.content.bottom = m.content.top;

        m.visibleRows = std::max(1, RectHeight(m.content) / kLineHeight);
        m.maxTop = std::max(0, static_cast<int>(lines_.size()) - m.visibleRows);
        m.maxHorizontal = std::max(0, maxTextWidth_ - std::max(1, RectWidth(m.content) - 8));

        if (m.showV) {
            m.vbar = {m.content.right, m.inner.top, m.inner.right, m.content.bottom};
            int trackH = RectHeight(m.vbar);
            int thumbH = trackH;
            if (!lines_.empty()) {
                thumbH = std::clamp(MulDiv(trackH, m.visibleRows, static_cast<int>(lines_.size())),
                                    std::min(kMinScrollThumb, trackH), trackH);
            }
            int range = std::max(0, trackH - thumbH);
            int top = m.vbar.top + (m.maxTop > 0 ? MulDiv(std::clamp(topIndex_, 0, m.maxTop), range, m.maxTop) : 0);
            m.vthumb = {m.vbar.left + 1, top + 1, m.vbar.right - 1, top + thumbH - 1};
        }

        if (m.showH) {
            m.hbar = {m.inner.left, m.content.bottom, m.content.right, m.inner.bottom};
            int trackW = RectWidth(m.hbar);
            int contentW = std::max(1, RectWidth(m.content));
            int totalW = contentW + m.maxHorizontal;
            int thumbW = std::clamp(MulDiv(trackW, contentW, std::max(1, totalW)),
                                    std::min(kMinScrollThumb, trackW), trackW);
            int range = std::max(0, trackW - thumbW);
            int left = m.hbar.left + (m.maxHorizontal > 0 ? MulDiv(std::clamp(horizontalOffset_, 0, m.maxHorizontal), range, m.maxHorizontal) : 0);
            m.hthumb = {left + 1, m.hbar.top + 1, left + thumbW - 1, m.hbar.bottom - 1};
        }

        if (m.showV && m.showH) {
            m.corner = {m.content.right, m.content.bottom, m.inner.right, m.inner.bottom};
        }
        return m;
    }

    void ClampListState() {
        if (lines_.empty()) {
            selectedIndex_ = 0;
            topIndex_ = 0;
            horizontalOffset_ = 0;
            return;
        }
        selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(lines_.size()) - 1);
        ListMetrics m = BuildListMetrics();
        topIndex_ = std::clamp(topIndex_, 0, m.maxTop);
        horizontalOffset_ = std::clamp(horizontalOffset_, 0, m.maxHorizontal);
    }

    void KeepSelectionWithNextLineVisible() {
        if (lines_.empty()) return;

        ListMetrics m = BuildListMetrics();
        if (selectedIndex_ < topIndex_) {
            topIndex_ = selectedIndex_;
        } else {
            bool hasNextLine = selectedIndex_ + 1 < static_cast<int>(lines_.size());
            int reservedRowsBelow = (hasNextLine && m.visibleRows > 1) ? 1 : 0;
            int lastComfortableRow = topIndex_ + m.visibleRows - 1 - reservedRowsBelow;
            if (selectedIndex_ > lastComfortableRow) {
                topIndex_ = selectedIndex_ - m.visibleRows + 1 + reservedRowsBelow;
            }
        }

        topIndex_ = std::clamp(topIndex_, 0, m.maxTop);
    }

    void DrawModernScrollbar(HDC hdc, const RECT& track, const RECT& thumb, bool hot) {
        HBRUSH trackBrush = CreateSolidBrush(kScrollTrackColor);
        FillRect(hdc, &track, trackBrush);
        DeleteObject(trackBrush);

        COLORREF thumbColor = hot ? kScrollThumbHotColor : kScrollThumbColor;
        HBRUSH thumbBrush = CreateSolidBrush(thumbColor);
        HPEN thumbPen = CreatePen(PS_SOLID, 1, thumbColor);
        HGDIOBJ oldBrush = SelectObject(hdc, thumbBrush);
        HGDIOBJ oldPen = SelectObject(hdc, thumbPen);
        RoundRect(hdc, thumb.left, thumb.top, thumb.right, thumb.bottom, 5, 5);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(thumbPen);
        DeleteObject(thumbBrush);
    }

    void PaintList() {
        ClampListState();

        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(list_, &ps);
        RECT rc{};
        GetClientRect(list_, &rc);
        int w = std::max(1, RectWidth(rc));
        int h = std::max(1, RectHeight(rc));

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        FillRect(mem, &rc, panelBrush_);
        FrameRect(mem, &rc, borderBrush_);

        ListMetrics m = BuildListMetrics();
        int saved = SaveDC(mem);
        IntersectClipRect(mem, m.content.left, m.content.top, m.content.right, m.content.bottom);
        SelectObject(mem, font_);
        SetBkMode(mem, TRANSPARENT);

        for (int row = 0; row < m.visibleRows; ++row) {
            int idx = topIndex_ + row;
            if (idx >= static_cast<int>(lines_.size())) break;
            RECT rowRc{m.content.left, m.content.top + row * kLineHeight,
                       m.content.right, m.content.top + (row + 1) * kLineHeight};
            if (idx == selectedIndex_) {
                HBRUSH sel = CreateSolidBrush(kSelectColor);
                FillRect(mem, &rowRc, sel);
                DeleteObject(sel);
            }

            RECT textRc = rowRc;
            textRc.left += 4 - horizontalOffset_;
            textRc.right = textRc.left + std::max(maxTextWidth_, RectWidth(m.content));
            SetTextColor(mem, idx == selectedIndex_ ? RGB(255, 255, 255)
                                                     : TextColorForLine(lines_[idx]));
            DrawTextW(mem, lines_[idx].c_str(), -1, &textRc,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_NOCLIP);
        }

        if (lines_.empty()) {
            RECT hint = m.content;
            hint.left += 6;
            SetTextColor(mem, kMutedTextColor);
            DrawTextW(mem, L"Add Text để nạp file...", -1, &hint,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        }
        RestoreDC(mem, saved);

        if (m.showV) {
            bool hot = listHoverPart_ == ListPart::VThumb || dragPart_ == ListPart::VThumb;
            DrawModernScrollbar(mem, m.vbar, m.vthumb, hot);
        }
        if (m.showH) {
            bool hot = listHoverPart_ == ListPart::HThumb || dragPart_ == ListPart::HThumb;
            DrawModernScrollbar(mem, m.hbar, m.hthumb, hot);
        }
        if (m.showV && m.showH) {
            FillRect(mem, &m.corner, panelBrush_);
        }

        BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(list_, &ps);
    }

    ListPart HitTestListPart(const ListMetrics& m, int x, int y) const {
        if (m.showV && Contains(m.vthumb, x, y)) return ListPart::VThumb;
        if (m.showH && Contains(m.hthumb, x, y)) return ListPart::HThumb;
        if (m.showV && Contains(m.vbar, x, y)) return ListPart::VTrack;
        if (m.showH && Contains(m.hbar, x, y)) return ListPart::HTrack;
        return ListPart::None;
    }

    void SetListHoverPart(ListPart part) {
        if (listHoverPart_ != part) {
            listHoverPart_ = part;
            InvalidateRect(list_, nullptr, FALSE);
        }
    }

    int ListIndexAtPoint(const ListMetrics& m, int x, int y) const {
        if (!Contains(m.content, x, y) || lines_.empty()) return -1;
        int row = (y - m.content.top) / kLineHeight;
        int idx = topIndex_ + row;
        return (idx >= 0 && idx < static_cast<int>(lines_.size())) ? idx : -1;
    }

    void HideTooltip() {
        tooltipLineIndex_ = -1;
        tooltipText_.clear();
        if (tooltip_ && IsWindowVisible(tooltip_)) ShowWindow(tooltip_, SW_HIDE);
    }

    void PaintTooltip(HWND hwnd) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);

        HBRUSH back = CreateSolidBrush(kTooltipBackColor);
        FillRect(hdc, &rc, back);
        DeleteObject(back);

        HPEN pen = CreatePen(PS_SOLID, 1, kTooltipBorderColor);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        RECT textRc = rc;
        InflateRect(&textRc, -8, -6);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kTextColor);
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, smallFont_ ? smallFont_ : font_));
        DrawTextW(hdc, tooltipText_.c_str(), -1, &textRc,
                  DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
    }

    SIZE MeasureTooltipText(const std::wstring& text) {
        SIZE size{120, kLineHeight + 14};
        if (!tooltip_ || text.empty()) return size;

        HDC hdc = GetDC(tooltip_);
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, smallFont_ ? smallFont_ : font_));
        RECT rc{0, 0, 640, 0};
        DrawTextW(hdc, text.c_str(), -1, &rc,
                  DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);
        SelectObject(hdc, oldFont);
        ReleaseDC(tooltip_, hdc);

        size.cx = std::clamp(RectWidth(rc) + 16, 120, 680);
        size.cy = std::clamp(RectHeight(rc) + 12, kLineHeight + 14, 240);
        return size;
    }

    void ShowLineTooltip(int idx, int x, int y) {
        if (!tooltip_ || idx < 0 || idx >= static_cast<int>(lines_.size()) || lines_[idx].empty()) {
            HideTooltip();
            return;
        }

        if (tooltipLineIndex_ != idx || tooltipText_ != lines_[idx]) {
            tooltipLineIndex_ = idx;
            tooltipText_ = lines_[idx];
            InvalidateRect(tooltip_, nullptr, TRUE);
        }

        SIZE size = MeasureTooltipText(tooltipText_);
        POINT pt{x + 16, y + 20};
        ClientToScreen(list_, &pt);

        HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi)) {
            if (pt.x + size.cx > mi.rcWork.right) pt.x = mi.rcWork.right - size.cx - 4;
            if (pt.y + size.cy > mi.rcWork.bottom) pt.y -= size.cy + 34;
            pt.x = std::max(mi.rcWork.left + 4, pt.x);
            pt.y = std::max(mi.rcWork.top + 4, pt.y);
        }

        SetWindowPos(tooltip_, HWND_TOPMOST, pt.x, pt.y, size.cx, size.cy,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void ScrollVertical(int rows) {
        if (rows == 0) return;
        HideTooltip();
        ListMetrics m = BuildListMetrics();
        int before = topIndex_;
        topIndex_ = std::clamp(topIndex_ + rows, 0, m.maxTop);
        if (topIndex_ != before) InvalidateRect(list_, nullptr, FALSE);
    }

    void ScrollHorizontal(int pixels) {
        if (pixels == 0) return;
        HideTooltip();
        ListMetrics m = BuildListMetrics();
        int before = horizontalOffset_;
        horizontalOffset_ = std::clamp(horizontalOffset_ + pixels, 0, m.maxHorizontal);
        if (horizontalOffset_ != before) InvalidateRect(list_, nullptr, FALSE);
    }

    void HandleListButtonDown(int x, int y) {
        SetFocus(list_);
        ListMetrics m = BuildListMetrics();
        ListPart part = HitTestListPart(m, x, y);
        if (part == ListPart::VThumb || part == ListPart::HThumb) {
            HideTooltip();
            dragPart_ = part;
            dragStartMouse_ = (part == ListPart::VThumb) ? y : x;
            dragStartTop_ = topIndex_;
            dragStartHorizontal_ = horizontalOffset_;
            SetCapture(list_);
            return;
        }
        if (part == ListPart::VTrack) {
            HideTooltip();
            ScrollVertical(y < m.vthumb.top ? -m.visibleRows : m.visibleRows);
            return;
        }
        if (part == ListPart::HTrack) {
            HideTooltip();
            ScrollHorizontal(x < m.hthumb.left ? -RectWidth(m.content) : RectWidth(m.content));
            return;
        }
        if (Contains(m.content, x, y) && !lines_.empty()) {
            int row = (y - m.content.top) / kLineHeight;
            int idx = topIndex_ + row;
            if (idx >= 0 && idx < static_cast<int>(lines_.size())) {
                SelectIndex(idx);
            }
        }
    }

    void HandleListMouseMove(int x, int y) {
        if (dragPart_ == ListPart::VThumb) {
            ListMetrics m = BuildListMetrics();
            int trackRange = std::max(1, RectHeight(m.vbar) - RectHeight(m.vthumb));
            topIndex_ = std::clamp(dragStartTop_ + MulDiv(y - dragStartMouse_, m.maxTop, trackRange), 0, m.maxTop);
            InvalidateRect(list_, nullptr, FALSE);
            return;
        }
        if (dragPart_ == ListPart::HThumb) {
            ListMetrics m = BuildListMetrics();
            int trackRange = std::max(1, RectWidth(m.hbar) - RectWidth(m.hthumb));
            horizontalOffset_ = std::clamp(dragStartHorizontal_ + MulDiv(x - dragStartMouse_, m.maxHorizontal, trackRange),
                                           0, m.maxHorizontal);
            InvalidateRect(list_, nullptr, FALSE);
            return;
        }

        ListMetrics m = BuildListMetrics();
        ListPart part = HitTestListPart(m, x, y);
        SetListHoverPart(part);
        if (part == ListPart::None) {
            ShowLineTooltip(ListIndexAtPoint(m, x, y), x, y);
        } else {
            HideTooltip();
        }
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, list_, 0};
        TrackMouseEvent(&tme);
    }

    void HandleListKeyDown(WPARAM key) {
        HideTooltip();
        if (lines_.empty()) return;
        switch (key) {
            case VK_UP: SelectIndex(selectedIndex_ - 1); break;
            case VK_DOWN: SelectIndex(selectedIndex_ + 1); break;
            case VK_PRIOR: SelectIndex(selectedIndex_ - BuildListMetrics().visibleRows); break;
            case VK_NEXT: SelectIndex(selectedIndex_ + BuildListMetrics().visibleRows); break;
            case VK_HOME: SelectIndex(0); break;
            case VK_END: SelectIndex(static_cast<int>(lines_.size()) - 1); break;
            case VK_LEFT: ScrollHorizontal(-32); break;
            case VK_RIGHT: ScrollHorizontal(32); break;
        }
    }

    LRESULT HandleListMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_PAINT:
                PaintList();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_LBUTTONDOWN:
                HandleListButtonDown(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                return 0;
            case WM_MOUSEMOVE:
                HandleListMouseMove(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                return 0;
            case WM_LBUTTONUP:
                if (dragPart_ != ListPart::None) {
                    HideTooltip();
                    dragPart_ = ListPart::None;
                    ReleaseCapture();
                    InvalidateRect(list_, nullptr, FALSE);
                }
                return 0;
            case WM_MOUSELEAVE:
                SetListHoverPart(ListPart::None);
                HideTooltip();
                return 0;
            case WM_MOUSEWHEEL: {
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                ScrollVertical(delta > 0 ? -3 : 3);
                return 0;
            }
            case WM_KEYDOWN:
                HandleListKeyDown(wp);
                return 0;
            case WM_SETCURSOR:
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND ButtonById(int id) const {
        switch (id) {
            case ID_ADD: return addButton_;
            case ID_PIN: return pinButton_;
            case ID_SAVE: return saveButton_;
            case ID_OPEN: return openButton_;
            case ID_ONOFF: return onoffButton_;
            case ID_EXPAND: return expandButton_;
            case ID_GDOCS: return googleDocsButton_;
            case ID_HOTKEY: return hotkeyButton_;
            case ID_GUIDE: return guideButton_;
            case ID_PTS: return ptsButton_;
        }
        return nullptr;
    }

    void SetHoveredButton(int id) {
        if (hoverButtonId_ == id) return;
        int old = hoverButtonId_;
        hoverButtonId_ = id;
        if (HWND oldButton = ButtonById(old)) InvalidateRect(oldButton, nullptr, TRUE);
        if (HWND newButton = ButtonById(id)) InvalidateRect(newButton, nullptr, TRUE);
    }

    void FillClient(HDC hdc) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        FillRect(hdc, &rc, backBrush_);
    }

    void OnCommand(int id) {
        switch (id) {
            case ID_STATUS:
                OpenUpdateDownloadUrl();
                break;
            case ID_ADD:
                ChooseAndLoadFile();
                break;
            case ID_PIN:
                topMost_ = !topMost_;
                ApplyTopMost(true);
                break;
            case ID_SAVE:
                SaveConfig(true);
                break;
            case ID_OPEN:
                OpenSaved();
                break;
            case ID_ONOFF:
                SetEnabled(!enabled_, true);
                break;
            case ID_EXPAND:
                SetExpanded(!expanded_, true);
                break;
            case ID_GDOCS:
                LoadGoogleDocsFromPrompt();
                break;
            case ID_HOTKEY:
                ChangePasteHotkey();
                break;
            case ID_GUIDE:
                ShowGuidePopup();
                break;
            case ID_PTS:
                ShowPtsPopup();
                break;
        }
    }

    void ChooseAndLoadFile() {
        bool wasEnabled = enabled_;
        SetEnabled(false, false);

        wchar_t path[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Text and Word files (*.txt;*.docx)\0*.txt;*.docx\0Text files (*.txt)\0*.txt\0Word documents (*.docx)\0*.docx\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Add Text";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameW(&ofn)) {
            LoadFile(path, 0, true);
        }
        SetEnabled(wasEnabled, false);
    }

    void OpenSaved() {
        bool keepEnabled = enabled_;
        bool keepTopMost = topMost_;
        LoadConfig(false);
        enabled_ = keepEnabled;
        topMost_ = keepTopMost;
        if (savedFile_.empty()) {
            SetStatus(L"Chưa có vị trí đã save. Bấm Add Text trước.");
            return;
        }
        LoadFile(savedFile_, savedIndex_, true);
    }

    bool LoadFile(const std::wstring& path, int selectIndex, bool feedback) {
        std::vector<std::wstring> loaded;
        std::wstring error;
        if (!LoadDocumentLines(path, loaded, error)) {
            SetStatus(error);
            MessageBoxW(hwnd_, error.c_str(), L"ToolType", MB_ICONERROR | MB_OK);
            return false;
        }

        lines_.swap(loaded);
        currentFile_ = path;
        selectedIndex_ = 0;
        topIndex_ = 0;
        horizontalOffset_ = 0;
        endOfTextReached_ = false;
        HideTooltip();
        UpdateHorizontalExtent();
        SelectIndex(std::clamp(selectIndex, 0, static_cast<int>(lines_.size()) - 1));

        if (feedback) {
            wchar_t msg[256];
            std::swprintf(msg, 256, L"Đã nạp %u dòng từ %ls.",
                          static_cast<unsigned>(lines_.size()), FileNameOnly(path).c_str());
            SetStatus(msg);
        }
        InvalidateRect(list_, nullptr, TRUE);
        RefreshHookState();
        return true;
    }

    int CurrentIndex() const {
        if (lines_.empty()) return 0;
        return std::clamp(selectedIndex_, 0, static_cast<int>(lines_.size()) - 1);
    }

    bool IsSkippableLine(int idx) const {
        if (idx < 0 || idx >= static_cast<int>(lines_.size())) return true;
        return !IsPasteableTextLine(lines_[static_cast<size_t>(idx)]);
    }

    int FindNextTextLine(int start) const {
        if (lines_.empty()) return -1;
        if (start < 0) start = 0;
        if (start >= static_cast<int>(lines_.size())) return -1;
        for (int i = start; i < static_cast<int>(lines_.size()); ++i) {
            if (!IsSkippableLine(i)) return i;
        }
        return -1;
    }

    int FindPreviousTextLine(int start) const {
        if (lines_.empty()) return -1;
        if (start >= static_cast<int>(lines_.size())) start = static_cast<int>(lines_.size()) - 1;
        if (start < 0) return -1;
        for (int i = start; i >= 0; --i) {
            if (!IsSkippableLine(i)) return i;
        }
        return -1;
    }

    int NormalizeTextIndex(int idx) const {
        int next = FindNextTextLine(idx);
        if (next >= 0) return next;
        return FindPreviousTextLine(idx);
    }

    void SelectIndex(int idx, bool resetEndState = true) {
        if (lines_.empty()) return;
        int normalized = NormalizeTextIndex(idx);
        if (normalized < 0) return;
        selectedIndex_ = normalized;
        if (resetEndState) endOfTextReached_ = false;
        KeepSelectionWithNextLineVisible();
        InvalidateRect(list_, nullptr, FALSE);
    }

    void UpdateHorizontalExtent() {
        int maxWidth = 0;
        HDC hdc = GetDC(list_);
        HFONT old = reinterpret_cast<HFONT>(SelectObject(hdc, font_));
        SIZE size{};
        for (const auto& line : lines_) {
            if (line.empty()) continue;
            if (GetTextExtentPoint32W(hdc, line.c_str(), static_cast<int>(line.size()), &size)) {
                maxWidth = std::max(maxWidth, static_cast<int>(size.cx + 20));
            }
        }
        SelectObject(hdc, old);
        ReleaseDC(list_, hdc);
        maxTextWidth_ = std::min(maxWidth, 30000);
        ClampListState();
        InvalidateRect(list_, nullptr, FALSE);
    }

    bool SetClipboardText(const std::wstring& text) {
        if (!OpenClipboard(hwnd_)) return false;
        EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!mem) {
            CloseClipboard();
            return false;
        }
        void* ptr = GlobalLock(mem);
        if (!ptr) {
            GlobalFree(mem);
            CloseClipboard();
            return false;
        }
        std::memcpy(ptr, text.c_str(), bytes);
        GlobalUnlock(mem);
        if (!SetClipboardData(CF_UNICODETEXT, mem)) {
            GlobalFree(mem);
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return true;
    }

    HWND FocusPasteTarget(HWND targetWindow) {
        HWND target = targetWindow ? targetWindow : GetForegroundWindow();
        if (!target) return nullptr;
        HWND root = GetAncestor(target, GA_ROOT);
        if (!root) root = target;
        if (root == hwnd_ || IsChild(hwnd_, root)) return nullptr;

        DWORD targetThread = GetWindowThreadProcessId(root, nullptr);
        DWORD currentThread = GetCurrentThreadId();
        DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        HWND focusWindow = nullptr;
        GUITHREADINFO gui{};
        gui.cbSize = sizeof(gui);
        if (targetThread && GetGUIThreadInfo(targetThread, &gui)) {
            focusWindow = gui.hwndFocus;
        }
        if (!focusWindow || GetAncestor(focusWindow, GA_ROOT) != root) {
            focusWindow = target;
        }

        bool attachTarget = targetThread && targetThread != currentThread;
        bool attachForeground = foregroundThread && foregroundThread != currentThread && foregroundThread != targetThread;
        if (attachTarget) AttachThreadInput(currentThread, targetThread, TRUE);
        if (attachForeground) AttachThreadInput(currentThread, foregroundThread, TRUE);

        if (IsIconic(root)) ShowWindow(root, SW_RESTORE);
        SetForegroundWindow(root);
        BringWindowToTop(root);
        SetActiveWindow(root);
        SetFocus(focusWindow);

        if (attachForeground) AttachThreadInput(currentThread, foregroundThread, FALSE);
        if (attachTarget) AttachThreadInput(currentThread, targetThread, FALSE);

        return focusWindow;
    }

    bool TryDirectPaste(HWND focusWindow) {
        if (!focusWindow || focusWindow == hwnd_ || IsChild(hwnd_, focusWindow)) return false;

        wchar_t className[128] = {};
        if (!GetClassNameW(focusWindow, className, 128)) return false;
        std::wstring cls(className);
        std::transform(cls.begin(), cls.end(), cls.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });

        bool standardTextBox = cls.find(L"edit") != std::wstring::npos ||
                               cls.find(L"richedit") != std::wstring::npos ||
                               cls.find(L"textbox") != std::wstring::npos;
        if (!standardTextBox) return false;

        DWORD_PTR unused = 0;
        return SendMessageTimeoutW(focusWindow, WM_PASTE, 0, 0,
                                   SMTO_ABORTIFHUNG, 150, &unused) != 0;
    }

    void SendCtrlV(HWND targetWindow) {
        HWND focusWindow = FocusPasteTarget(targetWindow);
        Sleep(35);
        if (TryDirectPaste(focusWindow)) return;

        INPUT input[4]{};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_CONTROL;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = 'V';
        input[2].type = INPUT_KEYBOARD;
        input[2].ki.wVk = 'V';
        input[2].ki.dwFlags = KEYEVENTF_KEYUP;
        input[3].type = INPUT_KEYBOARD;
        input[3].ki.wVk = VK_CONTROL;
        input[3].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, input, sizeof(INPUT));
    }

    void ApplyTopMost(bool feedback) {
        SetWindowPos(hwnd_, topMost_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        InvalidateRect(pinButton_, nullptr, TRUE);
        if (feedback) SetStatus(topMost_ ? L"Đã ghim cửa sổ lên trên cùng." : L"Đã bỏ ghim cửa sổ.");
    }

    void SetEnabled(bool value, bool feedback) {
        enabled_ = value;
        if (!RefreshHookState()) return;
        InvalidateRect(onoffButton_, nullptr, TRUE);
        if (feedback) {
            SetStatus(enabled_ ? L"Đã bật: " + HotkeyLabel() + L" sẽ dán dòng."
                               : L"Đã tắt hoạt động phím tắt.");
        }
    }

    bool ShouldInstallHook() const {
        // Keep the global keyboard hook inactive until it is actually useful.
        // This makes ToolType less intrusive and reduces suspicious idle behavior
        // for heuristic antivirus scanners.
        return enabled_ && !lines_.empty();
    }

    bool RefreshHookState() {
        if (!ShouldInstallHook()) {
            UninstallHook();
            return true;
        }
        if (!InstallHook()) {
            enabled_ = false;
            UninstallHook();
            InvalidateRect(onoffButton_, nullptr, TRUE);
            SetStatus(L"Không bật được phím tắt " + HotkeyLabel() + L".");
            return false;
        }
        return true;
    }

    bool InstallHook() {
        if (hook_) return true;
        hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &ToolTypeApp::KeyboardProc,
                                  GetModuleHandleW(nullptr), 0);
        return hook_ != nullptr;
    }

    void UninstallHook() {
        if (hook_) {
            UnhookWindowsHookEx(hook_);
            hook_ = nullptr;
        }
    }

    void SetStatus(const std::wstring& text) {
        SetWindowTextW(status_, text.c_str());
        InvalidateRect(status_, nullptr, TRUE);
    }

    void HandleUpdateCheckResult(UpdateCheckResult* result) {
        if (!result) return;
        if (result->updateAvailable && IsHttpUrl(result->downloadUrl)) {
            updateDownloadUrl_ = result->downloadUrl;
            SetStatus(kUpdateAvailableMessage);
        } else {
            updateDownloadUrl_.clear();
            SetStatus(kLatestVersionMessage);
        }
        delete result;
    }

    void OpenUpdateDownloadUrl() {
        if (updateDownloadUrl_.empty()) return;
        HINSTANCE opened = ShellExecuteW(hwnd_, L"open", updateDownloadUrl_.c_str(),
                                         nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(opened) <= 32) {
            SetStatus(L"Không mở được link tải bản mới.");
        }
    }

    void ShowEndOfTextPopup() {
        constexpr wchar_t kEndTextMessage[] = L"Đã đến cuối của văn bản";
        SetStatus(kEndTextMessage);
        MessageBoxW(hwnd_, kEndTextMessage, L"ToolType",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }

    std::wstring HotkeyLabel() const {
        return KeyNameForVk(pasteHotkeyVk_);
    }

    void UpdateHotkeyButtonText() {
        if (!hotkeyButton_) return;
        std::wstring label = ShortKeyNameForVk(pasteHotkeyVk_);
        SetWindowTextW(hotkeyButton_, label.c_str());
        InvalidateRect(hotkeyButton_, nullptr, TRUE);
    }

    void UpdateExpandButtonText() {
        if (!expandButton_) return;
        SetWindowTextW(expandButton_, expanded_ ? L"Hide" : L"Expand");
        InvalidateRect(expandButton_, nullptr, TRUE);
    }

    bool IsPasteHotkey(DWORD vkCode) const {
        if (pasteHotkeyVk_ == kDefaultPasteHotkeyVk) {
            return vkCode == VK_TAB || vkCode == VK_F4;
        }
        return vkCode == pasteHotkeyVk_;
    }

    void ChangePasteHotkey() {
        bool wasEnabled = enabled_;
        SetEnabled(false, false);

        DWORD selected = pasteHotkeyVk_;
        if (PromptForHotkey(selected)) {
            pasteHotkeyVk_ = selected;
            UpdateHotkeyButtonText();
            SaveConfig(false);
            SetStatus(L"Đã đổi phím dán: " + HotkeyLabel() + L".");
        }
        SetEnabled(wasEnabled, false);
    }

    void SetExpanded(bool value, bool feedback) {
        if (expanded_ == value) return;
        expanded_ = value;
        UpdateExpandButtonText();
        HideTooltip();

        RECT rc{};
        GetWindowRect(hwnd_, &rc);
        int height = std::max(1, RectHeight(rc));
        int width = expanded_ ? kExpandedWindowWidth : kWindowWidth;
        int x = rc.left;
        int y = rc.top;
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (monitor && GetMonitorInfoW(monitor, &mi)) {
            if (x + width > mi.rcWork.right) x = std::max(mi.rcWork.left, mi.rcWork.right - width);
            if (x < mi.rcWork.left) x = mi.rcWork.left;
            if (y + height > mi.rcWork.bottom) y = std::max(mi.rcWork.top, mi.rcWork.bottom - height);
            if (y < mi.rcWork.top) y = mi.rcWork.top;
        }
        SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        InvalidateRect(hwnd_, nullptr, TRUE);
        if (feedback) {
            SetStatus(expanded_ ? L"Đã mở thêm cột chức năng." : L"Đã ẩn cột chức năng.");
        }
    }

    bool PromptForText(const wchar_t* title, const wchar_t* label, std::wstring& value,
                       bool validateGoogleDocsLink = false) {
        WNDCLASSW wc{};
        if (!GetClassInfoW(instance_, L"ToolTypeInputDialog", &wc)) {
            wc.lpfnWndProc = &ToolTypeApp::InputDialogWndProc;
            wc.hInstance = instance_;
            wc.lpszClassName = L"ToolTypeInputDialog";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            if (!RegisterClassW(&wc)) return false;
        }

        InputDialogState state{};
        state.app = this;
        state.kind = InputDialogState::Kind::TextInput;
        state.value = value;
        state.validateGoogleDocsLink = validateGoogleDocsLink;

        RECT owner{};
        GetWindowRect(hwnd_, &owner);
        int width = 500;
        int height = 154;
        int x = owner.left + (RectWidth(owner) - width) / 2;
        int y = owner.top + (RectHeight(owner) - height) / 2;
        DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED | (topMost_ ? WS_EX_TOPMOST : 0);
        HWND dialog = CreateWindowExW(exStyle, L"ToolTypeInputDialog", title,
                                      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER,
                                      x, y, width, height, hwnd_, nullptr, instance_, &state);
        if (!dialog) return false;
        SetLayeredWindowAttributes(dialog, 0, kWindowAlpha, LWA_ALPHA);

        HWND labelWnd = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                        14, 14, 468, 22, dialog, nullptr, instance_, nullptr);
        state.edit = CreateWindowExW(0, L"EDIT", value.c_str(),
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                     14, 43, 468, 25, dialog, nullptr, instance_, nullptr);
        HWND ok = CreateWindowExW(0, L"BUTTON", L"OK",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                                  302, 84, 84, 30, dialog, reinterpret_cast<HMENU>(IDOK),
                                  instance_, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"Hủy",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                      398, 84, 84, 30, dialog, reinterpret_cast<HMENU>(IDCANCEL),
                                      instance_, nullptr);
        for (HWND control : {labelWnd, state.edit, ok, cancel}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        }

        EnableWindow(hwnd_, FALSE);
        ShowWindow(dialog, SW_SHOW);
        SetFocus(state.edit);
        SendMessageW(state.edit, EM_SETSEL, 0, -1);

        RunPtsDialogMessageLoop(dialog);

        EnableWindow(hwnd_, TRUE);
        SetForegroundWindow(hwnd_);
        if (state.accepted) value = state.value;
        return state.accepted;
    }

    bool PromptForHotkey(DWORD& value) {
        WNDCLASSW wc{};
        if (!GetClassInfoW(instance_, L"ToolTypeInputDialog", &wc)) {
            wc.lpfnWndProc = &ToolTypeApp::InputDialogWndProc;
            wc.hInstance = instance_;
            wc.lpszClassName = L"ToolTypeInputDialog";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            if (!RegisterClassW(&wc)) return false;
        }

        InputDialogState state{};
        state.app = this;
        state.kind = InputDialogState::Kind::KeyCapture;
        state.capturedVk = value;

        RECT owner{};
        GetWindowRect(hwnd_, &owner);
        int width = 430;
        int height = 168;
        int x = owner.left + (RectWidth(owner) - width) / 2;
        int y = owner.top + (RectHeight(owner) - height) / 2;
        DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED | (topMost_ ? WS_EX_TOPMOST : 0);
        HWND dialog = CreateWindowExW(exStyle, L"ToolTypeInputDialog", L"Chọn phím dán",
                                      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER,
                                      x, y, width, height, hwnd_, nullptr, instance_, &state);
        if (!dialog) return false;
        SetLayeredWindowAttributes(dialog, 0, kWindowAlpha, LWA_ALPHA);

        std::wstring info = L"Phím hiện tại: " + HotkeyLabel() +
                            L"\nNhấn phím bất kỳ để chọn. Esc để hủy.";
        state.info = CreateWindowExW(0, L"STATIC", info.c_str(),
                                     WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     16, 16, 398, 46, dialog, nullptr, instance_, nullptr);
        HWND defaultButton = CreateWindowExW(0, L"BUTTON", L"Mặc định Tab/F4",
                                             WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                             106, 84, 140, 30, dialog,
                                             reinterpret_cast<HMENU>(ID_DEFAULT_HOTKEY),
                                             instance_, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"Hủy",
                                      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      258, 84, 80, 30, dialog,
                                      reinterpret_cast<HMENU>(IDCANCEL),
                                      instance_, nullptr);
        for (HWND control : {state.info, defaultButton, cancel}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        }

        EnableWindow(hwnd_, FALSE);
        ShowWindow(dialog, SW_SHOW);
        SetFocus(dialog);

        MSG msg{};
        while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        EnableWindow(hwnd_, TRUE);
        SetForegroundWindow(hwnd_);
        if (state.accepted) value = state.capturedVk;
        return state.accepted;
    }

    std::wstring GoogleDocsExportUrl(const std::wstring& rawLink) const {
        std::wstring link = TrimCopy(rawLink);
        while (!link.empty() && (link.front() == L'"' || link.front() == L'\'')) link.erase(link.begin());
        while (!link.empty() && (link.back() == L'"' || link.back() == L'\'')) link.pop_back();

        if (link.find(L"http://") != 0 && link.find(L"https://") != 0) return L"";
        if (link.find(L"docs.google.com") == std::wstring::npos ||
            link.find(L"/document/") == std::wstring::npos) {
            return L"";
        }
        if (link.find(L"/export") != std::wstring::npos &&
            link.find(L"format=txt") != std::wstring::npos) {
            return link;
        }

        const std::wstring publishedMarker = L"/document/d/e/";
        size_t published = link.find(publishedMarker);
        if (published != std::wstring::npos) {
            size_t idStart = published + publishedMarker.size();
            size_t idEnd = link.find_first_of(L"/?#", idStart);
            std::wstring id = link.substr(idStart, idEnd == std::wstring::npos ? std::wstring::npos : idEnd - idStart);
            if (id.empty()) return L"";
            return L"https://docs.google.com/document/d/e/" + id + L"/pub?output=txt";
        }

        const std::wstring standardMarker = L"/document/d/";
        size_t standard = link.find(standardMarker);
        if (standard != std::wstring::npos) {
            size_t idStart = standard + standardMarker.size();
            size_t idEnd = link.find_first_of(L"/?#", idStart);
            std::wstring id = link.substr(idStart, idEnd == std::wstring::npos ? std::wstring::npos : idEnd - idStart);
            if (id.empty() || id == L"e") return L"";
            return L"https://docs.google.com/document/d/" + id + L"/export?format=txt";
        }
        return L"";
    }

    bool DownloadUrlBytes(const std::wstring& url, std::vector<uint8_t>& out, std::wstring& error) {
        HINTERNET internet = InternetOpenW(L"ToolType/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                           nullptr, nullptr, 0);
        if (!internet) {
            error = L"Không khởi tạo được kết nối mạng.";
            return false;
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                      INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE;
        HINTERNET request = InternetOpenUrlW(internet, url.c_str(), nullptr, 0, flags, 0);
        if (!request) {
            InternetCloseHandle(internet);
            error = L"Không mở được link Google Docs. Hãy kiểm tra link và quyền chia sẻ.";
            return false;
        }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                           &status, &statusSize, nullptr) && status >= 400) {
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            if (status == 401 || status == 403 || status == 404) {
                error = L"Link Google Docs sai hoặc chưa công khai.";
            } else {
                error = L"Không tải được Google Docs. Hãy kiểm tra lại link hoặc kết nối mạng.";
            }
            return false;
        }

        out.clear();
        uint8_t buffer[16 * 1024];
        for (;;) {
            DWORD read = 0;
            if (!InternetReadFile(request, buffer, sizeof(buffer), &read)) {
                InternetCloseHandle(request);
                InternetCloseHandle(internet);
                error = L"Lỗi khi tải nội dung Google Docs.";
                return false;
            }
            if (read == 0) break;
            if (out.size() + read > 16u * 1024u * 1024u) {
                InternetCloseHandle(request);
                InternetCloseHandle(internet);
                error = L"Nội dung Google Docs quá lớn.";
                return false;
            }
            out.insert(out.end(), buffer, buffer + read);
        }

        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        if (out.empty()) {
            error = L"Google Docs không trả về nội dung.";
            return false;
        }
        return true;
    }

    bool LooksLikeHtml(const std::wstring& text) const {
        std::wstring head = text.substr(0, std::min<size_t>(text.size(), 1024));
        std::transform(head.begin(), head.end(), head.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        return head.find(L"<html") != std::wstring::npos ||
               head.find(L"<!doctype") != std::wstring::npos ||
               head.find(L"<body") != std::wstring::npos;
    }

    bool LoadTextContent(const std::wstring& sourceName, const std::wstring& text,
                         int selectIndex, bool feedback, std::wstring& error) {
        std::vector<std::wstring> loaded = SplitLines(text);
        bool hasPasteableLine = std::any_of(loaded.begin(), loaded.end(), [](const std::wstring& line) {
            return IsPasteableTextLine(line);
        });
        if (!hasPasteableLine) {
            error = sourceName + L" không có dòng văn bản nào.";
            return false;
        }

        lines_.swap(loaded);
        currentFile_.clear();
        selectedIndex_ = 0;
        topIndex_ = 0;
        horizontalOffset_ = 0;
        endOfTextReached_ = false;
        HideTooltip();
        UpdateHorizontalExtent();
        SelectIndex(std::clamp(selectIndex, 0, static_cast<int>(lines_.size()) - 1));

        if (feedback) {
            wchar_t msg[256];
            std::swprintf(msg, 256, L"Đã nạp %u dòng từ %ls.",
                          static_cast<unsigned>(lines_.size()), sourceName.c_str());
            SetStatus(msg);
        }
        InvalidateRect(list_, nullptr, TRUE);
        RefreshHookState();
        return true;
    }

    void LoadGoogleDocsFromPrompt() {
        bool wasEnabled = enabled_;
        SetEnabled(false, false);

        std::wstring link;
        bool accepted = PromptForText(L"Thêm text từ Google Docs",
                                      L"Dán link Google Docs đã chia sẻ công khai:", link, true);
        if (!accepted) {
            SetEnabled(wasEnabled, false);
            return;
        }

        std::wstring exportUrl = GoogleDocsExportUrl(link);
        if (exportUrl.empty()) {
            SetEnabled(wasEnabled, false);
            std::wstring error = L"Link Google Docs không hợp lệ.";
            SetStatus(error);
            MessageBoxW(hwnd_, error.c_str(), L"ToolType", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            return;
        }

        SetStatus(L"Đang tải Google Docs...");
        std::vector<uint8_t> bytes;
        std::wstring error;
        if (!DownloadUrlBytes(exportUrl, bytes, error)) {
            SetEnabled(wasEnabled, false);
            SetStatus(error);
            MessageBoxW(hwnd_, error.c_str(), L"ToolType", MB_OK | MB_ICONERROR | MB_TOPMOST);
            return;
        }

        std::wstring text = DecodeTextBytes(bytes);
        if (LooksLikeHtml(text)) {
            SetEnabled(wasEnabled, false);
            error = L"Không lấy được bản text. Hãy bật chia sẻ công khai hoặc dùng link Google Docs chuẩn.";
            SetStatus(error);
            MessageBoxW(hwnd_, error.c_str(), L"ToolType", MB_OK | MB_ICONERROR | MB_TOPMOST);
            return;
        }

        if (!LoadTextContent(L"Google Docs", text, 0, true, error)) {
            SetEnabled(wasEnabled, false);
            SetStatus(error);
            MessageBoxW(hwnd_, error.c_str(), L"ToolType", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            return;
        }
        SetEnabled(wasEnabled, false);
    }

    void ShowGuidePopup() {
        bool wasEnabled = enabled_;
        SetEnabled(false, false);
        std::wstring guide =
            L"Hướng dẫn sử dụng ToolType\n\n"
            L"1. Add Text: nạp file .txt hoặc .docx.\n"
            L"2. Expand: mở thêm cột chức năng.\n"
            L"3. GDocs: dán link Google Docs công khai để nạp text.\n"
            L"4. Nút phím dán: bấm rồi nhấn phím bất kỳ để chọn phím dán. Mặc định là Tab/F4.\n"
            L"5. On/Off: bật hoặc tắt phím dán nhanh.\n"
            L"6. Khi bật, nhấn " + HotkeyLabel() + L" ở cửa sổ khác để dán dòng đang chọn rồi tự nhảy dòng.\n"
            L"7. Dòng trống và dòng bắt đầu bằng // vẫn hiển thị nhưng sẽ bị bỏ qua khi dán.\n"
            L"8. Dòng bắt đầu bằng *, -, > sẽ được bỏ ký tự đánh dấu và chuẩn hóa khoảng trắng khi dán.\n"
            L"9. Save/Open lưu và mở lại vị trí file hiện tại.\n"
            L"10. Pts: sao lưu/khôi phục cài đặt Photoshop và font tùy chỉnh bằng file .afang.";
        MessageBoxW(hwnd_, guide.c_str(), L"Hướng dẫn ToolType",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
        SetEnabled(wasEnabled, false);
    }

    void PumpPtsDialogMessages(HWND dialog) {
        MSG quit{};
        if (PeekMessageW(&quit, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE)) return;
        MSG msg{};
        while (PeekMessageW(&msg, dialog, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(msg.wParam));
                break;
            }
            if (!dialog || !IsWindow(dialog) || !IsDialogMessageW(dialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    void UpdatePtsProgress(PtsDialogState* state, int percent, const std::wstring& message,
                           bool pumpMessages = true) {
        if (!state || !state->dialog) return;
        int nextProgress = std::clamp(percent, 0, 100);
        ULONGLONG now = GetTickCount64();
        bool terminal = nextProgress == 0 || nextProgress == 100;
        bool changedPercent = nextProgress != state->progress;
        bool dueForPaint = terminal || changedPercent ||
                           now - state->lastProgressPaint >= 75;
        state->progress = nextProgress;
        if (!dueForPaint) return;

        std::wstring compactMessage = CompactPtsNotice(message);
        state->lastProgressPaint = now;
        state->lastProgressMessage = compactMessage;
        if (state->status) SetWindowTextW(state->status, compactMessage.c_str());
        if (state->percent) {
            std::wstring pct = std::to_wstring(state->progress) + L"%";
            SetWindowTextW(state->percent, pct.c_str());
        }
        InvalidateRect(state->dialog, &state->progressRect, TRUE);
        UpdateWindow(state->dialog);
        if (state->status) UpdateWindow(state->status);
        if (state->percent) UpdateWindow(state->percent);
        if (pumpMessages) PumpPtsDialogMessages(state->dialog);
    }

    void SetPtsDialogBusy(PtsDialogState* state, bool busy) {
        if (!state) return;
        state->busy = busy;
        for (HWND button : {state->backupSettings, state->backupFonts, state->backupAll,
                            state->restore}) {
            if (button) EnableWindow(button, busy ? FALSE : TRUE);
        }
        if (state->close) {
            EnableWindow(state->close, TRUE);
            SetWindowTextW(state->close, busy ? L"Hủy" : L"Đóng");
        }
        if (!busy) {
            state->cancelRequested = false;
            state->cancellation.reset();
            state->operationKind = static_cast<int>(PtsOperationKind::None);
        }
    }

    bool StartPtsUiBackgroundOperation(PtsDialogState* state, PtsOperationKind kind,
                                       const PtsBackgroundOperation& operation,
                                       const std::wstring& startMessage) {
        if (!state || !state->dialog || state->busy) return false;
        SetPtsDialogBusy(state, true);
        state->operationKind = static_cast<int>(kind);
        PtsCancellationHandle cancellation;
        try {
            cancellation = CreatePtsCancellationHandle();
            state->cancellation = cancellation;
        } catch (...) {
            SetPtsDialogBusy(state, false);
            UpdatePtsProgress(state, 0, L"Không đủ bộ nhớ để bắt đầu thao tác Pts.");
            return false;
        }
        UpdatePtsProgress(state, 0, startMessage, false);

        std::wstring startError;
        if (!StartPtsBackgroundTask(state->dialog, operation, cancellation,
                                    state->workerThread, startError)) {
            SetPtsDialogBusy(state, false);
            UpdatePtsProgress(state, 0, startError, false);
            MessageBoxW(state->dialog, startError.c_str(), L"Pts",
                        MB_OK | MB_ICONERROR | MB_SETFOREGROUND |
                            (topMost_ ? MB_TOPMOST : 0));
            return false;
        }
        return true;
    }

    void HandlePtsBackgroundProgress(PtsDialogState* state,
                                     const PtsProgressMessage& update) {
        if (!state || !state->busy || state->cancelRequested) return;
        UpdatePtsProgress(state, update.percent, update.message, false);
    }

    void RequestPtsOperationCancellation(PtsDialogState* state) {
        if (!state || !state->busy || state->cancelRequested) return;
        PtsCancellationHandle cancellation =
            std::static_pointer_cast<PtsCancellationState>(state->cancellation);
        if (!RequestPtsCancellation(cancellation)) {
            if (state->close) {
                SetWindowTextW(state->close, L"Hoàn tất");
                EnableWindow(state->close, FALSE);
            }
            UpdatePtsProgress(state, state->progress,
                              L"Đang hoàn tất file sao lưu; không thể hủy ở bước cuối.",
                              false);
            SetStatus(L"Pts: đang hoàn tất bước cuối...");
            return;
        }
        state->cancelRequested = true;
        if (state->close) {
            SetWindowTextW(state->close, L"Đang dừng");
            EnableWindow(state->close, FALSE);
        }
        UpdatePtsProgress(state, state->progress,
                          L"Đang dừng an toàn sau tệp hiện tại...", false);
        SetStatus(L"Pts: đang dừng thao tác...");
    }

    void HandlePtsBackgroundCompletion(PtsDialogState* state,
                                       const PtsBackgroundResult& result) {
        if (!state) return;
        if (state->workerThread) {
            WaitForSingleObject(state->workerThread, INFINITE);
            CloseHandle(state->workerThread);
            state->workerThread = nullptr;
        }
        PtsOperationKind kind = static_cast<PtsOperationKind>(state->operationKind);
        SetPtsDialogBusy(state, false);

        if (result.cancelled) {
            std::wstring message = result.error.empty()
                                       ? L"Đã hủy thao tác."
                                       : result.error;
            UpdatePtsProgress(state, state->progress, message, false);
            SetStatus(kind == PtsOperationKind::Backup
                          ? L"Pts: đã hủy sao lưu."
                          : L"Pts: đã dừng khôi phục.");
            return;
        }

        const bool backup = kind == PtsOperationKind::Backup;
        const wchar_t* title = backup ? L"Sao lưu Pts" : L"Khôi phục Pts";
        if (result.ok) {
            const std::wstring summary = result.summary.empty()
                                             ? (backup ? L"Đã sao lưu xong."
                                                       : L"Đã khôi phục xong.")
                                             : result.summary;
            UpdatePtsProgress(state, 100, summary, false);
            SetStatus(backup ? L"Pts: đã tạo file sao lưu .afang."
                             : L"Pts: đã khôi phục xong.");
            MessageBoxW(state->dialog, summary.c_str(), title,
                        MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND |
                            (topMost_ ? MB_TOPMOST : 0));
        } else {
            const std::wstring error = result.error.empty()
                                           ? L"Thao tác Pts thất bại."
                                           : result.error;
            UpdatePtsProgress(state, state->progress, error, false);
            SetStatus(backup ? L"Pts: sao lưu thất bại."
                             : L"Pts: khôi phục thất bại.");
            MessageBoxW(state->dialog, error.c_str(), title,
                        MB_OK | MB_ICONERROR | MB_SETFOREGROUND |
                            (topMost_ ? MB_TOPMOST : 0));
        }
    }

    std::wstring EnsureAfangExtension(std::wstring path) const {
        if (ExtensionLower(path) != L".afang") path += L".afang";
        return path;
    }

    bool PromptForAfangSave(HWND owner, const wchar_t* title, const wchar_t* defaultName,
                            std::wstring& path) {
        wchar_t buffer[MAX_PATH] = {};
        std::wcsncpy(buffer, defaultName, MAX_PATH - 1);
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner ? owner : hwnd_;
        ofn.lpstrFilter = L"File sao lưu Afang (*.afang)\0*.afang\0Tất cả file (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.lpstrDefExt = L"afang";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&ofn)) return false;
        path = EnsureAfangExtension(buffer);
        return true;
    }

    bool PromptForAfangOpen(HWND owner, std::wstring& path) {
        wchar_t buffer[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner ? owner : hwnd_;
        ofn.lpstrFilter = L"File sao lưu Afang (*.afang)\0*.afang\0Tất cả file (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Khôi phục từ file .afang";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn)) return false;
        path = buffer;
        return true;
    }

    bool PromptForChoice(HWND owner, const wchar_t* title, const wchar_t* prompt,
                         const std::vector<std::wstring>& options, int& selectedIndex) {
        if (options.empty()) return false;
        if (options.size() == 1) {
            selectedIndex = 0;
            return true;
        }

        WNDCLASSW wc{};
        if (!GetClassInfoW(instance_, L"ToolTypeChoiceDialog", &wc)) {
            wc.lpfnWndProc = &ToolTypeApp::ChoiceDialogWndProc;
            wc.hInstance = instance_;
            wc.lpszClassName = L"ToolTypeChoiceDialog";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            if (!RegisterClassW(&wc)) return false;
        }

        HWND parent = owner ? owner : hwnd_;
        ChoiceDialogState state{};
        state.app = this;
        state.title = title ? title : L"ToolType";
        state.prompt = prompt ? prompt : L"Chọn một mục:";
        state.options = options;
        state.selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(options.size()) - 1);

        RECT ownerRc{};
        GetWindowRect(parent, &ownerRc);
        const UINT dpi = QueryPtsDialogDpi(parent);
        auto scaled = [dpi](int value) { return ScalePtsMetric(value, dpi); };
        int width = scaled(520);
        int baseListHeight = std::clamp(32 + static_cast<int>(options.size()) * 22, 96, 220);
        int listHeight = scaled(baseListHeight);
        int height = scaled(baseListHeight + 144);
        int x = ownerRc.left + (RectWidth(ownerRc) - width) / 2;
        int y = ownerRc.top + (RectHeight(ownerRc) - height) / 2;
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        HMONITOR monitor = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
        if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
            int maxX = std::max(monitorInfo.rcWork.left, monitorInfo.rcWork.right - width);
            int maxY = std::max(monitorInfo.rcWork.top, monitorInfo.rcWork.bottom - height);
            x = std::clamp(x, static_cast<int>(monitorInfo.rcWork.left), maxX);
            y = std::clamp(y, static_cast<int>(monitorInfo.rcWork.top), maxY);
        }
        DWORD exStyle = PtsDialogExtendedStyle(topMost_);
        HWND dialog = CreateWindowExW(exStyle, L"ToolTypeChoiceDialog", state.title.c_str(),
                                      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER,
                                      x, y, width, height, parent, nullptr, instance_, &state);
        if (!dialog) return false;
        HFONT dialogFont = CreatePtsDialogFont(font_, dpi);
        HFONT controlFont = dialogFont ? dialogFont : font_;
        HWND labelWnd = CreateWindowExW(0, L"STATIC", state.prompt.c_str(),
                                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                        scaled(14), scaled(14), width - scaled(28), scaled(40),
                                        dialog, nullptr, instance_, nullptr);
        state.list = CreateWindowExW(0, L"LISTBOX", nullptr,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                         WS_VSCROLL | LBS_NOTIFY,
                                     scaled(14), scaled(60), width - scaled(28), listHeight,
                                     dialog, nullptr, instance_, nullptr);
        for (const auto& option : options) {
            SendMessageW(state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option.c_str()));
        }
        SendMessageW(state.list, LB_SETCURSEL, static_cast<WPARAM>(state.selectedIndex), 0);

        int buttonY = scaled(72) + listHeight;
        HWND ok = CreateWindowExW(0, L"BUTTON", L"OK",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                                  width - scaled(206), buttonY, scaled(84), scaled(30), dialog,
                                  reinterpret_cast<HMENU>(IDOK),
                                  instance_, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"Hủy",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                      width - scaled(110), buttonY, scaled(84), scaled(30), dialog,
                                      reinterpret_cast<HMENU>(IDCANCEL), instance_, nullptr);
        for (HWND control : {labelWnd, state.list, ok, cancel}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), TRUE);
        }

        EnableWindow(parent, FALSE);
        ShowWindow(dialog, SW_SHOW);
        SetFocus(state.list);

        MSG msg{};
        BOOL getMessageResult = TRUE;
        while (IsWindow(dialog) &&
               (getMessageResult = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
            if (!IsDialogMessageW(dialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        if (getMessageResult == 0) {
            if (IsWindow(dialog)) DestroyWindow(dialog);
            PostQuitMessage(static_cast<int>(msg.wParam));
        }

        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
        if (state.accepted) selectedIndex = state.selectedIndex;
        if (dialogFont) DeleteObject(dialogFont);
        return state.accepted;
    }

    void ApplyBackupVersionSelection(const PtsPhotoshopVersion& version,
                                     PtsBackupOptions& options) const {
        options.selectedPhotoshopVersionLabel = version.label;
        options.selectedPhotoshopVersionKeys.clear();
        options.selectedPhotoshopVersionLabels.clear();
        if (!version.label.empty()) {
            options.selectedPhotoshopVersionLabels.insert(LowerWide(version.label));
        }
        for (const auto& root : version.roots) {
            options.selectedPhotoshopVersionKeys.insert(root.versionKey);
        }
    }

    void ApplyRestoreTargetVersion(const PtsPhotoshopVersion& version,
                                   PtsRestoreOptions& options) const {
        options.targetVersionLabel = version.label;
        options.targetAppDataRelativeRoot = RootForToken(version, L"APPDATA");
        options.targetLocalAppDataRelativeRoot = RootForToken(version, L"LOCALAPPDATA");
        if (options.targetAppDataRelativeRoot.empty()) {
            options.targetAppDataRelativeRoot = DefaultPhotoshopRelativeRootForLabel(version.label);
        }
        if (options.targetLocalAppDataRelativeRoot.empty()) {
            options.targetLocalAppDataRelativeRoot = DefaultPhotoshopRelativeRootForLabel(version.label);
        }
    }

    std::wstring PhotoshopVersionChoiceLabel(const std::wstring& label, size_t) const {
        return label;
    }

    bool PrepareBackupOptions(HWND owner, bool includeSettings, PtsBackupOptions& options) {
        options = PtsBackupOptions{};
        if (!includeSettings) return true;

        std::vector<PtsPhotoshopVersion> versions = DiscoverPhotoshopVersions();
        if (versions.empty()) return true;
        if (versions.size() == 1) {
            ApplyBackupVersionSelection(versions.front(), options);
            return true;
        }

        std::vector<std::wstring> choices;
        for (const auto& version : versions) {
            choices.push_back(PhotoshopVersionChoiceLabel(version.label, version.roots.size()));
        }

        int selected = 0;
        if (!PromptForChoice(owner, L"Chọn phiên bản Photoshop",
                             L"Máy có nhiều phiên bản Photoshop. Chọn phiên bản muốn sao lưu:",
                             choices, selected)) {
            return false;
        }
        ApplyBackupVersionSelection(versions[static_cast<size_t>(selected)], options);
        return true;
    }

    bool PrepareRestoreOptions(HWND owner, const std::wstring& path, PtsRestoreOptions& options,
                               std::wstring& error,
                               const PtsProgressCallback& progress = {},
                               const PtsCancellationCallback& cancelled = {}) {
        options = PtsRestoreOptions{};
        error.clear();

        PtsArchiveScanResult scan{};
        if (!InspectPtsArchiveVersions(path, scan, error, progress, cancelled)) return false;
        options.archiveHasSettings = scan.hasSettings;
        options.archiveValidated = true;
        if (!scan.hasSettings) return true;
        if (scan.versions.empty()) {
            error = L"File .afang không có thông tin phiên bản Photoshop để khôi phục cài đặt.";
            return false;
        }

        std::sort(scan.versions.begin(), scan.versions.end(),
                  [](const PtsArchiveVersion& a, const PtsArchiveVersion& b) {
                      return LowerWide(a.label) < LowerWide(b.label);
                  });

        if (scan.versions.size() > 1) {
            std::vector<std::wstring> choices;
            for (const auto& version : scan.versions) {
                choices.push_back(PhotoshopVersionChoiceLabel(version.label, version.rootKeys.size()));
            }
            int selected = 0;
            if (!PromptForChoice(owner, L"Chọn phiên bản trong .afang",
                                 L"File sao lưu có nhiều phiên bản Photoshop. Chọn phiên bản cài đặt muốn khôi phục:",
                                 choices, selected)) {
                return false;
            }
            const auto& source = scan.versions[static_cast<size_t>(selected)];
            options.sourceVersionLabel = source.label;
            options.sourceVersionKeys = source.rootKeys;
        } else {
            options.sourceVersionLabel = scan.versions.front().label;
        }

        std::vector<PtsPhotoshopVersion> installed = DiscoverPhotoshopVersions();
        if (installed.size() > 1) {
            std::vector<std::wstring> choices;
            for (const auto& version : installed) {
                choices.push_back(PhotoshopVersionChoiceLabel(version.label, version.roots.size()));
            }
            int selected = 0;
            if (!PromptForChoice(owner, L"Chọn Photoshop đích",
                                 L"Máy có nhiều phiên bản Photoshop. Chọn phiên bản trên máy để khôi phục vào:",
                                 choices, selected)) {
                return false;
            }
            ApplyRestoreTargetVersion(installed[static_cast<size_t>(selected)], options);
        } else if (installed.size() == 1) {
            ApplyRestoreTargetVersion(installed.front(), options);
        }
        return true;
    }

    void RunPtsBackup(PtsDialogState* state, bool includeSettings, bool includeFonts,
                      const wchar_t* defaultName) {
        UpdatePtsProgress(state, 0, L"Đang chuẩn bị sao lưu...", false);
        if (includeSettings && IsPhotoshopRunning()) {
            std::wstring error = L"Photoshop đang chạy. Hãy đóng Photoshop trước khi sao lưu "
                                 L"để workspace, bố cục và tùy chọn hiện tại được ghi đầy đủ xuống đĩa.";
            UpdatePtsProgress(state, 0, error);
            MessageBoxW(state ? state->dialog : hwnd_, error.c_str(), L"Sao lưu Pts",
                        MB_OK | MB_ICONWARNING | MB_SETFOREGROUND |
                            (topMost_ ? MB_TOPMOST : 0));
            return;
        }

        PtsBackupOptions options;
        if (!PrepareBackupOptions(state ? state->dialog : hwnd_, includeSettings, options)) {
            UpdatePtsProgress(state, 0, L"Đã hủy chọn phiên bản Photoshop.");
            return;
        }

        std::wstring path;
        if (!PromptForAfangSave(state ? state->dialog : hwnd_, L"Lưu file sao lưu .afang", defaultName, path)) {
            UpdatePtsProgress(state, 0, L"Đã hủy chọn file sao lưu.");
            return;
        }

        std::shared_ptr<void> backupOptions;
        try {
            backupOptions = std::make_shared<PtsBackupOptions>(options);
        } catch (...) {
            UpdatePtsProgress(state, 0, L"Không đủ bộ nhớ để bắt đầu sao lưu.");
            return;
        }
        PtsBackgroundOperation operation =
            [path, includeSettings, includeFonts, backupOptions](
                const PtsProgressCallback& progress,
                const PtsCancellationCallback& cancelled,
                const PtsCommitCallback& beginCommit,
                std::wstring& summary, std::wstring& error) {
                auto typedOptions = std::static_pointer_cast<PtsBackupOptions>(backupOptions);
                return CreatePtsBackupArchive(path, includeSettings, includeFonts, *typedOptions,
                                              progress, summary, error, cancelled,
                                              PtsPhotoshopRunningCallback{}, beginCommit);
            };
        StartPtsUiBackgroundOperation(state, PtsOperationKind::Backup, operation,
                                       L"Đang sao lưu. Có thể bấm Hủy trước bước hoàn tất cuối.");
    }

    void RunPtsRestore(PtsDialogState* state) {
        UpdatePtsProgress(state, 0, L"Chọn file .afang cần khôi phục...", false);
        std::wstring path;
        if (!PromptForAfangOpen(state ? state->dialog : hwnd_, path)) {
            UpdatePtsProgress(state, 0, L"Đã hủy chọn file khôi phục.");
            return;
        }

        PtsRestoreOptions options;
        std::wstring prepareError;
        SetPtsDialogBusy(state, true);
        state->operationKind = static_cast<int>(PtsOperationKind::Restore);
        try {
            state->cancellation = CreatePtsCancellationHandle();
        } catch (...) {
            SetPtsDialogBusy(state, false);
            UpdatePtsProgress(state, 0, L"Không đủ bộ nhớ để kiểm tra file .afang.");
            return;
        }
        auto progress = [this, state](int percent, const std::wstring& message) {
            UpdatePtsProgress(state, percent, message);
        };
        std::shared_ptr<void> preflightCancellation = state->cancellation;
        PtsCancellationCallback cancelled = [preflightCancellation] {
            return IsPtsCancellationRequested(
                std::static_pointer_cast<PtsCancellationState>(preflightCancellation));
        };
        UpdatePtsProgress(state, state ? state->progress : 0, L"Đang kiểm tra file .afang...");
        if (!PrepareRestoreOptions(state ? state->dialog : hwnd_, path, options, prepareError,
                                   progress, cancelled)) {
            bool wasCancelled = state->cancelRequested || cancelled();
            SetPtsDialogBusy(state, false);
            if (wasCancelled) {
                UpdatePtsProgress(state, 0, L"Đã hủy khôi phục; chưa có thay đổi nào được áp dụng.", false);
                SetStatus(L"Pts: đã hủy khôi phục.");
                return;
            }
            if (prepareError.empty()) {
                UpdatePtsProgress(state, 0, L"Đã hủy chọn phiên bản khôi phục.");
            } else {
                UpdatePtsProgress(state, 0, prepareError);
                MessageBoxW(state->dialog, prepareError.c_str(), L"Khôi phục Pts",
                            MB_OK | MB_ICONERROR | MB_SETFOREGROUND |
                                (topMost_ ? MB_TOPMOST : 0));
            }
            return;
        }

        if (state->cancelRequested || cancelled()) {
            SetPtsDialogBusy(state, false);
            UpdatePtsProgress(state, 0, L"Đã hủy khôi phục; chưa có thay đổi nào được áp dụng.", false);
            SetStatus(L"Pts: đã hủy khôi phục.");
            return;
        }

        if (options.archiveHasSettings && IsPhotoshopRunning()) {
            std::wstring error = L"Photoshop đang chạy. Hãy đóng Photoshop trước khi khôi phục "
                                 L"để bố cục, tùy chọn và trạng thái công cụ không bị ghi đè "
                                 L"khi Photoshop thoát.";
            UpdatePtsProgress(state, 0, error);
            MessageBoxW(state->dialog, error.c_str(), L"Khôi phục Pts",
                        MB_OK | MB_ICONWARNING | MB_SETFOREGROUND |
                             (topMost_ ? MB_TOPMOST : 0));
            SetPtsDialogBusy(state, false);
            return;
        }

        SetPtsDialogBusy(state, false);
        std::shared_ptr<void> restoreOptions;
        try {
            restoreOptions = std::make_shared<PtsRestoreOptions>(options);
        } catch (...) {
            UpdatePtsProgress(state, 0, L"Không đủ bộ nhớ để bắt đầu khôi phục.");
            return;
        }
        PtsBackgroundOperation operation =
            [path, restoreOptions](const PtsProgressCallback& workerProgress,
                            const PtsCancellationCallback& workerCancelled,
                            const PtsCommitCallback&,
                            std::wstring& summary, std::wstring& error) {
                auto typedOptions = std::static_pointer_cast<PtsRestoreOptions>(restoreOptions);
                return RestorePtsBackupArchive(path, *typedOptions, workerProgress,
                                               summary, error, workerCancelled);
            };
        StartPtsUiBackgroundOperation(state, PtsOperationKind::Restore, operation,
                                       L"Đang khôi phục. Bấm Hủy để dừng; thay đổi đã áp dụng không tự hoàn tác.");
    }

    void HandlePtsDialogCommand(PtsDialogState* state, int id) {
        if (!state) return;
        if (id == IDCANCEL) {
            if (state->busy) {
                RequestPtsOperationCancellation(state);
            } else {
                DestroyWindow(state->dialog);
            }
            return;
        }
        if (state->busy) return;
        switch (id) {
            case ID_PTS_BACKUP_SETTINGS:
                RunPtsBackup(state, true, false, L"photoshop-settings.afang");
                break;
            case ID_PTS_BACKUP_FONTS:
                RunPtsBackup(state, false, true, L"photoshop-fonts.afang");
                break;
            case ID_PTS_BACKUP_ALL:
                RunPtsBackup(state, true, true, L"photoshop-all.afang");
                break;
            case ID_PTS_RESTORE:
                RunPtsRestore(state);
                break;
        }
    }

    HWND CreatePtsDialogButton(HWND dialog, const wchar_t* text, int id, int x, int y, int w, int h) {
        HWND button = CreateWindowExW(0, L"BUTTON", text,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                      x, y, w, h, dialog, reinterpret_cast<HMENU>(id),
                                      instance_, nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return button;
    }

    void ShowPtsPopup() {
        WNDCLASSW wc{};
        if (!GetClassInfoW(instance_, L"ToolTypePtsDialog", &wc)) {
            wc.lpfnWndProc = &ToolTypeApp::PtsDialogWndProc;
            wc.hInstance = instance_;
            wc.lpszClassName = L"ToolTypePtsDialog";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            if (!RegisterClassW(&wc)) return;
        }

        bool wasEnabled = enabled_;
        SetEnabled(false, false);

        PtsDialogState state{};
        state.app = this;

        const UINT dpi = QueryPtsDialogDpi(hwnd_);
        const PtsDialogLayout layout = CalculatePtsDialogLayout(dpi);
        DWORD exStyle = PtsDialogExtendedStyle(topMost_);
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_CLIPCHILDREN;
        RECT desiredClient{0, 0, layout.clientWidth, layout.clientHeight};
        AdjustWindowRectEx(&desiredClient, style, FALSE, exStyle);
        int width = RectWidth(desiredClient);
        int height = RectHeight(desiredClient);

        RECT owner{};
        GetWindowRect(hwnd_, &owner);
        int x = owner.left + (RectWidth(owner) - width) / 2;
        int y = owner.top + (RectHeight(owner) - height) / 2;
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
            int maxX = std::max(monitorInfo.rcWork.left, monitorInfo.rcWork.right - width);
            int maxY = std::max(monitorInfo.rcWork.top, monitorInfo.rcWork.bottom - height);
            x = std::clamp(x, static_cast<int>(monitorInfo.rcWork.left), maxX);
            y = std::clamp(y, static_cast<int>(monitorInfo.rcWork.top), maxY);
        }
        HWND dialog = CreateWindowExW(exStyle, L"ToolTypePtsDialog", L"Pts - Công cụ Photoshop",
                                      style,
                                      x, y, width, height, hwnd_, nullptr, instance_, &state);
        if (!dialog) {
            SetEnabled(wasEnabled, false);
            return;
        }
        HFONT dialogFont = CreatePtsDialogFont(font_, dpi);
        HFONT controlFont = dialogFont ? dialogFont : font_;

        auto left = [](const RECT& rect) { return static_cast<int>(rect.left); };
        auto top = [](const RECT& rect) { return static_cast<int>(rect.top); };
        auto widthOf = [](const RECT& rect) { return static_cast<int>(rect.right - rect.left); };
        auto heightOf = [](const RECT& rect) { return static_cast<int>(rect.bottom - rect.top); };
        state.progressRect = layout.progress;

        HWND title = CreateWindowExW(0, L"STATIC", L"Sao lưu / Khôi phục Photoshop",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                    left(layout.title), top(layout.title), widthOf(layout.title),
                                    heightOf(layout.title), dialog, nullptr, instance_, nullptr);
        state.backupSettings = CreatePtsDialogButton(
            dialog, L"Sao lưu cài đặt", ID_PTS_BACKUP_SETTINGS,
            left(layout.backupSettings), top(layout.backupSettings),
            widthOf(layout.backupSettings), heightOf(layout.backupSettings));
        state.backupFonts = CreatePtsDialogButton(
            dialog, L"Sao lưu font", ID_PTS_BACKUP_FONTS,
            left(layout.backupFonts), top(layout.backupFonts),
            widthOf(layout.backupFonts), heightOf(layout.backupFonts));
        state.backupAll = CreatePtsDialogButton(
            dialog, L"Sao lưu cài đặt + font", ID_PTS_BACKUP_ALL,
            left(layout.backupAll), top(layout.backupAll),
            widthOf(layout.backupAll), heightOf(layout.backupAll));
        state.restore = CreatePtsDialogButton(
            dialog, L"Khôi phục", ID_PTS_RESTORE,
            left(layout.restore), top(layout.restore), widthOf(layout.restore),
            heightOf(layout.restore));
        state.status = CreateWindowExW(0, L"STATIC", L"Sẵn sàng. Chọn thao tác để bắt đầu.",
                                       WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                       left(layout.status), top(layout.status),
                                       widthOf(layout.status), heightOf(layout.status), dialog,
                                       nullptr, instance_, nullptr);
        state.percent = CreateWindowExW(0, L"STATIC", L"0%",
                                        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_NOPREFIX,
                                        left(layout.percent), top(layout.percent),
                                        widthOf(layout.percent), heightOf(layout.percent), dialog,
                                        nullptr, instance_, nullptr);
        state.close = CreatePtsDialogButton(
            dialog, L"Đóng", IDCANCEL, left(layout.close), top(layout.close),
            widthOf(layout.close), heightOf(layout.close));

        for (HWND control : {title, state.backupSettings, state.backupFonts,
                             state.backupAll, state.restore, state.status,
                             state.percent, state.close}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), TRUE);
        }

        EnableWindow(hwnd_, FALSE);
        ShowWindow(dialog, SW_SHOW);
        SetForegroundWindow(dialog);

        RunPtsDialogMessageLoop(dialog);
        if (dialogFont) DeleteObject(dialogFont);

        EnableWindow(hwnd_, TRUE);
        SetForegroundWindow(hwnd_);
        SetEnabled(wasEnabled, false);
    }

    std::wstring ConfigPath() const {
        return GetModuleDirectory() + L"\\ToolType.ini";
    }

    void LoadConfig(bool showErrors) {
        std::vector<uint8_t> bytes;
        std::wstring error;
        if (!ReadFileBytes(ConfigPath(), bytes, error)) {
            if (showErrors) SetStatus(L"Chưa có file save.");
            return;
        }
        std::wstring text = DecodeTextBytes(bytes);
        auto lines = SplitLines(text);
        for (const auto& line : lines) {
            if (line.rfind(L"file=", 0) == 0) {
                savedFile_ = line.substr(5);
            } else if (line.rfind(L"index=", 0) == 0) {
                savedIndex_ = std::max(0, _wtoi(line.c_str() + 6));
            } else if (line.rfind(L"topmost=", 0) == 0) {
                topMost_ = _wtoi(line.c_str() + 8) != 0;
            } else if (line.rfind(L"enabled=", 0) == 0) {
                // On/Off is intentionally a startup default, not a persisted preference.
                // Older configs may contain enabled=0; ignore it so the tool opens On by default.
                enabled_ = true;
            } else if (line.rfind(L"hotkeyVk=", 0) == 0) {
                DWORD vk = static_cast<DWORD>(std::max(0, _wtoi(line.c_str() + 9)));
                if (vk == kDefaultPasteHotkeyVk || IsSelectablePasteHotkey(vk)) {
                    pasteHotkeyVk_ = vk;
                }
            } else if (line.rfind(L"hotkey=", 0) == 0) {
                switch (_wtoi(line.c_str() + 7)) {
                    case 1: pasteHotkeyVk_ = VK_F4; break;
                    case 2: pasteHotkeyVk_ = VK_TAB; break;
                    case 3: pasteHotkeyVk_ = VK_F8; break;
                    case 0:
                    default: pasteHotkeyVk_ = kDefaultPasteHotkeyVk; break;
                }
            }
        }
        UpdateHotkeyButtonText();
    }

    void SaveConfig(bool feedback) {
        if (!currentFile_.empty()) {
            savedFile_ = currentFile_;
            savedIndex_ = std::max(0, CurrentIndex());
        }

        std::wstring config =
            L"file=" + savedFile_ + L"\n" +
            L"index=" + std::to_wstring(savedIndex_) + L"\n" +
            L"topmost=" + std::to_wstring(topMost_ ? 1 : 0) + L"\n" +
            L"enabled=1\n" +
            L"hotkeyVk=" + std::to_wstring(pasteHotkeyVk_) + L"\n";
        WriteUtf8File(ConfigPath(), WideToUtf8(config));

        if (feedback) {
            if (savedFile_.empty()) {
                SetStatus(L"Chưa có file để save vị trí.");
            } else {
                wchar_t msg[160];
                std::swprintf(msg, 160, L"Đã save tại dòng %d.", savedIndex_ + 1);
                SetStatus(msg);
            }
        }
    }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND list_ = nullptr;
    HWND addButton_ = nullptr;
    HWND pinButton_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND onoffButton_ = nullptr;
    HWND expandButton_ = nullptr;
    HWND googleDocsButton_ = nullptr;
    HWND hotkeyButton_ = nullptr;
    HWND guideButton_ = nullptr;
    HWND ptsButton_ = nullptr;
    HWND status_ = nullptr;
    HWND tooltip_ = nullptr;
    HFONT font_ = nullptr;
    HFONT smallFont_ = nullptr;
    HBRUSH backBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HBRUSH borderBrush_ = nullptr;
    HHOOK hook_ = nullptr;
    WNDPROC buttonProc_ = nullptr;
    std::vector<std::wstring> lines_;
    std::wstring tooltipText_;
    std::wstring currentFile_;
    std::wstring savedFile_;
    std::wstring updateDownloadUrl_;
    int selectedIndex_ = 0;
    int topIndex_ = 0;
    int horizontalOffset_ = 0;
    int maxTextWidth_ = 0;
    int tooltipLineIndex_ = -1;
    int hoverButtonId_ = 0;
    ListPart listHoverPart_ = ListPart::None;
    ListPart dragPart_ = ListPart::None;
    int dragStartMouse_ = 0;
    int dragStartTop_ = 0;
    int dragStartHorizontal_ = 0;
    int savedIndex_ = 0;
    bool topMost_ = false;
    bool enabled_ = true;
    bool expanded_ = false;
    bool endOfTextReached_ = false;
    DWORD pasteHotkeyVk_ = kDefaultPasteHotkeyVk;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int nCmdShow) {
    ToolTypeApp app(instance);
    g_app = &app;
    if (!app.Create(nCmdShow)) {
        MessageBoxW(nullptr, L"Không khởi tạo được ToolType.", L"ToolType", MB_ICONERROR | MB_OK);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_app = nullptr;
    return static_cast<int>(msg.wParam);
}
