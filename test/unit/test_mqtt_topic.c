/**
 * @file test_topic.c  — unit test for topic matching
 */
#include <stdio.h>
#include <string.h>
#include "mqtt/mqtt_topic.h"

static int check(const char *sub, const char *pub, int expected) {
    int r = mqtt_topic_match(sub, pub);
    int ok = (r == expected);
    printf("  %s %s %s → %s\n",
           sub, pub, expected ? "should_match" : "should_not_match",
           ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static int check_valid(const char *topic, int is_sub, int expected) {
    int r = mqtt_topic_valid(topic, is_sub);
    int ok = (r == expected);
    printf("  valid(%s, sub=%d) → expect %s → %s\n",
           topic, is_sub, expected ? "true" : "false",
           ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

int main(void) {
    int fails = 0;
    fails += check("a/b", "a/b", 1);
    fails += check("a/b", "a/c", 0);
    fails += check("a/b/c", "a/b", 0);
    fails += check("a/+", "a/x", 1);
    fails += check("a/+", "a/x/y", 0);
    fails += check("+/+", "a/b", 1);
    fails += check("+", "anything", 1);
    fails += check("a/#", "a/x", 1);
    fails += check("a/#", "a/x/y", 1);
    fails += check("#", "anything/at/all", 1);
    fails += check("a/b/#", "a/b", 1);
    fails += check("a/b/#", "a/b/c/d", 1);
    fails += check("a/#", "b/x", 0);
    fails += check("+/b", "a/c", 0);
    fails += check_valid("a/b", 0, 1);
    fails += check_valid("a/b", 1, 1);
    fails += check_valid("a/+", 0, 0);
    fails += check_valid("a/+", 1, 1);
    fails += check_valid("#", 1, 1);
    fails += check_valid("#/a", 1, 0);
    fails += check_valid("+/+", 1, 1);
    fails += check_valid("a/+b", 1, 0);
    printf("\n%d failures\n", fails);
    return fails;
}
