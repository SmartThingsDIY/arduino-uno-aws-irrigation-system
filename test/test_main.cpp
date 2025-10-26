#include <Arduino.h>
#include <unity.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

void test_led_builtin_pin_number(void) {
    TEST_ASSERT_EQUAL(13, LED_BUILTIN);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_led_builtin_pin_number);
    UNITY_END();
}

void loop() {
    // not used in unit tests
}
