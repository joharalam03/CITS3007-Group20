#include "../bun.h"

#include <check.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

// Helper: terminate abnormally, after printing a message to stderr
void die(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "fatal error: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);

  abort();
}


// Helper: open a test fixture by name, relative to the tests/ directory.
static const char *fixture(const char *filename) {
    // For simplicity, tests assume they are run from the project root, and
    // test BUN files live in tests/fixtures/{valid,invalid}. Adjust if needed.
    static char path[256];
    int res = snprintf(path, sizeof(path), "tests/fixtures/%s", filename);
    if (res < 0) {
      die("snprintf failed: %s", strerror(errno));
    }
    if ((size_t) res > sizeof(path)) {
      die("filename '%s' too big for buffer (would write %d bytes to %zu-size buffer)",
          filename, res, sizeof(path));
    }
    return path;
}

// Example test suite: header parsing

START_TEST(test_valid_minimal) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/01-empty.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_uint_eq(ctx.header.magic, BUN_MAGIC);
    ck_assert_uint_eq(ctx.header.version_major, 1);
    ck_assert_uint_eq(ctx.header.version_minor, 0);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_bad_magic) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/01-bad-magic.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_unsupported_version) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/02-bad-version.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);
    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_valid_alt_empty) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/02-alt-empty.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_uint_eq(ctx.header.magic, BUN_MAGIC);
    ck_assert_uint_eq(ctx.header.version_major, 1);
    ck_assert_uint_eq(ctx.header.version_minor, 0);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_valid_one_asset_header) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/03-one-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_uint_eq(ctx.header.magic, BUN_MAGIC);
    ck_assert_uint_eq(ctx.header.asset_count, 1);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_bad_offset_alignment) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/03-bad-offset-alignment.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_section_past_eof) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/04-section-past-eof.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_overlapping_sections) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/05-overlapping-sections.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_asset_name_past_string_table) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/06-asset-name-past-string-table.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_asset_name_nonprintable) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/07-asset-name-nonprintable.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_truncated_file) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/08-truncated-file.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_misaligned_section_size) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/09-misaligned-section-size.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_overlapping_with_nonprintable) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/10-overlapping-with-nonprintable.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_second_asset_empty_name) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/11-second-asset-empty-name.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_asset_name_oob) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/12-asset-name-oob.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_asset_empty_name) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/13-asset-empty-name.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_rle_zero_count) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/14-rle-zero-count.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_rle_bomb) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/15-rle-bomb.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_rle_truncated) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/16-rle-truncated.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_rle_odd_size) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/17-rle-odd-size.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_valid_binary_asset) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/04-binary-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_valid_multi_assets_slack) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/05-multi-assets-slack.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);

    fprintf(stderr, "parse_assets returned: %d\n", r);
    fprintf(stderr, "violation_count: %zu\n", ctx.violation_count);
    for (size_t i = 0; i < ctx.violation_count; i++) {
        fprintf(stderr, "violation[%zu]: %s\n", i, ctx.violations[i].message);
    }

    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_valid_rle_asset) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/06-rle-valid.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_valid_rle_large_stream) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/07-rle-large-stream.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST
/* START_TEST(test_violation_grows_capacity) {
    BunParseContext ctx = {0};

    for (int i = 0; i < 50; i++) {
        bun_add_violation(&ctx, "violation %d", i);
    }
    ck_assert_uint_eq(ctx.violation_count, 50);
    ck_assert_uint_ge(ctx.violation_capacity, 50);
    bun_ctx_free(&ctx);
}
END_TEST */

START_TEST(open_missing_file){
    BunParseContext ctx = {0};
    bun_result_t r = bun_open("tests/fixtures/does_not_exist.bun", &ctx);
    ck_assert_int_eq(r, BUN_ERR_IO);
    bun_ctx_free(&ctx);

}
END_TEST

// Assemble a test suite from our tests

START_TEST(test_print_summary_empty_file) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/01-empty.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    FILE *out = tmpfile();
    ck_assert_ptr_ne(out, NULL);

    bun_print_summary(&ctx, out);

    fclose(out);
    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_print_summary_binary_asset) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/04-binary-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    FILE *out = tmpfile();
    ck_assert_ptr_ne(out, NULL);

    bun_print_summary(&ctx, out);

    fclose(out);
    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_print_summary_valid_one_asset) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/03-one-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    FILE *out = tmpfile();
    ck_assert_ptr_ne(out, NULL);

    bun_print_summary(&ctx, out);

    fclose(out);
    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_parse_assets_zero_assets) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/01-empty.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);
    ck_assert_ptr_ne(ctx.records, NULL);
    ck_assert_uint_eq(ctx.record_count, 0);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_parse_assets_without_header) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_ERR_USAGE);
}
END_TEST

START_TEST(test_zlib_unsupported) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/18-zlib-unsupported.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_invalid_compression_unsupported) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/19-invalid-compression.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_nonzero_checksum_unsupported) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/20-nonzero-checksum.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_unknown_flags_unsupported) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/21-unknown-flags.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_UNSUPPORTED);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_print_summary_null_args) {
    FILE *out = tmpfile();
    ck_assert_ptr_ne(out, NULL);

    bun_print_summary(NULL, out);
    bun_print_summary(NULL, NULL);

    fclose(out);
}
END_TEST

