#pragma once

#include <string_view>
#include <array>
#include <optional>
#include <cstdint>

// ─────────────────────────────────────────────
//  DESIGN GOALS
//  1. Zero heap allocations — no new, no malloc, no std::string
//  2. Zero-copy — string_views point INTO the caller's buffer
//  3. Single-threaded, no locks
//  4. C++17, STL only
// ─────────────────────────────────────────────

namespace http {

// Maximum headers we support — fixed array, no vector
static constexpr std::size_t MAX_HEADERS = 32;

// ── Result type ──────────────────────────────
// We never throw. Every function returns a ParseResult.
enum class ParseError {
    None,
    InvalidMethod,        // unrecognised HTTP method
    InvalidRequestLine,   // malformed first line
    InvalidHeader,        // header missing ':' or empty name
    TooManyHeaders,       // exceeded MAX_HEADERS
    MissingCRLF,          // expected \r\n not found
    BufferTooShort,       // input ended unexpectedly
};

// A single header key/value pair
// Both are string_views — zero copy, zero allocation
struct Header {
    std::string_view name;   // e.g. "Content-Type"
    std::string_view value;  // e.g. "application/json"
};

// The parsed HTTP request
// Everything points into the original input buffer — caller must keep it alive
struct HttpRequest {
    std::string_view method;   // GET, POST, PUT, DELETE, etc.
    std::string_view path;     // /api/v1/users
    std::string_view version;  // HTTP/1.1

    // Fixed-size array — no heap, no vector
    std::array<Header, MAX_HEADERS> headers{};
    std::size_t header_count = 0;

    std::string_view body;     // everything after \r\n\r\n

    // Convenience: find a header value by name (case-insensitive)
    std::optional<std::string_view> get_header(std::string_view name) const noexcept;
};

// Parse result — either a request or an error
struct ParseResult {
    HttpRequest request;
    ParseError  error    = ParseError::None;
    std::size_t consumed = 0;  // how many bytes were parsed

    bool ok() const noexcept { return error == ParseError::None; }
};

// ── Main parser class ─────────────────────────
// Stateless — parse() takes a buffer and returns a result
// No member variables that allocate heap memory
class Parser {
public:
    // Parse a full HTTP/1.1 request from a raw buffer
    // buf      — pointer to the raw bytes (caller owns this memory)
    // len      — number of bytes in buf
    // returns  — ParseResult with either a valid request or an error
    [[nodiscard]]
    static ParseResult parse(const char* buf, std::size_t len) noexcept;

private:
    // ── Internal helpers ─────────────────────
    // All take a pointer + remaining length, advance the pointer,
    // return false on failure. No allocations anywhere.

    // Parse "METHOD /path HTTP/1.1\r\n"
    static bool parse_request_line(
        const char*&     pos,        // current position (advanced in place)
        const char*      end,        // one past last valid byte
        HttpRequest&     out,
        ParseError&      err
    ) noexcept;

    // Parse all "Name: Value\r\n" lines until "\r\n"
    static bool parse_headers(
        const char*&     pos,
        const char*      end,
        HttpRequest&     out,
        ParseError&      err
    ) noexcept;

    // Find the next occurrence of needle in [pos, end)
    // Returns pointer to needle, or nullptr if not found
    static const char* find(
        const char* pos,
        const char* end,
        std::string_view needle
    ) noexcept;

    // Trim leading and trailing spaces/tabs from a string_view
    static std::string_view trim(std::string_view sv) noexcept;

public:
    // Case-insensitive string comparison (for header lookup)
    // Public so HttpRequest::get_header can use it
    static bool iequal(std::string_view a, std::string_view b) noexcept;

private:
};

} // namespace http