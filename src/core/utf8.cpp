#include "core/utf8.hpp"

namespace utf8 {

namespace {

bool continuation(unsigned char c) {
    return (c & 0xC0u) == 0x80u;
}

size_t validSequenceLength(std::string_view text, size_t offset) {
    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    size_t length = 1;
    if (lead >= 0xC2u && lead <= 0xDFu) length = 2;
    else if (lead >= 0xE0u && lead <= 0xEFu) length = 3;
    else if (lead >= 0xF0u && lead <= 0xF4u) length = 4;
    else return 1;

    if (offset + length > text.size()) return 1;
    for (size_t i = 1; i < length; ++i)
        if (!continuation(static_cast<unsigned char>(text[offset + i]))) return 1;

    // Reject overlong encodings, UTF-16 surrogate code points and values over U+10FFFF.
    const unsigned char b1 = static_cast<unsigned char>(text[offset + 1]);
    if (length == 3 && ((lead == 0xE0u && b1 < 0xA0u) ||
                        (lead == 0xEDu && b1 >= 0xA0u))) return 1;
    if (length == 4 && ((lead == 0xF0u && b1 < 0x90u) ||
                        (lead == 0xF4u && b1 >= 0x90u))) return 1;
    return length;
}

}  // namespace

size_t codePointBytes(std::string_view text, size_t offset) {
    if (offset >= text.size()) return 0;
    return validSequenceLength(text, offset);
}

size_t codePointCount(std::string_view text) {
    size_t count = 0;
    for (size_t offset = 0; offset < text.size();) {
        offset += codePointBytes(text, offset);
        ++count;
    }
    return count;
}

std::string charAt(std::string_view text, size_t index) {
    size_t current = 0;
    for (size_t offset = 0; offset < text.size();) {
        const size_t length = codePointBytes(text, offset);
        if (current == index) return std::string(text.substr(offset, length));
        offset += length;
        ++current;
    }
    return {};
}

std::string substring(std::string_view text, size_t start, size_t length) {
    size_t offset = 0;
    size_t current = 0;
    while (offset < text.size() && current < start) {
        offset += codePointBytes(text, offset);
        ++current;
    }
    if (current < start) return {};

    const size_t begin = offset;
    while (offset < text.size() && current < start + length) {
        offset += codePointBytes(text, offset);
        ++current;
    }
    return std::string(text.substr(begin, offset - begin));
}

}  // namespace utf8
