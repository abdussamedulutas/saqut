#ifndef SAQUT_CORE_UTF8
#define SAQUT_CORE_UTF8

#include <string>
#include <string_view>

namespace utf8 {

// Returns the byte length of the code point beginning at offset. Invalid
// leading bytes are treated as one byte so malformed input cannot loop.
size_t codePointBytes(std::string_view text, size_t offset);
size_t codePointCount(std::string_view text);
std::string charAt(std::string_view text, size_t index);
std::string substring(std::string_view text, size_t start, size_t length);
// Returns the code-point index of the first occurrence of `needle`, or
// std::string::npos if absent. Code-point index matches the unit used by
// charAt/substring/length (ADR-024), so the result composes with them byte
// positions do not.
size_t indexOf(std::string_view text, std::string_view needle);
std::string lower(std::string_view text);
std::string upper(std::string_view text);
std::string fromBytes(std::string_view bytes);

}  // namespace utf8

#endif
