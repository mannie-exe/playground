#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace playground::support {

enum class TextBoundary { Grapheme, Word, Line };

// All UI text is valid UTF-8 without embedded NUL, limited to INT32_MAX bytes.
// Invalid text throws invalid_argument; oversized text throws length_error.
void validateTextUTF8(std::string_view text);

// ICU root-locale boundaries, using the linked ICU Unicode version. Offsets
// are UTF-8 bytes; include 0 and text.size(), or just 0 for empty text.
std::vector<std::size_t> textBoundaries(std::string_view text, TextBoundary kind);

} // namespace playground::support
