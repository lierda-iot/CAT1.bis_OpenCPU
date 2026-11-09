#include "unity.h"

#include "demo_lvgl_photo_lfs_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_crc32_known_vector_and_empty(void)
{
    /* Standard CRC-32 check value: CRC32("123456789") == 0xCBF43926. */
    uint32_t crc;

    crc = demo_lvgl_photo_lfs_core_crc32_update(0xFFFFFFFFu, "123456789", 9u);
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u,
                             demo_lvgl_photo_lfs_core_crc32_finish(crc));

    crc = demo_lvgl_photo_lfs_core_crc32_update(0xFFFFFFFFu, NULL, 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, demo_lvgl_photo_lfs_core_crc32_finish(crc));
}

static void test_crc32_incremental_update(void)
{
    uint32_t one_shot;
    uint32_t incremental;

    one_shot = demo_lvgl_photo_lfs_core_crc32_update(0xFFFFFFFFu,
                                                    "hello world", 11u);
    incremental = demo_lvgl_photo_lfs_core_crc32_update(0xFFFFFFFFu, "hello", 5u);
    incremental = demo_lvgl_photo_lfs_core_crc32_update(incremental, " world", 6u);
    TEST_ASSERT_EQUAL_UINT32(one_shot, incremental);
}

static void test_id_validity_boundaries(void)
{
    char long_id[64];
    unsigned int i;

    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_id_is_valid(NULL));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_id_is_valid(""));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_id_is_valid("has space"));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_id_is_valid("a/b"));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_id_is_valid("dot."));

    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_id_is_valid("abc123"));
    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_id_is_valid("builtin_001"));
    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_id_is_valid("a-b_c"));
    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_id_is_valid("A-Z_0-9"));

    /* Exactly 32 chars is allowed, 33 is rejected. */
    for (i = 0; i < 32u; ++i) {
        long_id[i] = 'a';
    }
    long_id[32] = '\0';
    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_id_is_valid(long_id));
    long_id[32] = 'a';
    long_id[33] = '\0';
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_id_is_valid(long_id));
}

static void test_build_paths_representative_formats(void)
{
    TEST_ASSERT_EQUAL_STRING("bjp",
        demo_lvgl_photo_lfs_core_format_name(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP));
    TEST_ASSERT_EQUAL_STRING("gif",
        demo_lvgl_photo_lfs_core_format_name(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP,
                          demo_lvgl_photo_lfs_core_format_from_name(NULL));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF,
                          demo_lvgl_photo_lfs_core_format_from_name("gif"));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF,
                          demo_lvgl_photo_lfs_core_format_from_name("GIF"));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP,
                          demo_lvgl_photo_lfs_core_format_from_name("rgb565"));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP,
                          demo_lvgl_photo_lfs_core_format_from_name("unknown"));

    char path[96];

    TEST_ASSERT_EQUAL_INT(0,
        demo_lvgl_photo_lfs_core_build_path(
            "builtin_001", DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/flash/baji/photos/photo_builtin_001.bjp", path);

    TEST_ASSERT_EQUAL_INT(0,
        demo_lvgl_photo_lfs_core_build_path(
            "abc", DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_GIF, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/flash/baji/gifs/abc.gif", path);

    TEST_ASSERT_EQUAL_INT(0,
        demo_lvgl_photo_lfs_core_build_tmp_path(
            "abc", DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/flash/baji/tmp/photo_abc.tmp", path);
}

static void test_build_path_truncation_and_invalid(void)
{
    char path[32];

    /* Buffer too small for the full path -> rejected. */
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER,
        demo_lvgl_photo_lfs_core_build_path(
            "builtin_001", DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, sizeof(path)));

    /* Exact fit: "/flash/baji/photos/photo_a.bjp" is 29 chars + NUL. */
    TEST_ASSERT_EQUAL_INT(0,
        demo_lvgl_photo_lfs_core_build_path(
            "a", DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/flash/baji/photos/photo_a.bjp", path);

    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER,
        demo_lvgl_photo_lfs_core_build_path(
            "bad id", DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER,
        demo_lvgl_photo_lfs_core_build_path(NULL,
            DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER,
        demo_lvgl_photo_lfs_core_build_path("a",
            DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, NULL, 32u));
    TEST_ASSERT_EQUAL_INT(DEMO_LVGL_PHOTO_LFS_CORE_INVALID_PARAMETER,
        demo_lvgl_photo_lfs_core_build_path("a",
            DEMO_LVGL_PHOTO_LFS_CORE_FORMAT_BJP, path, 0u));
}

static void test_rgb565_size_and_overflow(void)
{
    uint32_t size = 0u;

    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_rgb565_size(360u, 360u, &size));
    TEST_ASSERT_EQUAL_UINT32(259200u, size);

    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_rgb565_size(0u, 360u, &size));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_rgb565_size(360u, 0u, &size));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_rgb565_size(360u, 360u, NULL));

    /* 65535x65535 = 4294836225 pixels; *2 overflows uint32_t. */
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_rgb565_size(65535u, 65535u, &size));
}

static void test_builtin_id_detection(void)
{
    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_is_builtin_id("builtin_001"));
    TEST_ASSERT_TRUE(demo_lvgl_photo_lfs_core_is_builtin_id("builtin_"));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_is_builtin_id("builtin001"));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_is_builtin_id("user_001"));
    TEST_ASSERT_FALSE(demo_lvgl_photo_lfs_core_is_builtin_id(NULL));
}

typedef struct {
    char id[33];
    int value;
} test_find_item_t;

static void test_find_item_with_stride(void)
{
    test_find_item_t items[3];
    unsigned int i;

    for (i = 0; i < 3u; ++i) {
        items[i].value = (int)i * 10;
    }
    (void)snprintf(items[0].id, sizeof(items[0].id), "%s", "alpha");
    (void)snprintf(items[1].id, sizeof(items[1].id), "%s", "beta");
    (void)snprintf(items[2].id, sizeof(items[2].id), "%s", "gamma");

    TEST_ASSERT_EQUAL_INT(0,
        demo_lvgl_photo_lfs_core_find_item(items, 3u, sizeof(items[0]), "alpha"));
    TEST_ASSERT_EQUAL_INT(1,
        demo_lvgl_photo_lfs_core_find_item(items, 3u, sizeof(items[0]), "beta"));
    TEST_ASSERT_EQUAL_INT(2,
        demo_lvgl_photo_lfs_core_find_item(items, 3u, sizeof(items[0]), "gamma"));
    TEST_ASSERT_EQUAL_INT(-1,
        demo_lvgl_photo_lfs_core_find_item(items, 3u, sizeof(items[0]), "delta"));
    TEST_ASSERT_EQUAL_INT(-1,
        demo_lvgl_photo_lfs_core_find_item(NULL, 3u, sizeof(items[0]), "alpha"));
    TEST_ASSERT_EQUAL_INT(-1,
        demo_lvgl_photo_lfs_core_find_item(items, 3u, sizeof(items[0]), NULL));
    TEST_ASSERT_EQUAL_INT(-1,
        demo_lvgl_photo_lfs_core_find_item(items, 3u, 0u, "alpha"));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_crc32_known_vector_and_empty);
    RUN_TEST(test_crc32_incremental_update);
    RUN_TEST(test_id_validity_boundaries);
    RUN_TEST(test_build_paths_representative_formats);
    RUN_TEST(test_build_path_truncation_and_invalid);
    RUN_TEST(test_rgb565_size_and_overflow);
    RUN_TEST(test_builtin_id_detection);
    RUN_TEST(test_find_item_with_stride);
    return UNITY_END();
}
