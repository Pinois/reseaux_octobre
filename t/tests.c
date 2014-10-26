#include <stdlib.h>
#include <check.h>
#include "../common/protocol.h"

START_TEST(test_frame_create)
  frame frame;
  char data[MAX_PAYLOAD_SIZE] = "abcdefghijklmnopqrstuvwxyz";

  create_data_frame(16, data, &frame);

  ck_assert_int_eq(frame.type,1);
  ck_assert_int_eq(frame.window,0);
  ck_assert_int_eq(frame.seq,0);
  ck_assert_int_eq(frame.length,strlen(data));
  ck_assert_str_eq(frame.payload,data);
  ck_assert(valid_frame(frame));
END_TEST

START_TEST(test_ack_create)
  frame frame1, frame2;

  create_ack_frame(20, (MAX_WINDOW_SIZE / 2) - 1 , &frame1);

  ck_assert_int_eq(frame1.type,2);
  ck_assert_int_eq(frame1.window,15);
  ck_assert_int_eq(frame1.seq,4);
  ck_assert_int_eq(frame1.length,0);
  ck_assert_str_eq(frame1.payload,"");
  ck_assert(valid_frame(frame1));
  ck_assert(!create_ack_frame(20, MAX_WINDOW_SIZE / 2 , &frame2));
END_TEST

START_TEST(test_frame_crc)
  frame frame1, frame2;
  char data[MAX_PAYLOAD_SIZE] = "abcdefghijklmnopqrstuvwxyz";

  create_data_frame(0, data, &frame1);
  create_data_frame(0, data, &frame2);

  ck_assert_int_eq(frame1.crc,frame2.crc);
END_TEST

START_TEST(test_frame_serialization)
  frame frame1, frame2;
  char data[MAX_PAYLOAD_SIZE] = "abcdefghijklmnopqrstuvwxyz";
  char temp[MAX_PAYLOAD_SIZE];

  create_data_frame(3, data, &frame1);
  serialize(frame1, temp);
  unserialize(temp, &frame2);

  ck_assert_int_eq(frame1.type,frame2.type);
  ck_assert_int_eq(frame1.window,frame2.window);
  ck_assert_int_eq(frame1.seq,frame2.seq);
  ck_assert_int_eq(frame1.length,frame2.length);
  ck_assert_str_eq(frame1.payload,frame2.payload);
  ck_assert_int_eq(frame1.crc,frame2.crc);
END_TEST

Suite * protocol_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("Frame");

  tc_core = tcase_create("Core");

  tcase_add_test(tc_core, test_frame_create);
  tcase_add_test(tc_core, test_ack_create);
  tcase_add_test(tc_core, test_frame_crc);
  tcase_add_test(tc_core, test_frame_serialization);
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
