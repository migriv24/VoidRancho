/* rancho/json_stream.hpp — reading a JSON file of any size, one value at a time.
 *
 * A Void Core state document (or a Void Hormiga .miga v3) is ONE JSON value
 * holding the whole database, and a reader that parses it whole needs memory
 * proportional to it: Hormiga's 5×10^5-rune stress database took gigabytes
 * before a single rune reached the screen (okf/design/founding.md §2). This
 * reader holds a fixed buffer and whatever ONE value the caller asks for, so a
 * 20 GB file costs what a 20 MB file does. Ported unchanged from Hormiga.
 *
 * It is a pull reader: the caller walks the structure it expects, and takes
 * each value either RAW (`capture`: the exact bytes, for a rune or an edge to
 * be stored as written) or skips it. Strings can be streamed in chunks
 * (`string_chunks`), for a picture's base64 that is larger than anything else
 * in the file. Every failure says where it happened, as a byte offset. */
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>

namespace rancho {

class JsonStream {
public:
    /* Reads a file. `buffer` is how many bytes are held at once besides the
     * value being captured. */
    explicit JsonStream(const std::string& path, std::size_t buffer = 1 << 20);
    /* Reads a string already in memory (tests, and small documents). */
    static JsonStream from_text(std::string text);
    ~JsonStream();
    JsonStream(const JsonStream&) = delete;
    JsonStream& operator=(const JsonStream&) = delete;
    JsonStream(JsonStream&& o) noexcept;

    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }
    std::uint64_t offset() const { return base_ + pos_; } // bytes consumed
    std::uint64_t size() const { return size_; }          // the file's size, 0 if unknown

    /* Whitespace, then the next character without consuming it; 0 at the end. */
    char peek();
    /* Consume `c` (after whitespace) if it is next. */
    bool eat(char c);
    /* Consume `c` or fail. */
    bool expect(char c);

    /* A string value, decoded (escapes resolved). */
    bool string(std::string& out);
    /* A string value delivered in pieces as it is read, still JSON-escaped
     * except that the quotes are gone. For base64, which has no escapes. */
    bool string_chunks(const std::function<void(std::string_view)>& sink);
    /* The next value's exact bytes, whatever it is. */
    bool capture(std::string& out);
    /* The next value, discarded. */
    bool skip();

    /* Walk an object: `each(key)` must consume the key's value. */
    bool object(const std::function<bool(const std::string& key)>& each);
    /* Walk an array: `each()` must consume one element. */
    bool array(const std::function<bool()>& each);

    bool fail(const std::string& why);

private:
    JsonStream() = default;
    bool fill();           // more bytes into the buffer; false at the end
    int get();             // one byte, -1 at the end
    bool value(std::string* out, int depth);
    bool raw_string(std::string* out, bool decode, const std::function<void(std::string_view)>* sink);

    std::FILE* file_ = nullptr;
    std::string buf_;
    std::size_t pos_ = 0, cap_ = 1 << 20;
    std::uint64_t base_ = 0, size_ = 0;
    std::string error_;
};

} // namespace rancho
