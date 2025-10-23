#include "test_utils.h"
#include "../src/server/http.h"
#include <string.h>
#include <stdbool.h>


// ============================================================================
// TESTS DE HTTP PARSING
// ============================================================================

TEST(test_parse_simple_get) {
    const char *request = 
        "GET /status HTTP/1.0\r\n"
        "\r\n";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, 0);
    ASSERT_STR_EQ(req.method, "GET");
    ASSERT_STR_EQ(req.path, "/status");
    ASSERT_STR_EQ(req.query, "");
    ASSERT_STR_EQ(req.version, "HTTP/1.0");
}

TEST(test_parse_get_with_query) {
    const char *request = 
        "GET /isprime?n=97 HTTP/1.0\r\n"
        "\r\n";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, 0);
    ASSERT_STR_EQ(req.method, "GET");
    ASSERT_STR_EQ(req.path, "/isprime");
    ASSERT_STR_EQ(req.query, "n=97");
}

TEST(test_parse_with_headers) {
    const char *request = 
        "GET /test HTTP/1.0\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 42\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, 0);
    ASSERT_STR_EQ(req.host, "localhost:8080");
    ASSERT_EQ(req.content_length, 42);
    ASSERT_FALSE(req.connection_close); // keep-alive
}

TEST(test_parse_complex_query) {
    const char *request = 
        "GET /random?count=10&min=1&max=100 HTTP/1.0\r\n"
        "\r\n";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, 0);
    ASSERT_STR_EQ(req.path, "/random");
    ASSERT_STR_EQ(req.query, "count=10&min=1&max=100");
}

TEST(test_parse_malformed_no_version) {
    const char *request = "GET /test\r\n\r\n";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, -1); // Debe fallar
}

TEST(test_parse_malformed_no_crlf) {
    const char *request = "GET /test HTTP/1.0";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, -1); // Debe fallar
}

TEST(test_parse_http_1_1) {
    const char *request = 
        "GET /test HTTP/1.1\r\n"
        "\r\n";
    
    http_request_t req;
    int result = http_parse_request(request, &req);
    
    ASSERT_EQ(result, 0);
    ASSERT_STR_EQ(req.version, "HTTP/1.1");
}

// ============================================================================
// TESTS DE SPLIT URI
// ============================================================================

TEST(test_split_uri_with_query) {
    char path[256];
    char query[2048];
    
    http_split_uri("/isprime?n=97", path, query);
    
    ASSERT_STR_EQ(path, "/isprime");
    ASSERT_STR_EQ(query, "n=97");
}

TEST(test_split_uri_without_query) {
    char path[256];
    char query[2048];
    
    http_split_uri("/status", path, query);
    
    ASSERT_STR_EQ(path, "/status");
    ASSERT_STR_EQ(query, "");
}

TEST(test_split_uri_complex) {
    char path[256];
    char query[2048];
    
    http_split_uri("/test?a=1&b=2&c=3", path, query);
    
    ASSERT_STR_EQ(path, "/test");
    ASSERT_STR_EQ(query, "a=1&b=2&c=3");
}

// ============================================================================
// TESTS DE METHOD SUPPORT
// ============================================================================

TEST(test_method_get_supported) {
    ASSERT_TRUE(http_is_method_supported("GET"));
}

TEST(test_method_head_supported) {
    ASSERT_TRUE(http_is_method_supported("HEAD"));
}

TEST(test_method_post_not_supported) {
    ASSERT_FALSE(http_is_method_supported("POST"));
}

TEST(test_method_put_not_supported) {
    ASSERT_FALSE(http_is_method_supported("PUT"));
}

// ============================================================================
// TESTS DE STATUS TEXT
// ============================================================================

TEST(test_status_text_200) {
    ASSERT_STR_EQ(http_status_text(200), "OK");
}

TEST(test_status_text_404) {
    ASSERT_STR_EQ(http_status_text(404), "Not Found");
}

TEST(test_status_text_500) {
    ASSERT_STR_EQ(http_status_text(500), "Internal Server Error");
}

TEST(test_status_text_503) {
    ASSERT_STR_EQ(http_status_text(503), "Service Unavailable");
}

// ============================================================================
// TESTS DE PATH SAFETY
// ============================================================================

TEST(test_path_safe_normal) {
    ASSERT_TRUE(http_is_path_safe("/isprime"));
    ASSERT_TRUE(http_is_path_safe("/api/test"));
    ASSERT_TRUE(http_is_path_safe("/status"));
}

TEST(test_path_unsafe_dotdot) {
    ASSERT_FALSE(http_is_path_safe("../etc/passwd"));
    ASSERT_FALSE(http_is_path_safe("/test/../secret"));
    ASSERT_FALSE(http_is_path_safe("/api/../../file"));
}

TEST(test_path_unsafe_trailing_dotdot) {
    ASSERT_FALSE(http_is_path_safe("/test/.."));
}

// ============================================================================
// TESTS DE CONTENT TYPE
// ============================================================================

TEST(test_content_type_html) {
    ASSERT_STR_EQ(http_content_type_from_file("index.html"), "text/html");
}

TEST(test_content_type_json) {
    ASSERT_STR_EQ(http_content_type_from_file("data.json"), "application/json");
}

TEST(test_content_type_txt) {
    ASSERT_STR_EQ(http_content_type_from_file("file.txt"), "text/plain");
}

TEST(test_content_type_unknown) {
    ASSERT_STR_EQ(http_content_type_from_file("file.xyz"), "application/octet-stream");
}

TEST(test_content_type_no_extension) {
    ASSERT_STR_EQ(http_content_type_from_file("README"), "application/octet-stream");
}

// ============================================================================
// TEST SUITE
// ============================================================================

void test_http_parser_all() {
    printf("\n📦 Test Suite: HTTP Parser\n");
    printf("==========================================\n");
    
    // Tests de parsing
    RUN_TEST(test_parse_simple_get);
    RUN_TEST(test_parse_get_with_query);
    RUN_TEST(test_parse_with_headers);
    RUN_TEST(test_parse_complex_query);
    RUN_TEST(test_parse_malformed_no_version);
    RUN_TEST(test_parse_malformed_no_crlf);
    RUN_TEST(test_parse_http_1_1);
    
    // Tests de split URI
    RUN_TEST(test_split_uri_with_query);
    RUN_TEST(test_split_uri_without_query);
    RUN_TEST(test_split_uri_complex);
    
    // Tests de métodos
    RUN_TEST(test_method_get_supported);
    RUN_TEST(test_method_head_supported);
    RUN_TEST(test_method_post_not_supported);
    RUN_TEST(test_method_put_not_supported);
    
    // Tests de status text
    RUN_TEST(test_status_text_200);
    RUN_TEST(test_status_text_404);
    RUN_TEST(test_status_text_500);
    RUN_TEST(test_status_text_503);
    
    // Tests de path safety
    RUN_TEST(test_path_safe_normal);
    RUN_TEST(test_path_unsafe_dotdot);
    RUN_TEST(test_path_unsafe_trailing_dotdot);
    
    // Tests de content type
    RUN_TEST(test_content_type_html);
    RUN_TEST(test_content_type_json);
    RUN_TEST(test_content_type_txt);
    RUN_TEST(test_content_type_unknown);
    RUN_TEST(test_content_type_no_extension);
    
    printf("\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    test_http_parser_all();
    return 0;
}
