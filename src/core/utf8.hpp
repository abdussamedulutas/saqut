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

}  // namespace utf8

#endif
