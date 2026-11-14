#include "openosc/scpi/parser.hpp"
#include <cstring>

namespace openosc::scpi {
namespace {
bool space(char c) { return c == ' ' || c == '\t'; }
char upper(char c) { return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; }
}
void Parser::reset() {
    used_ = head_ = count_ = 0; path_[0] = 0; quote_ = 0; error_ = 0;
}
bool Parser::take(Command &command) {
    if (!count_) return false;
    command = commands_[head_]; head_ = (head_ + 1) % 4; --count_; return true;
}
bool Parser::feed(uint8_t byte) {
    if (byte == '\n' || (!quote_ && byte == ';')) return finish(byte == '\n');
    if (byte == '\r' && !quote_) return true;
    if (error_) return true;
    if (byte < 32 && byte != '\t') { error_ = -102; return true; }
    if (byte >= 127) { error_ = -102; return true; }
    if (used_ + 1 >= sizeof line_) { error_ = -363; return true; }
    if (byte == '"' || byte == '\'') {
        if (!quote_) quote_ = byte;
        else if (quote_ == byte) quote_ = 0;
    }
    line_[used_++] = byte; return true;
}
bool Parser::finish(bool end) {
    if (count_ == 4 && (used_ || error_)) return false;
    if (used_ || error_) {
        auto &command = commands_[(head_ + count_) % 4]; command = {};
        command.error = error_ ? error_ : quote_ ? -102 : 0;
        if (!command.error) { line_[used_] = 0; decode(command); }
        ++count_;
    }
    used_ = 0; quote_ = 0; error_ = 0;
    if (end) path_[0] = 0;
    return true;
}
void Parser::decode(Command &command) {
    const char *begin = line_;
    while (space(*begin)) ++begin;
    const bool root = *begin == ':';
    if (root) ++begin;
    const char *argument = begin;
    while (*argument && !space(*argument)) ++argument;
    size_t length = argument - begin;
    if (!length) { command.error = -102; return; }
    if (begin[length - 1] == '?') { command.query = true; --length; }
    const size_t prefix = !root && *begin != '*' ? std::strlen(path_) : 0;
    if (!length || prefix + length >= sizeof command.header) { command.error = -113; return; }
    std::memcpy(command.header, path_, prefix);
    for (size_t i = 0; i < length; ++i) {
        const char c = upper(begin[i]);
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ':' || (c == '*' && i == 0))) {
            command.error = -102; return;
        }
        command.header[prefix + i] = c;
    }
    command.header[prefix + length] = 0;
    if (*begin != '*') {
        const char *colon = std::strrchr(command.header, ':');
        const size_t size = colon ? colon - command.header + 1 : 0;
        std::memcpy(path_, command.header, size); path_[size] = 0;
    }
    while (space(*argument)) ++argument;
    size_t bytes = std::strlen(argument);
    while (bytes && space(argument[bytes - 1])) --bytes;
    if (bytes >= sizeof command.argument) { command.error = -363; return; }
    std::memcpy(command.argument, argument, bytes); command.argument[bytes] = 0;
}
} // namespace openosc::scpi
