#include <check.h>

START_TEST(dummy_test) {
    ck_assert_int_eq(1, 1);
}
END_TEST

Suite *bun_suite(void) {
    Suite *s = suite_create("bun");
    TCase *tc_core = tcase_create("core");

    tcase_add_test(tc_core, dummy_test);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = bun_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}