#include "unity.h"

#include "demo_gnss_core.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_coordinate_positive_and_negative_hemisphere(void)
{
    int32_t e7;

    TEST_ASSERT_TRUE(gnss_coordinate_to_e7("3103.33427", 'N', &e7));
    TEST_ASSERT_EQUAL_INT32(310555711, e7);

    TEST_ASSERT_TRUE(gnss_coordinate_to_e7("12015.55123", 'E', &e7));
    TEST_ASSERT_EQUAL_INT32(1202591871, e7);

    TEST_ASSERT_TRUE(gnss_coordinate_to_e7("3103.33427", 'S', &e7));
    TEST_ASSERT_EQUAL_INT32(-310555711, e7);

    TEST_ASSERT_TRUE(gnss_coordinate_to_e7("12015.55123", 'W', &e7));
    TEST_ASSERT_EQUAL_INT32(-1202591871, e7);
}

void test_coordinate_invalid_hemisphere_and_minutes(void)
{
    int32_t e7 = 123;

    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("3103.33427", 'X', &e7));
    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("3103.33427", 's', &e7));
    TEST_ASSERT_EQUAL_INT32(123, e7);

    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("31060.12345", 'N', &e7));
    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("31059.99999", 'S', &e7) == false);
}

void test_coordinate_null_empty_rejected(void)
{
    int32_t e7;

    TEST_ASSERT_FALSE(gnss_coordinate_to_e7(NULL, 'N', &e7));
    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("", 'N', &e7));
    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("3103.33427", 'N', NULL));
    TEST_ASSERT_FALSE(gnss_coordinate_to_e7("abc", 'N', &e7));
}

void test_coordinate_fraction_truncated_to_7_digits(void)
{
    int32_t e7;

    TEST_ASSERT_TRUE(gnss_coordinate_to_e7("3103.60123456", 'N', &e7));
    TEST_ASSERT_EQUAL_INT32(310600205, e7);
    TEST_ASSERT_TRUE(gnss_coordinate_to_e7("3103.60", 'N', &e7));
    TEST_ASSERT_EQUAL_INT32(310600000, e7);
}

void test_split_fields_nmea_sentence_and_empty(void)
{
    char copy[128];
    char *field[20];
    int count;

    count = gnss_split_fields(
        "$GNRMC,utc,A,3103.33427,N,12015.55123,E,0.0,0.0,date",
        copy, sizeof(copy), field, 20);
    TEST_ASSERT_EQUAL_INT(10, count);
    TEST_ASSERT_EQUAL_STRING("$GNRMC", field[0]);
    TEST_ASSERT_EQUAL_STRING("3103.33427", field[3]);
    TEST_ASSERT_EQUAL_CHAR('N', field[4][0]);
    TEST_ASSERT_EQUAL_STRING("12015.55123", field[5]);
    TEST_ASSERT_EQUAL_CHAR('E', field[6][0]);

    count = gnss_split_fields(",a,,b", copy, sizeof(copy), field, 20);
    TEST_ASSERT_EQUAL_INT(4, count);
    TEST_ASSERT_EQUAL_STRING("", field[0]);
    TEST_ASSERT_EQUAL_STRING("b", field[3]);

    TEST_ASSERT_EQUAL_INT(0, gnss_split_fields(NULL, copy, sizeof(copy),
                                               field, 20));
    TEST_ASSERT_EQUAL_INT(0, gnss_split_fields("", copy, sizeof(copy),
                                               field, 20));
}

int main(int argc, char **argv)
{
    (void)UnityParseOptions(argc, argv);
    UNITY_BEGIN();
    RUN_TEST(test_coordinate_positive_and_negative_hemisphere);
    RUN_TEST(test_coordinate_invalid_hemisphere_and_minutes);
    RUN_TEST(test_coordinate_null_empty_rejected);
    RUN_TEST(test_coordinate_fraction_truncated_to_7_digits);
    RUN_TEST(test_split_fields_nmea_sentence_and_empty);
    return UNITY_END();
}
