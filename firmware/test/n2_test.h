/* Mini-cadre de test hôte (sans dépendance externe). */
#ifndef N2_TEST_H
#define N2_TEST_H

#include <stdio.h>
#include <string.h>

static int n2t_fail;
static int n2t_run;

#define TEST_ASSERT(c)                                                                  \
    do {                                                                                \
        if (!(c)) {                                                                     \
            printf("  ECHEC %s:%d : %s\n", __FILE__, __LINE__, #c);                     \
            n2t_fail++;                                                                 \
            return;                                                                     \
        }                                                                               \
    } while (0)

#define TEST_ASSERT_EQ(a, b)                                                            \
    do {                                                                                \
        long long _a = (long long)(a), _b = (long long)(b);                             \
        if (_a != _b) {                                                                 \
            printf("  ECHEC %s:%d : %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a,  \
                   #b, _a, _b);                                                         \
            n2t_fail++;                                                                 \
            return;                                                                     \
        }                                                                               \
    } while (0)

#define TEST_ASSERT_STR(a, b)                                                           \
    do {                                                                                \
        if (strcmp((a), (b)) != 0) {                                                    \
            printf("  ECHEC %s:%d : \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b));  \
            n2t_fail++;                                                                 \
            return;                                                                     \
        }                                                                               \
    } while (0)

#define RUN_TEST(fn)                                                                    \
    do {                                                                                \
        int _f = n2t_fail;                                                              \
        n2t_run++;                                                                      \
        fn();                                                                           \
        printf("%s %s\n", n2t_fail == _f ? "ok   " : "ECHEC", #fn);                     \
    } while (0)

#define TEST_END() (printf("%d test(s), %d echec(s)\n", n2t_run, n2t_fail), n2t_fail ? 1 : 0)

#endif
