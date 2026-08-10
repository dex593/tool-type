#pragma once

#include <string>

enum class TextLineKind { Blank, Comment, Star, Dash, Quote, Normal };

TextLineKind ClassifyTextLine(const std::wstring& line);
bool IsPasteMarkerKind(TextLineKind kind);
std::wstring StripPasteMarker(const std::wstring& line, TextLineKind kind);
std::wstring NormalizePasteText(const std::wstring& text);
std::wstring PasteTextForLine(const std::wstring& line);
bool IsPasteableTextLine(const std::wstring& line);
