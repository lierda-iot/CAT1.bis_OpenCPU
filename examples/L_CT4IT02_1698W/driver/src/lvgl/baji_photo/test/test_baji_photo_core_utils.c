#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "baji_photo_core_utils.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_skip_ws_and_find_key(void)
{
    const char *json = "  {  \"value\" : \"abc\" , \"count\": 42 }";
    const char *start = json;
    const char *end = json + strlen(json);
    const char *trimmed = baji_photo_skip_ws(start, end);
    const char *value = baji_photo_find_json_key(start, end, "value");
    const char *count = baji_photo_find_json_key(start, end, "count");

    TEST_ASSERT_EQUAL_PTR(start + 2, trimmed);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(count);
    TEST_ASSERT_EQUAL_CHAR('"', *value);
    TEST_ASSERT_EQUAL_CHAR('4', *count);
}

void test_parse_json_string_and_numbers(void)
{
    const char *json =
        "{ \"id\": \"task_1234\\\\\\\"ok\", \"size\": 360, \"crc\": \"0x1A2B3C4D\", \"plain\": 0x11 }";
    const char *end = json + strlen(json);
    char id[32];
    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t plain = 0;

    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_json_string(json, end, "id", id, sizeof(id)));
    TEST_ASSERT_EQUAL_STRING("task_1234\\\"ok", id);
    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_json_u32(json, end, "size", &size));
    TEST_ASSERT_EQUAL_UINT32(360u, size);
    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_json_hex_u32(json, end, "crc", &crc));
    TEST_ASSERT_EQUAL_HEX32(0x1A2B3C4Du, crc);
    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_json_hex_u32(json, end, "plain", &plain));
    TEST_ASSERT_EQUAL_HEX32(0x11u, plain);
}

void test_parse_manifest_format_matches_project_macros(void)
{
    const char *gif_json = "{ \"format\": \"gif\" }";
    const char *jpeg_json = "{ \"format\": \"JPEG\" }";
    const char *png_json = "{ \"format\": \"png\" }";
    const char *bjp_json = "{ \"format\": \"rgb565\" }";
    const char *default_json = "{ \"id\": \"task_1\" }";
    baji_photo_format_t format = BAJI_PHOTO_FORMAT_BJP;

    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_manifest_format(gif_json, gif_json + strlen(gif_json), &format));
    TEST_ASSERT_EQUAL_INT(BAJI_PHOTO_FORMAT_GIF, format);
    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_manifest_format(jpeg_json, jpeg_json + strlen(jpeg_json), &format));
    TEST_ASSERT_EQUAL_INT(BAJI_PHOTO_FORMAT_JPEG, format);
    TEST_ASSERT_EQUAL_INT(-1, baji_photo_parse_manifest_format(png_json, png_json + strlen(png_json), &format));
    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_manifest_format(bjp_json, bjp_json + strlen(bjp_json), &format));
    TEST_ASSERT_EQUAL_INT(BAJI_PHOTO_FORMAT_BJP, format);
    TEST_ASSERT_EQUAL_INT(0, baji_photo_parse_manifest_format(default_json, default_json + strlen(default_json), &format));
    TEST_ASSERT_EQUAL_INT(BAJI_PHOTO_FORMAT_BJP, format);
}

void test_manifest_validators_reject_bad_values(void)
{
    TEST_ASSERT_EQUAL_INT(0, baji_photo_manifest_id_valid("task_abcd-1234"));
    TEST_ASSERT_EQUAL_INT(-1, baji_photo_manifest_id_valid("task bad"));
    TEST_ASSERT_EQUAL_INT(-1, baji_photo_manifest_id_valid(""));
    TEST_ASSERT_EQUAL_INT(0, baji_photo_manifest_remote_path_valid("badge/api/image/img_1234"));
    TEST_ASSERT_EQUAL_INT(-1, baji_photo_manifest_remote_path_valid("/badge/api/image/img_1234"));
    TEST_ASSERT_EQUAL_INT(-1, baji_photo_manifest_remote_path_valid("badge/api/../image"));
    TEST_ASSERT_EQUAL_INT(-1, baji_photo_manifest_remote_path_valid("badge/api/image/with space"));
}

void test_true_color_bounds_and_size_follow_project_limits(void)
{
    TEST_ASSERT_TRUE(baji_photo_true_color_dims_valid(360u, 360u));
    TEST_ASSERT_TRUE(baji_photo_true_color_dims_valid(1u, 1u));
    TEST_ASSERT_FALSE(baji_photo_true_color_dims_valid(0u, 1u));
    TEST_ASSERT_FALSE(baji_photo_true_color_dims_valid(361u, 360u));
    TEST_ASSERT_EQUAL_UINT32(360u * 360u * 2u, baji_photo_true_color_data_size(360u, 360u));
    TEST_ASSERT_EQUAL_UINT32(240u * 240u * 2u, baji_photo_true_color_data_size(240u, 240u));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_skip_ws_and_find_key);
    RUN_TEST(test_parse_json_string_and_numbers);
    RUN_TEST(test_parse_manifest_format_matches_project_macros);
    RUN_TEST(test_manifest_validators_reject_bad_values);
    RUN_TEST(test_true_color_bounds_and_size_follow_project_limits);
    return UNITY_END();
}
