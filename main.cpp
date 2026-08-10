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
#include <wininet.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
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

constexpr const wchar_t* kToolVersion = L"1.0";
constexpr const wchar_t* kUpdateCheckUrl =
    L"https://github.com/dex593/tool-type/raw/refs/heads/master/check.ini";
constexpr const wchar_t* kLatestVersionMessage =
    L"Bạn đang sử dụng phiên bản mới nhất.";
constexpr const wchar_t* kUpdateAvailableMessage =
    L"Đã có phiên bản mới, click để tải ngay.";

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
constexpr int ID_STATUS = 1201;
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
        ShowWindow(googleDocsButton_, SW_HIDE);
        ShowWindow(hotkeyButton_, SW_HIDE);
        ShowWindow(guideButton_, SW_HIDE);

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
        HWND extraButtons[] = {googleDocsButton_, hotkeyButton_, guideButton_};
        for (int i = 0; i < 3; ++i) {
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

    void DrawDialogButton(DRAWITEMSTRUCT* di) {
        if (!di || di->CtlType != ODT_BUTTON) return;
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        bool hot = (di->itemState & ODS_HOTLIGHT) != 0;
        RECT rc = di->rcItem;

        COLORREF fill = hot ? kButtonHoverColor : kButtonColor;
        COLORREF border = hot ? RGB(255, 255, 255) : kBorderColor;
        COLORREF textColor = hot ? RGB(0, 0, 0) : kTextColor;
        if (pressed) fill = BlendColor(fill, RGB(0, 0, 0), 22);

        FillSolidRect(di->hDC, rc, fill);
        DrawCrispOnePixelBorder(di->hDC, rc, border);

        wchar_t text[64] = {};
        GetWindowTextW(di->hwndItem, text, 64);
        RECT textRc = rc;
        if (pressed) OffsetRect(&textRc, 1, 1);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, textColor);
        SelectObject(di->hDC, font_);
        DrawTextW(di->hDC, text, -1, &textRc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

        MSG msg{};
        while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(dialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

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
            L"9. Save/Open lưu và mở lại vị trí file hiện tại.";
        MessageBoxW(hwnd_, guide.c_str(), L"Hướng dẫn ToolType",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
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
