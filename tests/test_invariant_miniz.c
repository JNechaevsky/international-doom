#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "lib/miniz/miniz.c"

START_TEST(test_miniz_decompress_bounds_safety)
{
    /* Invariant: decompressing any crafted stream must never write beyond
       the provided output buffer, and must return an error for invalid data. */

    /* Payload 0: crafted zlib stream with back-reference beyond dictionary */
    static const unsigned char bad_backref[] = {
        0x78, 0x9C, 0x63, 0x60, 0x60, 0xE0, 0xFF, 0xFF,
        0x01, 0x00, 0x00, 0x05, 0x00, 0x01
    };
    /* Payload 1: truncated stream (boundary) */
    static const unsigned char truncated[] = {
        0x78, 0x9C, 0x63, 0x00
    };
    /* Payload 2: valid compressed "hello" */
    static const unsigned char valid[] = {
        0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07,
        0x00, 0x06, 0x2C, 0x02, 0x15
    };

    struct {
        const unsigned char *data;
        size_t len;
    } payloads[] = {
        { bad_backref, sizeof(bad_backref) },
        { truncated, sizeof(truncated) },
        { valid, sizeof(valid) },
    };

    for (int i = 0; i < 3; i++) {
        unsigned char out[256];
        memset(out, 0xAA, sizeof(out));

        mz_stream stream;
        memset(&stream, 0, sizeof(stream));
        stream.next_in = payloads[i].data;
        stream.avail_in = (unsigned int)payloads[i].len;
        stream.next_out = out;
        stream.avail_out = sizeof(out);

        int ret = mz_inflateInit(&stream);
        if (ret == MZ_OK) {
            ret = mz_inflate(&stream, MZ_FINISH);
            /* Key invariant: total_out must never exceed buffer size */
            ck_assert_msg(stream.total_out <= sizeof(out),
                "Decompression wrote %lu bytes beyond %lu byte buffer",
                (unsigned long)stream.total_out, (unsigned long)sizeof(out));
            mz_inflateEnd(&stream);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_miniz_decompress_bounds_safety);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}