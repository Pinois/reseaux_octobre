#include <stdlib.h>
#include <check.h>
#include "../common/protocol.h"

START_TEST(test_frame_create)
  frame f;
  char data[MAX_PAYLOAD_SIZE] = "abcdefghijklmnopqrstuvwxyz";
  create_data_frame(0, data, &f);
  ck_assert_int_eq(1,1); 
END_TEST

Suite * protocol_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("Frame");

  tc_core = tcase_create("Core");

  tcase_add_test(tc_core, test_frame_create);
  suite_add_tcase(s, tc_core);

  return s;
}

int main(void)
{
  int number_failed;
  Suite *s;
  SRunner *sr;

  s = protocol_suite();
  sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (number_failed == 0)?EXIT_SUCCESS : EXIT_FAILURE;
}
