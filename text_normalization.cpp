#include "text_normalization.h"

#include <cwctype>

namespace {

std::wstring TrimLeftCopy(const std::wstring& text) {
    size_t first = 0;
    while (first < text.size() && iswspace(text[first])) ++first;
    return text.substr(first);
}

bool IsAsciiDigit(wchar_t ch) {
    return ch >= L'0' && ch <= L'9';
}

bool IsSentencePunctuation(wchar_t ch) {
    switch (ch) {
        case L',':
        case L'.':
        case L';':
        case L':':
        case L'!':
        case L'?':
            return true;
        default:
            return false;
    }
}

bool IsNoSpaceBeforePunctuation(wchar_t ch) {
    switch (ch) {
        case L')':
        case L']':
        case L'}':
        case L'%':
            return true;
        default:
            return IsSentencePunctuation(ch);
    }
}

bool IsNumericSeparator(wchar_t ch) {
    return ch == L',' || ch == L'.' || ch == L':';
}

bool EndsEllipsisRun(const std::wstring& text, size_t index) {
    return index >= 2 &&
           text[index] == L'.' &&
           text[index - 1] == L'.' &&
           text[index - 2] == L'.';
}

std::wstring CollapseWhitespace(const std::wstring& text) {
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

std::wstring RemoveSpacesBeforePunctuation(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());

    for (wchar_t ch : text) {
        if (IsNoSpaceBeforePunctuation(ch)) {
            while (!out.empty() && out.back() == L' ') out.pop_back();
        }
        out.push_back(ch);
    }
    return out;
}

std::wstring NormalizeSpacesAfterPunctuation(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        out.push_back(ch);

        if (!IsSentencePunctuation(ch)) continue;
        if (EndsEllipsisRun(text, i)) continue;

        wchar_t previous = out.size() >= 2 ? out[out.size() - 2] : L'\0';
        size_t next = i + 1;
        while (next < text.size() && text[next] == L' ') ++next;
        if (next >= text.size()) {
            i = text.size();
            break;
        }

        wchar_t nextChar = text[next];
        bool numericSeparator = IsNumericSeparator(ch) &&
                                IsAsciiDigit(previous) &&
                                IsAsciiDigit(nextChar);
        if (!numericSeparator && !IsNoSpaceBeforePunctuation(nextChar)) {
            out.push_back(L' ');
        }
        i = next - 1;
    }
    return out;
}

}  // namespace

TextLineKind ClassifyTextLine(const std::wstring& line) {
    std::wstring text = TrimLeftCopy(line);
    if (text.empty()) return TextLineKind::Blank;
    if (text.rfind(L"//", 0) == 0) return TextLineKind::Comment;
    if (text[0] == L'*') return TextLineKind::Star;
    if (text[0] == L'-') return TextLineKind::Dash;
    if (text[0] == L'>') return TextLineKind::Quote;
    return TextLineKind::Normal;
}

bool IsPasteMarkerKind(TextLineKind kind) {
    return kind == TextLineKind::Star || kind == TextLineKind::Dash || kind == TextLineKind::Quote;
}

std::wstring StripPasteMarker(const std::wstring& line, TextLineKind kind) {
    if (!IsPasteMarkerKind(kind)) return line;

    size_t marker = 0;
    while (marker < line.size() && iswspace(line[marker])) ++marker;
    if (marker < line.size()) ++marker;
    while (marker < line.size() && iswspace(line[marker])) ++marker;
    return line.substr(marker);
}

std::wstring NormalizePasteText(const std::wstring& text) {
    return NormalizeSpacesAfterPunctuation(
        RemoveSpacesBeforePunctuation(
            CollapseWhitespace(text)));
}

std::wstring PasteTextForLine(const std::wstring& line) {
    TextLineKind kind = ClassifyTextLine(line);
    if (kind == TextLineKind::Blank || kind == TextLineKind::Comment) return L"";
    return NormalizePasteText(StripPasteMarker(line, kind));
}

bool IsPasteableTextLine(const std::wstring& line) {
    return !PasteTextForLine(line).empty();
}
