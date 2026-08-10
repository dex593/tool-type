#include "../text_normalization.h"

#include <iostream>

namespace {

int failures = 0;

void ExpectEqual(const wchar_t* name,
                 const std::wstring& actual,
                 const std::wstring& expected) {
    if (actual == expected) return;

    std::wcerr << L"FAIL: " << name << L"\n"
               << L"  expected: [" << expected << L"]\n"
               << L"  actual:   [" << actual << L"]\n";
    ++failures;
}

}  // namespace

int main() {
    ExpectEqual(L"leading ellipsis remains attached to the following word",
                NormalizePasteText(L"...hôm"),
                L"...hôm");
    ExpectEqual(L"inline ellipsis remains attached when the source has no space",
                NormalizePasteText(L"Chờ...xem"),
                L"Chờ...xem");
    ExpectEqual(L"existing whitespace after an ellipsis collapses to one space",
                NormalizePasteText(L"...   hôm"),
                L"... hôm");
    ExpectEqual(L"a single sentence period still gains a missing space",
                NormalizePasteText(L"Xin chào.Hôm nay"),
                L"Xin chào. Hôm nay");
    ExpectEqual(L"numeric separators remain untouched",
                NormalizePasteText(L"3.14 lúc 12:30"),
                L"3.14 lúc 12:30");
    ExpectEqual(L"paste pipeline preserves an attached ellipsis after stripping a marker",
                PasteTextForLine(L"* ...hôm"),
                L"...hôm");
    ExpectEqual(L"paste pipeline strips dash markers",
                PasteTextForLine(L"  -   nội dung"),
                L"nội dung");
    ExpectEqual(L"paste pipeline strips quote markers",
                PasteTextForLine(L"> trích dẫn"),
                L"trích dẫn");
    ExpectEqual(L"blank lines are not pasteable",
                PasteTextForLine(L"   "),
                L"");
    ExpectEqual(L"comment lines are not pasteable",
                PasteTextForLine(L"  // ghi chú"),
                L"");
    ExpectEqual(L"spaces before punctuation and closers are removed",
                NormalizePasteText(L"Xin chào , bạn ! )  "),
                L"Xin chào, bạn!)");

    if (!IsPasteableTextLine(L"Nội dung") || IsPasteableTextLine(L"// ghi chú")) {
        std::cerr << "FAIL: pasteability classification\n";
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures << " normalization test(s) failed.\n";
        return 1;
    }

    std::cout << "All normalization tests passed.\n";
    return 0;
}
