/* Based on http://github.com/joewalness/tinytest
 *
 * TinyTest: A really really really tiny and simple no-hassle C unit-testing framework.
 *
 * Features:
 *   - No library dependencies. Not even itself. Just a header file.
 *   - Simple ANSI C. Should work with virtually every C or C++ compiler on
 *     virtually any platform.
 *   - Reports assertion failures, including expressions and line numbers.
 *   - Stops test on first failed assertion.
 *   - ANSI color output for maximum visibility.
 *   - Easy to embed in apps for runtime tests (e.g. environment tests).
 *
 * Example Usage:
 *
 *    #include "tinytest.h"
 *    #include "mylib.h"
 *
 *    void test_sheep()
 *    {
 *      ASSERT("Sheep are cool", are_sheep_cool());
 *      ASSERT_EQUALS(4, sheep.legs);
 *    }
 *
 *    void test_cheese()
 *    {
 *      ASSERT("Cheese is tangy", cheese.tanginess > 0);
 *      ASSERT_STRING_EQUALS("Wensleydale", cheese.name);
 *    }
 *
 *    int main()
 *    {
 *      RUN(test_sheep);
 *      RUN(test_cheese);
 *      return TEST_REPORT();
 *    }
 *
 * To run the tests, compile the tests as a binary and run it.
 *
 * Project home page: http://github.com/joewalnes/tinytest
 *
 * 2010, -Joe Walnes <joe@walnes.com> http://joewalnes.com
 */

#ifndef __TINYTEST_INCLUDED_H__
#define __TINYTEST_INCLUDED_H__

#include <stdio.h>
#include <stdlib.h>

/* Main assertion method */
#define ASSERT(msg, expression) if (!tap_assert(__FILE__, __LINE__, (msg), (#expression), (expression) ? 1 : 0)) return

/* Convenient assertion methods */
/* TODO: Generate readable error messages for assert_equals or assert_str_equals */
#define ASSERT_EQUALS(expected, actual) ASSERT((#actual), (expected) == (actual))
#define ASSERT_STRING_EQUALS(expected, actual) ASSERT((#actual), strcmp((expected),(actual)) == 0)

/* Run a test() function */
#define RUN(test_function) tap_execute((#test_function), (test_function))
#define TEST_REPORT() tap_report()

#define TAP_COLOR_CODE 0x1B
#define TAP_COLOR_RED "[1;31m"
#define TAP_COLOR_GREEN "[1;32m"
#define TAP_COLOR_RESET "[0m"

int tap_asserts = 0;
int tap_passes = 0;
int tap_fails = 0;
const char *tap_current_name = NULL;

void tap_execute(const char* name, void (*test_function)()) {
  tap_current_name = name;
  printf("# %s\n", name);
  test_function();
}

int tap_assert(const char* file, int line, const char* msg, const char* expression, int pass) {
  tap_asserts++;

  if (pass) {
    tap_passes++;
    printf("%c%sok%c%s %d - %s\n",
      TAP_COLOR_CODE, TAP_COLOR_GREEN,
      TAP_COLOR_CODE, TAP_COLOR_RESET,
      tap_asserts,
      msg
    );
  } else {
    tap_fails++;
    printf(
      "%c%snot ok%c%s %d - %s\n"
      "  On %s:%d, in test %s()\n"
      "    %s\n"
      ,
      TAP_COLOR_CODE, TAP_COLOR_RED,
      TAP_COLOR_CODE, TAP_COLOR_RESET,
      tap_asserts, msg,
      file, line, tap_current_name,
      expression
    );
  }

  return pass;
}

int tap_report(void) {
  printf(
    "1..%d\n"
    "# tests %d\n"
    "# pass  %d\n"
    "# fail  %d\n",
    tap_asserts,
    tap_asserts,
    tap_passes,
    tap_fails
  );
  return tap_fails ? 2 : 0;
}

#endif // __TINYTEST_INCLUDED_H__
