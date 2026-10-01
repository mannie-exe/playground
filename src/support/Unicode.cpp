#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

#include <unicode/ubrk.h>
#include <unicode/utext.h>
#include <unicode/utf8.h>

#include <support/Unicode.hpp>

namespace playground::support {
namespace {
void check(UErrorCode status) {
  if (U_FAILURE(status))
    throw std::invalid_argument(u_errorName(status));
}
} // namespace

void validateTextUTF8(std::string_view text) {
  if (text.size() > std::size_t(std::numeric_limits<int32_t>::max()))
    throw std::length_error("Text too large for Unicode services");
  for (int32_t i = 0; i < int32_t(text.size());) {
    UChar32 cp;
    U8_NEXT(text.data(), i, int32_t(text.size()), cp);
    if (cp < 0 || cp == 0)
      throw std::invalid_argument("Text must be valid UTF-8 without NUL");
  }
}

std::vector<std::size_t> textBoundaries(std::string_view text,
                                      TextBoundary kind) {
  if (kind != TextBoundary::Grapheme && kind != TextBoundary::Word &&
      kind != TextBoundary::Line)
    throw std::invalid_argument("Unknown text boundary kind");
  validateTextUTF8(text);
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<UText, decltype(&utext_close)> source{
      utext_openUTF8(nullptr, text.data(), int64_t(text.size()), &error),
      utext_close};
  check(error);
  std::unique_ptr<UBreakIterator, decltype(&ubrk_close)> breaks{
      ubrk_open(kind == TextBoundary::Grapheme ? UBRK_CHARACTER
                : kind == TextBoundary::Word   ? UBRK_WORD
                                               : UBRK_LINE,
                "", nullptr, 0, &error),
      ubrk_close};
  check(error);
  ubrk_setUText(breaks.get(), source.get(), &error);
  check(error);
  std::vector<std::size_t> result;
  for (int32_t p = ubrk_first(breaks.get()); p != UBRK_DONE;
       p = ubrk_next(breaks.get()))
    result.push_back(std::size_t(p));
  return result;
}

} // namespace playground::support
