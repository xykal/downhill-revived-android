#include "android_log_limit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct dhd_log_limit limit = {0};
    unsigned written = 0, notices = 0;
    for (unsigned i = 0; i < 10000000; i++) {
        int result = dhd_log_limit_next(&limit, 128);
        written += result == 1;
        notices += result == 2;
        assert(limit.bytes <= DHD_ANDROID_LOG_MAX);
    }
    assert(written > 0 && notices == 1);
    assert(dhd_log_limit_next(&limit, 1) == 0);
    struct dhd_log_limit huge = {0};
    assert(dhd_log_limit_next(&huge, (size_t)-1) == 2);
    assert(dhd_log_limit_next(&huge, 1) == 0);
    printf("Android log cap: passed, %u lines, %zu bytes, one notice\n", written, limit.bytes);
    return 0;
}
