#include "unity.h"

#include "demo_xiaozhi_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_parse_ws_url_host_port_path(void)
{
    char host[64];
    char path[64];
    int port = 0;

    TEST_ASSERT_EQUAL_INT(0,
        demo_xiaozhi_core_parse_ws_url(
            "wss://api.example.com:8443/v1/voice",
            host, sizeof(host), path, sizeof(path), &port));
    TEST_ASSERT_EQUAL_STRING("api.example.com", host);
    TEST_ASSERT_EQUAL_STRING("/v1/voice", path);
    TEST_ASSERT_EQUAL_INT(8443, port);
}

static void test_parse_ws_url_default_port_and_root(void)
{
    char host[64];
    char path[64];
    int port = 0;

    TEST_ASSERT_EQUAL_INT(0,
        demo_xiaozhi_core_parse_ws_url(
            "wss://xiaozhi.example.com",
            host, sizeof(host), path, sizeof(path), &port));
    TEST_ASSERT_EQUAL_STRING("xiaozhi.example.com", host);
    TEST_ASSERT_EQUAL_STRING("/", path);
    TEST_ASSERT_EQUAL_INT(443, port);

    TEST_ASSERT_EQUAL_INT(0,
        demo_xiaozhi_core_parse_ws_url(
            "wss://xiaozhi.example.com/",
            host, sizeof(host), path, sizeof(path), &port));
    TEST_ASSERT_EQUAL_STRING("/", path);
    TEST_ASSERT_EQUAL_INT(443, port);
}

static void test_parse_ws_url_invalid_and_truncation(void)
{
    char host[8];
    char path[64];
    int port = 0;

    TEST_ASSERT_EQUAL_INT(-1,
        demo_xiaozhi_core_parse_ws_url(
            NULL, host, sizeof(host), path, sizeof(path), &port));
    TEST_ASSERT_EQUAL_INT(-1,
        demo_xiaozhi_core_parse_ws_url(
            "http://example.com/", host, sizeof(host), path, sizeof(path), &port));
    /* Empty host (original code path returns -2 before the NULL checks). */
    TEST_ASSERT_EQUAL_INT(-2,
        demo_xiaozhi_core_parse_ws_url(
            "wss://", host, sizeof(host), path, sizeof(path), &port));

    /* Host buffer too small -> -2. */
    TEST_ASSERT_EQUAL_INT(-2,
        demo_xiaozhi_core_parse_ws_url(
            "wss://example.com/", host, sizeof(host), path, sizeof(path), &port));
}

static void test_fnv1a_known_vector_and_seed(void)
{
    /* FNV-1a 32 check values with the standard offset basis. */
    TEST_ASSERT_EQUAL_UINT32(0x811C9DC5u,
                             demo_xiaozhi_core_fnv1a("", 2166136261u));
    TEST_ASSERT_EQUAL_UINT32(0x4F9F2CABu,
                             demo_xiaozhi_core_fnv1a("hello", 2166136261u));

    /* Same text, different seed -> different hash; NULL keeps seed. */
    TEST_ASSERT_NOT_EQUAL_UINT32(demo_xiaozhi_core_fnv1a("abc", 1u),
                                 demo_xiaozhi_core_fnv1a("abc", 2u));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu,
                             demo_xiaozhi_core_fnv1a(NULL, 0xDEADBEEFu));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_parse_ws_url_host_port_path);
    RUN_TEST(test_parse_ws_url_default_port_and_root);
    RUN_TEST(test_parse_ws_url_invalid_and_truncation);
    RUN_TEST(test_fnv1a_known_vector_and_seed);
    return UNITY_END();
}
