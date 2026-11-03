/* json_stream.cpp — see rancho/json_stream.hpp. Ported unchanged from Void
 * Hormiga's src/store/json_stream.cpp (2026-10-27). */
#include "rancho/json_stream.hpp"

#include <utility>

namespace rancho {

namespace {
bool is_ws(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

void put_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

int hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
} // namespace

JsonStream::JsonStream(const std::string& path, std::size_t buffer) : cap_(buffer ? buffer : 1 << 20) {
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        error_ = "cannot open " + path;
        return;
    }
    if (std::fseek(file_, 0, SEEK_END) == 0) {
        const long long n = (long long)
#if defined(_WIN32)
            _ftelli64(file_);
#else
            ftello(file_);
#endif
        if (n > 0) size_ = (std::uint64_t)n;
        std::fseek(file_, 0, SEEK_SET);
    }
}

JsonStream JsonStream::from_text(std::string text) {
    JsonStream s;
    s.size_ = text.size();
    s.buf_ = std::move(text);
    return s;
}

JsonStream::~JsonStream() {
    if (file_) std::fclose(file_);
}

JsonStream::JsonStream(JsonStream&& o) noexcept
    : file_(std::exchange(o.file_, nullptr)), buf_(std::move(o.buf_)), pos_(o.pos_), cap_(o.cap_),
      base_(o.base_), size_(o.size_), error_(std::move(o.error_)) {}

bool JsonStream::fill() {
    if (!file_) return false;
    base_ += buf_.size();
    buf_.resize(cap_);
    const std::size_t n = std::fread(buf_.data(), 1, cap_, file_);
    buf_.resize(n);
    pos_ = 0;
    return n > 0;
}

int JsonStream::get() {
    if (pos_ >= buf_.size() && !fill()) return -1;
    return (unsigned char)buf_[pos_++];
}

bool JsonStream::fail(const std::string& why) {
    if (error_.empty()) error_ = why + " (at byte " + std::to_string(offset()) + ")";
    return false;
}

char JsonStream::peek() {
    for (;;) {
        if (pos_ >= buf_.size() && !fill()) return 0;
        const char c = buf_[pos_];
        if (!is_ws((unsigned char)c)) return c;
        ++pos_;
    }
}

bool JsonStream::eat(char c) {
    if (peek() != c) return false;
    ++pos_;
    return true;
}

bool JsonStream::expect(char c) {
    if (eat(c)) return true;
    const char got = peek();
    return fail(std::string("expected '") + c + "', found " + (got ? std::string("'") + got + "'" : "the end"));
}

/* At an opening quote. `out` + !decode: the raw bytes, quotes included.
 * `out` + decode: the decoded text. `sink`: the raw content in chunks. */
bool JsonStream::raw_string(std::string* out, bool decode, const std::function<void(std::string_view)>* sink) {
    if (!expect('"')) return false;
    if (out && !decode) *out += '"';
    std::string chunk;
    auto flush = [&] {
        if (sink && !chunk.empty()) {
            (*sink)(chunk);
            chunk.clear();
        }
    };
    for (;;) {
        int c = get();
        if (c < 0) return fail("a string never closed");
        if (c == '"') {
            if (out && !decode) *out += '"';
            flush();
            return true;
        }
        if (c != '\\') {
            if (out) *out += (char)c;
            if (sink) {
                chunk += (char)c;
                if (chunk.size() >= (1 << 16)) flush();
            }
            continue;
        }
        const int e = get();
        if (e < 0) return fail("a string never closed");
        if (!decode) { // raw: keep the escape as written
            if (out) {
                *out += '\\';
                *out += (char)e;
            }
            if (sink) {
                chunk += '\\';
                chunk += (char)e;
            }
            continue;
        }
        switch (e) {
        case '"': *out += '"'; break;
        case '\\': *out += '\\'; break;
        case '/': *out += '/'; break;
        case 'b': *out += '\b'; break;
        case 'f': *out += '\f'; break;
        case 'n': *out += '\n'; break;
        case 'r': *out += '\r'; break;
        case 't': *out += '\t'; break;
        case 'u': {
            auto four = [&](unsigned& cp) {
                cp = 0;
                for (int i = 0; i < 4; ++i) {
                    const int h = hex(get());
                    if (h < 0) return false;
                    cp = cp * 16 + (unsigned)h;
                }
                return true;
            };
            unsigned cp = 0;
            if (!four(cp)) return fail("a bad \\u escape");
            if (cp >= 0xD800 && cp <= 0xDBFF) { // a surrogate pair
                unsigned lo = 0;
                if (get() != '\\' || get() != 'u' || !four(lo) || lo < 0xDC00 || lo > 0xDFFF)
                    return fail("a lone surrogate");
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
            put_utf8(*out, cp);
            break;
        }
        default: return fail("an unknown escape");
        }
    }
}

bool JsonStream::string(std::string& out) {
    out.clear();
    return raw_string(&out, true, nullptr);
}

bool JsonStream::string_chunks(const std::function<void(std::string_view)>& sink) {
    return raw_string(nullptr, false, &sink);
}

/* Any value, its raw bytes appended to `out` when given. Containers are
 * scanned flat — depth and string state, nothing else — because a value
 * captured here is parsed again by whoever stores it. */
bool JsonStream::value(std::string* out, int) {
    const char c = peek();
    if (!c) return fail("a value was expected, the file ended");
    if (c == '"') return raw_string(out, false, nullptr);
    if (c == '{' || c == '[') {
        int depth = 0;
        bool in_string = false;
        for (;;) {
            const int b = get();
            if (b < 0) return fail("an object or array never closed");
            if (out) *out += (char)b;
            if (in_string) {
                if (b == '\\') {
                    const int n = get();
                    if (n < 0) return fail("a string never closed");
                    if (out) *out += (char)n;
                } else if (b == '"') {
                    in_string = false;
                }
                continue;
            }
            if (b == '"') in_string = true;
            else if (b == '{' || b == '[') ++depth;
            else if (b == '}' || b == ']') {
                if (--depth == 0) return true;
            }
        }
    }
    // a number, true, false or null: up to the next delimiter
    bool any = false;
    for (;;) {
        if (pos_ >= buf_.size() && !fill()) break;
        const char b = buf_[pos_];
        if (is_ws((unsigned char)b) || b == ',' || b == '}' || b == ']' || b == ':') break;
        if (out) *out += b;
        ++pos_;
        any = true;
    }
    return any || fail("a value was expected");
}

bool JsonStream::capture(std::string& out) {
    out.clear();
    return value(&out, 0);
}

bool JsonStream::skip() { return value(nullptr, 0); }

bool JsonStream::object(const std::function<bool(const std::string& key)>& each) {
    if (!expect('{')) return false;
    if (eat('}')) return true;
    std::string key;
    for (;;) {
        if (!string(key) || !expect(':')) return false;
        if (!each(key)) return false;
        if (eat(',')) continue;
        return expect('}');
    }
}

bool JsonStream::array(const std::function<bool()>& each) {
    if (!expect('[')) return false;
    if (eat(']')) return true;
    for (;;) {
        if (!each()) return false;
        if (eat(',')) continue;
        return expect(']');
    }
}

} // namespace rancho
