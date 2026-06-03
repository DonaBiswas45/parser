#include "http_parser.hpp"
#include <cctype>    // std::tolower
#include <cstring>   // std::memchr, std::memcmp

namespace http {

// ─────────────────────────────────────────────────────────────
//  Parser::parse  —  entry point
// ─────────────────────────────────────────────────────────────
ParseResult Parser::parse(const char* buf, std::size_t len) noexcept {
    ParseResult result{};

    if (!buf || len == 0) {
        result.error = ParseError::BufferTooShort;
        return result;
    }

    const char* pos = buf;        // current read position
    const char* end = buf + len;  // one past the last byte

    // Step 1: parse "GET /path HTTP/1.1\r\n"
    if (!parse_request_line(pos, end, result.request, result.error)) {
        return result;
    }

    // Step 2: parse headers until blank line
    if (!parse_headers(pos, end, result.request, result.error)) {
        return result;
    }

    // Step 3: everything remaining is the body
    // pos now points just after the \r\n\r\n blank line
    if (pos < end) {
        result.request.body = std::string_view(pos, static_cast<std::size_t>(end - pos));
    }

    result.consumed = static_cast<std::size_t>(pos - buf);
    result.error    = ParseError::None;
    return result;
}

// ─────────────────────────────────────────────────────────────
//  parse_request_line
//  Parses:  METHOD SP path SP HTTP/version CRLF
//  Example: GET /api/users HTTP/1.1\r\n
// ─────────────────────────────────────────────────────────────
bool Parser::parse_request_line(
    const char*& pos,
    const char*  end,
    HttpRequest& out,
    ParseError&  err
) noexcept {

    // ── Find end of request line (\r\n) ──────
    const char* line_end = find(pos, end, "\r\n");
    if (!line_end) {
        err = ParseError::MissingCRLF;
        return false;
    }

    // Work within [pos, line_end) — the first line only
    const char* p     = pos;
    const char* l_end = line_end;

    // ── Extract METHOD ───────────────────────
    // Method ends at the first space
    const char* sp1 = static_cast<const char*>(
        std::memchr(p, ' ', static_cast<std::size_t>(l_end - p))
    );
    if (!sp1) {
        err = ParseError::InvalidRequestLine;
        return false;
    }

    out.method = std::string_view(p, static_cast<std::size_t>(sp1 - p));

    // Basic method validation — must be non-empty alphabetic chars
    if (out.method.empty()) {
        err = ParseError::InvalidMethod;
        return false;
    }

    p = sp1 + 1;  // skip space

    // ── Extract PATH ─────────────────────────
    // Path ends at the next space
    const char* sp2 = static_cast<const char*>(
        std::memchr(p, ' ', static_cast<std::size_t>(l_end - p))
    );
    if (!sp2) {
        err = ParseError::InvalidRequestLine;
        return false;
    }

    out.path = std::string_view(p, static_cast<std::size_t>(sp2 - p));
    p = sp2 + 1;  // skip space

    // ── Extract HTTP VERSION ─────────────────
    // Version is everything from here to end of line
    out.version = std::string_view(p, static_cast<std::size_t>(l_end - p));

    if (out.version.empty()) {
        err = ParseError::InvalidRequestLine;
        return false;
    }

    // Advance pos past the \r\n
    pos = line_end + 2;
    return true;
}

// ─────────────────────────────────────────────────────────────
//  parse_headers
//  Parses lines of "Name: Value\r\n" until blank line "\r\n"
// ─────────────────────────────────────────────────────────────
bool Parser::parse_headers(
    const char*& pos,
    const char*  end,
    HttpRequest& out,
    ParseError&  err
) noexcept {

    while (pos < end) {

        // A bare \r\n means end of headers (blank line)
        if (pos + 1 < end && pos[0] == '\r' && pos[1] == '\n') {
            pos += 2;  // skip blank line, pos now points at body
            return true;
        }

        // Find end of this header line
        const char* line_end = find(pos, end, "\r\n");
        if (!line_end) {
            err = ParseError::MissingCRLF;
            return false;
        }

        // ── Find the colon separator ─────────
        // Header format: "Name: Value"
        const char* colon = static_cast<const char*>(
            std::memchr(pos, ':', static_cast<std::size_t>(line_end - pos))
        );
        if (!colon) {
            err = ParseError::InvalidHeader;
            return false;
        }

        std::string_view name  = trim(std::string_view(pos, static_cast<std::size_t>(colon - pos)));
        std::string_view value = trim(std::string_view(colon + 1, static_cast<std::size_t>(line_end - colon - 1)));

        if (name.empty()) {
            err = ParseError::InvalidHeader;
            return false;
        }

        // ── Store in fixed array ─────────────
        // No push_back, no heap — just index into our array
        if (out.header_count >= MAX_HEADERS) {
            err = ParseError::TooManyHeaders;
            return false;
        }

        out.headers[out.header_count++] = Header{ name, value };

        pos = line_end + 2;  // advance past \r\n
    }

    // If we reach here, we never saw the blank line
    // This means the request is incomplete
    err = ParseError::MissingCRLF;
    return false;
}

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────

// Find needle in [pos, end) — no std::string, no allocation
const char* Parser::find(
    const char* pos,
    const char* end,
    std::string_view needle
) noexcept {
    if (needle.empty() || pos >= end) return nullptr;

    std::size_t remaining = static_cast<std::size_t>(end - pos);
    std::size_t nlen      = needle.size();

    if (nlen > remaining) return nullptr;

    // Scan using memchr for the first character, then memcmp
    // This is faster than a naive loop for longer needles
    const char* p   = pos;
    const char* lim = end - nlen + 1;

    while (p < lim) {
        // Find first char of needle quickly
        const char* found = static_cast<const char*>(
            std::memchr(p, needle[0], static_cast<std::size_t>(lim - p))
        );
        if (!found) return nullptr;

        // Check full needle match
        if (std::memcmp(found, needle.data(), nlen) == 0) {
            return found;
        }

        p = found + 1;  // no match, advance one byte
    }

    return nullptr;
}

// Trim leading/trailing whitespace from a string_view
// No allocation — just moves the start/end pointers
std::string_view Parser::trim(std::string_view sv) noexcept {
    // Trim leading spaces and tabs
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) {
        sv.remove_prefix(1);
    }
    // Trim trailing spaces and tabs
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t')) {
        sv.remove_suffix(1);
    }
    return sv;
}

// Case-insensitive comparison for header name lookup
bool Parser::iequal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        // std::tolower is fine here — header names are ASCII
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────
//  HttpRequest::get_header
// ─────────────────────────────────────────────────────────────
std::optional<std::string_view> HttpRequest::get_header(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < header_count; ++i) {
        if (Parser::iequal(headers[i].name, name)) {
            return headers[i].value;
        }
    }
    return std::nullopt;
}

} // namespace http