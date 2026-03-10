/**
 * @file test_protocol.c
 * @brief Unity unit tests for the ASCII protocol parser.
 *
 * These tests run on the host OS via ESP-IDF's unity-based test runner.
 * Run with: idf.py -T test/ build
 */
#include "unity.h"
#include "protocol.h"

static QueueHandle_t s_queue;

void setUp(void) {
    s_queue = protocol_init();
    TEST_ASSERT_NOT_NULL(s_queue);
}

void tearDown(void) {
    /* Drain the queue between tests */
    protocol_cmd_t dummy;
    while (xQueueReceive(s_queue, &dummy, 0) == pdTRUE) {}
}

/* ---- protocol_parse_and_enqueue tests ---- */

void test_led_set_on(void) {
    protocol_parse_and_enqueue("LED:SET 1", -1);

    protocol_cmd_t cmd;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(s_queue, &cmd, 0));
    TEST_ASSERT_EQUAL(DEVICE_LED,  cmd.device);
    TEST_ASSERT_EQUAL(ACTION_SET,  cmd.action);
    TEST_ASSERT_EQUAL(1,           cmd.value);
    TEST_ASSERT_EQUAL(-1,          cmd.response_fd);
}

void test_led_set_off(void) {
    protocol_parse_and_enqueue("LED:SET 0", -1);

    protocol_cmd_t cmd;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(s_queue, &cmd, 0));
    TEST_ASSERT_EQUAL(0, cmd.value);
}

void test_led_get(void) {
    protocol_parse_and_enqueue("LED:GET", 42);

    protocol_cmd_t cmd;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(s_queue, &cmd, 0));
    TEST_ASSERT_EQUAL(DEVICE_LED,  cmd.device);
    TEST_ASSERT_EQUAL(ACTION_GET,  cmd.action);
    TEST_ASSERT_EQUAL(42,          cmd.response_fd);
}

void test_unknown_device_dropped(void) {
    protocol_parse_and_enqueue("FOO:SET 1", -1);

    protocol_cmd_t dummy;
    /* Queue must remain empty — unknown device should be discarded */
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(s_queue, &dummy, 0));
}

void test_unknown_action_dropped(void) {
    protocol_parse_and_enqueue("LED:BAD", -1);

    protocol_cmd_t dummy;
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(s_queue, &dummy, 0));
}

/* ---- protocol_format_response tests ---- */

void test_format_response_ok(void) {
    char buf[16];
    protocol_format_response(buf, sizeof(buf), true, -1);
    TEST_ASSERT_EQUAL_STRING("OK\n", buf);
}

void test_format_response_with_value(void) {
    char buf[16];
    protocol_format_response(buf, sizeof(buf), true, 1);
    TEST_ASSERT_EQUAL_STRING("1\n", buf);
}

void test_format_response_error(void) {
    char buf[16];
    protocol_format_response(buf, sizeof(buf), false, -1);
    TEST_ASSERT_EQUAL_STRING("ERR\n", buf);
}

void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_led_set_on);
    RUN_TEST(test_led_set_off);
    RUN_TEST(test_led_get);
    RUN_TEST(test_unknown_device_dropped);
    RUN_TEST(test_unknown_action_dropped);
    RUN_TEST(test_format_response_ok);
    RUN_TEST(test_format_response_with_value);
    RUN_TEST(test_format_response_error);
    UNITY_END();
}
