#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tf_total = 0;
static int tf_passed = 0;
static int tf_failed = 0;
static int tf_current_failed = 0;
static const char *tf_current_test = NULL;

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
    int _e = (int)(expected); int _a = (int)(actual); \
    if (_e != _a) { \
        printf("\n    FAIL %s:%d: expected %d, got %d", __FILE__, __LINE__, _e, _a); \
        tf_current_failed = 1; \
    } \
} while(0)

#define TEST_ASSERT_EQUAL(expected, actual) TEST_ASSERT_EQUAL_INT(expected, actual)

#define TEST_ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        printf("\n    FAIL %s:%d: expected true", __FILE__, __LINE__); \
        tf_current_failed = 1; \
    } \
} while(0)

#define TEST_ASSERT_FALSE(condition) do { \
    if ((condition)) { \
        printf("\n    FAIL %s:%d: expected false", __FILE__, __LINE__); \
        tf_current_failed = 1; \
    } \
} while(0)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) do { \
    int _e = (int)(expected); int _a = (int)(actual); \
    if (_e == _a) { \
        printf("\n    FAIL %s:%d: expected != %d", __FILE__, __LINE__, _e); \
        tf_current_failed = 1; \
    } \
} while(0)

#define TEST_ASSERT_GREATER_THAN(threshold, actual) do { \
    int _t = (int)(threshold); int _a = (int)(actual); \
    if (_a <= _t) { \
        printf("\n    FAIL %s:%d: expected > %d, got %d", __FILE__, __LINE__, _t, _a); \
        tf_current_failed = 1; \
    } \
} while(0)

#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual) do { \
    int _t = (int)(threshold); int _a = (int)(actual); \
    if (_a < _t) { \
        printf("\n    FAIL %s:%d: expected >= %d, got %d", __FILE__, __LINE__, _t, _a); \
        tf_current_failed = 1; \
    } \
} while(0)

#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual) do { \
    int _t = (int)(threshold); int _a = (int)(actual); \
    if (_a > _t) { \
        printf("\n    FAIL %s:%d: expected <= %d, got %d", __FILE__, __LINE__, _t, _a); \
        tf_current_failed = 1; \
    } \
} while(0)

#define RUN_TEST(func) do { \
    tf_current_failed = 0; \
    tf_current_test = #func; \
    printf("  %-60s ", #func); \
    func(); \
    tf_total++; \
    if (tf_current_failed) { \
        tf_failed++; \
        printf(" [FAIL]\n"); \
    } else { \
        tf_passed++; \
        printf(" [OK]\n"); \
    } \
} while(0)

#define TEST_SUITE_BEGIN(name) \
    printf("\n=== %s ===\n", name);

#define TEST_SUITE_RESULTS() do { \
    printf("\n-------------------------------------------\n"); \
    printf("Tests: %d total, %d passed, %d failed\n", tf_total, tf_passed, tf_failed); \
    printf("-------------------------------------------\n"); \
    return tf_failed > 0 ? 1 : 0; \
} while(0)

#endif
