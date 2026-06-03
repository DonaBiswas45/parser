#include "http_parser.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

// Simple test harness — no external dependencies
// Each test prints PASS or FAIL

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(expr) do { \
    ++tests_run; \
    if (expr) { \
        std::cout << "  PASS: " #expr "\n"; \
        ++tests_passed; \
    } else { \
        std::cout << "  FAIL: " #expr "  (line " << __LINE__ << ")\n"; \
    } \
} while(0)

// ─────────────────────────────────────────────
void test_simple_get() {
    std::cout << "\n[ test_simple_get ]\n";

    const char* raw =
        "GET /api/users HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Accept: application/json\r\n"
        "\r\n";

    auto result = http::Parser::parse(raw, std::strlen(raw));

    CHECK(result.ok());
    CHECK(result.request.method  == "GET");
    CHECK(result.request.path    == "/api/users");
    CHECK(result.request.version == "HTTP/1.1");
    CHECK(result.request.header_count == 2);
    CHECK(result.request.headers[0].name  == "Host");
    CHECK(result.request.headers[0].value == "example.com");
    CHECK(result.request.headers[1].name  == "Accept");
    CHECK(result.request.headers[1].value == "application/json");
    CHECK(result.request.body.empty());
}

// ─────────────────────────────────────────────
void test_post_with_body() {
    std::cout << "\n[ test_post_with_body ]\n";

    const char* raw =
        "POST /api/login HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "{\"user\":\"dona\",\"pw\":\"1234\"}";

    auto result = http::Parser::parse(raw, std::strlen(raw));

    CHECK(result.ok());
    CHECK(result.request.method == "POST");
    CHECK(result.request.path   == "/api/login");
    CHECK(result.request.header_count == 3);
    CHECK(result.request.body == "{\"user\":\"dona\",\"pw\":\"1234\"}");

    // Test get_header (case-insensitive)
    auto ct = result.request.get_header("content-type");
    CHECK(ct.has_value());
    CHECK(ct.value() == "application/json");
}

// ─────────────────────────────────────────────
void test_header_whitespace_trimming() {
    std::cout << "\n[ test_header_whitespace_trimming ]\n";

    // Headers with extra spaces around the value
    const char* raw =
        "GET / HTTP/1.1\r\n"
        "X-Custom:   some value   \r\n"
        "X-Tab:\t tabbed \t\r\n"
        "\r\n";

    auto result = http::Parser::parse(raw, std::strlen(raw));

    CHECK(result.ok());
    CHECK(result.request.get_header("X-Custom").value() == "some value");
    CHECK(result.request.get_header("X-Tab").value()    == "tabbed");
}

// ─────────────────────────────────────────────
void test_error_missing_crlf() {
    std::cout << "\n[ test_error_missing_crlf ]\n";

    // No \r\n after request line
    const char* raw = "GET /path HTTP/1.1";
    auto result = http::Parser::parse(raw, std::strlen(raw));

    CHECK(!result.ok());
    CHECK(result.error == http::ParseError::MissingCRLF);
}

// ─────────────────────────────────────────────
void test_error_invalid_header() {
    std::cout << "\n[ test_error_invalid_header ]\n";

    // Header line with no colon
    const char* raw =
        "GET / HTTP/1.1\r\n"
        "BadHeaderNoColon\r\n"
        "\r\n";

    auto result = http::Parser::parse(raw, std::strlen(raw));

    CHECK(!result.ok());
    CHECK(result.error == http::ParseError::InvalidHeader);
}

// ─────────────────────────────────────────────
void test_empty_buffer() {
    std::cout << "\n[ test_empty_buffer ]\n";

    auto result = http::Parser::parse(nullptr, 0);
    CHECK(!result.ok());
    CHECK(result.error == http::ParseError::BufferTooShort);
}

// ─────────────────────────────────────────────
void test_delete_method() {
    std::cout << "\n[ test_delete_method ]\n";

    const char* raw =
        "DELETE /api/users/42 HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Authorization: Bearer token123\r\n"
        "\r\n";

    auto result = http::Parser::parse(raw, std::strlen(raw));

    CHECK(result.ok());
    CHECK(result.request.method == "DELETE");
    CHECK(result.request.path   == "/api/users/42");

    auto auth = result.request.get_header("Authorization");
    CHECK(auth.has_value());
    CHECK(auth.value() == "Bearer token123");
}

// ─────────────────────────────────────────────
int main() {
    std::cout << "=== HTTP Parser Tests ===\n";

    test_simple_get();
    test_post_with_body();
    test_header_whitespace_trimming();
    test_error_missing_crlf();
    test_error_invalid_header();
    test_empty_buffer();
    test_delete_method();

    std::cout << "\n=== Results: "
              << tests_passed << "/" << tests_run << " passed ===\n";

    return (tests_passed == tests_run) ? 0 : 1;
}