START_TEST(test_print_summary_null_out) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/01-empty.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    bun_print_summary(&ctx, NULL);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_bun_open_missing_file) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open("tests/fixtures/does-not-exist.bun", &ctx);
    ck_assert_int_eq(r, BUN_ERR_IO);
}
END_TEST

START_TEST(test_many_asset_violations_growth) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("invalid/22-many-asset-violations.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_MALFORMED);

    ck_assert(ctx.violation_count >= 10);

    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_print_summary_long_name) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/08-long-name.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    FILE *out = tmpfile();
    ck_assert_ptr_ne(out, NULL);

    bun_print_summary(&ctx, out);

    fclose(out);
    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

START_TEST(test_print_summary_empty_data) {
    BunParseContext ctx = {0};

    bun_result_t r = bun_open(fixture("valid/09-empty-data-asset.bun"), &ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_header(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    r = bun_parse_assets(&ctx);
    ck_assert_int_eq(r, BUN_OK);

    FILE *out = tmpfile();
    ck_assert_ptr_ne(out, NULL);

    bun_print_summary(&ctx, out);

    fclose(out);
    bun_close(&ctx);
    bun_ctx_free(&ctx);
}
END_TEST

static Suite *bun_suite(void) {
    Suite *s = suite_create("bun-suite");

    // Note that "TCase" is more like a sub-suite than a single test case
    TCase *tc_header = tcase_create("header-tests");
    tcase_add_test(tc_header, test_valid_minimal);
    tcase_add_test(tc_header, test_bad_magic);
    tcase_add_test(tc_header, test_unsupported_version);
    tcase_add_test(tc_header, test_valid_alt_empty);
    tcase_add_test(tc_header, test_valid_one_asset_header);
    tcase_add_test(tc_header, test_bad_offset_alignment);
    tcase_add_test(tc_header, test_section_past_eof);
    tcase_add_test(tc_header, test_overlapping_sections);
    tcase_add_test(tc_header, test_truncated_file);
    tcase_add_test(tc_header, test_misaligned_section_size);
    tcase_add_test(tc_header, test_overlapping_with_nonprintable);
    suite_add_tcase(s, tc_header);

    // TODO: add further test cases and TCases (e.g. "assets", "compression")
    TCase *tc_assets = tcase_create("asset-tests");
    tcase_add_test(tc_assets, test_asset_name_past_string_table);
    tcase_add_test(tc_assets, test_asset_name_nonprintable);
    tcase_add_test(tc_assets, test_second_asset_empty_name);
    tcase_add_test(tc_assets, test_asset_name_oob);
    tcase_add_test(tc_assets, test_asset_empty_name);
    tcase_add_test(tc_assets, test_rle_zero_count);
    tcase_add_test(tc_assets, test_rle_bomb);
    tcase_add_test(tc_assets, test_rle_truncated);
    tcase_add_test(tc_assets, test_rle_odd_size);
    tcase_add_test(tc_assets, test_valid_binary_asset);
    tcase_add_test(tc_assets, test_valid_multi_assets_slack);
    tcase_add_test(tc_assets, test_valid_rle_asset);
    tcase_add_test(tc_assets, test_valid_rle_large_stream);
    tcase_add_test(tc_assets, test_print_summary_valid_one_asset);
    tcase_add_test(tc_assets, test_print_summary_binary_asset);
    tcase_add_test(tc_assets, test_parse_assets_zero_assets);
    tcase_add_test(tc_assets, test_print_summary_empty_file);
    tcase_add_test(tc_assets, test_parse_assets_without_header);
    tcase_add_test(tc_assets, test_zlib_unsupported);
    tcase_add_test(tc_assets, test_invalid_compression_unsupported);
    tcase_add_test(tc_assets, test_nonzero_checksum_unsupported);
    tcase_add_test(tc_assets, test_unknown_flags_unsupported);
    tcase_add_test(tc_assets, test_print_summary_null_args);
    tcase_add_test(tc_assets, test_print_summary_null_out);
    tcase_add_test(tc_assets, test_bun_open_missing_file);
    tcase_add_test(tc_assets, test_many_asset_violations_growth);
    tcase_add_test(tc_assets, test_print_summary_long_name);
    tcase_add_test(tc_assets, test_print_summary_empty_data);
    suite_add_tcase(s, tc_assets);

    // Custom tests
    TCase *tc_custom = tcase_create("our_custom_tests");
    //tcase_add_test(tc_custom, test_violation_grows_capacity);
    tcase_add_test(tc_custom, open_missing_file);
    suite_add_tcase(s, tc_custom);

    return s;
}


int main(void) {
    Suite   *s  = bun_suite();
    SRunner *sr = srunner_create(s);

    // see https://libcheck.github.io/check/doc/check_html/check_3.html#SRunner-Output for different output options.
    // e.g. pass CK_VERBOSE if you want to see successes as well as failures.
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

