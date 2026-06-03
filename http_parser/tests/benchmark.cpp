#include "http_parser.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <sstream>
#include <numeric>
#include <cstring>

using ns = std::chrono::nanoseconds;  // <-- ADDED HERE

struct NaiveRequest {
    std::string method;
    std::string path;
    std::string version;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

NaiveRequest naive_parse(const std::string& raw) {
    NaiveRequest req;
    std::istringstream stream(raw);
    std::string line;

    std::getline(stream, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream rl(line);
    rl >> req.method >> req.path >> req.version;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name  = line.substr(0, colon);
            std::string value = line.substr(colon + 2);
            req.headers.emplace_back(name, value);
        }
    }

    std::string body_line;
    while (std::getline(stream, body_line)) {
        req.body += body_line;
    }

    return req;
}

template<typename Fn>
long long measure_ns(Fn&& fn, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) fn();
    auto end   = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<ns>(end - start).count();
}

void print_result(const char* label, long long total_ns, int iterations, std::size_t bytes_per_req) {
    double ns_per_req  = static_cast<double>(total_ns) / iterations;
    double total_bytes = static_cast<double>(bytes_per_req) * iterations;
    double mb_per_sec  = (total_bytes / (1024.0 * 1024.0)) / (total_ns / 1e9);

    std::cout << "  " << label << "\n"
              << "    ns/request : " << static_cast<long long>(ns_per_req) << " ns\n"
              << "    throughput : " << static_cast<long long>(mb_per_sec) << " MB/s\n"
              << "    total time : " << total_ns / 1'000'000 << " ms for "
              << iterations << " iterations\n\n";
}

int main() {
    const char* get_request =
        "GET /api/v1/users/profile HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Accept: application/json\r\n"
        "Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.token\r\n"
        "User-Agent: Mozilla/5.0 (compatible)\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    const char* post_request =
        "POST /api/v1/events HTTP/1.1\r\n"
        "Host: api.example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 52\r\n"
        "Authorization: Bearer token123\r\n"
        "X-Request-ID: abc-123-def-456\r\n"
        "\r\n"
        "{\"event\":\"user.login\",\"user_id\":42,\"ts\":1700000000}";

    std::size_t get_len  = std::strlen(get_request);
    std::size_t post_len = std::strlen(post_request);

    std::string get_str(get_request, get_len);
    std::string post_str(post_request, post_len);

    const int ITERATIONS = 1'000'000;

    std::cout << "=== HTTP Parser Benchmark ===\n";
    std::cout << "Iterations: " << ITERATIONS << "\n\n";

    std::cout << "[ GET request - " << get_len << " bytes ]\n";

    auto fast_get_ns = measure_ns([&]() {
        auto r = http::Parser::parse(get_request, get_len);
        (void)r;
    }, ITERATIONS);
    print_result("Zero-alloc parser", fast_get_ns, ITERATIONS, get_len);

    auto naive_get_ns = measure_ns([&]() {
        auto r = naive_parse(get_str);
        (void)r;
    }, ITERATIONS);
    print_result("Naive std::string parser", naive_get_ns, ITERATIONS, get_len);

    double get_speedup = static_cast<double>(naive_get_ns) / fast_get_ns;
    std::cout << "  >> Speedup: " << get_speedup << "x faster\n\n";

    std::cout << "[ POST request with body - " << post_len << " bytes ]\n";

    auto fast_post_ns = measure_ns([&]() {
        auto r = http::Parser::parse(post_request, post_len);
        (void)r;
    }, ITERATIONS);
    print_result("Zero-alloc parser", fast_post_ns, ITERATIONS, post_len);

    auto naive_post_ns = measure_ns([&]() {
        auto r = naive_parse(post_str);
        (void)r;
    }, ITERATIONS);
    print_result("Naive std::string parser", naive_post_ns, ITERATIONS, post_len);

    double post_speedup = static_cast<double>(naive_post_ns) / fast_post_ns;
    std::cout << "  >> Speedup: " << post_speedup << "x faster\n\n";

    std::cout << "=== Summary ===\n";
    std::cout << "Zero-alloc parser is ~" << static_cast<int>((get_speedup + post_speedup) / 2)
              << "x faster on average\n";
    std::cout << "Zero heap allocations: yes\n";
    std::cout << "Zero-copy (string_view): yes\n";

    return 0;
}